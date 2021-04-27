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
		CharmSystem() :icharmid(0) { systype = Zweistein::XYDetector::Systemtype::Charm; };



		bool singleModuleXYSize(Zweistein::Format::EventData eventdataformat, unsigned short& x, unsigned short& y) {

			switch (eventdataformat) {
			case Zweistein::Format::EventData::Mdll:
				x = (unsigned short)Charm_sizeX;
				y = (unsigned short)Charm_sizeY;
				return true;
			}
			return false;
		}

		bool listmode_connect(std::list<Mcpd8::Parameters>& _devlist, boost::asio::io_service& io_service)
		{
			bool rv = MesytecSystem::listmode_connect(_devlist, io_service);
			for (int n = 1; n <= deviceparam[0].n_charm_units; n++) {
				auto tmp = deviceparam[0];
				tmp.offset = (n - 1) * Charm_sizeX;
				deviceparam.insert(std::pair<const unsigned char, Mesytec::DeviceParameter>(n, tmp));
			}
			return rv;
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

		void charm_analyzebuffer(Mcpd8::DataPacket& datapacket);


	};

}