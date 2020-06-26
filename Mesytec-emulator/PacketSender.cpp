/***************************************************************************
 *   Copyright (C) 2019 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/
#include <boost/locale.hpp>
#include <boost/array.hpp>
#include <boost/chrono.hpp>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/asio/basic_raw_socket.hpp>
#include "asio-rawsockets/raw.hpp"
#include "asio-rawsockets/ipv4_header.hpp"
#include <boost/foreach.hpp>
#include <boost/exception/all.hpp>
#include <boost/function.hpp>
#include <iostream>
#include "Mcpd8.DataPacket.hpp"
#include "Mesytec.RandomData.hpp"
#include "Zweistein.PrettyBytes.hpp"
#include "Zweistein.GetLocalInterfaces.hpp"
#include "Zweistein.deHumanizeNumber.hpp"
#include "Mcpd8.CmdPacket.hpp"
#include <boost/system/error_code.hpp>
#include <boost/exception/error_info.hpp>
#include <errno.h>
#include "Zweistein.bitreverse.hpp"
#include "rfc1071_checksum.hpp"
#include "cmake_variables.h"
#include "Mesytec.Mpsd8.hpp"
#include "Mcpd8.enums.hpp"
#include "PacketSender.Params.hpp"
#include "CounterMonitor.hpp"

using boost::asio::ip::udp;
boost::mutex coutGuard;
boost::thread_group worker_threads; // not used

boost::array< Mcpd8::CmdPacket, 1> cmd_recv_buf;

udp::socket* psocket = nullptr;
udp::endpoint current_remote_endpoint;
udp::endpoint remote_endpoint;
udp::endpoint listen_endpoint;

boost::chrono::nanoseconds zeropoint = boost::chrono::nanoseconds::zero();


boost::atomic<unsigned short> daq_status = Mcpd8::Status::sync_ok;
boost::atomic<unsigned short> devid = 0;
boost::atomic<unsigned short> runid = 0;

Mcpd8::EventDataFormat dataformat = Mcpd8::EventDataFormat::Mpsd8;

boost::atomic<long> requestedEventspersecond = 1; // default value at startup
int MAX_NEVENTS = 250;
boost::atomic<int> nevents = MAX_NEVENTS;
boost::atomic<int> coutevery_npackets = requestedEventspersecond / nevents;
boost::atomic<int>DataeveryNOnly = 1;
int minHeartbeatRate = 51;

const int N_MPSD8 = 8;

Mesytec::Mpsd8::Module Module[N_MPSD8];
unsigned short Module_Id[N_MPSD8] = { Mesy::ModuleId::MPSD8 ,Mesy::ModuleId::MPSD8 ,Mesy::ModuleId::MPSD8 ,Mesy::ModuleId::MPSD8 ,
									Mesy::ModuleId::MPSD8 ,Mesy::ModuleId::MPSD8 ,Mesy::ModuleId::MPSD8 ,Mesy::ModuleId::MPSD8 };

unsigned short adc[2];
unsigned short daq[2];
unsigned short ttloutputs;
unsigned short ttlinputs;
unsigned short eventcounter[3];
unsigned short param[4][3];

unsigned short auxtimer[4];

void fill48bit(unsigned short data[3], long long value) {

	value &= 0x0000FFFFFFFFFFFF;
	data[0]=
	data[1]=
	data[2]=

}

boost::atomic<bool> retry = true;

void setRate(long requested) {
	boost::mutex::scoped_lock lock(coutGuard);
	std::string df = "MPSD8";
	if(dataformat == Mcpd8::EventDataFormat::Mdll) df="MDLL";

	std::cout << "Hello from Charm-mesytec emulator, requested "<<df<<"  : " << requested << " Events/s" << std::endl;
	if (requested < 1) {
		std::cout << "OUT OF RANGE: setting new rate to 1 Event/second" << std::endl;
		requested = 1;
	}
	int tmp = MAX_NEVENTS;

	if (requested < minHeartbeatRate) { tmp = 0; }
	else {
		while (requested < minHeartbeatRate * tmp) {
			tmp--;
		};
	}

	

	if (tmp < 1) {
		tmp = 1;
		// so some rate between minHeartbeatRate and 0
		DataeveryNOnly = minHeartbeatRate / requested;
	}
	else DataeveryNOnly = 1;
	nevents = tmp;
	requestedEventspersecond = requested;
	coutevery_npackets = requestedEventspersecond / nevents;
	if (coutevery_npackets < minHeartbeatRate) 	coutevery_npackets = minHeartbeatRate;
	std::cout << "packages contain " << nevents << " events each." << std::endl;
	std::cout << "coutevery_npackets=" << coutevery_npackets << std::endl;
	std::cout << "DataeveryNOnly=" << DataeveryNOnly << std::endl;
	
}

void start_receive();

void handle_receive(const boost::system::error_code& error,
	std::size_t bytes_transferred) {
	Mcpd8::CmdPacket cp = cmd_recv_buf[0];
	if (cp.Type != Mesy::BufferType::COMMAND) {
		boost::mutex::scoped_lock lock(coutGuard);
			//std::cout << "cp.Type="<< std::bitset<16>(cp.Type)<<" handle_receive(error=" << error << ", bytes_transferred=" << bytes_transferred << ")" << " SKIPPED: NOT A COMMAND PACKET" << std::endl;
	}
	else if (bytes_transferred > sizeof(Mcpd8::CmdPacket)) {
		boost::mutex::scoped_lock lock(coutGuard);
		std::cout << "handle_receive(" << error << "," << bytes_transferred << ")" << " SKIPPED: >sizeof(Mcpd8::CmdPacket)" << std::endl;
	}
	else {
		
			bool sendanswer = true;
			{
				boost::mutex::scoped_lock lock(coutGuard);

				if (bytes_transferred != cp.Length * sizeof(short)) {
					std::cout << "RECEIVED:" << "cp.Length(" << cp.Length << " word(s) !=" << bytes_transferred << "(=" << bytes_transferred / sizeof(short) << " word(s))" << std::endl;
					cp.headerLength = Mcpd8::CmdPacket::defaultLength;
					cp.Length = Mcpd8::CmdPacket::defaultLength;

				}
				std::cout <<std::endl << "RECEIVED:" << cp << std::endl;
			}
			
			if (cp.cmd!= Mcpd8::Cmd::SETID && (cp.deviceStatusdeviceId >> 8) != devid) {
					cp.cmd |= (unsigned short) 0x8000; 
					boost::mutex::scoped_lock lock(coutGuard);
					std::cout << "device id mismatch: our id= " << devid << std::endl;
					// following switch will not find any values then
			}
			
			

			switch (cp.cmd) {
			case Mcpd8::Cmd::CONTINUE:
			case Mcpd8::Cmd::START:
				daq_status|= Mcpd8::Status::DAQ_Running;
				remote_endpoint=current_remote_endpoint;
				break;
			case Mcpd8::Cmd::STOP:
				daq_status &= ~Mcpd8::Status::DAQ_Running;
				remote_endpoint = current_remote_endpoint;
				break;
			case Mcpd8::Cmd::SETID:
				if (cp.data[0] > 7) {
					cp.cmd |= (unsigned short)0x8000;
					boost::mutex::scoped_lock lock(coutGuard);
					std::cout << "device id ("<< cp.data[0]<<" requested must be < 8" << std::endl;
					// following switch will not find any values then
				}
				else {
					PacketSenderParams::setDevId(cp.data[0]);
					devid = cp.data[0];
				}
				cp.Length = Mcpd8::CmdPacket::defaultLength + 1;
				remote_endpoint = current_remote_endpoint;
				
				break;

			case Mcpd8::Cmd::SETPROTOCOL:
			{
				cp.data[0] = 0;
				cp.data[4] = 0;
				cp.data[8] = 0;
				cp.data[9] = 0;
				cp.data[10] = 0;
				cp.Length = Mcpd8::CmdPacket::defaultLength + 14;
				remote_endpoint = current_remote_endpoint;
				break;

			}
			case Mcpd8::Cmd::SETRUNID:
				runid = cp.data[0];
				cp.Length = Mcpd8::CmdPacket::defaultLength + 1;
				break;



			case Mcpd8::Cmd::SETCLOCK:
			{


				boost::chrono::nanoseconds requested = Mcpd8::DataPacket::timeStamp(&cp.data[0]);
				boost::chrono::nanoseconds ns = boost::chrono::steady_clock::now().time_since_epoch();
				zeropoint = ns - requested;
				break;
			}
			case Mcpd8::Cmd::SETCELL: {
				//
				break;
			}

			case Mcpd8::Cmd::SETAUXTIMER: {
				assert(cp.data[0] >= 0 && cp.data[0] < 4);
				auxtimer[cp.data[0]] = cp.data[1];
				cp.Length = Mcpd8::CmdPacket::defaultLength + 2;
				break;
			}
			case Mcpd8::Cmd::SETPARAMETERS: {
				break;
			}


			case Mcpd8::Cmd::GETCAPABILITIES:
				cp.data[0] = Mcpd8::TX_CAP::P | Mcpd8::TX_CAP::TP | Mcpd8::TX_CAP::TPA;
				cp.data[1] = Mesytec::Mpsd8::Module::tx_cap_default;
				cp.Length = Mcpd8::CmdPacket::defaultLength + 2;
				break;

			case Mcpd8::Cmd::SETCAPABILITIES:
				cp.data[0] = Mesytec::Mpsd8::Module::tx_cap_default;
				cp.Length = Mcpd8::CmdPacket::defaultLength + 1;
				break;

			case Mcpd8::Cmd::GETPARAMETERS:
			{
				cp.data[0] = adc[0]; //ADC1
				cp.data[1] = adc[1]; //ADC2
				cp.data[2] = daq[0]; //DAC1
				cp.data[3] = daq[1]; //DAC2
				cp.data[4] = ttloutputs; // TTLOUTPUTS (2bits)
				cp.data[5] = ttlinputs; // TTL inputs (6 bits)
				memcpy(&cp.data[6], &eventcounter[0], sizeof(unsigned short) * 3);
				memcpy(&cp.data[9], &param[0][0], sizeof(unsigned short) * 12);
				/*
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
				*/
				cp.Length = Mcpd8::CmdPacket::defaultLength + 21;
				break;
			}

			case Mcpd8::Cmd::SETGAIN_MPSD: {
				assert(cp.data[0] >= 0 && cp.data[0] < N_MPSD8);
				if (cp.data[1] == N_MPSD8) {
					for (int i = 0; i < N_MPSD8; i++) Module[cp.data[0]].gain[i] = (unsigned char) cp.data[2];
				}
				else Module[cp.data[0]].gain[cp.data[1]] = (unsigned char)cp.data[2];
				cp.Length = Mcpd8::CmdPacket::defaultLength + 3;

				break;
			}

			
			case Mcpd8::Cmd::SETTHRESH: {
				assert(cp.data[0] >= 0 && cp.data[0] < N_MPSD8);
				Module[cp.data[0]].threshold = (unsigned char) cp.data[1];
				cp.Length = Mcpd8::CmdPacket::defaultLength + 2;

				break;
			}
			case Mcpd8::Cmd::SETMODE: {
				if (cp.data[0] == N_MPSD8) for (int i = 0; i < N_MPSD8; i++) Module[i].mode = (Mesytec::Mpsd8::Mode) cp.data[1];
				else {
					assert(cp.data[0] >= 0 && cp.data[0] < N_MPSD8);
					Module[cp.data[0]].mode = (Mesytec::Mpsd8::Mode) cp.data[1];
				}
				cp.Length = Mcpd8::CmdPacket::defaultLength + 2;
				break;
			}

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
				if (Module_Id[cp.data[0]] != Mesy::ModuleId::MPSD8P) {
					cp.cmd |= 0x8000;
					break;
				}
				//cp.data[0] is Mpsd device num
				cp.data[1] = Module[cp.data[0]].tx_caps;
				cp.data[2] = Module[cp.data[0]].tx_cap_setting;
				cp.data[3] = Module[cp.data[0]].firmware; // firmware
				cp.Length = Mcpd8::CmdPacket::defaultLength + 4;
				break;
			}

			case Mcpd8::Internal_Cmd::READID:
			{
				for (int i = 0; i < N_MPSD8; i++) {	cp.data[i] = Module_Id[i];	}
				cp.data[N_MPSD8] = 0xadea;
				{
					boost::mutex::scoped_lock lock(coutGuard);
					for (int i = 0; i < N_MPSD8; i++) {
						std::cout << Module_Id[i] << " ";
					}
				}
				cp.Length = Mcpd8::CmdPacket::defaultLength + N_MPSD8 + 1;
				break;
			}
			case Mcpd8::Internal_Cmd::GETVER:
			{
				cp.data[0] = 8; //Version major
				cp.data[1] = 20; // Version minor
				unsigned char fpgamaj = 5;
				unsigned char fpgamin = 1;
				cp.data[2] = (fpgamaj << 8) | fpgamin;
				cp.Length = Mcpd8::CmdPacket::defaultLength + 3;

				{
					boost::mutex::scoped_lock lock(coutGuard);

					/* so liest es qmesydaq (falsch ein) 9 => 90 was nicht korrekt ist             ------------------------
					// code aus mcpd8.cpp line# 2015 ff
					float m_version = cp.data[1];
					while (m_version > 1)	m_version /= 10.;
					m_version += cp.data[0];
					float m_fpgaVersion = cp.data[2] & 0xFF;
					while (m_fpgaVersion > 1)m_fpgaVersion /= 10.;
					m_fpgaVersion += cp.data[2] >> 8;
					//-----------------------------------------------------------------------------------------------------
					std::cout << "Version: " << m_version << ", FPGA Version:" << m_fpgaVersion << std::endl;
					*/
					std::cout << "Version:" << cp.data[0] << "." << cp.data[1] << " FPGA Version:" << (cp.data[2]>>8)<<"."<<(cp.data[2]&0xff) << std::endl;

				}
				remote_endpoint = current_remote_endpoint;
				break;
			}


			case Mcpd8::Internal_Cmd::READPERIREG:
			{
				//cp.data[0] = channel   //mcpsd8 channel (up to 8)
				
				if (cp.data[0] < 0 || cp.data[0] >= N_MPSD8) {
					cp.cmd |= 0x8000;
					break;
				}
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
				cp.Length = Mcpd8::CmdPacket::defaultLength + 3;
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
				if(cp.Length < Mcpd8::CmdPacket::defaultLength + 2) {
					cp.cmd |= 0x8000;
					boost::mutex::scoped_lock lock(coutGuard);
					std::cout << "needs  2 data words as parameter" << std::endl;
					break;
				}
				
				long rate = cp.data[1] << 16 | cp.data[0];
				setRate(rate);
				break;
			}

			}
			if (sendanswer) {
				cp.deviceStatusdeviceId = daq_status | (devid << 8);
				boost::chrono::nanoseconds ns = boost::chrono::steady_clock::now().time_since_epoch();
				Mcpd8::DataPacket::setTimeStamp(cp.time, ns - zeropoint);
				Mcpd8::CmdPacket::Send(psocket, cp, current_remote_endpoint);
				{
					boost::mutex::scoped_lock lock(coutGuard);
					std::cout<<"\r"<< boost::chrono::time_fmt(boost::chrono::timezone::local) << boost::chrono::system_clock::now() << " SENDING ANSWER:" << cp << std::endl;
				}
			}
		
	}
	if(retry==true)start_receive();
}
void start_receive() {
	psocket->async_receive_from(boost::asio::buffer(cmd_recv_buf), current_remote_endpoint, boost::bind(&handle_receive,
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





void catch_ctrlc(const boost::system::error_code& error, int signal_number) {
	{	
		boost::mutex::scoped_lock lock(coutGuard);
		std::cout << "handling signal " << signal_number << std::endl;
	}
	if (signal_number == 2) {
		retry = false;
	//	boost::throw_exception(std::runtime_error("aborted by user"));
	//	throw std::runtime_error("aborted by user");
		//throw boost::system::system_error{ boost::system::errc::make_error_code(boost::system::errc::owner_dead) };
		
	}
	
}


int main(int argc, char* argv[])
{

	try {
		boost::locale::generator gen;
		std::locale loc = gen("de-DE");
		std::locale::global(loc);
		boost::filesystem::path::imbue(loc);
	}
	catch (std::exception& ex) {
		LOG_ERROR << ex.what() << std::endl;
	}
	
	std::string appName = boost::filesystem::basename(argv[0]);
	PacketSenderParams::ReadIni(appName,"charm");
	devid = PacketSenderParams::getDevId();
	const unsigned short port = 54321;
	boost::array< Mcpd8::DataPacket, 1> dp;

	zeropoint= boost::chrono::steady_clock::now().time_since_epoch();
	
	if (argc == 1) {
		boost::mutex::scoped_lock lock(coutGuard);
		std::cout << "Specify Event Rate per second , 2K -> 2000, 1M->1000000  [MDLL]" << std::endl;
	}
	if (argc >= 2) {
		long long num = 0;
		std::string argv1(argv[1]);
		requestedEventspersecond = (long)Zweistein::Dehumanize(argv1);
		//requestedEventspersecond = std::stoi(argv[1]);
	}
	
	if (argc == 3) {
		std::string argv2 = argv[2];
		if (argv2 == "MDLL") 	dataformat = Mcpd8::EventDataFormat::Mdll;
	}
			
	{
		boost::mutex::scoped_lock lock(coutGuard);
		std::cout << "waiting for START command" << std::endl;
	}
	setRate(requestedEventspersecond);

	unsigned short bufnum = 0;
	int length = 0;
	
	do{
		boost::thread_group worker_threads2;
		try {
			
			boost::asio::io_service io_service;
			boost::asio::signal_set signals(io_service, SIGINT, SIGSEGV);
			//signals.async_wait(boost::bind(&boost::asio::io_service::stop, &io_service));
			signals.async_wait(&catch_ctrlc);

			std::string local_ip = defaultNETWORKCARDINTERFACE;
			std::list<std::string> localinterfaces = std::list<std::string>();
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

			listen_endpoint = udp::endpoint(boost::asio::ip::address::from_string(local_ip), port);

			boost::function<void()> t;
			t = [&io_service, &port, &socket]() {

				try {
					socket.open(listen_endpoint.protocol());
					socket.set_option(boost::asio::socket_base::reuse_address(true));
					socket.bind(listen_endpoint);
					//socket.set_option(	boost::asio::ip::multicast::join_group(multicast_address));
					psocket = &socket;
					start_receive();
					io_service.run();
				}
				catch (boost::exception& e) {
					boost::mutex::scoped_lock lock(coutGuard);
					std::cout << boost::diagnostic_information(e);
				}

			};
			worker_threads2.create_thread(boost::bind(t));
			boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));
			int iloop = 0;
			int innerloop = 0;
			int maxdots = 10;
			
		long long currentcount = 0;
		long long lastcount = 0;
		long delaynanos = 0;
		boost::chrono::system_clock::time_point start = boost::chrono::system_clock::now();
		const int maxX = 64; //8 slots with 8 channels each

		do {
			int newcounts = 0;
			dp[0].Number = bufnum++;
			dp[0].Length = dp[0].headerLength;
			for (int k = 0; k < COUNTER_MONITOR_COUNT; k++) {
				Mcpd8::DataPacket::setParam(param[k], iloop*k); // only to fill some data
				dp[0].param[k][0] = param[k][0];
				dp[0].param[k][1] = param[k][1];
				dp[0].param[k][2] = param[k][2];
			}
			
			bool bfilldata = false;
			if (DataeveryNOnly == 1) bfilldata = true;
			else if (iloop % (DataeveryNOnly *DataeveryNOnly ) == 0) bfilldata = true;

			if(bfilldata){
				for (int i = 0; i < nevents; i++) {
					if(dataformat==Mcpd8::EventDataFormat::Mpsd8)	Zweistein::Random::Mpsd8EventRandomData((unsigned short*)(&dp[0].events[i]), i, maxX);
					if(dataformat==Mcpd8::EventDataFormat::Mdll) Zweistein::Random::MdllEventRandomData((unsigned short*)(&dp[0].events[i]), i);
				}
				newcounts = nevents;
			}
						
			dp[0].deviceStatusdeviceId = daq_status | (devid << 8); // set to running
			dp[0].Length = newcounts * 3 + dp[0].headerLength;
			if (dataformat == Mcpd8::EventDataFormat::Mpsd8) dp[0].Type = Mesy::BufferType::DATA;
			if (dataformat == Mcpd8::EventDataFormat::Mdll) dp[0].Type = Mesy::BufferType::MDLL;

			boost::chrono::nanoseconds ns=boost::chrono::steady_clock::now().time_since_epoch();
			Mcpd8::DataPacket::setTimeStamp(&dp[0].time[0],ns-zeropoint);
			
			
			if (daq_status&Mcpd8::Status::DAQ_Running) {
#ifdef _SPOOF_IP
				//size_t bytessent = raw_sendto(rawsocket, spoofed_endpoint, remote_endpoint, boost::asio::buffer((void*)(&dp[0]), dp[0].Length * sizeof(unsigned short)));
#else
				size_t bytessent = socket.send_to(boost::asio::buffer((void*)(&dp[0]), dp[0].Length * sizeof(unsigned short)), remote_endpoint);
#endif
				currentcount += newcounts;
			}
			
			if (iloop % (coutevery_npackets*DataeveryNOnly) == 0) {
				boost::this_thread::sleep_for(boost::chrono::nanoseconds(delaynanos));
				boost::chrono::duration<double> sec = boost::chrono::system_clock::now() - start;
				start = boost::chrono::system_clock::now();
				long countsadded = currentcount - lastcount;
				double evtspersecond = sec.count() != 0 ? (double)(countsadded) / sec.count() : 0;
				if (evtspersecond > 0.0 && countsadded>0) {
					long nanos = 1000000000L * (double)countsadded / evtspersecond;
					long wanted = 1000000000L * (double) countsadded /(double) requestedEventspersecond;
					//std::cout<<std::endl << "nanos=" << nanos << ", wanted=" << wanted << std::endl<<std::endl;
					delaynanos += wanted - nanos;
					if (delaynanos <= 0) delaynanos = 0;
				}
				else delaynanos = 1000L * 1000L * 1000L; // 1 second
				
				lastcount = currentcount;
				int dots = innerloop % maxdots;
				{
					boost::mutex::scoped_lock lock(coutGuard);
					boost::chrono::duration<double> elapsed(ns - zeropoint);
					if (retry == false) break;
					std::cout << "\r" << std::setprecision(1) << std::fixed << evtspersecond << " Events/s, (" << Zweistein::PrettyBytes((size_t)(evtspersecond * sizeof(Mesy::Mpsd8Event))) << "/s)" <<" elapsed:"<< elapsed;
					if (innerloop != 0 /*first gives wrong results*/) for (int i = 0; i < maxdots; i++) std::cout << (dots == i ? "." : " ");
					std::cout.flush();
				}
				innerloop++;

			}
			iloop++;

		} while (!io_service.stopped());

	}

		catch (boost::system::system_error const& e) {
			boost::mutex::scoped_lock lock(coutGuard);
			std::cout << e.what() << ": " << e.code() << " - " << e.code().message() << "\n";
			auto a = e.code();
			int err = a.value();
			if (err == 10013) std::cout << "You must run program with administrator rights" << std::endl;
		}
		catch (boost::exception & e) {
			boost::mutex::scoped_lock lock(coutGuard);
			std::cout << boost::diagnostic_information(e);
		}

		// if source is a Ctrl-C then exit here
		if (retry == false) {
			boost::mutex::scoped_lock lock(coutGuard);
			std::cout << "Ctrl-C handled => exit" << std::endl;
			break;
		}
		
		worker_threads2.interrupt_all();
		boost::this_thread::sleep_for(boost::chrono::seconds(5));
		
		
		//return -1;
   }while (retry);
   return 0;
}
