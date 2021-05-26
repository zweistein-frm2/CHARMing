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
#ifndef BOOST_BIND_GLOBAL_PLACEHOLDERS
#define BOOST_BIND_GLOBAL_PLACEHOLDERS
#endif
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/filesystem.hpp>
#include "Mcpd8.Parameters.hpp"
#include "Zweistein.enums.Generator.hpp"
#include "Zweistein.GetLocalInterfaces.hpp"
#include "Zweistein.HomePath.hpp"
#include "Zweistein.Logger.hpp"
#include "Zweistein.GetConfigDir.hpp"
namespace Mesytec {

	 namespace Config {

		extern boost::filesystem::path DATAHOME;
		extern boost::filesystem::path BINNINGFILE;
		extern boost::property_tree::ptree root;
		inline bool get(std::list<Mcpd8::Parameters>& _devlist, boost::filesystem::path inidirectory,bool checknetwork=true) {
			bool rv = true;

			try {
				std::string oursystem = "MsmtSystem";
				std::string mesytecdevice = "MesytecDevice";
				std::string charmdevice = "CharmDevice";
				std::string punkt = ".";
				std::string mcpd_ip = "mcpd_ip";
				std::string mcpd_port = "mcpd_port";
				std::string mcpd_id = "mcpd_id";
				std::string data_host = "data_host";
				std::string datahome = "DataHome";
				std::string binningfile = "BinningFile";
				std::string counteradc="CounterADC";
				std::string modulethresgains = "Threshold_and_Gains";

				std::string n_charm_units = "n_charm_units";
				const int maxModule = 8;
				const int maxCounter = 8;
				const int MaxDevices = 4;



				DATAHOME = root.get<std::string>(oursystem + punkt + datahome, Zweistein::GetHomePath().string());
				root.put<std::string>(oursystem + punkt + datahome, DATAHOME.string());

				BINNINGFILE = root.get<std::string>(oursystem + punkt + binningfile, inidirectory.empty()?"": (inidirectory /= "binning.json").string());
				root.put<std::string>(oursystem + punkt + binningfile, BINNINGFILE.string());

				for (int n = 0; n < MaxDevices; n++) {
					Mcpd8::Parameters p1;
					std::stringstream ss;
					ss << oursystem << punkt << mesytecdevice << n << punkt;
					std::string s2 = ss.str();

					if (n == 0) p1.mcpd_ip = root.get<std::string>(s2 + mcpd_ip, Mcpd8::Parameters::defaultIpAddress);
					else p1.mcpd_ip = root.get<std::string>(s2 + mcpd_ip);
					root.put<std::string>(s2 + mcpd_ip, p1.mcpd_ip);

					if (n == 0) p1.mcpd_port = root.get<unsigned short>(s2 + mcpd_port, Mcpd8::Parameters::defaultUdpPort);
					else p1.mcpd_port = root.get<unsigned short>(s2 + mcpd_port);
					root.put<unsigned short>(s2 + mcpd_port, p1.mcpd_port);

					if (n == 0) p1.mcpd_id = root.get<unsigned char>(s2 + mcpd_id, Mcpd8::Parameters::defaultmcpd_id);
					else p1.mcpd_id = root.get<unsigned char>(s2 + mcpd_id);
					root.put<unsigned char>(s2 + mcpd_id, p1.mcpd_id);

					if (n == 0) p1.data_host = root.get<std::string>(s2 + data_host, "0.0.0.0");
					else p1.data_host = root.get<std::string>(s2 + data_host);
					root.put<std::string>(s2 + data_host, p1.data_host);


					std::string savednetworkcard;
					if (n == 0) {
						savednetworkcard = root.get<std::string>(s2 + "networkcard", "192.168.168.100");
						root.put<std::string>(s2 + "networkcard", savednetworkcard);
						p1.networkcard = savednetworkcard;

					}
					else p1.networkcard = root.get<std::string>(s2 + "networkcard");

					if (checknetwork && !Zweistein::InterfaceExists(p1.networkcard)) {
						LOG_ERROR << s2 + "networkcard=" << p1.networkcard << " not found." << std::endl;
						rv = false;
						continue;
					}

					if (n == 0) {
						auto a3 = magic_enum::enum_name(Zweistein::Format::EventData::Mpsd8);
						std::string m3 = std::string(a3.data(), a3.size());
						std::string enum_tmp = root.get<std::string>(s2 + "eventdataformat", m3);
						auto a = magic_enum::enum_cast<Zweistein::Format::EventData>(enum_tmp);
						if (!a) {
							std::cout << "option not found : possible values are ";
							constexpr auto & names = magic_enum::enum_names<Zweistein::Format::EventData>();
							for (const auto& n : names) std::cout << " " << n;
							std::cout << std::endl;
							rv = false;
							continue;
						}
						else p1.eventdataformat = a.value();
					}
					else {
						std::string enum_tmp = root.get<std::string>(s2 + "eventdataformat");
						auto a = magic_enum::enum_cast<Zweistein::Format::EventData>(enum_tmp);
						if (!a) {
							std::cout << "option not found : possible values are ";
							constexpr auto& names = magic_enum::enum_names<Zweistein::Format::EventData>();
							for (const auto& n : names) std::cout << " " << n;
							std::cout << std::endl;
							rv = false;
							continue;
						}
						else p1.eventdataformat = a.value();
					}
					auto a4 = magic_enum::enum_name(p1.eventdataformat);
					std::string m4 = std::string(a4.data(), a4.size());
					root.put<std::string>(s2 + "eventdataformat", m4);

					if (n == 0) {
						auto a5 = magic_enum::enum_name(Zweistein::DataGenerator::NucleoSimulator);
						std::string m5 = std::string(a5.data(), a5.size());
						std::string enum_tmp = root.get<std::string>(s2 + "datagenerator", m5);
						auto a = magic_enum::enum_cast<Zweistein::DataGenerator>(enum_tmp);
						if (!a) {
							std::cout << "option not found : possible values are ";
							constexpr auto& names = magic_enum::enum_names<Zweistein::DataGenerator>();
							for (const auto& n : names) std::cout << " " << n;
							std::cout << std::endl;
							rv = false;
							continue;
						}
						else p1.datagenerator = a.value();
					}
					else {
						std::string enum_tmp = root.get<std::string>(s2 + "datagenerator");
						auto a = magic_enum::enum_cast<Zweistein::DataGenerator>(enum_tmp);
						if (!a) {
							std::cout << "option not found : possible values are ";
							constexpr auto& names = magic_enum::enum_names<Zweistein::DataGenerator>();
							for (const auto& n : names) std::cout << " " << n;
							std::cout << std::endl;
							rv = false;
							continue;
						}
						else p1.datagenerator = a.value();
					}
					auto a6 = magic_enum::enum_name(p1.datagenerator);
					std::string m6 = std::string(a6.data(), a6.size());
					root.put<std::string>(s2 + "datagenerator", m6);

					for (int c = 0; c < maxCounter; c++) {
						std::string countercell = s2+ counteradc + std::to_string(c);
						std::string r=root.get<std::string>(countercell, "");
						root.put<std::string>(countercell, r);
						p1.counterADC[c] = r;
					}

					for (int m = 0; m < maxModule; m++) {
						std::string currmodule = s2 + modulethresgains + std::to_string(m);
						std::string r = root.get<std::string>(currmodule, "");
						root.put<std::string>(currmodule, r);
						p1.moduleparam[m] = r;
					}



					std::stringstream ss2;
					ss2 << oursystem << punkt << charmdevice << n << punkt;
					std::string s3 = ss2.str();
					if (n == 0) p1.n_charm_units = root.get<unsigned short>(s3 + "n_charm_units", Charm::Parameters::default_n_charm_units);
					else p1.n_charm_units = root.get<unsigned short>(s3 + "n_charm_units", Charm::Parameters::default_n_charm_units);
					root.put<unsigned short>(s3 + "n_charm_units", p1.n_charm_units);

					bool bcanpushback = true;

					for (auto& d : _devlist) {
						if (d.networkcard == p1.networkcard && d.mcpd_ip == p1.mcpd_ip) {
							bcanpushback = false;
							LOG_ERROR << "CONFIGURATION ERROR:" << s2 + mcpd_ip + "(" << p1.mcpd_ip << ") already in use =>skipped" << std::endl;
							rv = false;

						}
						if (d.mcpd_id == p1.mcpd_id) {
							bcanpushback = false;
							LOG_ERROR << "CONFIGURATION ERROR:" << s2 + "mcpd_id(" << p1.mcpd_id << ") already in use =>skipped" << std::endl;
							rv = false;
						}
					}


					// for devices on same network interface ip addresses must be distinct
					// for all devices devid must be distinct
					if (bcanpushback) _devlist.push_back(p1);


				}
			}
			catch (boost::exception& ) {
				//LOG_DEBUG<< boost::diagnostic_information(e)<<std::endl;
			}
			return rv;
		}
		static bool AddRoi(std::string roi, std::string key) {


			try {
				for (boost::property_tree::ptree::value_type& row : root.get_child(key))
				{
					if (row.second.data() == roi) return false;
				}
			}
			catch (std::exception& ) {}


			boost::property_tree::ptree cell;
			cell.put_value(roi);


			try {
				boost::property_tree::ptree node_rois = root.get_child(key);
				boost::property_tree::ptree::value_type row = std::make_pair("", cell);
				node_rois.push_back(row);
				root.erase(key);
				root.add_child(key, node_rois);

			}
			catch (std::exception& ) {
				boost::property_tree::ptree node_rois;
				node_rois.push_back(std::make_pair("", cell));
				root.add_child(key, node_rois);
			}



			try {
				boost::property_tree::write_json(Zweistein::Config::inipath.string(), root);
				std::stringstream ss_1;
				boost::property_tree::write_json(ss_1, root);
				std::cout << ss_1.str() << std::endl;
			}
			catch (std::exception& e) { // exception expected, //std::cout << boost::diagnostic_information(e);
				std::cout << e.what() << " for writing." << std::endl;
			}
			return true;


		}
	}

}