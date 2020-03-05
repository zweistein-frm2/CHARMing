/***************************************************************************
 *   Copyright (C) 2019 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/
#pragma once
#include <string>
#include <boost/asio.hpp>
#include <uv.h>


namespace Zweistein {

void GetLocalInterfaces(std::list<std::string>& localinterfaces) {
	char buf[512];
	uv_interface_address_t* info;
	int count, i;
	uv_interface_addresses(&info, &count);
	i = count;
	while (i--) {
		uv_interface_address_t interface = info[i];
		if (interface.is_internal) continue;
		if (interface.address.address4.sin_family == AF_INET) {
			uv_ip4_name(&interface.address.address4, buf, sizeof(buf));
			std::string address = std::string(buf);
			localinterfaces.push_back(address);
		}
		else if (interface.address.address4.sin_family == AF_INET6) {
			uv_ip6_name(&interface.address.address6, buf, sizeof(buf));
			std::string address = std::string(buf);
			//localinterfaces.push_back(address);
		}
	}
	uv_free_interface_addresses(info, count);
	

}


	std::string askforInterfaceIfUnknown(std::string proposed) {

		std::list<std::string> localinterfaces = std::list<std::string>();
	    Zweistein::GetLocalInterfaces(localinterfaces);
		auto _a = std::find(localinterfaces.begin(), localinterfaces.end(), proposed);
		if (_a == localinterfaces.end()) {
			std::cout << "interfaces on this machine are:";
			int i = 0;
			BOOST_FOREACH(std::string str, localinterfaces) { std::cout << str << "(" << i++ << ")  "; }
			std::cout << std::endl << "Choose interface to use: (0),... ";
			int choice = 2;
			std::cin >> choice;
			i = 0;
			BOOST_FOREACH(std::string str, localinterfaces) { if (i++ == choice) proposed = str; }
		}
		return proposed;
	}
}

