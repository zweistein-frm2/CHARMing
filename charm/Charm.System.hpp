/*                          _              _                _
	___  __ __ __  ___     (_)     ___    | |_     ___     (_)    _ _
   |_ /  \ V  V / / -_)    | |    (_-<    |  _|   / -_)    | |   | ' \
  _/__|   \_/\_/  \___|   _|_|_   /__/_   _\__|   \___|   _|_|_  |_||_|
_|"""""|_|"""""|_|"""""|_|"""""|_|"""""|_|"""""|_|"""""|_|"""""|_|"""""|
"`-0-0-'"`-0-0-'"`-0-0-'"`-0-0-'"`-0-0-'"`-0-0-'"`-0-0-'"`-0-0-'"`-0-0-'

   Copyright (C) 2019 - 2020 by Andreas Langhoff
										 <andreas.langhoff@frm2.tum.de>
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation;*/

#pragma once
#include "Mesytec.Mcpd8.hpp"
namespace Charm {
	class CharmSystem : public Mesytec::MesytecSystem {
	public:
		int icharmid;
		CharmSystem():icharmid(0) {};
		void start_receive_charm() {

			deviceparam.at(icharmid).socket->async_receive_from(boost::asio::buffer(charm_buf), deviceparam.at(icharmid).charm_cmd_endpoint, boost::bind(&CharmSystem::handle__charm_receive, this,
				boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
		}

		void handle__charm_receive(const boost::system::error_code& error,
			std::size_t bytes_transferred) {
			LOG_DEBUG << "handle__charm_receive(" << error.message() << " ,bytes_transferred=" << bytes_transferred << std::endl;
			for (int i = 0; i < bytes_transferred; i++) {
				LOG_DEBUG << std::hex << charm_buf[i] << " ";
				if (i % 8 == 0) LOG_DEBUG << std::endl;
			}
			LOG_DEBUG << std::dec << std::endl;
			start_receive_charm();
		}
		bool singleModuleXYSize(Zweistein::Format::EventData eventdataformat, unsigned short& x, unsigned short& y) {
			bool rv = Mesytec::MesytecSystem::singleModuleXYSize(eventdataformat,x,y);
			if(rv) return rv;

			switch (eventdataformat) {
			  case Zweistein::Format::EventData::Charm:
				x = (unsigned short)Charm_sizeX;
				y = (unsigned short)Charm_sizeY;
				return true;
			}
			return false;
		}

		bool connect(std::list<Mcpd8::Parameters>& _devlist, boost::asio::io_service& io_service) {
			bool rv = Mesytec::MesytecSystem::connect(_devlist, io_service);

				for (const auto& [key, value] : deviceparam) {
					if (value.datagenerator == Zweistein::DataGenerator::Charm || value.datagenerator == Zweistein::DataGenerator::CharmSimulator) {
						icharmid = key;
					}
				}
     			return rv;
		}

		void Send(std::pair<const unsigned char, Mesytec::DeviceParameter>& kvp, Mcpd8::Internal_Cmd cmd, unsigned long param = 0) {
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
					LOG_WARNING << ss1.str() << std::endl;
					break;
				}
				short* sp = (short*)&cmdbytes[10];
				*sp = data;
				size_t bytessent = kvp.second.socket->send_to(boost::asio::buffer(cmdbytes), kvp.second.charm_cmd_endpoint);
				simulatordatarate = param;
			}
			break;
			default:
				Mesytec::MesytecSystem::Send(kvp, cmd, param);
			}
		}

		inline void analyzebuffer(Mcpd8::DataPacket& datapacket) {

			unsigned char id = Mcpd8::DataPacket::getId(datapacket.deviceStatusdeviceId);
			auto& params = deviceparam[id];
			unsigned short numevents = datapacket.numEvents();
			boost::chrono::nanoseconds headertime = Mcpd8::DataPacket::timeStamp(datapacket.time);
			unsigned short neutrons = 0;
			//boost::chrono::nanoseconds elapsed = headertime - boost::chrono::nanoseconds( starttime);
			// in mesytec time

			//if (numevents == 0) LOG_WARNING << "0 EVENTS, TimeStamp:" << headertime << std::endl;

			switch (datapacket.GetBuffertype()) {
			  case Mesy::BufferType::MDLL:
				if (datapacket.Number != (unsigned short)(params.lastbufnum + 1)) {
					boost::chrono::duration<double> sec = boost::chrono::system_clock::now() - params.lastmissed_time;
					unsigned short missed = (unsigned short)(datapacket.Number - (unsigned short)(params.lastbufnum + 1));
					params.lastmissed_count += missed;
					if (sec.count() > 1) {
						boost::chrono::system_clock::time_point tps(getStart());
						if (boost::chrono::system_clock::now() - tps >= boost::chrono::seconds(3)) {
							LOG_ERROR << params.mcpd_endpoint << ": MISSED " << params.lastmissed_count << " BUFFER(s)" << std::endl;
						}
						params.lastmissed_time = boost::chrono::system_clock::now();
						params.lastmissed_count = 0;
					}

				}
				for (int c = 0; c < COUNTER_MONITOR_COUNT; c++) {
					CounterMonitor[c] += (unsigned long long) datapacket.param[c][0] + (unsigned long long) datapacket.param[c][1] << 16 + (unsigned long long)datapacket.param[c][2] << 32;
				}
				params.lastbufnum = datapacket.Number;
				for (int i = 0; i < numevents; i++) {
					Zweistein::Event Ev = Zweistein::Event(Charm::CharmEvent::fromMpsd8(&datapacket.events[i]), headertime, params.offset, params.module_id);
					PushNeutronEventOnly_HandleOther(Ev);
				}
				break;
			}
		}
		boost::array< unsigned char, 64> charm_buf;

	};

}