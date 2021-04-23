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
#include <boost/function.hpp>
#include <boost/filesystem.hpp>
#include "Mesytec.Mcpd8.hpp"
#include "Mesytec.listmode.hpp"
#include "Mesytec.config.hpp"
#include <iomanip>
#include <ctime>
#include <sstream>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/code_converter.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/atomic.hpp>
#include "Zweistein.PrettyBytes.hpp"
#include "Zweistein.Logger.hpp"

namespace Mesytec {
	inline std::string writelistmodeFileNameInfo() {
		boost::filesystem::path tmppath = Mesytec::Config::DATAHOME;
		return tmppath.string() + std::string("YYmmdd_H-M-S.mdat");
	}
	extern boost::atomic<bool> bListmodeWriting ;
	void writeListmode(boost::asio::io_service& io_service, boost::shared_ptr < Mesytec::MesytecSystem> device1);
}