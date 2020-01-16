/***************************************************************************
 *   Copyright (C) 2019 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/

#pragma once
#include <boost/function.hpp>
#include "Mesytec.Mcpd8.hpp"
#include "Mesytec.listmode.hpp"
#include <iomanip>
#include <ctime>
#include <sstream>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/code_converter.hpp>
#include <boost/iostreams/device/mapped_file.hpp>


namespace Mesytec {

	std::string writelistmodeFileNameInfo() {
		boost::filesystem::path tmppath = Zweistein::GetHomePath();
		return tmppath.string() + std::string("YYmmdd_H-M-S.mdat)");
	}
	
	void writeListmode(boost::asio::io_service& io_service,Mesytec::MesytecSystem &device1) {

		namespace bio = boost::iostreams;
		boost::filesystem::path tmppath = Zweistein::GetHomePath();
		boost::filesystem::space_info si = boost::filesystem::space(tmppath);
		auto t = std::time(nullptr);
		auto tm = *std::localtime(&t);
		std::ostringstream oss;
		oss << std::put_time(&tm, "%Y%m%d_%H-%M-%S");
		auto str = oss.str();
		tmppath.append(oss.str()+".mdat");
		size_t byteswritten = 0;
		try {
			
			bio::mapped_file_params params(tmppath.string());
			size_t keepspace = 10LL *1000LL* 1000LL * 1000LL; //10GB
			params.new_file_size = si.available> keepspace*3/2? si.available-keepspace:si.available;
			params.flags = bio::mapped_file::mapmode::readwrite;

			bio::stream<bio::mapped_file_sink> f;
			f.open(bio::mapped_file_sink(params));
				
			std::string header = u8"mesytec psd listmode data\nheader length: 2 lines\n";
			f.write(header.c_str(), header.length());
			f.write(Mesytec::listmode::header_separator, sizeof(Mesytec::listmode::header_separator));
			{
				boost::mutex::scoped_lock lock(coutGuard);
				std::cout << "Writing to:" << tmppath << std::endl;
			}
			Mcpd8::DataPacket dp;
			int i = 0;
			do {
				while (device1.data.listmodequeue.pop(dp)) {
					/*
					i++;
					{
						boost::mutex::scoped_lock lock(coutGuard);
						std::cout << ev;
					}*/
					short* sp = (short*)&dp;
					int len = dp.BytesUsed();
					for (int i = 0; i <len/sizeof(short); i++) 	boost::endian::endian_reverse_inplace(sp[i]);
					f.write(reinterpret_cast<char*>(&dp), len);
					f.write(Mesytec::listmode::datablock_separator, sizeof(Mesytec::listmode::datablock_separator));
				}
			} while (!io_service.stopped());
			f.write(Mesytec::listmode::closing_signature, sizeof(Mesytec::listmode::closing_signature));
			byteswritten =f.tellp();
			f.close();
		}
		catch (std::exception & e) {
			boost::mutex::scoped_lock lock(coutGuard);
			std::cerr << e.what() << std::endl;
		}
		std::filesystem::resize_file(tmppath.string(), byteswritten);
		{
			boost::mutex::scoped_lock lock(coutGuard);
			std::cout << Zweistein::PrettyBytes(byteswritten) << " written to:"<< tmppath.string() << std::endl;
		}
	}


}