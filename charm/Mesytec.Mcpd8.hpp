/***************************************************************************
 *   Copyright (C) 2019 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/

#pragma once
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
#include "Mcpd8.enums.hpp"
#include <csignal>
#include <asioext/unique_file_handle.hpp>
#include <asioext/file_handle.hpp>
#include <asioext/open.hpp>
#include <asioext/standard_streams.hpp>

#include <asioext/file.hpp>
#include <asioext/thread_pool_file_service.hpp>
#include <asioext/open_flags.hpp>

#include <asioext/linear_buffer.hpp>

#include "Mesytec.listmode.hpp"
#include "MesytecSystem.Data.hpp"
#include "Mcpd8.CmdPacket.hpp"
#include "Mcpd8.Parameters.hpp"
#include "Mesytec.DeviceParameter.hpp"


#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>

typedef boost::shared_mutex Lock;
typedef boost::unique_lock< Lock >  WriteLock;
typedef boost::shared_lock< Lock >  ReadLock;

using boost::asio::ip::udp;

namespace Mesytec {
	typedef boost::error_info<struct tag_my_info, int> my_info;
	struct cmd_error : virtual boost::exception, virtual std::exception { };
	enum cmd_errorcode {
		OK=0,
		SEND_CMD_TIMEOUT=1,
		CMD_NOT_UNDERSTOOD=2,
		CMD_RESPONSE_WRONG_LENGTH=3,
		CMD_RESPONSE_ZERO_LENGTH=4
	};

	
	class MesytecSystem {
	public:
		static std::map<const unsigned short, Mesytec::DeviceParameter> deviceparam;
		MesytecSystem():recv_buf(), cmd_recv_queue(5),simulatordatarate(20), inputFromListmodeFile(false),connected(false),internalerror(cmd_errorcode::OK), currentrunid(0), pio_service(nullptr), eventdataformat(Mcpd8::EventDataFormat::Undefined),icharmid(0){
			 wait_response = false;
		}
		~MesytecSystem(){
			for (const auto& [key, value] : deviceparam) {
				if (value.bNewSocket && value.socket) value.socket->close();
			}
			if (pstrand) delete pstrand;
			
		}
		bool daq_running = false;
		unsigned short currentrunid;
		int icharmid;
		cmd_errorcode internalerror;
		boost::asio::io_service* pio_service;
		Mcpd8::EventDataFormat eventdataformat;
		
		Mcpd8::Data data;
		bool inputFromListmodeFile;
		boost::atomic<bool> connected;
		boost::asio::io_service::strand *pstrand;
		bool connect(std::list<Mcpd8::Parameters> &_devlist, boost::asio::io_service &io_service)
		{
			data.widthX = 0;
			data.widthY = 0;
			pstrand=new boost::asio::io_service::strand(io_service);
			boost::system::error_code ec;
			for(Mcpd8::Parameters p:_devlist) {
				Mesytec::DeviceParameter mp;

				local_endpoint = udp::endpoint(boost::asio::ip::address::from_string(p.networkcard), p.mcpd_port);
				mp.mcpd_endpoint = udp::endpoint(boost::asio::ip::address::from_string(p.mcpd_ip),p.mcpd_port);
				{
					boost::mutex::scoped_lock lock(coutGuard);
					std::cout << "Local bind " << local_endpoint << " to " << mp.mcpd_endpoint << std::endl;
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
					mp.socket->bind(local_endpoint, ec);
				}

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
				
				if (mp.datagenerator == Mesytec::DataGenerator::Charm || mp.datagenerator == Mesytec::DataGenerator::CharmSimulator) {
					mp.charm_cmd_endpoint = udp::endpoint(boost::asio::ip::address::from_string(p.mcpd_ip), p.charm_port);
				}
				if (!skip) {
					mp.offset = data.widthX;
					deviceparam.insert(std::pair<const unsigned short, Mesytec::DeviceParameter>(p.mcpd_id, mp));
					switch (eventdataformat) {
					case Mcpd8::EventDataFormat::Mpsd8:
						data.widthY = (unsigned short)Mesy::Mpsd8Event::sizeY;
						data.widthX+= (unsigned short)(Mesy::Mpsd8Event::sizeMODID * Mesy::Mpsd8Event::sizeSLOTS );
						break;
					case Mcpd8::EventDataFormat::Mdll:
						data.widthX+= (unsigned short)(Mesy::MdllEvent::sizeX);
						data.widthY = (unsigned short)Mesy::MdllEvent::sizeY;
						break;
					case Mcpd8::EventDataFormat::Charm:
						data.widthX+= (unsigned short)(Mesy::MdllEvent::sizeX);
						data.widthY = (unsigned short)Mesy::MdllEvent::sizeY;
						break;
					}
				}
			}
			
			
			pio_service = &io_service;
			worker_threads.create_thread(boost::bind(&MesytecSystem::watch_incoming_packets, this));		//worker_threads.add_thread(new boost::thread(&MesytecSystem::watch_incoming_packets, this, &io_service));
			boost::this_thread::sleep_for(boost::chrono::milliseconds(100)); // we want watch_incoming_packets active

			for (const auto& [key, value] : deviceparam) {
				if(value.bNewSocket) start_receive(value);

				if (value.datagenerator == Mesytec::DataGenerator::Charm || value.datagenerator == Mesytec::DataGenerator::CharmSimulator) {
					icharmid = key;

				}

			}
		
			for (auto& kvp : deviceparam) {
				Send(kvp, Mcpd8::Cmd::SETID, kvp.first); // set ids for Mcpd8 devices
			}
			
			for (auto& onid : oldidnewid) {
				auto it= deviceparam.find(onid.first);
				if (it != deviceparam.end()) {
					boost::mutex::scoped_lock lock(coutGuard);
					deviceparam.insert(std::pair<const unsigned short, Mesytec::DeviceParameter>(onid.second, it->second));
					deviceparam.erase(it);
					std::cout << (it->second.mcpd_endpoint.address()).to_string() << "SETID: NEW ID=" << onid.second << " (OLD ID=" << onid.first << ")" << std::endl;

				}
				else {
					boost::mutex::scoped_lock lock(coutGuard);
					std::cout << "ERROR: " << std::endl;
				}
				
			}

			for (  auto &kvp: deviceparam) {
				Send(kvp, Mcpd8::Internal_Cmd::GETVER); //
				Send(kvp, Mcpd8::Internal_Cmd::READID);
				Send(kvp, Mcpd8::Cmd::GETPARAMETERS);
				
				if (kvp.second.datagenerator == Mesytec::DataGenerator::Mcpd8 || kvp.second.datagenerator == Mesytec::DataGenerator::NucleoSimulator) {
					for (int i = 0; i < Mesy::Mpsd8Event::sizeSLOTS; i++) {
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
			connected = true;
			return connected;
		}
		long simulatordatarate;
		bool CmdSupported(Mesytec::DeviceParameter& mp, unsigned short cmd) {

			using namespace magic_enum::ostream_operators;
			auto cmd_1 = magic_enum::enum_cast<Mcpd8::Cmd>(cmd);
			auto cmd_2 = magic_enum::enum_cast<Mcpd8::Internal_Cmd>(cmd);

			bool rv = true;

			if (mp.datagenerator == Mesytec::DataGenerator::NucleoSimulator) {

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
		void Send(std::pair<const unsigned short, Mesytec::DeviceParameter> &kvp,Mcpd8::Internal_Cmd cmd, unsigned long param = 0) {
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
					boost::mutex::scoped_lock lock(coutGuard);
					using namespace magic_enum::ostream_operators;
					auto cmd_2 = magic_enum::enum_cast<Mcpd8::Internal_Cmd>(cmdpacket.cmd);
					std::cout << cmd_2 << ":data must be >2" << std::endl;
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
				boost::mutex::scoped_lock lock(coutGuard);
				// SETNUCLEORATEEVENTSPERSECOND has 0x8000 bit always set, so it is falsely as CMD not understood 
				if (int const* mi = boost::get_error_info<Mesytec::my_info>(x)) {
					auto  my_code = magic_enum::enum_cast<Mesytec::cmd_errorcode>(*mi);
				//	if (my_code != Mesytec::cmd_errorcode::CMD_NOT_UNDERSTOOD) throw;

				}

			}
			simulatordatarate = param;
			break;
			case Mcpd8::Internal_Cmd::GETVER:
				response = Send(kvp, cmdpacket);
				if (response.Length - response.headerLength < 3) {
					if (response.Length - response.headerLength == 0) internalerror = Mesytec::cmd_errorcode::CMD_RESPONSE_ZERO_LENGTH;
					else  internalerror = Mesytec::cmd_errorcode::CMD_RESPONSE_WRONG_LENGTH;
					throw cmd_error() << my_info(internalerror);
				}

				{
					boost::mutex::scoped_lock lock(coutGuard);
					std::cout << "CPU " << response.data[0] << "." << response.data[1] << ", FPGA " << (response.data[2] & 0xff) << "." << ((response.data[2] >> 8) & 0xff) << std::endl;
				}
				kvp.second.firmware_major = response.data[0];
				kvp.second.firmware_minor = response.data[1];
				break;


			case Mcpd8::Internal_Cmd::READID: {
				response = Send(kvp, cmdpacket);
				int n_modules = (response.Length - response.headerLength);
				if (n_modules < 0 || n_modules > 8) {
					boost::mutex::scoped_lock lock(coutGuard);
					std::cout << "ERROR:" << response << " n_modules range [0..." << Mesy::Mpsd8Event::sizeMODID << "]" << std::endl;
					break;
				}
				{
					boost::mutex::scoped_lock lock(coutGuard);
					using namespace magic_enum::ostream_operators;
					for (int i = 0; i < n_modules; i++) {
						auto mid = magic_enum::enum_cast<Mesy::ModuleId>(response.data[i]);
						if (i == 0) std::cout << (kvp.second.mcpd_endpoint.address()).to_string() << " ";
						if (mid.has_value()) {
							kvp.second.module_id[i] = mid.value();
						}
						std::cout << kvp.second.module_id[i] << " ";
					}
					std::cout << std::endl;
				}
				break;
			}

			case Mcpd8::Internal_Cmd::READPERIREG:{
				assert(param >= 0 && param < Mesy::Mpsd8Event::sizeMODID);
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

		void Send(std::pair<const unsigned short, Mesytec::DeviceParameter>& kvp,Mcpd8::Cmd cmd,unsigned short param=0) {
			Mcpd8::CmdPacket cmdpacket;
			Mcpd8::CmdPacket response;
			cmdpacket.cmd = cmd;
			switch (cmd) {
			case Mcpd8::Cmd::RESET:
			case Mcpd8::Cmd::START:
				kvp.second.lastbufnum = 0;
			case Mcpd8::Cmd::STOP:
			case Mcpd8::Cmd::CONTINUE:
				Send(kvp, cmdpacket, true);
				break;
			case Mcpd8::Cmd::SETID:{
				cmdpacket.data[0] = param;
				// problem here: oldid must be known, otherwise command rejected
				// we just loop through old ids from 0 to 7 
				int id = 0;
				std::list<unsigned char> ids = std::list<unsigned char>();
				for (int i = 0; i < 8; i++) {
					if (i != kvp.first) ids.push_back((unsigned char)i);
				}
				ids.push_front((unsigned char) kvp.first);

				for (auto &id: ids) {
					cmdpacket.deviceStatusdeviceId = ((unsigned char)id) << 8;
					cmdpacket.Length = Mcpd8::CmdPacket::defaultLength + 1;
					try {
						cmdpacket.Length = Mcpd8::CmdPacket::defaultLength + 1;
						response = Send(kvp, cmdpacket);
						unsigned short devid = response.data[0];
						break;
					}

					catch (Mesytec::cmd_error& x) {
						boost::mutex::scoped_lock lock(coutGuard);
						if (int const* mi = boost::get_error_info<Mesytec::my_info>(x)) {
							auto  my_code = magic_enum::enum_cast<Mesytec::cmd_errorcode>(*mi);
							if (my_code == Mesytec::cmd_errorcode::CMD_NOT_UNDERSTOOD) continue;

						}

					}
				}
				if (id == 8) {
					internalerror = Mesytec::cmd_errorcode::SEND_CMD_TIMEOUT;
					boost::throw_exception( cmd_error() << my_info(internalerror));
				}
				}
				break;
			case Mcpd8::Cmd::SETRUNID:
				cmdpacket.data[0] = param;
				cmdpacket.Length= Mcpd8::CmdPacket::defaultLength + 1;
				response=Send(kvp,cmdpacket);
				if (response.data[0] != param)
				{
					boost::mutex::scoped_lock lock(coutGuard);
					using namespace magic_enum::ostream_operators;
					std::cout<< (kvp.second.mcpd_endpoint.address()).to_string()<<" " << cmd << " ERROR :requested=" << param << " returned=" << response.data[0] << std::endl;
				}
				
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
					boost::mutex::scoped_lock lock(coutGuard);
					using namespace magic_enum::ostream_operators;
					std::cout << cmd << "RESPONSE OUTSIDE RANGE: data[0]=" << response.data[0] << std::endl;

				}
				else {
					boost::mutex::scoped_lock lock(coutGuard);
					using namespace magic_enum::ostream_operators;

					std::cout << cmd << ":MPSD device number(" << response.data[0] << ") " << kvp.second.module_id[response.data[0]];
					std::cout << " Capabilities(";
					for (auto& c : magic_enum::enum_values<Mcpd8::TX_CAP>()) {
						if (response.data[1] & c) std::cout << c << " ";
					}
					std::cout << ")" << std::endl;
					std::cout << " Firmware=" << response.data[3] << std::endl;
					std::cout << "Current tx format:";
					for (auto& c : magic_enum::enum_values<Mcpd8::TX_CAP>()) {
						if (response.data[2] & c) std::cout << c << " ";
					}

					std::cout <<  std::endl;
					 
				}
				

				//10 11 
				break;
			}
		}
		void SendAll(Mcpd8::Cmd cmd) {
			//if (internalerror != Mesytec::cmd_errorcode::OK) return;
			{
				boost::mutex::scoped_lock lock(coutGuard);
				using namespace magic_enum::ostream_operators;
				std::cout << "SendAll(" << cmd << ")" << std::endl;
			}
			if (cmd == Mcpd8::Cmd::START) {
				for (std::pair<const unsigned short, Mesytec::DeviceParameter>& kvp : deviceparam) {
					Send(kvp, Mcpd8::Cmd::SETRUNID,currentrunid);
				}
				currentrunid++;
				daq_running = true;
			}
			if (cmd == Mcpd8::Cmd::STOP) {
				daq_running = false;

			}
			for (std::pair<const unsigned short, Mesytec::DeviceParameter> &kvp : deviceparam) {
				Send(kvp, cmd);
			}
			
		}
		
	private:
		boost::atomic<bool> wait_response;
		Mcpd8::CmdPacket& Send(std::pair<const unsigned short, Mesytec::DeviceParameter>& kvp,Mcpd8::CmdPacket& cmdpacket,bool waitresponse=true) {
			CmdSupported(kvp.second, cmdpacket.cmd);
			unsigned short cmd_target_id = 0;
			for (auto& [key,value] : deviceparam) {
				if (value.socket == kvp.second.socket && value.mcpd_endpoint == kvp.second.mcpd_endpoint) {
					cmd_target_id = key;
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
			
			if (cmdpacket.cmd != Mcpd8::Cmd::SETID) {
				cmdpacket.deviceStatusdeviceId = ((unsigned char)kvp.first) << 8;
			}
			// SETID is a special case where old id must be in cmdpacket.deviceStatusdeviceId
			cmdpacket.Number = Mcpd8::sendcounter++;
			static Mcpd8::CmdPacket response;	//static is needed on Linux 
			auto start=boost::chrono::system_clock::now();
			{
				boost::mutex::scoped_lock lock(coutGuard);
				std::cout << "\r" << boost::chrono::time_fmt(boost::chrono::timezone::local) << start << " SEND(" << ss_cmd.str() << ") to " <<kvp.second.mcpd_endpoint<<":";
				//<< cmdpacket << " ";

			}
			int timeout_ms = 800; // mesytec has slow response to GETVER (550 ms)  

			for (int i = 0; i < timeout_ms/50;i++) {
				if (wait_response == false) break;
				boost::this_thread::sleep_for(boost::chrono::milliseconds(50));
			}
			assert(wait_response == false);

		
		

			boost::function<void()> send = [this,&kvp, &cmdpacket]() {
				auto start = boost::chrono::system_clock::now();
				Mcpd8::CmdPacket::Send(kvp.second.socket, cmdpacket, kvp.second.mcpd_endpoint);
				do {
					pio_service->run_one();
				}while(wait_response==true);
			};

			if (waitresponse)	worker_threads.create_thread(boost::bind(send)); // so use thread to send and wait here
			else Mcpd8::CmdPacket::Send(kvp.second.socket, cmdpacket, kvp.second.mcpd_endpoint); 
			
			
			if (!waitresponse) {
				boost::mutex::scoped_lock lock(coutGuard);
				std::cout << " =>RESPONSE UNKNOWN (not checked)" << std::endl;
				return response;
			}
			
			wait_response = true;
			
			for (int l = 0; l < timeout_ms / 50; l++) {
				boost::this_thread::sleep_for(boost::chrono::milliseconds(50));
				ReadLock r_lock(cmdLock);
				int s = (int) cmd_recv_queue.size();
				for (int i = 0; i < cmd_recv_queue.size(); i++) {
					response = cmd_recv_queue[i];
					if (response.Number == cmdpacket.Number) {
							if (response.cmd == cmdpacket.cmd || (response.cmd == (cmdpacket.cmd | 0x8000))) {
								wait_response = false;
							if (response.cmd & 0x8000) {
								boost::mutex::scoped_lock lock(coutGuard);
								boost::chrono::milliseconds ms = boost::chrono::duration_cast<boost::chrono::milliseconds> (boost::chrono::system_clock::now() - start);
								std::cout <<  " =>WARNING: NOT UNDERSTOOD! +"<<ms<<"ms" << std::endl;
								internalerror = Mesytec::cmd_errorcode::CMD_NOT_UNDERSTOOD;
								throw cmd_error() << my_info(internalerror);
							}
							else {
								boost::mutex::scoped_lock lock(coutGuard);
								boost::chrono::milliseconds ms = boost::chrono::duration_cast<boost::chrono::milliseconds> (boost::chrono::system_clock::now() - start);
								std::cout << " =>RESPONSE OK(" << response.Length - response.headerLength << " items) +"<<ms <<"ms"<< std::endl;
							}
							return response;
							internalerror = Mesytec::cmd_errorcode::OK;
						}
					}
				}
				
				
			}
		
			{
				boost::mutex::scoped_lock lock(coutGuard);
				wait_response = false;
				std::cout << " =>ERROR: Timeout(" << timeout_ms << " ms)" << std::endl;
				internalerror = Mesytec::cmd_errorcode::SEND_CMD_TIMEOUT;
				throw (cmd_error() << my_info(internalerror));
				
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
		void start_receive(const Mesytec::DeviceParameter &mp) {
			mp.socket->async_receive(boost::asio::buffer(recv_buf), boost::bind(&MesytecSystem::handle_receive, this,
				boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
		//	mp.socket->async_receive_from(boost::asio::buffer(recv_buf), mp.mcpd_endpoint,boost::bind(&MesytecSystem::handle_receive, this,
		//		boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
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
				std::cout << boost::chrono::time_fmt(boost::chrono::timezone::local) << boost::chrono::system_clock::now() << " handle_receive(" << error.message() << " ,bytes_transferred=" << bytes_transferred << ", DataPacket=" << dp << std::endl;
				std::cout << "PACKET BUFNUM: " << (unsigned short)(dp.Number) << std::endl;
				//memset(&recv_buf[0], 0, sizeof(Mcpd8::DataPacket));
			}
			unsigned short id = Mcpd8::DataPacket::getId(dp.deviceStatusdeviceId);
			auto &mp = deviceparam[id];
	
			if (dp.Type == Zweistein::reverse_u16(Mesy::BufferType::COMMAND)) {
			
				Mcpd8::CmdPacket p = dp;
				{
					WriteLock w_lock(cmdLock);
					cmd_recv_queue.push_front(p);
				}
				{
				//	boost::mutex::scoped_lock lock(coutGuard);
				//	std::cout<<std::endl << boost::chrono::time_fmt(boost::chrono::timezone::local) << boost::chrono::system_clock::now() << " CMD RECEIVED:" << p << std::endl;
				}
			}
			else if(daq_running) {
					// Mesy::BufferType::DATA
					data.packetqueue.push(dp);
					data.packetqueuecount++;
					data.listmodequeue.push(dp);
					data.listmodepacketcount++;
			}

			start_receive(mp);
		}


		void PushNeutronEventOnly_HandleOther(Zweistein::Event& Ev) {
			if (Ev.type == Mesy::EventType::NEUTRON) {
				data.evntqueue.push(Ev);
				data.evntcount++;
			}
			else {

			}
			
		}
		
		
		
		public:
		void analyzebuffer(Mcpd8::DataPacket &datapacket)
		{
			unsigned short id = Mcpd8::DataPacket::getId(datapacket.deviceStatusdeviceId);
			auto &params = deviceparam[id];
			unsigned short numevents = datapacket.numEvents();
			long long headertime = datapacket.timeStamp();
			unsigned short neutrons = 0;
			switch (datapacket.GetBuffertype()) {
			case Mesy::BufferType::DATA: 
				
				if (datapacket.Number != (unsigned short)(params.lastbufnum + 1)) {
					boost::chrono::duration<double> sec = boost::chrono::system_clock::now() - params.lastmissed_time;
					unsigned short missed = (unsigned short)(datapacket.Number - (unsigned short)(params.lastbufnum + 1));
					params.lastmissed_count += missed;
					if (sec.count() > 1) {
						boost::mutex::scoped_lock lock(coutGuard);
						std::cout << boost::chrono::time_fmt(boost::chrono::timezone::local) << boost::chrono::system_clock::now()<<": "<<params.mcpd_endpoint << ": MISSED " << params.lastmissed_count << " BUFFER(s)" << std::endl;
						params.lastmissed_time = boost::chrono::system_clock::now();
						params.lastmissed_count = 0;
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
					boost::mutex::scoped_lock lock(coutGuard);
					std::cout << boost::chrono::time_fmt(boost::chrono::timezone::local) << boost::chrono::system_clock::now() << " MISSED " << (unsigned short)(datapacket.Number - (unsigned short)(params.lastbufnum + 1)) << " BUFFER(s)" << std::endl;
				}
				params.lastbufnum = datapacket.Number;

				for (int i = 0; i < numevents; i++) {
					Zweistein::Event Ev = Zweistein::Event(Mesy::MdllEvent::fromMpsd8(&datapacket.events[i]), headertime, params.offset, params.module_id);
					PushNeutronEventOnly_HandleOther(Ev);
				}
			
				break;


			}

			
		}
		private:
		udp::endpoint local_endpoint;
		
		boost::array< unsigned char, 64> charm_buf;
		boost::circular_buffer<Mcpd8::CmdPacket> cmd_recv_queue;
		Lock cmdLock;

		boost::array< Mcpd8::DataPacket, 1> recv_buf;
		std::map<const unsigned short, unsigned short> oldidnewid = std::map<const unsigned short, unsigned short>();
		
	};
	std::map<const unsigned short, Mesytec::DeviceParameter> MesytecSystem::deviceparam = std::map<const unsigned short, Mesytec::DeviceParameter>();
	
	
}




