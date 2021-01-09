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



		bool singleModuleXYSize(Zweistein::Format::EventData eventdataformat, unsigned short& x, unsigned short& y) {

			switch (eventdataformat) {
			case Zweistein::Format::EventData::Mdll:
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

				for (int n = 1; n <= deviceparam[0].n_charm_units; n++) {
					auto tmp = deviceparam[0];
					tmp.offset = (n - 1) * Charm_sizeX;
					deviceparam.insert(std::pair<const unsigned char, Mesytec::DeviceParameter>(n, tmp));
				}



     			return rv;
		}


		inline void analyzebuffer(Mcpd8::DataPacket& datapacket) {

			unsigned char id = Mcpd8::DataPacket::getId(datapacket.deviceStatusdeviceId);
			if (id == 0) {
				LOG_ERROR << "id == 0 " << std::endl;

			}
			// id is now offset , id = 1  offsetx 0, id =2 => offsetx = 1024
			auto& params = deviceparam[1]; // always 0
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
					CounterMonitor[c] += (unsigned long long) datapacket.param[c][0] + (((unsigned long long) datapacket.param[c][1]) << 16) + (((unsigned long long) datapacket.param[c][2]) << 32);
				}
				params.lastbufnum = datapacket.Number;
				for (int i = 0; i < numevents; i++) {
					Zweistein::Event Ev = Zweistein::Event(Charm::CharmEvent::fromMpsd8(&datapacket.events[i]), headertime, deviceparam[id].offset, params.module_id);
					Ev.Amplitude = 16;  // as might be resized from 4x4 to 1 pixel then event count is at least 1
					PushNeutronEventOnly_HandleOther(Ev);
				}
				break;
			}
		}
	};

}