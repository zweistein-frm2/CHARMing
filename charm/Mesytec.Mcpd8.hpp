/***************************************************************************
 *   Copyright (C) 2019 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/

#pragma once
#include "stdafx.h"

/* Using blocking queue:
 * #include <mutex>
 * #include <queue>
 */
#include <boost/lockfree/spsc_queue.hpp>
#include <boost/exception/all.hpp>
#include <boost/array.hpp>
#include <vector>
#include <map>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/chrono.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/endian/conversion.hpp>
#include "Mcpd8.enums.hpp"
#include <csignal>
#include "Zweistein.Event.hpp"
#include <asioext/unique_file_handle.hpp>
#include <asioext/file_handle.hpp>
#include <asioext/open.hpp>
#include <asioext/standard_streams.hpp>

#include <asioext/file.hpp>
#include <asioext/thread_pool_file_service.hpp>
#include <asioext/open_flags.hpp>

#include <asioext/linear_buffer.hpp>

#include "Mesytec.listmode.hpp"
#include "Mcpd8.CmdPacket.hpp"
#include "Mcpd8.Parameters.hpp"
#include "Mesytec.enums.Generator.hpp"


using boost::asio::ip::udp;

namespace Mcpd8 {
	class Data {
	public:
		static const int EVENTQUEUESIZE = 500000; // high number needed
		boost::atomic<long long> evntcount = 0;
		boost::lockfree::spsc_queue<Zweistein::Event, boost::lockfree::capacity<EVENTQUEUESIZE>> evntqueue;
		boost::atomic<EventDataFormat> Format = EventDataFormat::Undefined;
		boost::atomic<long long> listmodepacketcount = 0;
		static const int LISTMODEWRITEQUEUESIZE = 10000;
		boost::lockfree::spsc_queue<DataPacket, boost::lockfree::capacity<LISTMODEWRITEQUEUESIZE>> listmodequeue;
		boost::atomic<long long> packetqueuecount = 0;
		static const int PACKETQUEUESIZE = 10000;
		boost::lockfree::spsc_queue<DataPacket, boost::lockfree::capacity<PACKETQUEUESIZE>> packetqueue;
		/* Using blocking queue:
 * std::queue<int> queue;
 * std::mutex mutex;
 */
	};
}


namespace Mesytec {

	
	struct DeviceParameter {
		udp::endpoint mcpd_endpoint;
		udp::endpoint charm_cmd_endpoint;
		udp::socket* socket;
		unsigned short lastbufnum;
		DataGenerator datagenerator;
		Mesy::ModuleId modules[Mesy::Mpsd8Event::sizeSLOTS];
		DeviceParameter() {
			for (int i = 0; i < Mesy::Mpsd8Event::sizeSLOTS; i++) modules[i] = Mesy::ModuleId::NOMODULE;
		}
		
	};
	class MesytecSystem {
	public:
		static std::map<unsigned short, Mesytec::DeviceParameter> deviceparam;
		MesytecSystem():minid(0),maxid(0),recv_buf(),simulatordatarate(20),lastmissed_count(0), lastmissed_time(boost::chrono::system_clock::now()),
			 inputFromListmodeFile(false), firstcallsendwaitresponse(true),cmd_target(0), widthX(64), widthY(64), pio_service(nullptr), eventdataformat(Mcpd8::EventDataFormat::Undefined),icharmid(0){}
		~MesytecSystem(){
			for (std::pair<unsigned short, Mesytec::DeviceParameter> pair : deviceparam) {
	//			if (pair.second.socket) pair.second.socket->close();
			}
			
		}
		int icharmid;
		boost::asio::io_service* pio_service;
		Mcpd8::EventDataFormat eventdataformat;
		unsigned short widthX;
		unsigned short widthY;
		int minid;
		int maxid;
		Mcpd8::Data data;
		bool inputFromListmodeFile;
		bool listmoderead_first=true;
		void readListmode(std::vector<std::string> & filenames, boost::asio::io_service& io_service) {
			for (std::string fname:filenames) {
				asioext::file source(io_service, fname, asioext::open_flags::access_read | asioext::open_flags::open_existing);
				char buffer[0x5ef]; // 0x5ef  is naughty boundary
				boost::system::error_code ec;
				boost::system::error_code error;
				size_t bufnum = 0;
				size_t bytesWrittenbypreviousBuffer = 0;
				size_t possible_remaining_n = std::string::npos;
				size_t possible_nc = std::string::npos;
				bool headerfound = false;
				bool closing_sigfound = false;
				size_t total_bytes_processed = 0;
				size_t transferred = 0;
				size_t data_packets_found = 0;
				listmoderead_first = true;
				do {
					const std::size_t bytes_read = source.read_some(boost::asio::buffer(buffer), ec);

					{
						boost::mutex::scoped_lock lock(coutGuard);
						std::cout << "\r" << filenames[0] << ": " << total_bytes_processed << " bytes processed \t";
					}
					size_t from = 0;
					size_t to = bytes_read;
					std::string nhneedle(Mesytec::listmode::header_separator, Mesytec::listmode::header_separator + sizeof(Mesytec::listmode::header_separator));
					std::string nneedle(Mesytec::listmode::datablock_separator, Mesytec::listmode::datablock_separator + sizeof(Mesytec::listmode::datablock_separator));
					std::string ncneedle(Mesytec::listmode::closing_signature, Mesytec::listmode::closing_signature + sizeof(Mesytec::listmode::closing_signature));
					do {
						std::string_view  haystack(buffer + from, bytes_read - from);
						size_t nh = haystack.find(nhneedle);
						size_t n = haystack.find(nneedle);
						size_t nc = haystack.find(ncneedle);
						if (nh != std::string::npos) {
							nh += from;
							if (headerfound) std::cout << "Multiple header_separator found at pos:" << total_bytes_processed + nh << std::endl;
						}
						if (n != std::string::npos) n += from;
						if (nc != std::string::npos) {
							nc += from;
							if (closing_sigfound) std::cout << "Multiple closing_sigfound found at pos:" << total_bytes_processed + nc << std::endl;

						}

						if (possible_remaining_n != std::string::npos) {
							size_t n_advance = 0;
							// now find needle for size_of(datablock_separator) - possible_remaining_n
							std::string_view  haystackEndOnly(buffer, possible_remaining_n);
							std::string n_restofneedle(Mesytec::listmode::datablock_separator + (sizeof(Mesytec::listmode::datablock_separator) - possible_remaining_n)
								, Mesytec::listmode::datablock_separator + sizeof(Mesytec::listmode::datablock_separator));
							if (haystackEndOnly.find(n_restofneedle) != std::string::npos) {
								// needle confirmed, so remove possible_n from 
								bytesWrittenbypreviousBuffer -= sizeof(Mesytec::listmode::datablock_separator) - possible_remaining_n;
								transferred -= sizeof(Mesytec::listmode::datablock_separator) - possible_remaining_n;
								short* sp = (short*)&recv_buf[0];
								for (int i = 0; i < transferred / sizeof(short); i++) 	boost::endian::big_to_native_inplace(sp[i]);
								listmoderead_analyzebuffer(error, transferred, recv_buf[0]);
								data_packets_found++;
								bytesWrittenbypreviousBuffer = 0;
								transferred = 0;
								from = possible_remaining_n;
							}
							possible_remaining_n = std::string::npos;
						}

						if (!headerfound && (nh != std::string::npos)) {
							from = nh + sizeof(Mesytec::listmode::header_separator);
							headerfound = true;
						}


						// we assume header_separator will be within the first read_buffer
						// have to check for closing separator also in the future

						if (n == std::string::npos && nc == std::string::npos) {

							//nothing found, so check end of buffer for cut of f signatures
							for (int i = 1; i < sizeof(Mesytec::listmode::datablock_separator); i++) {
								//i from 1 to 7
								size_t sep_size = sizeof(Mesytec::listmode::datablock_separator) - i;
								std::string_view  haystackBeginOnly(buffer + (bytes_read - sep_size), sep_size);
								std::string n_possibleneedle(Mesytec::listmode::datablock_separator, Mesytec::listmode::datablock_separator + sep_size);
								if (haystackBeginOnly.find(n_possibleneedle) != std::string::npos) {
									possible_remaining_n = i;
									break;
								};


							}
						}
						to = n;
						if (nc != std::string::npos) {
							closing_sigfound = true;
							to = nc;
						}
						if (headerfound && (from != std::string::npos)) {
							char* dest = ((char*)&recv_buf[0]) + bytesWrittenbypreviousBuffer;
							size_t n_advance = 0;
							if (to == std::string::npos) {
								to = bytes_read;
								bytesWrittenbypreviousBuffer += to - from;
								if (to - from > sizeof(Mcpd8::DataPacket)) {
									BOOST_THROW_EXCEPTION(std::runtime_error("DataPacket not recognized"));
								}
								memcpy(dest, buffer + from, to - from);
								transferred += to - from;
								n_advance = to;
							}
							else {
								// dann ist wohl recv_buf[0] gefüllt
								if (to - from > sizeof(Mcpd8::DataPacket)) {
									BOOST_THROW_EXCEPTION(std::runtime_error("DataPacket not recognized"));
								}
								memcpy(dest, buffer + from, to - from);
								transferred += to - from;
								short* sp = (short*)&recv_buf[0];
								for (int i = 0; i < transferred / sizeof(short); i++) 	boost::endian::big_to_native_inplace(sp[i]);
								listmoderead_analyzebuffer(error, transferred, recv_buf[0]);
								data_packets_found++;
								transferred = 0;
								bytesWrittenbypreviousBuffer = 0;
								n_advance = to + sizeof(Mesytec::listmode::datablock_separator);

							}
							if (n_advance > bytes_read) {
								n_advance = bytes_read;
								to = n_advance;
							}
							from = n_advance;
							to = bytes_read;
						}
						if (from == bytes_read) break;
						if (closing_sigfound) break;
						if (ec) {
							if (ec == boost::asio::error::eof || ec == boost::asio::error::broken_pipe) {
								from = std::string::npos;
								break;
							}

							else
								throw std::system_error(ec);
						}

					} while (from != std::string::npos);
					bufnum++;
					total_bytes_processed += bytes_read;
				} while (!ec);
				{
					boost::mutex::scoped_lock lock(coutGuard);
					std::cout << "\r" << filenames[0] << ": " << total_bytes_processed << " bytes processed ,data_packets_found=" << data_packets_found << " " << std::endl;
					if (!headerfound) std::cout << "headerfound=" << std::boolalpha << headerfound << " " << std::endl;
					if (!closing_sigfound) std::cout << "closing_sigfound=" << std::boolalpha << closing_sigfound << " " << std::endl;

				}

			}
		}
		void connect(std::list<Mcpd8::Parameters> &_devlist, boost::asio::io_service &io_service)
		{
			int idfirst = minid;
			int id = idfirst;
			for(Mcpd8::Parameters p:_devlist) {
				Mesytec::DeviceParameter mp;

				local_endpoint = udp::endpoint(boost::asio::ip::address::from_string(p.networkcard), p.mcpd_port);
				mp.mcpd_endpoint = udp::endpoint(boost::asio::ip::address::from_string(p.mcpd_ip),p.mcpd_port);
				{
					boost::mutex::scoped_lock lock(coutGuard);
					std::cout << "Local bind " << local_endpoint << " to " << mp.mcpd_endpoint << std::endl;
				}
				mp.socket = new udp::socket(io_service);
				mp.socket->open(udp::v4());
				mp.socket->set_option(boost::asio::socket_base::broadcast(true));
				mp.socket->set_option(boost::asio::socket_base::reuse_address(true));
				//	socket->set_option(asio::ip::multicast::join_group((address.to_v4().any())));
				// remember only for address in range 224.0.0.0 to 239.255...
				mp.socket->bind(local_endpoint);

				bool skip = false;
				mp.datagenerator = p.datagenerator;
				if (eventdataformat == Mcpd8::EventDataFormat::Undefined) {
					eventdataformat = p.eventdataformat;
				}
				else {
					if (p.eventdataformat != eventdataformat) {
						using namespace magic_enum::ostream_operators;
						boost::mutex::scoped_lock lock(coutGuard);
						std::cout << "ERROR: " << p.eventdataformat << "different to " << eventdataformat << std::endl;
						std::cout << "skipping " << mp.datagenerator << " " << mp.mcpd_endpoint.address().to_string() << std::endl;
						skip = true;
					}
				}
				mp.lastbufnum = 0;
				if (mp.datagenerator == Mesytec::DataGenerator::NucleoSimulator) {
					//for (int i = 0; i < Mesy::Mpsd8Event::sizeSLOTS; i++) mp.modules[i] = Mesy::ModuleId::MPSD8;
				}
				if (mp.datagenerator == Mesytec::DataGenerator::Mcpd8 || mp.datagenerator == Mesytec::DataGenerator::NucleoSimulator) {
					for (int i = 0; i < Mesy::Mpsd8Event::sizeSLOTS; i++) {
						//Send(mp, Mcpd8::Cmd::GETMPSD8PLUSPARAMETERS, i);
						
					}
				}
				if (mp.datagenerator == Mesytec::DataGenerator::Charm || mp.datagenerator == Mesytec::DataGenerator::CharmSimulator) {
					mp.charm_cmd_endpoint = udp::endpoint(boost::asio::ip::address::from_string(p.mcpd_ip), p.charm_port);
				}
				if (!skip) {
					deviceparam.insert(std::pair<unsigned short, Mesytec::DeviceParameter>(id, mp));
					
				}
				maxid = id;
				id++;

			}
			switch (eventdataformat) {
			case Mcpd8::EventDataFormat::Mpsd8:
				widthY = (unsigned short) Mesy::Mpsd8Event::sizeY;
				widthX = (unsigned short) (Mesy::Mpsd8Event::sizeMODID * Mesy::Mpsd8Event::sizeSLOTS*deviceparam.size());
				break;
			case Mcpd8::EventDataFormat::Mdll:
				widthX = (unsigned short) (Mesy::MdllEvent::sizeX * deviceparam.size());
				widthY = (unsigned short)Mesy::MdllEvent::sizeY;
				break;
			case Mcpd8::EventDataFormat::Charm:
				widthX = (unsigned short) (Mesy::MdllEvent::sizeX * deviceparam.size());
				widthY = (unsigned short) Mesy::MdllEvent::sizeY;
				break;

			}

			for (std::pair<unsigned short, Mesytec::DeviceParameter> p : deviceparam) {
				start_receive(deviceparam.at(p.first));

				if (p.second.datagenerator == Mesytec::DataGenerator::Charm || p.second.datagenerator == Mesytec::DataGenerator::CharmSimulator) {
					icharmid = p.first;

				}

			}
			pio_service = &io_service;
			worker_threads.create_thread(boost::bind(&MesytecSystem::watch_incoming_packets, this));		//worker_threads.add_thread(new boost::thread(&MesytecSystem::watch_incoming_packets, this, &io_service));
			boost::this_thread::sleep_for(boost::chrono::milliseconds(200)); // we want watch_incoming_packets active

			for (int i = minid; i <= maxid; i++) {
				//Send(deviceparam.at(i), Mcpd8::Cmd::SETID, i); // set ids for Mcpd8 devices
				Send(deviceparam.at(i), Mcpd8::Internal_Cmd::GETVER); // 
				//Send(deviceparam.at(i), Mcpd8::Cmd::GETPARAMETERS);
			}


		}
		long simulatordatarate;
		bool CmdSupported(Mesytec::DeviceParameter& mp, unsigned short cmd) {

			using namespace magic_enum::ostream_operators;
			auto cmd_1 = magic_enum::enum_cast<Mcpd8::Cmd>(cmd);
			auto cmd_2 = magic_enum::enum_cast<Mcpd8::Internal_Cmd>(cmd);

			bool rv = true;

			if (mp.datagenerator == Mesytec::DataGenerator::NucleoSimulator) {

				if (cmd_1.has_value()) {
					if (cmd_1 == Mcpd8::Cmd::SETPULSER || cmd_1 == Mcpd8::Cmd::GETMPSD8PLUSPARAMETERS) {
						boost::mutex::scoped_lock lock(coutGuard);
						using namespace magic_enum::ostream_operators;
						std::cout << mp.datagenerator << " not supported:" << cmd_1 << std::endl;
						rv = false;
					}
				}
				if (cmd_2.has_value()) {
					if (cmd_2 == Mcpd8::Internal_Cmd::CHARMPATTERNGENERATOR || cmd_2 == Mcpd8::Internal_Cmd::CHARMSETEVENTRATE) {
						boost::mutex::scoped_lock lock(coutGuard);
						using namespace magic_enum::ostream_operators;
						std::cout << mp.datagenerator << " not supported:" << cmd_2 << std::endl;
						rv = false;
					}
				}

			}

			if (mp.datagenerator == Mesytec::DataGenerator::Mcpd8) {
				if (cmd_2.has_value()) {
					if (cmd_2 == Mcpd8::Internal_Cmd::SETNUCLEORATEEVENTSPERSECOND
						|| cmd_2 == Mcpd8::Internal_Cmd::CHARMPATTERNGENERATOR || cmd_2 == Mcpd8::Internal_Cmd::CHARMSETEVENTRATE) {
						boost::mutex::scoped_lock lock(coutGuard);
						using namespace magic_enum::ostream_operators;
						std::cout << mp.datagenerator << " not supported:" << cmd_2 << std::endl;
						rv = false;
					}
				}

			}
			return rv;
		}
		void Send(Mesytec::DeviceParameter &mp,Mcpd8::Internal_Cmd cmd, unsigned long param = 0) {
			Mcpd8::CmdPacket cmdpacket;
			Mcpd8::CmdPacket response;
			cmdpacket.cmd = cmd;
			switch (cmd) {
			case Mcpd8::Internal_Cmd::CHARMPATTERNGENERATOR:
				{
					char cmdbytes[] = { '\x06','\x00','\x06','\x00','\x04','\x00','\x01','\x00','\x00','\x8a','\x00','\x00'};
					if (param == 1) cmdbytes[10] = '\x01';
					size_t bytessent=mp.socket->send_to(boost::asio::buffer(cmdbytes),mp.charm_cmd_endpoint);

				}
				break;
			case Mcpd8::Internal_Cmd::CHARMSETEVENTRATE:
				{
				char cmdbytes[] = { '\x06','\x00','\x06','\x00','\x04','\x00','\x02','\x00','\x30','\x8a','\x00','\x00' };
				unsigned short data =(unsigned short)( 50 * 1000000 / (param));

				if (data <= 2) {
					boost::mutex::scoped_lock lock(coutGuard);
					using namespace magic_enum::ostream_operators;
					auto cmd_2 = magic_enum::enum_cast<Mcpd8::Internal_Cmd>(cmdpacket.cmd);
					std::cout << cmd_2 << ":data must be >2" << std::endl;
					break;
				}
				short* sp = (short *)&cmdbytes[10];
				*sp = data;
				size_t bytessent = mp.socket->send_to(boost::asio::buffer(cmdbytes), mp.charm_cmd_endpoint);
				simulatordatarate = param;
				}
				break;
			case Mcpd8::Internal_Cmd::SETNUCLEORATEEVENTSPERSECOND:
			    {
					unsigned long* p = reinterpret_cast<unsigned long*>(&cmdpacket.data[0]);
					*p = param;
					cmdpacket.Length += 2;
			    }
				response = Send(mp,cmdpacket);
				simulatordatarate = param;
				break;
			case Mcpd8::Internal_Cmd::GETVER:
				response=Send(mp,cmdpacket);
				{
					boost::mutex::scoped_lock lock(coutGuard);
					std::cout << "CPU " << response.data[0] << "." << response.data[1] << ", FPGA " << (response.data[2] & 0xff) << "." << ((response.data[2]>>8)&0xff) << std::endl;
				}
				break;
			}
		}

		void Send(Mesytec::DeviceParameter &mp,Mcpd8::Cmd cmd,unsigned short param=0) {
			Mcpd8::CmdPacket cmdpacket;
			Mcpd8::CmdPacket response;
			cmdpacket.cmd = cmd;
			switch (cmd) {
			case Mcpd8::Cmd::RESET:
			case Mcpd8::Cmd::START:
				mp.lastbufnum = 0;
			case Mcpd8::Cmd::STOP:
			case Mcpd8::Cmd::CONTINUE:
				Send(mp,cmdpacket, false);
				break;
			case Mcpd8::Cmd::SETID:
			    cmdpacket.data[0] = param;
				cmdpacket.Length = Mcpd8::CmdPacket::defaultLength + 1;
				response=Send(mp,cmdpacket);
				break;
			case Mcpd8::Cmd::SETRUNID:
				cmdpacket.data[0] = param;
				cmdpacket.Length= Mcpd8::CmdPacket::defaultLength + 1;
				response=Send(mp,cmdpacket);
				break;
			case Mcpd8::Cmd::GETPARAMETERS:
				response = Send(mp,cmdpacket);
				break;
			case Mcpd8::Cmd::GETCAPABILITIES:
				response = Send(mp,cmdpacket);
				//10 11 
				break;

		
			case Mcpd8::Cmd::GETMPSD8PLUSPARAMETERS:
				cmdpacket.data[0] = param;
				cmdpacket.Length = Mcpd8::CmdPacket::defaultLength + 1;
				response = Send(mp, cmdpacket);
				assert(cmdpacket.data[0] < 8);
				{
					boost::mutex::scoped_lock lock(coutGuard);
					using namespace magic_enum::ostream_operators;
					std::cout << cmd << ":MPSD device number(" << param << "),Firmware=" << cmdpacket.data[3]<<", "; 
					if (cmdpacket.data[3] != 0) {
						mp.modules[cmdpacket.data[0]] = Mesy::ModuleId::MPSD8;
					
					}

					std::cout << mp.modules[cmdpacket.data[0]]<< std::endl;
					 
				}
				

				//10 11 
				break;
			}
		}
		void SendAll(Mcpd8::Cmd cmd) {
			for (std::pair<unsigned short, Mesytec::DeviceParameter> pair : deviceparam) {
				Send(pair.second, cmd);
			}
			{
				boost::mutex::scoped_lock lock(coutGuard);
				using namespace magic_enum::ostream_operators;
				std::cout << "SendAll(" << cmd << ")" << std::endl;
			}
		}
		
	private:
		bool firstcallsendwaitresponse;
		Mcpd8::CmdPacket& Send(Mesytec::DeviceParameter& mp,Mcpd8::CmdPacket& cmdpacket,bool waitresponse=true) {
			if (!CmdSupported(mp, cmdpacket.cmd)) return cmdpacket;

			for (auto& d : deviceparam) {
				if (d.second.socket == mp.socket && d.second.mcpd_endpoint == mp.mcpd_endpoint) {
					cmd_target = d.first;
					break; // to stop searching
				}
			}
			

			using namespace magic_enum::ostream_operators;
			auto cmd_1 = magic_enum::enum_cast<Mcpd8::Cmd>(cmdpacket.cmd);
			auto cmd_2 = magic_enum::enum_cast<Mcpd8::Internal_Cmd>(cmdpacket.cmd);
			std::stringstream ss_cmd;
			if (cmd_1.has_value()) 	ss_cmd << cmd_1 << "(" << static_cast<magic_enum::underlying_type_t<Mcpd8::Cmd>>(cmdpacket.cmd) << ")";
			else {
				ss_cmd << cmd_2;
				if (!cmd_2.has_value()) ss_cmd << "(0x" << std::hex << cmdpacket.cmd << ")";
				else ss_cmd << "(" << static_cast<magic_enum::underlying_type_t<Mcpd8::Internal_Cmd>>(cmdpacket.cmd) << ")";
			}
			if (inputFromListmodeFile) {
				boost::mutex::scoped_lock lock(coutGuard);
				std::cout << "Cannot Send "<< ss_cmd.str()  <<" because inputFromListmodeFile==true" << std::endl;
				return cmdpacket;
			}
			
			

			Mcpd8::CmdPacket::Send(mp.socket, cmdpacket,mp.mcpd_endpoint);
			
			Mcpd8::CmdPacket response;
			if (!waitresponse) {
				boost::mutex::scoped_lock lock(coutGuard);
				std::cout << "\rSEND:" << cmdpacket << " =>RESPONSE UNKNOWN (not checked)" << std::endl;
				return response;
			}
			int timeout_ms = 1300;
			if (firstcallsendwaitresponse) {
				firstcallsendwaitresponse = false;
				timeout_ms = 4000;
			}
			
			for (int l = 0; l < timeout_ms/10; l++) {
				boost::this_thread::sleep_for(boost::chrono::milliseconds(10));
				while(cmd_recv_queue.pop(response)) {
					if (response.cmd == cmdpacket.cmd ||(response.cmd == (cmdpacket.cmd|0x8000))) {
						if (response.cmd & 0x8000) {
							boost::mutex::scoped_lock lock(coutGuard);
							std::cout << "\rSEND:" << cmdpacket << " =>ERROR:"<< "Send(" << ss_cmd.str() << ") NOT UNDERSTOOD! " << std::endl;
						}
						else {
							boost::mutex::scoped_lock lock(coutGuard);
							std::cout << "\rSEND:" << cmdpacket << " =>RESPONSE OK" << std::endl;
						}
						
						return response;
					}
				}
			}
			{
				boost::mutex::scoped_lock lock(coutGuard);
				std::cout << "\rSEND:" << cmdpacket << " =>ERROR:" << "Send(" << ss_cmd.str() << ") timeout(" << timeout_ms << " ms)" << std::endl;
			}
			return response;
		}

		void start_receive_charm(){
			
			deviceparam.at(icharmid).socket->async_receive_from(boost::asio::buffer(charm_buf), deviceparam.at(icharmid).charm_cmd_endpoint, boost::bind(&MesytecSystem::handle__charm_receive, this,
				boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
		}

		void handle__charm_receive(const boost::system::error_code& error,
			std::size_t bytes_transferred) {

			if (true) {
				boost::mutex::scoped_lock lock(coutGuard);
				std::cout << "handle__charm_receive(" << error.message() << " ,bytes_transferred=" << bytes_transferred << std::endl;
				for (int i = 0; i < bytes_transferred; i++) {
					std::cout << std::hex << charm_buf[i] << " ";
					if (i % 16 == 0) std::cout << std::endl;
				}
				std::cout << std::endl;
			}
			start_receive_charm();
		}
		void start_receive(Mesytec::DeviceParameter mp) {

			mp.socket->async_receive_from(boost::asio::buffer(recv_buf), mp.mcpd_endpoint, boost::bind(&MesytecSystem::handle_receive, this,
				boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
		}
		void watch_incoming_packets() {
			try {
				Mcpd8::DataPacket dp;
				do {
					while (data.packetqueue.pop(dp)) {
						analyzebuffer(dp);
					}
				} while (!pio_service->stopped());

			}
			catch (boost::exception & e) { 
				boost::mutex::scoped_lock lock(coutGuard);
				std::cout << boost::diagnostic_information(e); 
			}
			{
				boost::mutex::scoped_lock lock(coutGuard);
				std::cout << "watch_incoming_packets() exiting..." << std::endl;
			}
		}
		

		void handle_receive(const boost::system::error_code& error,
			std::size_t bytes_transferred) {
			Mcpd8::DataPacket& dp = recv_buf[0];
			
			if (error) {
				boost::mutex::scoped_lock lock(coutGuard);
				std::cout << "handle_receive(" << error.message() << " ,bytes_transferred=" << bytes_transferred << ", DataPacket=" << dp << std::endl;
				std::cout << "PACKET BUFNUM: " << (unsigned short)(dp.Number) << std::endl;
				//memset(&recv_buf[0], 0, sizeof(Mcpd8::DataPacket));
			}
			unsigned short id = Mcpd8::DataPacket::getId(dp.deviceStatusdeviceId);
			if ((id > maxid || id<minid) && dp.Type!= Zweistein::reverse_u16(Mesy::BufferType::COMMAND)) {
				boost::mutex::scoped_lock lock(coutGuard);
				std::cout << "CRITICAL: Packet has wrong id:" << id << " (minid allowed:" << minid << "," << "(maxid allowed:" << maxid << ")" << std::endl;
				std::raise(SIGTERM);
				
			}
			else {
				if (dp.Type != Zweistein::reverse_u16(Mesy::BufferType::COMMAND)) {
					data.packetqueue.push(dp);
					data.packetqueuecount++;
					data.listmodequeue.push(dp);
					data.listmodepacketcount++;
					start_receive(deviceparam.at(id));
				}
				else {
					if (cmd_target >= minid && cmd_target <= maxid) {
						Mcpd8::CmdPacket p = dp;
						{
						//	boost::mutex::scoped_lock lock(coutGuard);
						//	std::cout<<"\rRECEIVED:"<< p << std::endl;

						}
						cmd_recv_queue.push(p);

						start_receive(deviceparam.at(cmd_target));
					}
				}
				
			}
			
			
		
		}


		void PushNeutronEventOnly_HandleOther(Zweistein::Event& Ev) {
			if (Ev.type == Mesy::EventType::NEUTRON) {
				data.evntqueue.push(Ev);
				data.evntcount++;
			}
			else {

			}
			
		}
		void listmoderead_analyzebuffer(const boost::system::error_code& error,
			std::size_t bytes_transferred, Mcpd8::DataPacket& datapacket) {
			if (listmoderead_first) {
				listmoderead_first = false;
				Mesytec::DeviceParameter mp;
				mp.lastbufnum = datapacket.Number-1;
				
				minid=Mcpd8::DataPacket::getId(datapacket.deviceStatusdeviceId);
				maxid = Mcpd8::DataPacket::getId(datapacket.deviceStatusdeviceId); 

				switch (datapacket.GetBuffertype()) {
				case Mesy::BufferType::DATA:
					data.Format = Mcpd8::EventDataFormat::Mpsd8;
					widthY = Mesy::Mpsd8Event::sizeY;
					widthX = Mesy::Mpsd8Event::sizeMODID * Mesy::Mpsd8Event::sizeSLOTS;
					
					// one device has maximum 64 (8 mpsd8 with 8 channels each)
					// problem, we don't know how many devices were used in the measurement.
					//  2 solutions: A: use a msmt.json reflecting the real measurement.
					//               B: scan the file for all possible mcpd8 Id[s]. Each Id can handle 64 channels
					break;
				case Mesy::BufferType::MDLL:
					data.Format = Mcpd8::EventDataFormat::Mdll;
					widthX= Mesy::MdllEvent::sizeX;
					widthY= Mesy::MdllEvent::sizeY;
				
					break;
				}
				deviceparam.insert(std::pair<unsigned short, Mesytec::DeviceParameter>(Mcpd8::DataPacket::getId(datapacket.deviceStatusdeviceId), mp));
				boost::this_thread::sleep_for(boost::chrono::milliseconds(500));
			}
			analyzebuffer(datapacket);
		}
		
		unsigned long lastmissed_count;
		boost::chrono::system_clock::time_point lastmissed_time;
		void analyzebuffer(Mcpd8::DataPacket &datapacket)
		{
			unsigned short id = Mcpd8::DataPacket::getId(datapacket.deviceStatusdeviceId);
			unsigned short offset = (id - minid);
			auto params = deviceparam.at(id);
			unsigned short numevents = datapacket.numEvents();
			long long headertime = datapacket.timeStamp();
			unsigned short neutrons = 0;
			switch (datapacket.GetBuffertype()) {
			case Mesy::BufferType::DATA: 
				
				if (datapacket.Number != (unsigned short)(params.lastbufnum + 1)) {
					boost::chrono::duration<double> sec = boost::chrono::system_clock::now() - lastmissed_time;
					unsigned short missed = (unsigned short)(datapacket.Number - (unsigned short)(params.lastbufnum + 1));
					lastmissed_count += missed;
					if (sec.count() > 1) {
						boost::mutex::scoped_lock lock(coutGuard);
						std::cout << "MISSED " << lastmissed_count << " BUFFER(s)" << std::endl;
						lastmissed_time = boost::chrono::system_clock::now();
						lastmissed_count = 0;
					}

				}
				deviceparam.at(id).lastbufnum = datapacket.Number;

				for (int i = 0; i < numevents; i++) {
					Zweistein::Event Ev = Zweistein::Event(&datapacket.events[i], headertime, offset * Mesy::Mpsd8Event::sizeSLOTS * Mesy::Mpsd8Event::sizeMODID, params.modules);
					PushNeutronEventOnly_HandleOther(Ev);
				}
			
				break;
			case Mesy::BufferType::MDLL: 
				
				if (datapacket.Number != (unsigned short)(params.lastbufnum + 1)) {
					boost::mutex::scoped_lock lock(coutGuard);
					std::cout << "MISSED " << (unsigned short)(datapacket.Number - (unsigned short)(params.lastbufnum + 1)) << " BUFFER(s)" << std::endl;
				}
				deviceparam.at(id).lastbufnum = datapacket.Number;

				for (int i = 0; i < numevents; i++) {
					Zweistein::Event Ev = Zweistein::Event(Mesy::MdllEvent::fromMpsd8(&datapacket.events[i]), headertime, offset * Mesy::MdllEvent::sizeX, params.modules);
					PushNeutronEventOnly_HandleOther(Ev);
				}
			
				break;


			}

			
		}
		unsigned short cmd_target;
		udp::endpoint local_endpoint;
		boost::array< Mcpd8::DataPacket,1> recv_buf;
		boost::array< unsigned char, 64> charm_buf;
		boost::lockfree::spsc_queue<Mcpd8::CmdPacket, boost::lockfree::capacity<10> > cmd_recv_queue;
	};
	std::map<unsigned short, Mesytec::DeviceParameter> MesytecSystem::deviceparam = std::map<unsigned short, Mesytec::DeviceParameter>();
	
	
}




