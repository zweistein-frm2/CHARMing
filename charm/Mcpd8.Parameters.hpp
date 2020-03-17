/***************************************************************************
 *   Copyright (C) 2019 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/
#pragma once
#include <string>
#include "Mcpd8.enums.hpp"
#include "Mesytec.enums.Generator.hpp"

namespace Mcpd8 {

	struct Parameters {
		static const std::string defaultIpAddress;
		static const unsigned short defaultUdpPort;
		static const unsigned char defaultmcpd_id;
		EventDataFormat eventdataformat;
		std::string networkcard;
		std::string mcpd_ip;
		std::string data_host;
		unsigned short mcpd_port;
		unsigned short charm_port;
		unsigned char mcpd_id;
		Mesytec::DataGenerator datagenerator;
		void print(std::ostream& os) const {
			using namespace magic_enum::ostream_operators;
			os << "mcpd_ip:" << mcpd_ip << " networkcard:" << networkcard << " data_host:" << data_host << " mcpd_port:" << mcpd_port << " eventdataformat:" << eventdataformat << " datagenerator:" << " mcpd_id:" << mcpd_id << std::endl;
		}
	};
	const std::string Parameters::defaultIpAddress = "192.168.168.121";
	const unsigned short Parameters::defaultUdpPort = 54321;
	const unsigned char Parameters::defaultmcpd_id = 0;
}

namespace Charm {
	struct Parameters {
		static const unsigned short defaultUdpPort;
	};
	const unsigned short Parameters::defaultUdpPort = 54340;
}
