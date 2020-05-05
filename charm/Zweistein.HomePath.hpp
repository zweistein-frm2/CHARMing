/***************************************************************************
 *   Copyright (C) 2019 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/

#pragma once
#include <codecvt>
#include <string>
#include <boost/filesystem.hpp>
#include <boost/exception/all.hpp>

namespace Zweistein {
	inline boost::filesystem::path GetHomePath() {
		std::string home;
		char* p = std::getenv("HOME");
		if (p == NULL) {
			p = std::getenv("USERPROFILE");
			if (p == NULL) {
				p = std::getenv("HOMEDRIVE");
				if (p == NULL)  BOOST_THROW_EXCEPTION(std::runtime_error("HOMEDRIVE not in environment"));
				home = p;
				p = std::getenv("HOMEPATH");
				if (p == NULL)  BOOST_THROW_EXCEPTION(std::runtime_error("HOMEPATH not in environment"));
				home += p;
			}
			else home = p;
		}
		else home = p;
		auto it = home.rbegin();
		if (it != home.rend()) {
			if (*it != boost::filesystem::path::preferred_separator) {
				home += boost::filesystem::path::preferred_separator;
			}
		}

#ifdef UNICODE
		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
		boost::filesystem::path homepath(conv.from_bytes(home));
#else
		boost::filesystem::path homepath(home);
#endif
		return homepath;
	}

}
