/***************************************************************************
 *   Copyright (C) 2019 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/

#include "stdafx.h"
#include <boost/exception/all.hpp>
#include <boost/array.hpp>
#include <boost/circular_buffer.hpp>
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
#include <boost/tokenizer.hpp>
#include "Mcpd8.enums.hpp"
#include <csignal>
#include <asioext/file.hpp>
#include <asioext/thread_pool_file_service.hpp>
#include <asioext/open_flags.hpp>

#include <asioext/linear_buffer.hpp>

#include "Mesytec.listmode.hpp"
#include "MesytecSystem.Data.hpp"
#include "Mcpd8.CmdPacket.hpp"
#include "Mcpd8.Parameters.hpp"
#include "Mesytec.DeviceParameter.hpp"
#include "Zweistein.Locks.hpp"
#include "Zweistein.Logger.hpp"
#include "Zweistein.ping.hpp"
#include "Mesytec.Mcpd8.hpp"

using boost::asio::ip::udp;

namespace Mesytec {
	
		MesytecSystem::MesytecSystem():recv_buf(), cmd_recv_queue(5), internalerror(cmd_errorcode::OK), currentrunid(0),
			lastpacketqueuefull_missedcount(0), lastlistmodequeuefull_missedcount(0),
			lasteventqueuefull_missedcount(0), inputFromListmodeFile(false),
			eventdataformat(Mcpd8::EventDataFormat::Undefined),icharmid(0){
				
		}
		MesytecSystem::~MesytecSystem(){
			for (const auto& [key, value] : deviceparam) {
				if (value.bNewSocket && value.socket) value.socket->close();
			}
			if (pstrand) delete pstrand;
		}
		void MesytecSystem::initatomicortime_point() {
			auto now = boost::chrono::system_clock::now();
			started = now;
			stopped = now;
			simulatordatarate = 51;
			connected = false;
			daq_running = false;
			write2disk = false;
			auto max= boost::chrono::system_clock::time_point::max();
			lasteventqueuefull = max;
			lastpacketqueuefull = max;
			lastlistmodequeuefull=max;
			wait_response = false;

		}
		bool MesytecSystem::listmode_connect(std::list<Mcpd8::Parameters>& _devlist, boost::asio::io_service& io_service)
		{
			pio_service = &io_service;
			data.widthX = 0;
			data.widthY = 0;

			for (Mcpd8::Parameters& p : _devlist) {
				Mesytec::DeviceParameter mp;
				bool skip = false;
				mp.datagenerator = p.datagenerator;
				if (eventdataformat == Mcpd8::EventDataFormat::Undefined) {
					eventdataformat = p.eventdataformat;
				}
				else {
					if (p.eventdataformat != eventdataformat) {
						using namespace magic_enum::ostream_operators;
						LOG_ERROR << "ERROR: " << p.eventdataformat << "different to " << eventdataformat << std::endl;
						LOG_ERROR << "skipping " << mp.datagenerator << " " << mp.mcpd_endpoint.address().to_string() << std::endl;
						skip = true;
					}
				}
				mp.lastbufnum = 0;

				if (!skip) {
					mp.offset = data.widthX;
					deviceparam.insert(std::pair<const unsigned char, Mesytec::DeviceParameter>(p.mcpd_id, mp));
					switch (eventdataformat) {
					case Mcpd8::EventDataFormat::Mpsd8:
						data.widthY = (unsigned short)Mpsd8_sizeY;
						data.widthX += (unsigned short)(Mpsd8_sizeMODID * Mpsd8_sizeSLOTS);
						break;
					case Mcpd8::EventDataFormat::Mdll:
						data.widthX += (unsigned short)Mdll_sizeX;
						data.widthY = (unsigned short)Mdll_sizeY;
						break;
					case Mcpd8::EventDataFormat::Charm:
						data.widthX += (unsigned short)Mdll_sizeX;
						data.widthY = (unsigned short)Mdll_sizeY;
						break;
					}
				}



			}
			if (data.widthX == 0) data.widthX = 1;
			if (data.widthY == 0) data.widthY = 1;
			connected = true;
			boost::chrono::system_clock::time_point tpstarted = started;
			boost::chrono::milliseconds ms = boost::chrono::duration_cast<boost::chrono::milliseconds> (boost::chrono::system_clock::now() - tpstarted);
			{
				LOG_INFO << "MESYTECHSYSTEM LISTMODEREAD READY: +" << ms << std::endl;
			}
			return connected;

		}
		bool MesytecSystem::connect(std::list<Mcpd8::Parameters> &_devlist, boost::asio::io_service &io_service)
		{
			pio_service = &io_service;
			data.widthX = 0;
			data.widthY = 0;
			if (pstrand) delete pstrand;
			pstrand=new boost::asio::io_service::strand(io_service);
			boost::system::error_code ec;
			for(Mcpd8::Parameters& p:_devlist) {
				Mesytec::DeviceParameter mp;
				for(int c=0;c<8;c++){ mp.counterADC[c]=p.counterADC[c];	}
				for (int m = 0; m < 8; m++) { mp.moduleparam[m] = p.moduleparam[m]; }

	//			Zweistein::ping(p.mcpd_ip); // sometime a ping is needed to wake up Mesytec

				local_endpoint = udp::endpoint(boost::asio::ip::address::from_string(p.networkcard), p.mcpd_port);
				mp.mcpd_endpoint = udp::endpoint(boost::asio::ip::address::from_string(p.mcpd_ip),p.mcpd_port);
				{
					LOG_DEBUG << "Local bind " << local_endpoint << " to " << mp.mcpd_endpoint << std::endl;
				}
				bool local_endpoint_is_bound = false;
				
				for (const auto& [key,value] : deviceparam) {
					if (local_endpoint == value.socket->local_endpoint()) {
						mp.socket = value.socket;
						mp.bNewSocket = false;
						local_endpoint_is_bound = true;
						break;
					}
				}
				
				if (!local_endpoint_is_bound) {
					mp.socket = new udp::socket(io_service);
					mp.bNewSocket = true;
					mp.socket->open(udp::v4());
					//mp.socket->set_option(boost::asio::socket_base::reuse_address(true));
					//	socket->set_option(asio::ip::multicast::join_group((address.to_v4().any())));
					// remember only for address in range 224.0.0.0 to 239.255...
					mp.socket->bind(local_endpoint);
				}

				bool skip = false;
				mp.datagenerator = p.datagenerator;
				if (eventdataformat == Mcpd8::EventDataFormat::Undefined) {
					eventdataformat = p.eventdataformat;
				}
				else {
					if (p.eventdataformat != eventdataformat) {
						using namespace magic_enum::ostream_operators;
						LOG_ERROR << "ERROR: " << p.eventdataformat << "different to " << eventdataformat << std::endl;
						LOG_ERROR << "skipping " << mp.datagenerator << " " << mp.mcpd_endpoint.address().to_string() << std::endl;
						skip = true;
					}
				}
				mp.lastbufnum = 0;
				
				if (mp.datagenerator == Mesytec::DataGenerator::Charm || mp.datagenerator == Mesytec::DataGenerator::CharmSimulator) {
					mp.charm_cmd_endpoint = udp::endpoint(boost::asio::ip::address::from_string(p.mcpd_ip), p.charm_port);
				}
				if (!skip) {
					mp.offset = data.widthX;
					deviceparam.insert(std::pair<const unsigned char, Mesytec::DeviceParameter>(p.mcpd_id, mp));
					switch (eventdataformat) {
					case Mcpd8::EventDataFormat::Mpsd8:
						data.widthY = (unsigned short) Mpsd8_sizeY;
						data.widthX+= (unsigned short)(Mpsd8_sizeMODID * Mpsd8_sizeSLOTS );
						break;
					case Mcpd8::EventDataFormat::Mdll:
						data.widthX+= (unsigned short)Mdll_sizeX;
						data.widthY = (unsigned short)Mdll_sizeY;
						break;
					case Mcpd8::EventDataFormat::Charm:
						data.widthX+= (unsigned short)Mdll_sizeX;
						data.widthY = (unsigned short) Mdll_sizeY;
						break;
					}
				}
			}
			
			worker_threads.create_thread(boost::bind(&MesytecSystem::watch_incoming_packets, this));		//worker_threads.add_thread(new boost::thread(&MesytecSystem::watch_incoming_packets, this, &io_service));
			boost::this_thread::sleep_for(boost::chrono::milliseconds(100)); // we want watch_incoming_packets active

			for (const auto& [key, value] : deviceparam) {
				if(value.bNewSocket) start_receive(value,key);

				if (value.datagenerator == Mesytec::DataGenerator::Charm || value.datagenerator == Mesytec::DataGenerator::CharmSimulator) {
					icharmid = key;

				}

			}
			
			for (auto& kvp : deviceparam) {
				Send(kvp, Mcpd8::Cmd::SETID, kvp.first); // set ids for Mcpd8 devices
			}
			
			for (auto& onid : oldidnewid) {
				auto it= deviceparam.find(onid.second);
				if (it != deviceparam.end()) {
					LOG_INFO << (it->second.mcpd_endpoint.address()).to_string() << " SETID: NEW ID=" << (int)onid.second << " (OLD ID=" <<(int)onid.first << ")"<< std::endl;
				}
				else {
					LOG_ERROR << __FILE__ <<":"<<__LINE__ << std::endl;
				}
				
			}

			for (  auto &kvp: deviceparam) {
				Send(kvp, Mcpd8::Internal_Cmd::GETVER); //
				Send(kvp, Mcpd8::Internal_Cmd::READID);
				Send(kvp, Mcpd8::Cmd::GETPARAMETERS);
				for (int c = 0; c < 8; c++) {  // SETCELL
					std::string ca = kvp.second.counterADC[c];
					if (!ca.empty()) {
						Mcpd8::CmdPacket  cmdpacket;
						cmdpacket.cmd= Mcpd8::Cmd::SETCELL;
						cmdpacket.Length = Mcpd8::CmdPacket::defaultLength + 3;

						typedef boost::tokenizer<boost::char_separator<char>> tokenizer;
						tokenizer tok{ca };
						cmdpacket.data[0] = c;
						int n = 1;
						for (tokenizer::iterator it = tok.begin(); it != tok.end(); ++it) {
							cmdpacket.data[n] = std::atoi((*it).c_str());
							if (++n >= 3) break;
						}
						try {
							Mcpd8::CmdPacket::Send(kvp.second.socket, cmdpacket, kvp.second.mcpd_endpoint);
						}
						catch (Mesytec::cmd_error& x) {
							if (int const* mi = boost::get_error_info<Mesytec::my_info>(x)) {
								auto  my_code = magic_enum::enum_cast<Mesytec::cmd_errorcode>(*mi);
								if (my_code.has_value()) {
									auto c1_name = magic_enum::enum_name(my_code.value());
									LOG_ERROR << c1_name << std::endl;
								}
							}
						}
						catch (boost::exception& e) {
							LOG_ERROR << boost::diagnostic_information(e) << std::endl;
						}
					}
				}
				for (int m = 0; m < 8; m++) {  // SETMPSDGAIN SETMPSDTHRES
					std::string ca = kvp.second.moduleparam[m];
					if (!ca.empty()) {
						typedef boost::tokenizer<boost::char_separator<char>> tokenizer;
						tokenizer tok{ ca };
						int n = 1;
						for (tokenizer::iterator it = tok.begin(); it != tok.end(); ++it) {
							if (n == 0) {
								unsigned short thres=std::atoi((*it).c_str());
								long param = thres << 16 | (unsigned short)m;
									Send(kvp, Mcpd8::Cmd::SETTHRESH, param);
							}
							else {
								Mcpd8::CmdPacket  cmdpacket;
								cmdpacket.cmd = Mcpd8::Cmd::SETGAIN_MPSD;
								cmdpacket.Length = Mcpd8::CmdPacket::defaultLength + 3;
								cmdpacket.data[0] = m;
								cmdpacket.data[1] = n - 1;
								cmdpacket.data[2] = std::atoi((*it).c_str());
								try {
									Mcpd8::CmdPacket::Send(kvp.second.socket, cmdpacket, kvp.second.mcpd_endpoint);
								}
								catch (Mesytec::cmd_error& x) {
									if (int const* mi = boost::get_error_info<Mesytec::my_info>(x)) {
										auto  my_code = magic_enum::enum_cast<Mesytec::cmd_errorcode>(*mi);
										if (my_code.has_value()) {
											auto c1_name = magic_enum::enum_name(my_code.value());
											LOG_ERROR << c1_name << std::endl;
										}
									}
								}
								catch (boost::exception& e) {
									LOG_ERROR << boost::diagnostic_information(e) << std::endl;
								}


							}
							
							if (++n >= 8) break;
						}
						
					}
				}
				if (kvp.second.datagenerator == Mesytec::DataGenerator::Mcpd8 || kvp.second.datagenerator == Mesytec::DataGenerator::NucleoSimulator) {
					for (int i = 0; i < Mpsd8_sizeSLOTS; i++) {
						if (kvp.second.module_id[i] == Mesy::ModuleId::MPSD8P) {
							Send(kvp, Mcpd8::Cmd::GETMPSD8PLUSPARAMETERS, i);
						}
						if (kvp.second.firmware_major > 10) {}
						if (kvp.second.module_id[i] == Mesy::ModuleId::MPSD8 || kvp.second.module_id[i] == Mesy::ModuleId::MPSD8OLD) {
							Send(kvp, Mcpd8::Internal_Cmd::READPERIREG, i);
						}
					}
				}
			}
			for (auto& kvp : deviceparam) {
				Send(kvp, Mcpd8::Cmd::SETCLOCK, Mesy::TimePoint::ZERO);
			}
			int ndevices = (int) deviceparam.size();
			if (ndevices > 1) {
				// so we have to set Master / slave
				int i = 0;
				for (auto& kvp : deviceparam) {
					unsigned short sync_bus_termination_off = 1;
					unsigned short master = 1;
					unsigned short slave = 0;
					if (i == 0) {
						Send(kvp, Mcpd8::Cmd::SETTIMING, master); //
					}
					else Send(kvp, Mcpd8::Cmd::SETTIMING, i < ndevices ?  (long)slave & ((long)sync_bus_termination_off<< 16):(long) slave);
					i++;
				}
			}
			connected = true;
			boost::chrono::system_clock::time_point tpstarted = started;
			boost::chrono::milliseconds ms = boost::chrono::duration_cast<boost::chrono::milliseconds> (boost::chrono::system_clock::now() - tpstarted);
			{
				LOG_INFO << "MESYTECHSYSTEM CONNECTED: +" << ms << std::endl;
			}
			return connected;
		}
		bool MesytecSystem::CmdSupported(Mesytec::DeviceParameter& mp, unsigned short cmd) {

			using namespace magic_enum::ostream_operators;
			auto cmd_1 = magic_enum::enum_cast<Mcpd8::Cmd>(cmd);
			auto cmd_2 = magic_enum::enum_cast<Mcpd8::Internal_Cmd>(cmd);

			bool rv = true;

			if (mp.datagenerator == Mesytec::DataGenerator::NucleoSimulator) {

				if (cmd_2.has_value()) {
					if (cmd_2 == Mcpd8::Internal_Cmd::CHARMPATTERNGENERATOR || cmd_2 == Mcpd8::Internal_Cmd::CHARMSETEVENTRATE) {
							using namespace magic_enum::ostream_operators;
							//LOG_ERROR << mp.datagenerator << " not supported:" << cmd_2 << std::endl; // TODO string conversion?
						rv = false;
					}
				}
			}
			if (mp.datagenerator == Mesytec::DataGenerator::Mcpd8) {
				if (cmd_2.has_value()) {
					if (cmd_2 == Mcpd8::Internal_Cmd::SETNUCLEORATEEVENTSPERSECOND
						|| cmd_2 == Mcpd8::Internal_Cmd::CHARMPATTERNGENERATOR || cmd_2 == Mcpd8::Internal_Cmd::CHARMSETEVENTRATE) {
						using namespace magic_enum::ostream_operators;
						//LOG_ERROR << mp.datagenerator << " not supported:" << cmd_2 << std::endl; // TODO string conversion?
						rv = false;
					}
				}

			}
			return rv;
		}
		void MesytecSystem::Send(std::pair<const unsigned char, Mesytec::DeviceParameter> &kvp,Mcpd8::Internal_Cmd cmd, unsigned long param) {
			Mcpd8::CmdPacket cmdpacket;
			Mcpd8::CmdPacket response;
			cmdpacket.cmd = cmd;
			switch (cmd) {
			case Mcpd8::Internal_Cmd::CHARMPATTERNGENERATOR:
			{
				char cmdbytes[] = { '\x06','\x00','\x06','\x00','\x04','\x00','\x01','\x00','\x00','\x8a','\x00','\x00' };
				if (param == 1) cmdbytes[10] = '\x01';
				size_t bytessent = kvp.second.socket->send_to(boost::asio::buffer(cmdbytes), kvp.second.charm_cmd_endpoint);

			}
			break;
			case Mcpd8::Internal_Cmd::CHARMSETEVENTRATE:
			{
				char cmdbytes[] = { '\x06','\x00','\x06','\x00','\x04','\x00','\x02','\x00','\x30','\x8a','\x00','\x00' };
				unsigned short data = (unsigned short)(50 * 1000000 / (param));

				if (data <= 2) {
					using namespace magic_enum::ostream_operators;
					auto cmd_2 = magic_enum::enum_cast<Mcpd8::Internal_Cmd>(cmdpacket.cmd);
					std::stringstream ss1;
					ss1 << cmd_2 << ":data must be >2";
					LOG_WARNING << ss1.str()<<std::endl;
					break;
				}
				short* sp = (short*)&cmdbytes[10];
				*sp = data;
				size_t bytessent = kvp.second.socket->send_to(boost::asio::buffer(cmdbytes),kvp.second.charm_cmd_endpoint);
				simulatordatarate = param;
			}
			break;
			case Mcpd8::Internal_Cmd::SETNUCLEORATEEVENTSPERSECOND:
			{
				unsigned long* p = reinterpret_cast<unsigned long*>(&cmdpacket.data[0]);
				*p = param;
				cmdpacket.Length += 2;
			}
			try {
				response = Send(kvp, cmdpacket);
				
			}
			catch (Mesytec::cmd_error& x) {
				// SETNUCLEORATEEVENTSPERSECOND has 0x8000 bit always set, so it is falsely as CMD not understood 
				if (int const* mi = boost::get_error_info<Mesytec::my_info>(x)) {
					auto  my_code = magic_enum::enum_cast<Mesytec::cmd_errorcode>(*mi);
					if (my_code != Mesytec::cmd_errorcode::CMD_NOT_UNDERSTOOD) {
						throw;
					}
					simulatordatarate = param;
				}

			}
			
			break;
			case Mcpd8::Internal_Cmd::GETVER:
				response = Send(kvp, cmdpacket);
				if (response.Length - response.headerLength < 3) {
					if (response.Length - response.headerLength == 0) internalerror = Mesytec::cmd_errorcode::CMD_RESPONSE_ZERO_LENGTH;
					else  internalerror = Mesytec::cmd_errorcode::CMD_RESPONSE_WRONG_LENGTH;
					throw cmd_error() << my_info(internalerror);
				}

				{
					LOG_DEBUG << "CPU " << response.data[0] << "." << response.data[1] << ", FPGA " << ((response.data[2] >> 8) & 0xff) << "." << (response.data[2] & 0xff) << std::endl;
				}
				kvp.second.firmware_major = response.data[0];
				kvp.second.firmware_minor = response.data[1];
				break;


			case Mcpd8::Internal_Cmd::READID: {
				response = Send(kvp, cmdpacket);
				int n_modules = (response.Length - response.headerLength);
				if (n_modules < 0 || n_modules > 8) {
					n_modules = 8;
					//boost::mutex::scoped_lock lock(coutGuard);
					//LOG_ERROR << response << " n_modules range [0..." << Mesy::Mpsd8Event::sizeMODID << "]" << std::endl;
					//break;
				}
				{
					std::stringstream ss_1;
					
					using namespace magic_enum::ostream_operators;
					using namespace magic_enum::bitwise_operators;
					for (int i = 0; i < n_modules; i++) {
						auto mid = magic_enum::enum_cast<Mesy::ModuleId>(response.data[i]);
						if (i == 0) ss_1 << (kvp.second.mcpd_endpoint.address()).to_string() << " ";
						if (mid.has_value()) {
							kvp.second.module_id[i] = mid.value();
						}
						ss_1 << kvp.second.module_id[i] << " ";
					}
					LOG_INFO << ss_1.str() << std::endl;
				}
				break;
			}

			case Mcpd8::Internal_Cmd::READPERIREG:{

				using namespace magic_enum::ostream_operators;
				using namespace magic_enum::bitwise_operators;

				assert(param >= 0 && param < Mpsd8_sizeMODID);
				cmdpacket.data[0] = (unsigned short) param;

				for (int i = 0; i <= 2; i++) {
					unsigned short regnum = i;
					// 0 =>tx capabilities
					// 1 => tx cap settings
					// 2 => firmware
					cmdpacket.data[1] = regnum;
					cmdpacket.Length = Mcpd8::CmdPacket::defaultLength + 2;
					response = Send(kvp, cmdpacket);
					unsigned short slot = response.data[0];
					unsigned short capbitmap = response.data[2];
					unsigned short regword = response.data[1];
					switch (regword) {
					case 0:
						kvp.second.module[slot].tx_caps = response.data[2];
						break;
					case 1:
					{
						auto txcapsetting = magic_enum::enum_cast<Mcpd8::TX_CAP>(response.data[2]);
						if (txcapsetting.has_value()) kvp.second.module[slot].tx_cap_setting = txcapsetting.value();

					}
					break;
					case 2:
						kvp.second.module[slot].firmware = response.data[2];
						break;
					}
				}
				break;
			}
			}
		}
		void MesytecSystem::Send(std::pair<const unsigned char, Mesytec::DeviceParameter>& kvp,Mcpd8::Cmd cmd,unsigned long lparam) {

			unsigned short param = (unsigned short)lparam;
			unsigned short param1 = (unsigned short)(lparam >> 16);
			Mcpd8::CmdPacket cmdpacket;
			Mcpd8::CmdPacket response;
			cmdpacket.cmd = cmd;
			switch (cmd) {
			case Mcpd8::Cmd::RESET:
			case Mcpd8::Cmd::START:
				kvp.second.lastbufnum = 0;
			
			case Mcpd8::Cmd::CONTINUE:
			case Mcpd8::Cmd::STOP:
				Send(kvp, cmdpacket, true);
				break;
			case Mcpd8::Cmd::STOP_UNCHECKED:
				cmdpacket.cmd = Mcpd8::Cmd::STOP;
				Send(kvp, cmdpacket, false);
				break;



			case Mcpd8::Cmd::SETID:{
				cmdpacket.data[0] = param;
				// problem here: oldid must be known, otherwise command rejected
				// we just loop through old ids from 0 to loopuntil 
				// todo: command rejected but returns actual id which we can use!
				char loopuntil = 127 ;
				std::list<unsigned char> ids = std::list<unsigned char>();
				for (int i = 0; i < loopuntil; i++) {
					if (i != kvp.first) ids.push_back((unsigned char)i);
				}
				ids.push_front((unsigned char) kvp.first);
				for (auto &id: ids) {
					cmdpacket.deviceStatusdeviceId = ((unsigned char)id) << 8;
					cmdpacket.Length = Mcpd8::CmdPacket::defaultLength + 1;
					try {
						cmdpacket.Length = Mcpd8::CmdPacket::defaultLength + 1;
						response = Send(kvp, cmdpacket);
						unsigned char devid = (unsigned char) response.data[0];
						if (id != devid) oldidnewid.insert(std::pair<const unsigned char,unsigned char>(id, devid));
						LOG_INFO << (kvp.second.mcpd_endpoint.address()).to_string() << " DevId:" << (int)devid <<std::endl;
						break;
					}
					catch (Mesytec::cmd_error& x) {
						if (int const* mi = boost::get_error_info<Mesytec::my_info>(x)) {
							auto  my_code = magic_enum::enum_cast<Mesytec::cmd_errorcode>(*mi);
							if (my_code != Mesytec::cmd_errorcode::CMD_NOT_UNDERSTOOD) {
								throw;
							}
						}
					}
					if (id == loopuntil) {
						internalerror = Mesytec::cmd_errorcode::SEND_CMD_TIMEOUT;
						boost::throw_exception(cmd_error() << my_info(internalerror));
					}
				}
				
				}
				break;

			case Mcpd8::Cmd::SETTIMING:
				cmdpacket.data[0] = param;
				cmdpacket.data[1] = param1;
				cmdpacket.Length = Mcpd8::CmdPacket::defaultLength + 2;
				response = Send(kvp, cmdpacket);

				{
					
					using namespace magic_enum::ostream_operators;
					std::stringstream ss;
					ss << (kvp.second.mcpd_endpoint.address()).to_string() << " " << cmd;
					//ss << "(0x" << std::hex << static_cast<magic_enum::underlying_type_t<Mcpd8::Cmd>>(cmd) << std::dec << ")";
					ss << " ";
					if (response.data[0] == 0)  ss << "Slave";
					else ss << "Master";
					if (response.data[1] == 0) ss << " sync_bus_termination";
					else ss << " ";
					LOG_INFO<< ss.str()<<std::endl;


				}

				break;

			case Mcpd8::Cmd::SETCLOCK:
				if (param == Mesy::TimePoint::ZERO) {
					cmdpacket.data[0] = cmdpacket.data[1] = cmdpacket.data[2] = 0;
				}
				
				cmdpacket.Length = Mcpd8::CmdPacket::defaultLength + 3;
				response = Send(kvp, cmdpacket);
				{
			//		long long delta = Mcpd8::DataPacket::timeStamp(response.time) - Mcpd8::DataPacket::timeStamp(cmdpacket.data);
			//		LOG_INFO << delta/10 << " microseconds"<<std::endl;
				
				}
				break;

			case Mcpd8::Cmd::SETRUNID:
				cmdpacket.data[0] = param;
				cmdpacket.Length= Mcpd8::CmdPacket::defaultLength + 1;
				response=Send(kvp,cmdpacket);
				if (response.data[0] != param)
				{
					using namespace magic_enum::ostream_operators;
					LOG_ERROR<< (kvp.second.mcpd_endpoint.address()).to_string()<<" " << cmd << " requested=0x" <<std::hex << param << " returned=0x" << response.data[0] <<std::dec << std::endl;
				}
				
				break;
			case Mcpd8::Cmd::SETTHRESH:
				cmdpacket.data[0] = param;
				cmdpacket.data[1] = param1;
				cmdpacket.Length = Mcpd8::CmdPacket::defaultLength + 2;
				response = Send(kvp, cmdpacket);
				break;
			case Mcpd8::Cmd::GETPARAMETERS:
				response = Send(kvp,cmdpacket);
				break;
			case Mcpd8::Cmd::GETCAPABILITIES:
				response = Send(kvp,cmdpacket);
				//10 11 
				break;

		
			case Mcpd8::Cmd::GETMPSD8PLUSPARAMETERS:
				cmdpacket.data[0] = param;
				cmdpacket.Length = Mcpd8::CmdPacket::defaultLength + 1;
				response = Send(kvp, cmdpacket);
				if (response.data[0] >= 8) {
					using namespace magic_enum::ostream_operators;
					LOG_ERROR << cmd << "RESPONSE OUTSIDE RANGE: data[0]=0x" <<std::hex<< response.data[0] <<std::dec<< std::endl;

				}
				else {
					using namespace magic_enum::ostream_operators;
					using namespace magic_enum::bitwise_operators;
					std::stringstream ss;

					ss << cmd << ":MPSD device number(" << response.data[0] << ") " << kvp.second.module_id[response.data[0]];
					ss << " Capabilities(";
					for (auto& c : magic_enum::enum_values<Mcpd8::TX_CAP>()) {
						if (response.data[1] & c) ss << c << " ";
					}
					ss << ")," ;
					ss << " Firmware=" << ((response.data[3]>>8) & 0xff) <<"."<<( response.data[3] & 0xff)  << std::endl;
					ss << "Current tx format:";
					for (auto& c : magic_enum::enum_values<Mcpd8::TX_CAP>()) {
						if (response.data[2] & c) ss << c << " ";
					}

					ss <<  std::endl;
					 
				}
				

				//10 11 
				break;
			}
		}
		void MesytecSystem::SendAll(Mcpd8::Cmd cmd) {
			//if (internalerror != Mesytec::cmd_errorcode::OK) return;
			{
				using namespace magic_enum::ostream_operators;
				std::stringstream ss;
				ss << cmd;
				LOG_DEBUG << "SendAll(" <<  ss.str() << ")" << std::endl;
			}


			if (cmd == Mcpd8::Cmd::START) {
				for (auto &kvp : deviceparam) {
					Send(kvp, Mcpd8::Cmd::SETRUNID,currentrunid);
				}
				started = boost::chrono::system_clock::now();
				currentrunid++;
				daq_running = true;
			}
			if (cmd == Mcpd8::Cmd::CONTINUE) {
				boost::chrono::system_clock::time_point tpstarted = started;
				boost::chrono::system_clock::time_point tpstopped = stopped;
				started = boost::chrono::system_clock::now() - (tpstopped - tpstarted);
				daq_running = true;

			}
			
			
			for (auto &kvp : deviceparam) {
				Send(kvp, cmd);
			}
			

			if (cmd == Mcpd8::Cmd::STOP) {
				stopped = boost::chrono::system_clock::now();
				daq_running = false;

			}
			
		}
		Mcpd8::CmdPacket& MesytecSystem::Send(std::pair<const unsigned char, Mesytec::DeviceParameter>& kvp,Mcpd8::CmdPacket& cmdpacket,bool waitresponse) {
			std::stringstream ss_log;
			int timeout_ms = 750; //GETVER takes up to 520 ms if no MPSD8(+) is connected
			for (int i = 0; i < timeout_ms / 25; i++) {
				if (wait_response == false) break;
				boost::this_thread::sleep_for(boost::chrono::milliseconds(25));
			}
			assert(wait_response == false);
			CmdSupported(kvp.second, cmdpacket.cmd);
			using namespace magic_enum::ostream_operators;
			auto cmd_1 = magic_enum::enum_cast<Mcpd8::Cmd>(cmdpacket.cmd);
			auto cmd_2 = magic_enum::enum_cast<Mcpd8::Internal_Cmd>(cmdpacket.cmd);
			std::stringstream ss_cmd;
			if (cmd_1.has_value()) 	ss_cmd << cmd_1 
				//<< hexfmt((unsigned short) static_cast<magic_enum::underlying_type_t<Mcpd8::Cmd>>(cmdpacket.cmd))
				;
			else {
				ss_cmd << cmd_2;
				if (!cmd_2.has_value()) ss_cmd << hexfmt(cmdpacket.cmd);
				else {
					ss_cmd
					//	<< hexfmt((unsigned short) static_cast<magic_enum::underlying_type_t<Mcpd8::Internal_Cmd>>(cmdpacket.cmd))
					;
				}
			}
			if (inputFromListmodeFile) {
				LOG_WARNING << "Cannot Send "<< ss_cmd.str()  <<" because inputFromListmodeFile==true" << std::endl;
				return cmdpacket;
			}
			
			if (cmdpacket.cmd != Mcpd8::Cmd::SETID) {
				cmdpacket.deviceStatusdeviceId = ((unsigned char)kvp.first) << 8;
			}
			// SETID is a special case where old id must be in cmdpacket.deviceStatusdeviceId
			cmdpacket.Number = Mcpd8::sendcounter++;
			static Mcpd8::CmdPacket response;	//static is needed on Linux 
			auto start=boost::chrono::system_clock::now();
			{
					ss_log <<  " SEND(" << ss_cmd.str() << ") to " << kvp.second.mcpd_endpoint << ":";
				//LOG_DEBUG<< cmdpacket << std::endl;
			}
			boost::function<void()> send = [this,&kvp, &cmdpacket]() {
				auto start = boost::chrono::system_clock::now();
				try {
					Mcpd8::CmdPacket::Send(kvp.second.socket, cmdpacket, kvp.second.mcpd_endpoint);
					do {
					  pio_service->run_one();
					} while (wait_response == true);
				}
				catch (std::exception& ) {
					throw;
				}
			};

			if (waitresponse)	worker_threads.create_thread(boost::bind(send)); // so use thread to send and wait here
			else Mcpd8::CmdPacket::Send(kvp.second.socket, cmdpacket, kvp.second.mcpd_endpoint); 
			
			if (!waitresponse) {
				ss_log << " =>RESPONSE UNKNOWN (not checked)" ;
				LOG_DEBUG << ss_log.str() << std::endl;
				return response;
			}
			wait_response = true;
			for (int l = 0; l < timeout_ms / 50; l++) {
				boost::this_thread::sleep_for(boost::chrono::milliseconds(50));
				Zweistein::ReadLock r_lock(cmdLock);
				int s = (int) cmd_recv_queue.size();
				for (int i = 0; i < cmd_recv_queue.size(); i++) {
					response = cmd_recv_queue[i];
					if (response.Number == cmdpacket.Number) {
							if (response.cmd == cmdpacket.cmd || (response.cmd == (cmdpacket.cmd | 0x8000))) {
								wait_response = false;
							if (response.cmd & 0x8000) {
								boost::chrono::milliseconds ms = boost::chrono::duration_cast<boost::chrono::milliseconds> (boost::chrono::system_clock::now() - start);
								ss_log <<  " NOT UNDERSTOOD! +"<<ms ;
								ss_log << std::endl<< "RESPONSE: " << response << std::endl;
								LOG_WARNING << ss_log.str() << std::endl;
								internalerror = Mesytec::cmd_errorcode::CMD_NOT_UNDERSTOOD;
								throw cmd_error() << my_info(internalerror);
							}
							else {
								
								boost::chrono::milliseconds ms = boost::chrono::duration_cast<boost::chrono::milliseconds> (boost::chrono::system_clock::now() - start);
								ss_log << " RESPONSE OK(" << response.Length - response.headerLength << " items) +"<<ms ;
								ss_log << "RESPONSE: " <<  response;
								//LOG_DEBUG << response << std::endl;
							}

							if (response.cmd == Mcpd8::Cmd::RESET || response.cmd == Mcpd8::Cmd::START) {
								kvp.second.starttime=Mcpd8::DataPacket::timeStamp(&response.time[0]);
								data.evntcount = 0;
								bool ok = data.evntqueue.push(Zweistein::Event::Reset());
								if (!ok) LOG_ERROR << " cannot push Zweistein::Event::Reset()" << std::endl;
								// push reset_event?
							}
							if (response.cmd == Mcpd8::Cmd::RESET || response.cmd == Mcpd8::Cmd::STOP) {
								Mesytec::listmode::stopwriting = true;
							}

							return response;
							internalerror = Mesytec::cmd_errorcode::OK;
						}
					}
				}
				
				
			}
		
			{
				wait_response = false;
				ss_log << " Timeout(" << timeout_ms << " ms)" ;
				LOG_ERROR << ss_log.str() << std::endl;
				internalerror = Mesytec::cmd_errorcode::SEND_CMD_TIMEOUT;
				BOOST_THROW_EXCEPTION (cmd_error() << my_info(internalerror));
				
			}

			return response;
		}


		void MesytecSystem::PushNeutronEventOnly_HandleOther(Zweistein::Event& Ev) {
			if (Ev.type == Mesy::EventType::NEUTRON) {
				bool ok=data.evntqueue.push(Ev);
				if(ok)	data.evntcount++;
				else {
					boost::chrono::duration<double> sec = boost::chrono::system_clock::now() - lasteventqueuefull;
					lasteventqueuefull_missedcount += 1;
					if (sec.count() > 1) {
						LOG_ERROR  << ": EVENTQUEUE FULL , SKIPPED " << lasteventqueuefull_missedcount << " Event(s)" << std::endl;
						lasteventqueuefull = boost::chrono::system_clock::now();
						lasteventqueuefull_missedcount = 0;
					}
				}
			}
			else {

			}
			
		}
	    void MesytecSystem::analyzebuffer(Mcpd8::DataPacket &datapacket)
		{
			unsigned char id = Mcpd8::DataPacket::getId(datapacket.deviceStatusdeviceId);
			auto &params = deviceparam[id];
			unsigned short numevents = datapacket.numEvents();
			boost::chrono::nanoseconds headertime = Mcpd8::DataPacket::timeStamp(datapacket.time);
			unsigned short neutrons = 0;
			//boost::chrono::nanoseconds elapsed = headertime - boost::chrono::nanoseconds( starttime);
			// in mesytec time
			
			//if (numevents == 0) LOG_WARNING << "0 EVENTS, TimeStamp:" << headertime << std::endl;
			
			switch (datapacket.GetBuffertype()) {
			case Mesy::BufferType::DATA: 
				
			
				if (datapacket.Number != (unsigned short)(params.lastbufnum + 1)) {
					if (datapacket.Number == (unsigned char)(params.lastbufnum + 1)) {
						if (!warning_notified_bufnum_8bit) {
							warning_notified_bufnum_8bit = true;
							LOG_WARNING <<  "Buffer Number Counter 8 bit only (heuristic detection)" << std::endl;
						}
					}
					else {
						boost::chrono::duration<double> sec = boost::chrono::system_clock::now() - params.lastmissed_time;
						unsigned short missed = (unsigned short)(datapacket.Number - (unsigned short)(params.lastbufnum + 1));
						params.lastmissed_count += missed;
						if (sec.count() > 1) {
							boost::chrono::system_clock::time_point tps(started);
							if (boost::chrono::system_clock::now() - tps >= boost::chrono::seconds(3)) {
								LOG_ERROR << params.mcpd_endpoint << ": MISSED " << params.lastmissed_count << " BUFFER(s)" << std::endl;
							}
							params.lastmissed_time = boost::chrono::system_clock::now();
							params.lastmissed_count = 0;
						}
					}
				}
				params.lastbufnum = datapacket.Number;
				for (int i = 0; i < numevents; i++) {
					Zweistein::Event Ev = Zweistein::Event(&datapacket.events[i], headertime, params.offset , params.module_id);
					PushNeutronEventOnly_HandleOther(Ev);
				}		
				break;
			case Mesy::BufferType::MDLL: 
				if (datapacket.Number != (unsigned short)(params.lastbufnum + 1)) {
					boost::chrono::duration<double> sec = boost::chrono::system_clock::now() - params.lastmissed_time;
					unsigned short missed = (unsigned short)(datapacket.Number - (unsigned short)(params.lastbufnum + 1));
					params.lastmissed_count += missed;
					if (sec.count() > 1) {
						boost::chrono::system_clock::time_point tps(started);
						if (boost::chrono::system_clock::now() - tps >= boost::chrono::seconds(3)){
							LOG_ERROR << params.mcpd_endpoint << ": MISSED " << params.lastmissed_count << " BUFFER(s)" << std::endl;
						}
						params.lastmissed_time = boost::chrono::system_clock::now();
						params.lastmissed_count = 0;
					}

				}
				params.lastbufnum = datapacket.Number;
				for (int i = 0; i < numevents; i++) {
					Zweistein::Event Ev = Zweistein::Event(Mesy::MdllEvent::fromMpsd8(&datapacket.events[i]), headertime, params.offset, params.module_id);
					PushNeutronEventOnly_HandleOther(Ev);
				}
				break;
			}
		}
		std::map<const unsigned char, Mesytec::DeviceParameter> MesytecSystem::deviceparam = std::map<const unsigned char, Mesytec::DeviceParameter>();
	};





