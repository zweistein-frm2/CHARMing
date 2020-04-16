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
#include "Zweistein.Locks.hpp"
#include "Zweistein.Logger.hpp"


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
		static std::map<const unsigned char, Mesytec::DeviceParameter> deviceparam;
		MesytecSystem():recv_buf(), cmd_recv_queue(5), internalerror(cmd_errorcode::OK), currentrunid(0),
			lastpacketqueuefull_missedcount(0), lastlistmodequeuefull_missedcount(0),
			lasteventqueuefull_missedcount(0), inputFromListmodeFile(false),
			eventdataformat(Mcpd8::EventDataFormat::Undefined),icharmid(0){
				
		}
		~MesytecSystem(){
			for (const auto& [key, value] : deviceparam) {
				if (value.bNewSocket && value.socket) value.socket->close();
			}
			if (pstrand) delete pstrand;
		}

		void initatomicortime_point() {
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
		boost::atomic<long> simulatordatarate;
		boost::atomic<bool> daq_running;
		boost::atomic<boost::chrono::system_clock::time_point> started;
		boost::atomic<boost::chrono::system_clock::time_point> stopped;
		boost::atomic<bool> connected;

		boost::chrono::system_clock::time_point lasteventqueuefull;
		boost::chrono::system_clock::time_point lastpacketqueuefull;
		boost::chrono::system_clock::time_point lastlistmodequeuefull;
		boost::atomic<bool> write2disk;
		
		unsigned short currentrunid;
		int icharmid;
		cmd_errorcode internalerror;
		boost::asio::io_service *pio_service;
		Mcpd8::EventDataFormat eventdataformat;
		size_t lasteventqueuefull_missedcount;
		size_t lastpacketqueuefull_missedcount;
		size_t lastlistmodequeuefull_missedcount;
		
		
		Mcpd8::Data data;
		bool inputFromListmodeFile;
		
		boost::asio::io_service::strand *pstrand=nullptr;
		bool connect(std::list<Mcpd8::Parameters> &_devlist, boost::asio::io_service &io_service)
		{
			pio_service = &io_service;
			data.widthX = 0;
			data.widthY = 0;
			if (pstrand) delete pstrand;
			pstrand=new boost::asio::io_service::strand(io_service);
			boost::system::error_code ec;
			for(Mcpd8::Parameters& p:_devlist) {
				Mesytec::DeviceParameter mp;

				local_endpoint = udp::endpoint(boost::asio::ip::address::from_string(p.networkcard), p.mcpd_port);
				mp.mcpd_endpoint = udp::endpoint(boost::asio::ip::address::from_string(p.mcpd_ip),p.mcpd_port);
				{
					boost::mutex::scoped_lock lock(coutGuard);
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
						boost::mutex::scoped_lock lock(coutGuard);
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
					boost::mutex::scoped_lock lock(coutGuard);
					//it->second.socket->cancel();
					//start_receive(it->second, onid.second); // now we expect new id!
					//deviceparam.insert(std::pair<const unsigned char, Mesytec::DeviceParameter>(onid.second, it->second));
					//deviceparam.erase(it);
					LOG_INFO << (it->second.mcpd_endpoint.address()).to_string() << " SETID: NEW ID=" << (int)onid.second << " (OLD ID=" <<(int)onid.first << ")"<< std::endl;

				}
				else {
					boost::mutex::scoped_lock lock(coutGuard);
					LOG_ERROR << __FILE__ <<":"<<__LINE__ << std::endl;
				}
				
			}

			for (  auto &kvp: deviceparam) {
				Send(kvp, Mcpd8::Internal_Cmd::GETVER); //
				Send(kvp, Mcpd8::Internal_Cmd::READID);
				Send(kvp, Mcpd8::Cmd::GETPARAMETERS);
				
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
			int ndevices = deviceparam.size();
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
				boost::mutex::scoped_lock lock(coutGuard);
				LOG_INFO << "MESYTECHSYSTEM CONNECTED: +" << ms << std::endl;
			}
			return connected;
		}
		
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
				//	TODO	LOG_ERROR << mp.datagenerator << " not supported:" << cmd_2 << std::endl;
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
			//	TODO		LOG_ERROR << mp.datagenerator << " not supported:" << cmd_2 << std::endl;
						rv = false;
					}
				}

			}
			return rv;
		}
		void Send(std::pair<const unsigned char, Mesytec::DeviceParameter> &kvp,Mcpd8::Internal_Cmd cmd, unsigned long param = 0) {
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
				boost::mutex::scoped_lock lock(coutGuard);
				// SETNUCLEORATEEVENTSPERSECOND has 0x8000 bit always set, so it is falsely as CMD not understood 
				if (int const* mi = boost::get_error_info<Mesytec::my_info>(x)) {
					auto  my_code = magic_enum::enum_cast<Mesytec::cmd_errorcode>(*mi);
					if (my_code != Mesytec::cmd_errorcode::CMD_NOT_UNDERSTOOD) throw;
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
					boost::mutex::scoped_lock lock(coutGuard);
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

		void Send(std::pair<const unsigned char, Mesytec::DeviceParameter>& kvp,Mcpd8::Cmd cmd,unsigned long lparam=0) {

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
						boost::mutex::scoped_lock lock(coutGuard);
						if (int const* mi = boost::get_error_info<Mesytec::my_info>(x)) {
							auto  my_code = magic_enum::enum_cast<Mesytec::cmd_errorcode>(*mi);
							if (my_code == Mesytec::cmd_errorcode::CMD_NOT_UNDERSTOOD) continue;
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
					boost::mutex::scoped_lock lock(coutGuard);
					using namespace magic_enum::ostream_operators;
					LOG_ERROR<< (kvp.second.mcpd_endpoint.address()).to_string()<<" " << cmd << " requested=0x" <<std::hex << param << " returned=0x" << response.data[0] <<std::dec << std::endl;
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
					LOG_ERROR << cmd << "RESPONSE OUTSIDE RANGE: data[0]=0x" <<std::hex<< response.data[0] <<std::dec<< std::endl;

				}
				else {
					boost::mutex::scoped_lock lock(coutGuard);
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
		void SendAll(Mcpd8::Cmd cmd) {
			//if (internalerror != Mesytec::cmd_errorcode::OK) return;
			{
				boost::mutex::scoped_lock lock(coutGuard);
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
		
	private:
		boost::atomic<bool> wait_response;
		Mcpd8::CmdPacket& Send(std::pair<const unsigned char, Mesytec::DeviceParameter>& kvp,Mcpd8::CmdPacket& cmdpacket,bool waitresponse=true) {
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
				boost::mutex::scoped_lock lock(coutGuard);
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
				boost::mutex::scoped_lock lock(coutGuard);
					ss_log <<  " SEND(" << ss_cmd.str() << ") to " << kvp.second.mcpd_endpoint << ":";

				//LOG_DEBUG<< cmdpacket << std::endl;

			}
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
								boost::mutex::scoped_lock lock(coutGuard);
								boost::chrono::milliseconds ms = boost::chrono::duration_cast<boost::chrono::milliseconds> (boost::chrono::system_clock::now() - start);
								ss_log <<  " NOT UNDERSTOOD! +"<<ms ;
								ss_log << std::endl<< "RESPONSE: " << response << std::endl;
								LOG_WARNING << ss_log.str() << std::endl;
								internalerror = Mesytec::cmd_errorcode::CMD_NOT_UNDERSTOOD;
								throw cmd_error() << my_info(internalerror);
							}
							else {
								boost::mutex::scoped_lock lock(coutGuard);
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
								Mesytec::stopwriting = true;
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

		void start_receive_charm(){
			
			deviceparam.at(icharmid).socket->async_receive_from(boost::asio::buffer(charm_buf), deviceparam.at(icharmid).charm_cmd_endpoint, boost::bind(&MesytecSystem::handle__charm_receive, this,
				boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
		}

		void handle__charm_receive(const boost::system::error_code& error,
			std::size_t bytes_transferred) {

			if (true) {
				boost::mutex::scoped_lock lock(coutGuard);
				LOG_DEBUG << "handle__charm_receive(" << error.message() << " ,bytes_transferred=" << bytes_transferred << std::endl;
				for (int i = 0; i < bytes_transferred; i++) {
					LOG_DEBUG << std::hex << charm_buf[i] << " ";
					if (i % 8 == 0) LOG_DEBUG << std::endl;
				}
				LOG_DEBUG <<std::dec<< std::endl;
			}
			start_receive_charm();
		}
		void start_receive(const Mesytec::DeviceParameter &mp,unsigned char devid) {
			
			mp.socket->async_receive(boost::asio::buffer(recv_buf), pstrand->wrap(boost::bind(&MesytecSystem::handle_receive, this,
				boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, devid)));
			
			//mp.socket->async_receive_from(boost::asio::buffer(recv_buf), mp.mcpd_endpoint, boost::bind(&MesytecSystem::handle_receive, this,
			//	boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred,devid));
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
				LOG_ERROR << boost::diagnostic_information(e) << std::endl;
			}
			{
				boost::mutex::scoped_lock lock(coutGuard);
				LOG_DEBUG << "watch_incoming_packets() exiting..." << std::endl;
			}
		}
		

		void handle_receive(const boost::system::error_code& error,
			std::size_t bytes_transferred,unsigned char devid) {
			Mcpd8::DataPacket& dp = recv_buf[0];
			data.last_deviceStatusdeviceId = dp.deviceStatusdeviceId;
			if (error) {
				boost::mutex::scoped_lock lock(coutGuard);
				LOG_ERROR  << " handle_receive(" << error.message() << " ,bytes_transferred=" << bytes_transferred << ", DataPacket=" << dp << std::endl;
				LOG_ERROR << "PACKET BUFNUM: " << (unsigned short)(dp.Number) << std::endl;
				//memset(&recv_buf[0], 0, sizeof(Mcpd8::DataPacket));
			}
			
			auto &mp = deviceparam.at(devid);
	
			if (dp.Type == Mesy::BufferType::COMMAND) {
			
				Mcpd8::CmdPacket p = dp;
				{
					Zweistein::WriteLock w_lock(cmdLock);
					cmd_recv_queue.push_front(p);
				}
				
			}
			else {
				if (daq_running) {
					// Mesy::BufferType::DATA
					bool ok = data.packetqueue.push(dp);

					if (ok)	data.packetqueuecount++;
					else {
						boost::chrono::duration<double> sec = boost::chrono::system_clock::now() - lastpacketqueuefull;
						lastpacketqueuefull_missedcount += 1;
						if (sec.count() > 1) {
							boost::mutex::scoped_lock lock(coutGuard);
							LOG_ERROR << ": PACKETQUEUE FULL , SKIPPED " << lastpacketqueuefull_missedcount << " Event(s)" << std::endl;

							//			LOG_ERROR << std::endl << boost::chrono::time_fmt(boost::chrono::timezone::local) << boost::chrono::system_clock::now() << ": PACKETQUEUE FULL , SKIPPED " << lastpacketqueuefull_missedcount << " Event(s)" << std::endl;
							lastpacketqueuefull = boost::chrono::system_clock::now();
							lastpacketqueuefull_missedcount = 0;
						}
					}
					if (write2disk) {
						bool ok2 = data.listmodequeue.push(dp);
						if (ok2) data.listmodepacketcount++;
						else {
							boost::chrono::duration<double> sec = boost::chrono::system_clock::now() - lastlistmodequeuefull;
							lastlistmodequeuefull_missedcount += 1;
							if (sec.count() > 1) {
								boost::mutex::scoped_lock lock(coutGuard);
								//LOG_ERROR << std::endl << boost::chrono::time_fmt(boost::chrono::timezone::local) << boost::chrono::system_clock::now() << ": LISTMODEWRITEQUEUE FULL , SKIPPED " << lastlistmodequeuefull_missedcount << " Event(s)" << std::endl;
								LOG_ERROR << ": LISTMODEWRITEQUEUE FULL , SKIPPED " << lastlistmodequeuefull_missedcount << " Event(s)" << std::endl;

								lastlistmodequeuefull = boost::chrono::system_clock::now();
								lastlistmodequeuefull_missedcount = 0;
							}
						}
					}
				}
			}
			start_receive(mp,devid);
		}


		void PushNeutronEventOnly_HandleOther(Zweistein::Event& Ev) {
			if (Ev.type == Mesy::EventType::NEUTRON) {
				bool ok=data.evntqueue.push(Ev);
				if(ok)	data.evntcount++;
				else {
					boost::chrono::duration<double> sec = boost::chrono::system_clock::now() - lasteventqueuefull;
					lasteventqueuefull_missedcount += 1;
					if (sec.count() > 1) {
						boost::mutex::scoped_lock lock(coutGuard);
						//LOG_ERROR << std::endl << boost::chrono::time_fmt(boost::chrono::timezone::local) << boost::chrono::system_clock::now() << ": EVENTQUEUE FULL , SKIPPED " << lasteventqueuefull_missedcount << " Event(s)" << std::endl;
						LOG_ERROR  << ": EVENTQUEUE FULL , SKIPPED " << lasteventqueuefull_missedcount << " Event(s)" << std::endl;

						lasteventqueuefull = boost::chrono::system_clock::now();
						lasteventqueuefull_missedcount = 0;
					}
				}
			}
			else {

			}
			
		}
		
		
		
		public:
		void analyzebuffer(Mcpd8::DataPacket &datapacket)
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
					boost::chrono::duration<double> sec = boost::chrono::system_clock::now() - params.lastmissed_time;
					unsigned short missed = (unsigned short)(datapacket.Number - (unsigned short)(params.lastbufnum + 1));
					params.lastmissed_count += missed;
					if (sec.count() > 1) {
						boost::chrono::system_clock::time_point tps(started);
						if(boost::chrono::system_clock::now()-tps >= boost::chrono::seconds(3)){
							LOG_ERROR << params.mcpd_endpoint << ": MISSED " << params.lastmissed_count << " BUFFER(s)" << std::endl;
						}
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
		private:
		udp::endpoint local_endpoint;
		
		boost::array< unsigned char, 64> charm_buf;
		boost::circular_buffer<Mcpd8::CmdPacket> cmd_recv_queue;
		Zweistein::Lock cmdLock;

		boost::array< Mcpd8::DataPacket, 1> recv_buf;
		std::map<const unsigned char, unsigned char> oldidnewid = std::map<const unsigned char, unsigned char>();
		
	};
	std::map<const unsigned char, Mesytec::DeviceParameter> MesytecSystem::deviceparam = std::map<const unsigned char, Mesytec::DeviceParameter>();
	
	
}




