/***************************************************************************
 *   Copyright (C) 2019 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/
#pragma once
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include "Mcpd8.Parameters.hpp"
#include "Mesytec.enums.Generator.hpp"
#include "Zweistein.GetLocalInterfaces.hpp"

namespace Mesytec {
	namespace Config { 
		std::string oursystem = "MsmtSystem";
		std::string mesytecdevice = "MesytecDevice";
		std::string charmdevice = "CharmDevice";
		std::string punkt = ".";
		const int MaxDevices = 2;
		void get(boost::property_tree::ptree& root, std::list<Mcpd8::Parameters>& _devlist, boost::asio::io_service& io_service) {
			
			for (int n = 0; n < MaxDevices; n++) {
				Mcpd8::Parameters p1;
				std::stringstream ss;
				ss << oursystem << punkt << mesytecdevice << n << punkt;
				std::string s2 = ss.str();

				if (n == 0) p1.mcpd_ip = root.get<std::string>(s2 + "mcpd_ip", Mcpd8::Parameters::defaultIpAddress);
				else p1.mcpd_ip = root.get<std::string>(s2 + "mcpd_ip");
				root.put<std::string>(s2 + "mcpd_ip", p1.mcpd_ip);

				if (n == 0) p1.mcpd_port = root.get<unsigned short>(s2 + "mcpd_port", Mcpd8::Parameters::defaultUdpPort);
				else p1.mcpd_port = root.get<unsigned short>(s2 + "mcpd_port");
				root.put<unsigned short>(s2 + "mcpd_port", p1.mcpd_port);

				if (n == 0) p1.mcpd_id = root.get<unsigned char>(s2 + "mcpd_id", Mcpd8::Parameters::defaultmcpd_id);
				else p1.mcpd_id = root.get<unsigned char>(s2 + "mcpd_id");
				root.put<unsigned char>(s2 + "mcpd_id", p1.mcpd_id);

				if (n == 0) p1.data_host = root.get<std::string>(s2 + "data_host", "0.0.0.0");
				else p1.data_host = root.get<std::string>(s2 + "data_host");
				root.put<std::string>(s2 + "data_host", p1.data_host);

				std::stringstream ss2;
				ss2 << oursystem << punkt << charmdevice << n << punkt;
				std::string s3 = ss2.str();
				if (n == 0) p1.charm_port = root.get<unsigned short>(s3 + "charm_port", Charm::Parameters::defaultUdpPort);
				else p1.charm_port = root.get<unsigned short>(s3 + "charm_port", Charm::Parameters::defaultUdpPort);
				root.put<unsigned short>(s3 + "charm_port", p1.charm_port);
				std::string savednetworkcard;
				if (n == 0) {
					savednetworkcard = root.get<std::string>(s2 + "networkcard", "0.0.0.0");
					root.put<std::string>(s2 + "networkcard", savednetworkcard);
					p1.networkcard = Zweistein::askforInterfaceIfUnknown(savednetworkcard, io_service);
				}
				else p1.networkcard = root.get<std::string>(s2 + "networkcard");
				if (savednetworkcard != p1.networkcard) std::cout << s2 + "networkcard=" << p1.networkcard << " on puropse not saved in config" << std::endl;
							
				if (n == 0) {
					auto a3 = magic_enum::enum_name(Mcpd8::EventDataFormat::Mpsd8);
					std::string m3 = std::string(a3.data(), a3.size());
					std::string enum_tmp = root.get<std::string>(s2 + "eventdataformat",m3);
					auto a = magic_enum::enum_cast<Mcpd8::EventDataFormat>(enum_tmp);
					p1.eventdataformat = a.value();
				}
				else {
					std::string enum_tmp = root.get<std::string>(s2 + "eventdataformat");
					auto a = magic_enum::enum_cast<Mcpd8::EventDataFormat>(enum_tmp);
					p1.eventdataformat = a.value();
				}
				auto a4 = magic_enum::enum_name(p1.eventdataformat);
				std::string m4 = std::string(a4.data(), a4.size());
				root.put<std::string>(s2 + "eventdataformat", m4);

				if (n == 0) {
					auto a5 = magic_enum::enum_name(Mesytec::DataGenerator::NucleoSimulator);
					std::string m5 = std::string(a5.data(), a5.size());
					std::string enum_tmp = root.get<std::string>(s2 + "datagenerator", m5);
					auto a = magic_enum::enum_cast<Mesytec::DataGenerator>(enum_tmp);
					p1.datagenerator = a.value();
				}
				else {
					std::string enum_tmp = root.get<std::string>(s2 + "datagenerator");
					auto a = magic_enum::enum_cast<Mesytec::DataGenerator>(enum_tmp);
					p1.datagenerator = a.value();
				}
				auto a6 = magic_enum::enum_name(p1.datagenerator);
				std::string m6 = std::string(a6.data(), a6.size());
				root.put<std::string>(s2 + "datagenerator", m6);

				

				_devlist.push_back(p1);

			}
		}
	}

}