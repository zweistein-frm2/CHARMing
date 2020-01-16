/***************************************************************************
 *   Copyright (C) 2019 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/

#include <boost/array.hpp>
#include <boost/chrono.hpp>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/asio/basic_raw_socket.hpp>
#include "asio-rawsockets/raw.hpp"
#include "asio-rawsockets/ipv4_header.hpp"
#include <boost/foreach.hpp>
#include <boost/exception/all.hpp>
#include "Mcpd8.DataPacket.hpp"
#include "Mesytec.RandomData.hpp"
#include "Zweistein.PrettyBytes.hpp"
#include "Zweistein.GetLocalInterfaces.hpp"
#include "Zweistein.deHumanizeNumber.hpp"
#include "Mcpd8.CmdPacket.hpp"
#include <boost/exception/error_info.hpp>
#include <errno.h>
#include "Zweistein.bitreverse.hpp"
#include "rfc1071_checksum.hpp"
#include "cmake_variables.h"
#include "Mesytec.Mpsd8.hpp"

using boost::asio::ip::udp;
boost::mutex coutGuard;
boost::thread_group worker_threads;

boost::array< Mcpd8::CmdPacket, 1> cmd_recv_buf;

udp::socket* psocket=nullptr;
udp::endpoint local_endpoint;

boost::atomic<unsigned short> daq_running= Mcpd8::Status::DAQ_Stopped;
boost::atomic<unsigned short> devid = 0;
boost::atomic<unsigned short> runid = 0;
boost::atomic<long> requestedEventspersecond = 1 * 100;
boost::atomic<int> nevents = 250;
boost::atomic<int> coutevery_npackets = requestedEventspersecond / nevents;
int minRateperSecond = 51;



const int N_MPSD8 = 8;

Mesytec::Mpsd8::Module Module[N_MPSD8];
unsigned short Module_Id[N_MPSD8] = { Mesy::ModuleId::MPSD8 ,Mesy::ModuleId::MPSD8 ,Mesy::ModuleId::MPSD8 ,Mesy::ModuleId::MPSD8 ,
									Mesy::ModuleId::MPSD8 ,Mesy::ModuleId::MPSD8 ,Mesy::ModuleId::MPSD8 ,Mesy::ModuleId::MPSD8 };



void setRate(long requested) {
	std::cout << "Hello from Charm PacketSender, requested: " << requested << " Events/s" << std::endl;
	int tmp = nevents;
	while (requested< minRateperSecond * tmp) {
		tmp--;
	};

	if (tmp < 1) std::cout<<"NO ACTION: Eventrate too low (Minimum Eventrate is " + std::to_string(minRateperSecond) + " Events/second)"<<std::endl;
	else {
		nevents = tmp;
		requestedEventspersecond = requested;
		daq_running = Mcpd8::Status::DAQ_Running;
	}
	coutevery_npackets = requestedEventspersecond / nevents;
	if (coutevery_npackets < minRateperSecond) 	coutevery_npackets = minRateperSecond;
}

void start_receive();

void handle_receive(const boost::system::error_code& error,
	std::size_t bytes_transferred) {
	
	Mcpd8::CmdPacket& cp = cmd_recv_buf[0];
	if (bytes_transferred>0 && cp.Type== Zweistein::reverse_u16(Mesy::BufferType::COMMAND)) {
		
		bool sendanswer = true;
		{
			boost::mutex::scoped_lock lock(coutGuard);
			std::cout << cp << std::endl;
			if (bytes_transferred != cp.Length * sizeof(short)) {
				std::cout << "cp.Length(" << cp.Length << " word(s) !=" << bytes_transferred << "(=" << bytes_transferred/sizeof(short)<<" word(s))" << std::endl;
			}
		}
		switch (cp.cmd) {
		case Mcpd8::Cmd::START:
			daq_running = Mcpd8::Status::DAQ_Running;
			sendanswer = false;
			break;
		case Mcpd8::Cmd::STOP:
			daq_running = Mcpd8::Status::DAQ_Stopped;
			sendanswer = false;
			break;
		case Mcpd8::Cmd::SETID: 
			devid=cp.data[0];
			break;

		case Mcpd8::Cmd::SETPROTOCOL:
		{
			cp.data[0] = 0;
			cp.data[4] = 0;
			cp.data[8] = 0;
			cp.data[9] = 0;
			cp.data[10] = 0;
			break;

		}
		case Mcpd8::Cmd::SETRUNID:
			runid= cp.data[0];
			break;


		case Mcpd8::Cmd::SETCELL: {
			break;
		}

		case Mcpd8::Cmd::SETAUXTIMER: {
			break;
		}
		case Mcpd8::Cmd::SETPARAMETERS: {
			break;
		}

		
		case Mcpd8::Cmd::GETCAPABILITIES:
			cp.data[0] = Mcpd8::TX_CAP::POS_OR_AMP| Mcpd8::TX_CAP::TOF_POS_OR_AMP| Mcpd8::TX_CAP::TOF_POS_AND_AMP;
			cp.data[1]= Mesytec::Mpsd8::Module::tx_cap_default;
			cp.Length = Mcpd8::CmdPacket::defaultLength+2;
			break;

		case Mcpd8::Cmd::SETCAPABILITIES:
			cp.data[0] = Mesytec::Mpsd8::Module::tx_cap_default;
			cp.Length = Mcpd8::CmdPacket::defaultLength + 1;
			break;

		case Mcpd8::Cmd::GETPARAMETERS:
		{
			cp.data[0] = 0; //ADC1
			cp.data[1] = 0; //ADC2
			cp.data[2] = 0; //DAC1
			cp.data[3] = 0; //DAC2
			cp.data[4] = 0; // TTLOUTPUTS (2bits)
			cp.data[5] = 0; // TTL inputs (6 bits)
			long long eventcounter = 0;
			long long param0 = 0;
			long long param1 = 0;
			long long param2 = 0;
			long long param3 = 0;

			Mcpd8::CmdPacket::setparameter(&cp.data[6], eventcounter);
			Mcpd8::CmdPacket::setparameter(&cp.data[9], param0);
			Mcpd8::CmdPacket::setparameter(&cp.data[12], param1);
			Mcpd8::CmdPacket::setparameter(&cp.data[15], param2);
			Mcpd8::CmdPacket::setparameter(&cp.data[18], param3);
			cp.Length = Mcpd8::CmdPacket::defaultLength + 21;
			break;
		}

		case Mcpd8::Cmd::SETGAIN_MPSD: {
			assert(cp.data[0] >= 0 && cp.data[0] < N_MPSD8);
			if (cp.data[1] == N_MPSD8) {
				for (int i = 0; i < N_MPSD8; i++) Module[cp.data[0]].gain[i] = cp.data[2];
				
			}
			else Module[cp.data[0]].gain[cp.data[1]] = cp.data[2];


			break;
		}

		case Mcpd8::Cmd::SETTHRESH: {
			assert(cp.data[0] >= 0 && cp.data[0] < N_MPSD8);
		    Module[cp.data[0]].threshold = cp.data[1];


			break;
		}
		case Mcpd8::Cmd::SETMODE: {
			if (cp.data[0] == N_MPSD8) for (int i = 0; i < N_MPSD8; i++) Module[i].mode = (Mesytec::Mpsd8::Mode) cp.data[1];
			assert(cp.data[0] >= 0 && cp.data[0] < N_MPSD8);
			break;
		}
		/*
		case Mcpd8::Cmd::SETPULSER: {
			assert(cp.data[0] >= 0 && cp.data[0] < N_MPSD8);
			Module[cp.data[0]].pulserchannel = cp.data[1];
			Module[cp.data[0]].position = (Mesytec::Mpsd8::PulserPosition) cp.data[2];
			Module[cp.data[0]].pulseramplitude = cp.data[3];
			Module[cp.data[0]].pulseron = cp.data[4];
		
			break;
		}
		

		
		
		case Mcpd8::Cmd::GETMPSD8PLUSPARAMETERS:
		{
			assert(cp.data[0] >= 0 && cp.data[0] < N_MPSD8);
			//cp.data[0] is Mpsd device num
			cp.data[1] = Module[cp.data[0]].tx_caps;
			cp.data[2] = Module[cp.data[0]].tx_cap_setting;
			cp.data[3] = Module[cp.data[0]].firmware; // firmware
			cp.Length = Mcpd8::CmdPacket::defaultLength+ 4;
			break;
		}
		*/
		case Mcpd8::Internal_Cmd::READID:
		{
			for (int i = 0; i < N_MPSD8; i++) {
				cp.data[i] = Module_Id[i];
			}
			cp.Length = Mcpd8::CmdPacket::defaultLength + N_MPSD8;
			break;
		}
		case Mcpd8::Internal_Cmd::GETVER:
		{
			cp.data[0] = 8; //Version major
			cp.data[1] = 20; // Version minor
			unsigned char fpgamaj = 5;
			unsigned char fpgamin = 1;
			cp.data[2] = (fpgamaj << 8) | fpgamin;
			cp.Length = Mcpd8::CmdPacket::defaultLength+ 3;
			
			{
				boost::mutex::scoped_lock lock(coutGuard);

				// so liest es qmesydaq (falsch ein) 9 => 90 was nicht korrekt ist             ------------------------
				// code aus mcpd8.cpp line# 2015 ff
				float m_version = cp.data[1];
				while (m_version > 1)	m_version /= 10.;
				m_version += cp.data[0];
				float m_fpgaVersion = cp.data[2] & 0xFF;
				while (m_fpgaVersion > 1)m_fpgaVersion /= 10.;
				m_fpgaVersion += cp.data[2] >> 8;
				//-----------------------------------------------------------------------------------------------------
				std::cout << "Version: "<<m_version<<", FPGA Version:"<<m_fpgaVersion<<std::endl;

			}
			break;
		}
		

		case Mcpd8::Internal_Cmd::READPERIREG:
		{
			//cp.data[0] = channel   //mcpsd8 channel (up to 8)
			assert(cp.data[0] >= 0 && cp.data[0] < N_MPSD8);
			cp.data[2] = 0;
			switch (cp.data[1]) {
				case 0: 
					cp.data[2] = Module[cp.data[0]].tx_caps; 
					break;
				case 1:
					cp.data[2] = Module[cp.data[0]].tx_cap_setting;
					break;
				case 2:
					cp.data[2] = Module[cp.data[0]].firmware;
					break;
			}
			cp.Length= Mcpd8::CmdPacket::defaultLength + 3;
			break;
		}
		/*
		case Mcpd8::Internal_Cmd::WRITEPERIREG:
		{
			//cp.data[0] = channel   //mcpsd8 channel (up to 8)
			assert(cp.data[0] >= 0 && cp.data[0] < N_MPSD8);
			Module[cp.data[0]].reg=cp.data[2];
			cp.Length = Mcpd8::CmdPacket::defaultLength + 3;
			break;
		}
		*/

			
		case Mcpd8::Internal_Cmd::SETNUCLEORATEEVENTSPERSECOND:
		{
			assert(cp.Length > Mcpd8::CmdPacket::defaultLength+ 2);
			long rate = cp.data[1] << 16 | cp.data[0];
			setRate(rate);
			break;
		}
			
		}
		if(sendanswer) Mcpd8::CmdPacket::Send(psocket,cp, local_endpoint);
	}


	start_receive();
}
void start_receive() {
	psocket->async_receive_from(boost::asio::buffer(cmd_recv_buf), local_endpoint, boost::bind(&handle_receive,
		boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
};

template <class TheBuffer>
size_t raw_sendto(boost::asio::basic_raw_socket<asio::ip::raw>& sender, const udp::endpoint& spoofed_endpoint, udp::endpoint& receiver_endpoint, TheBuffer&& payload) {

	udp_header udp;
	udp.source_port(spoofed_endpoint.port());
	udp.destination_port(receiver_endpoint.port());
	udp.length((unsigned short)(udp.size() + payload.size())); // Header + Payload
	udp.checksum(0); // Optioanl for IPv4
	unsigned short sum = comp_chksum2buffers(udp.data().data(), udp.size(), payload.data(), payload.size());
	udp.checksum(sum);

	// Create the IPv4 header.
	ipv4_header ip;
	ip.version(4);                   // IPv4
	ip.header_length(ip.size() / 4); // 32-bit words
	ip.type_of_service(0);           // Differentiated service code point
	auto total_length = ip.size() + udp.size() + payload.size();
	ip.total_length(total_length); // Entire message.
	ip.identification(0);
	ip.dont_fragment(true);
	ip.more_fragments(false);
	ip.fragment_offset(0);
	ip.time_to_live(4);
	ip.source_address(spoofed_endpoint.address().to_v4());
	ip.destination_address(receiver_endpoint.address().to_v4());
	ip.protocol(IPPROTO_UDP);
	calculate_checksum(ip);
	
	boost::array<boost::asio::const_buffer, 3> buffers = { {
	boost::asio::buffer(ip.data()),
	boost::asio::buffer(udp.data()),
	boost::asio::buffer(payload)
  } };

	
	auto bytes_transferred = sender.send_to(buffers,
		asio::ip::raw::endpoint(receiver_endpoint.address(), receiver_endpoint.port()));
	assert(bytes_transferred == total_length);
	return bytes_transferred;
}

int main(int argc, char* argv[])
{

	char* cLocal = setlocale(LC_ALL, NULL);
	setlocale(LC_ALL, "de-DE");
	boost::filesystem::path::imbue(std::locale());

	const unsigned short port = 54321;
	
	boost::array< Mcpd8::DataPacket, 1> dp;

	

	if (argc == 1) {
		std::cout << "Specify Event Rate per second , 2K -> 2000, 1M->1000000" << std::endl;
	}
	if (argc == 2) {
		long long num = 0;
		std::string argv1(argv[1]);
		requestedEventspersecond=(long)Zweistein::Dehumanize(argv1);
		//requestedEventspersecond = std::stoi(argv[1]);
	}

	setRate(requestedEventspersecond);
	
	
	unsigned short bufnum = 0;
	int length = 0;

	try {

		boost::asio::io_service io_service;
		boost::asio::signal_set signals(io_service, SIGINT, SIGTERM);
		signals.async_wait(boost::bind(&boost::asio::io_service::stop, &io_service));
		
		std::string local_ip = defaultNETWORKCARDINTERFACE;
		std::list<std::string> localinterfaces = std::list<std::string>();
		//Zweistein::GetLocalInterfaces(localinterfaces, io_service);
		Zweistein::GetLocalInterfaces(localinterfaces);
		
		auto _a = std::find(localinterfaces.begin(), localinterfaces.end(), local_ip);
		if (_a == localinterfaces.end()) {
			std::cout << "interfaces on this machine are:";
			int i = 0;
			BOOST_FOREACH(std::string str, localinterfaces) { std::cout << str << "(" << i++ << ")  "; }
			int choice = 0;

			if (i > 1) {
				std::cout << std::endl << "Choose interface to use: (0),... ";
				std::cin >> choice;
			}
			i = 0;
			BOOST_FOREACH(std::string str, localinterfaces) { if (i++ == choice) local_ip = str; }
		}
		

		std::cout << "using interface " << local_ip << std::endl;
		

		udp::socket socket(io_service);
#ifdef _SPOOF_IP
		std::string spoofed_ip = "192.168.168.121";
		auto b = asio::ip::raw::endpoint(asio::ip::raw::v4(), 0);
		boost::asio::basic_raw_socket<asio::ip::raw> rawsocket(io_service,
			b);

		const boost::asio::ip::udp::endpoint spoofed_endpoint(
			boost::asio::ip::address_v4::from_string(spoofed_ip),
			port);
#endif
		local_endpoint = udp::endpoint(boost::asio::ip::address::from_string(local_ip), port);
		

		boost::function<void()> t;
		t = [&io_service, &port,&socket]() {

			try {
				socket.open(udp::v4());
				//socket.set_option(boost::asio::socket_base::broadcast(true));
				socket.set_option(boost::asio::socket_base::reuse_address(true));
				socket.bind(local_endpoint);
				psocket = &socket;
				start_receive();
				io_service.run();
			}
			catch (boost::exception & e) {
				boost::mutex::scoped_lock lock(coutGuard);
				std::cout << boost::diagnostic_information(e);
			}

		};

		worker_threads.create_thread(boost::bind(t));
		boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));



		int iloop = 0;
		int innerloop = 0;
		int maxdots = 10;
		
		
		
		std::cout << "packages contain " << nevents << " events each." << std::endl;
		std::cout << "coutevery_npackets=" << coutevery_npackets << std::endl;
		long long currentcount = 0;
		long long lastcount = 0;
		long delaynanos = 0;
		boost::chrono::system_clock::time_point start = boost::chrono::system_clock::now();
		const int maxX = 64; //8 slots with 8 channels each

		do {

			dp[0].Number = bufnum++;
			for (int i = 0; i < nevents; i++) {
				Zweistein::Random::Mpsd8EventRandomData((unsigned short*)(&dp[0].events[i]), i, maxX);

			}
			dp[0].deviceStatusdeviceId = daq_running | (devid << 8); // set to running
			dp[0].Length = nevents * 3 + dp[0].headerLength;
			dp[0].Type = Mesy::BufferType::DATA;
			Mcpd8::DataPacket::settimeNow48bit(dp[0].time);
			if (daq_running) {
#ifdef _SPOOF_IP
				//size_t bytessent = raw_sendto(rawsocket, spoofed_endpoint, local_endpoint, boost::asio::buffer((void*)(&dp[0]), dp[0].Length * sizeof(unsigned short)));
#else
				size_t bytessent = socket.send_to(boost::asio::buffer((void*)(&dp[0]), dp[0].Length * sizeof(unsigned short)), local_endpoint);
#endif
				currentcount += nevents;
			}
			
			if (iloop % coutevery_npackets == 0) {
				boost::this_thread::sleep_for(boost::chrono::nanoseconds(delaynanos));
				boost::chrono::duration<double> sec = boost::chrono::system_clock::now() - start;
				start = boost::chrono::system_clock::now();
				double evtspersecond = sec.count() != 0 ? (double)(currentcount - lastcount) / sec.count() : 0;
				if (evtspersecond != 0) {
					long nanos = 1000000000L * (currentcount - lastcount) / evtspersecond;
					long wanted = 1000000000L * (currentcount - lastcount) / requestedEventspersecond;
					delaynanos += wanted - nanos;
					if (delaynanos <= 0) delaynanos = 0;
				}
				else delaynanos = 1000L * 1000L * 1000L; // 1 second
				lastcount = currentcount;
				int dots = innerloop % maxdots;
				std::cout << "\r" << evtspersecond << " Events/s, (" << Zweistein::PrettyBytes((size_t)(evtspersecond * sizeof(Mesy::Mpsd8Event))) << "/s)" << "\t";
				if (innerloop != 0 /*first gives wrong results*/) for (int i = 0; i < maxdots; i++) std::cout << (dots == i ? "." : " ");
				std::cout.flush();
				innerloop++;

			}
			iloop++;

		} while (!io_service.stopped());

		return 0;
	}

	catch (boost::system::system_error const& e) {
		std::cout << e.what() << ": " << e.code() << " - " << e.code().message() << "\n";
		auto a = e.code();
		int err = a.value();
		if (err == 10013) {

			std::cout << "You must run program with administrator rights" << std::endl;
		}
	}
	catch (boost::exception & e) {
		boost::mutex::scoped_lock lock(coutGuard);
		std::cout << boost::diagnostic_information(e);
		
	}
	return -1;
}

