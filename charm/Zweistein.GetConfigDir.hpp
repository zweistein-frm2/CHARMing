/***************************************************************************
  *   Copyright (C) 2019 - 2020 by Andreas Langhoff
  *                                          <andreas.langhoff@frm2.tum.de> *
  *   This program is free software; you can redistribute it and/or modify  *
  *   it under the terms of the GNU General Public License as published by  *
  *   the Free Software Foundation;                                         *
  ***************************************************************************/

#pragma once
#include <boost/atomic.hpp>
#include <boost/filesystem.hpp>
#include <list>
#include <string>
#include "Zweistein.Logger.hpp"
#include "Zweistein.HomePath.hpp"

extern std::string PROJECT_NAME;
namespace Zweistein {

	
	namespace Config {
		extern boost::filesystem::path inipath;

		inline boost::filesystem::path PreferredDirectory() {
			boost::filesystem::path  r(Zweistein::GetHomePath());

			r = r.root_path();
			r /= "etc";
			return r;
		}
		inline boost::filesystem::path GetConfigDirectory() {
			
			boost::filesystem::path  r=PreferredDirectory();
		
			r= r.root_path();
			r /= "etc";
			bool use_etcdir = true;
			boost::system::error_code ec;
			if (!boost::filesystem::exists(r)) {
				use_etcdir = boost::filesystem::create_directory(r, ec);
			}

			if (ec) LOG_WARNING << ec.message() << ": " << r.string() << std::endl;
			else {

				r /= PROJECT_NAME;// +"-frm2";

				if (!boost::filesystem::exists(r)) {
					use_etcdir = boost::filesystem::create_directory(r, ec);
				}
				if (ec) LOG_WARNING << ec.message() << ": " << r.string()<<std::endl;
			}
			if (ec) use_etcdir = false;

			boost::filesystem::path inidirectory;
			if (!use_etcdir) {
				boost::filesystem::path homepath = Zweistein::GetHomePath();
				inidirectory = homepath;
				inidirectory /= "." + std::string(PROJECT_NAME);
				if (!boost::filesystem::exists(inidirectory)) {
					boost::filesystem::create_directory(inidirectory);
				}
				inidirectory += boost::filesystem::path::preferred_separator;
			}
			else inidirectory = r;

			return inidirectory;




		}


	}




}