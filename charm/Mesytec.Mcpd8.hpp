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
	struct my_error : virtual boost::exception, virtual std::exception { };
	enum my_errorcode {
		OK=0,
		SEND_CMD_TIMEOUT=1
	};

	
	class MesytecSystem {
	public:
		static std::map<unsigned short, Mesytec::DeviceParameter> deviceparam;
		MesytecSystem():recv_buf(), cmd_recv_queue(5),simulatordatarate(20),lastmissed_count(0), lastmissed_time(boost::chrono::system_clock::now()),
			 inputFromListmodeFile(false),connected(false),internalerror(my_errorcode::OK), firstcallsendwaitresponse(true),cmd_target(0), pio_service(nullptr), eventdataformat(Mcpd8::EventDataFormat::Undefined),icharmid(0){
			data.widthX = 64;
			data.widthY = 64;
		}
		~MesytecSystem(){
			for (std::pair<unsigned short, Mesytec::DeviceParameter> pair : deviceparam) {
	//			if (pair.second.socket) pair.second.socket->close();
			}
			
		}
		int icharmid;
		my_errorcode internalerror;
		boost::asio::io_service* pio_service;
		Mcpd8::EventDataFormat eventdataformat;
		
		Mcpd8::Data data;
		bool inputFromListmodeFile;
		boost::atomic<bool> connected;
	
		bool connect(std::list<Mcpd8::Parameters> &_devlist, boost::asio::io_service &io_service)
		{
			int idfirst = DeviceParameter::minid;
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
				
				
				if (mp.datagenerator == Mesytec::DataGenerator::Charm || mp.datagenerator == Mesytec::DataGenerator::CharmSimulator) {
					mp.charm_cmd_endpoint = udp::endpoint(boost::asio::ip::address::from_string(p.mcpd_ip), p.charm_port);
				}
				if (!skip) {
					deviceparam.insert(std::pair<unsigned short, Mesytec::DeviceParameter>(id, mp));
					
				}
				DeviceParameter::maxid = id;
				id++;

			}
			switch (eventdataformat) {
			case Mcpd8::EventDataFormat::Mpsd8:
				data.widthY = (unsigned short) Mesy::Mpsd8Event::sizeY;
				data.widthX = (unsigned short) (Mesy::Mpsd8Event::sizeMODID * Mesy::Mpsd8Event::sizeSLOTS*deviceparam.size());
				break;
			case Mcpd8::EventDataFormat::Mdll:
				data.widthX = (unsigned short) (Mesy::MdllEvent::sizeX * deviceparam.size());
				data.widthY = (unsigned short)Mesy::MdllEvent::sizeY;
				break;
			case Mcpd8::EventDataFormat::Charm:
				data.widthX = (unsigned short) (Mesy::MdllEvent::sizeX * deviceparam.size());
				data.widthY = (unsigned short) Mesy::MdllEvent::sizeY;
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
		
			
			for (int i = DeviceParameter::minid; i <= DeviceParameter::maxid; i++) {
				Mesytec::DeviceParameter mp=deviceparam.at(i);
				Send(mp, Mcpd8::Cmd::SETID, i); // set ids for Mcpd8 devices
				Send(mp, Mcpd8::Internal_Cmd::GETVER); // 
				Send(mp, Mcpd8::Cmd::GETPARAMETERS);
				Send(mp, Mcpd8::Internal_Cmd::READID);
				if (mp.datagenerator == Mesytec::DataGenerator::Mcpd8 || mp.datagenerator == Mesytec::DataGenerator::NucleoSimulator) {
				//	data.widthX = 0;
					for (int i = 0; i < Mesy::Mpsd8Event::sizeSLOTS; i++) {

						if (mp.module_id[i] == Mesy::ModuleId::MPSD8P) {
							Send(mp, Mcpd8::Cmd::GETMPSD8PLUSPARAMETERS, i);
							//data.widthX += 8;
						}
						if (mp.firmware_major > 10) {}
						if (mp.module_id[i] == Mesy::ModuleId::MPSD8 || mp.module_id[i] == Mesy::ModuleId::MPSD8OLD) {
							Send(mp, Mcpd8::Internal_Cmd::READPERIREG, i);
							//data.widthX += 8;
						}
						

					}
				}
			}
			// now calc xsize and ysize


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
				char cmdbytes[] = { '\x06','\x00','\x06','\x00','\x04','\x00','\x01','\x00','\x00','\x8a','\x00','\x00' };
				if (param == 1) cmdbytes[10] = '\x01';
				size_t bytessent = mp.socket->send_to(boost::asio::buffer(cmdbytes), mp.charm_cmd_endpoint);

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
			response = Send(mp, cmdpacket);
			simulatordatarate = param;
			break;
			case Mcpd8::Internal_Cmd::GETVER:
				response = Send(mp, cmdpacket);
				{
					boost::mutex::scoped_lock lock(coutGuard);
					std::cout << "CPU " << response.data[0] << "." << response.data[1] << ", FPGA " << (response.data[2] & 0xff) << "." << ((response.data[2] >> 8) & 0xff) << std::endl;
				}
				mp.firmware_major = response.data[0];
				mp.firmware_minor = response.data[1];
				break;


			case Mcpd8::Internal_Cmd::READID: {
				response = Send(mp, cmdpacket);
				int n_modules = response.Length - response.headerLength;
				if (n_modules < 0 || n_modules >= 20) {
					boost::mutex::scoped_lock lock(coutGuard);
					std::cout << "ERROR:" << response << " n_modules range [0..." << Mesy::Mpsd8Event::sizeMODID << "]" << std::endl;
					break;
				}
				for (int i = 0; i < n_modules; i++) {
					auto mid=magic_enum::enum_cast<Mesy::ModuleId>(response.data[i]);
					if(mid.has_value())	mp.module_id[i]=mid.value();
				}

				break;
			}

			case Mcpd8::Internal_Cmd::READPERIREG:{
				assert(param >= 0 && param < Mesy::Mpsd8Event::sizeMODID);
				cmdpacket.data[0] = param;

				for (int i = 0; i <= 2; i++) {
					unsigned short regnum = i;
					// 0 =>tx capabilities
					// 1 => tx cap settings
					// 2 => firmware
					cmdpacket.data[1] = regnum;
					cmdpacket.Length = Mcpd8::CmdPacket::defaultLength + 2;
					response = Send(mp, cmdpacket);
					unsigned short slot = response.data[0];
					unsigned short capbitmap = response.data[2];
					unsigned short regword = response.data[1];
					switch (regword) {
					case 0:
						mp.module[slot].tx_caps = response.data[2];
						break;
					case 1:
					{
						auto txcapsetting = magic_enum::enum_cast<Mcpd8::TX_CAP>(response.data[2]);
						if (txcapsetting.has_value()) mp.module[slot].tx_cap_setting = txcapsetting.value();

					}
					break;
					case 2:
						mp.module[slot].firmware = response.data[2];
						break;
					}
				}
				break;
			}
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
				mp.devid= response.data[0];
				break;
			case Mcpd8::Cmd::SETRUNID:
				cmdpacket.data[0] = param;
				cmdpacket.Length= Mcpd8::CmdPacket::defaultLength + 1;
				response=Send(mp,cmdpacket);
				mp.runid = response.data[0];
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
						mp.module_id[cmdpacket.data[0]] = Mesy::ModuleId::MPSD8;
					
					}

					std::cout << mp.module_id[cmdpacket.data[0]]<< std::endl;
					 
				}
				

				//10 11 
				break;
			}
		}
		void SendAll(Mcpd8::Cmd cmd) {
			if (internalerror != Mesytec::my_errorcode::OK) return;
			{
				boost::mutex::scoped_lock lock(coutGuard);
				using namespace magic_enum::ostream_operators;
				std::cout << "SendAll(" << cmd << ")" << std::endl;
			}

			for (std::pair<unsigned short, Mesytec::DeviceParameter> pair : deviceparam) {
				Send(pair.second, cmd);
			}
			
		}
		
	private:
		bool firstcallsendwaitresponse;
		Mcpd8::CmdPacket& Send(Mesytec::DeviceParameter& mp,Mcpd8::CmdPacket& cmdpacket,bool waitresponse=true) {
			if (internalerror != Mesytec::my_errorcode::OK) return cmdpacket;
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
			
			
			cmdpacket.Number = Mcpd8::sendcounter++;
			Mcpd8::CmdPacket sentpacket = cmdpacket;
			static Mcpd8::CmdPacket response;
			
			//static is needed on Linux 
			boost::function<void()> send = [this,&mp, &cmdpacket]() {
				Mcpd8::CmdPacket::Send(mp.socket, cmdpacket, mp.mcpd_endpoint);
				pio_service->run_one();

			};

			if (waitresponse)	worker_threads.create_thread(boost::bind(send)); // so use thread to send and wait here
			else Mcpd8::CmdPacket::Send(mp.socket, cmdpacket, mp.mcpd_endpoint); 
			
			
			if (!waitresponse) {
				boost::mutex::scoped_lock lock(coutGuard);
				std::cout << "\rSENT:" << cmdpacket << " =>RESPONSE UNKNOWN (not checked)" << std::endl;
				return response;
			}
			int timeout_ms = 2200;
			if (firstcallsendwaitresponse) {
				timeout_ms = 3200;
				firstcallsendwaitresponse = false;
				
			}
			
			for (int l = 0; l < timeout_ms / 50; l++) {
				boost::this_thread::sleep_for(boost::chrono::milliseconds(50));
				WriteLock r_lock(cmdLock);
				int s = cmd_recv_queue.size();
				for (int i = 0; i < cmd_recv_queue.size(); i++) {
					response = cmd_recv_queue[i];
					if (response.Number == sentpacket.Number) {
						if (response.cmd == sentpacket.cmd || (response.cmd == (sentpacket.cmd | 0x8000))) {
							if (response.cmd & 0x8000) {
								boost::mutex::scoped_lock lock(coutGuard);
								std::cout << "\rSENT:" << cmdpacket << " =>WARNING:" << "Send(" << ss_cmd.str() << ") NOT UNDERSTOOD! " << std::endl;
							}
							else {
								boost::mutex::scoped_lock lock(coutGuard);
								std::cout << "\rSENT:" << cmdpacket << " =>RESPONSE OK(" << response.Length - response.headerLength << " items)" << std::endl;
							}
							return response;
						}
					}
				}
				
				
			}
		
			{
				boost::mutex::scoped_lock lock(coutGuard);
				std::cout << "\rSENT:" << cmdpacket << " =>ERROR:" << "Send(" << ss_cmd.str() << ") timeout(" << timeout_ms << " ms)" << std::endl;
				internalerror = Mesytec::my_errorcode::SEND_CMD_TIMEOUT;
				throw my_error() << my_info(internalerror);
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
			if ((id > DeviceParameter::maxid || id< DeviceParameter::minid) && dp.Type!= Zweistein::reverse_u16(Mesy::BufferType::COMMAND)) {
				boost::mutex::scoped_lock lock(coutGuard);
				std::cout << "CRITICAL: Packet has wrong id:" << id << " (minid allowed:" << DeviceParameter::minid << "," << "(maxid allowed:" << DeviceParameter::maxid << ")" << std::endl;
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
					if (cmd_target >= DeviceParameter::minid && cmd_target <= DeviceParameter::maxid) {
						Mcpd8::CmdPacket p = dp;
						{
							WriteLock w_lock(cmdLock);
							cmd_recv_queue.push_back(p);
						}
						{
						//	boost::mutex::scoped_lock lock(coutGuard);
						//	std::cout << "handle_receive: " << p;
						}
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
		
		
		unsigned long lastmissed_count;
		boost::chrono::system_clock::time_point lastmissed_time;
		public:
		void analyzebuffer(Mcpd8::DataPacket &datapacket)
		{
			unsigned short id = Mcpd8::DataPacket::getId(datapacket.deviceStatusdeviceId);
			unsigned short offset = (id - DeviceParameter::minid);
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
					Zweistein::Event Ev = Zweistein::Event(&datapacket.events[i], headertime, offset * Mesy::Mpsd8Event::sizeSLOTS * Mesy::Mpsd8Event::sizeMODID, params.module_id);
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
					Zweistein::Event Ev = Zweistein::Event(Mesy::MdllEvent::fromMpsd8(&datapacket.events[i]), headertime, offset * Mesy::MdllEvent::sizeX, params.module_id);
					PushNeutronEventOnly_HandleOther(Ev);
				}
			
				break;


			}

			
		}
		private:
		unsigned short cmd_target;
		udp::endpoint local_endpoint;
		
		boost::array< unsigned char, 64> charm_buf;
		boost::circular_buffer<Mcpd8::CmdPacket> cmd_recv_queue;
		Lock cmdLock;

		boost::array< Mcpd8::DataPacket, 1> recv_buf;
		
	};
	std::map<unsigned short, Mesytec::DeviceParameter> MesytecSystem::deviceparam = std::map<unsigned short, Mesytec::DeviceParameter>();
	
	
}




