/***************************************************************************
 *   Copyright (C) 2019 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/

#pragma once
#include <string>
#include <boost/format.hpp>

namespace Zweistein {
	static std::string PrettyBytes(size_t bytes) {
		const char* suffixes[7] = { "B","kB","MB","GB","TB","PB","EB" };
		unsigned int s = 0; // which suffix to use
		double count = (double)bytes;
		while (count >= 1024 && s < 7)
		{
			s++;
			count /= 1024;
		}
		std::string rv;
		if (count - floor(count) == 0.0)
			rv = str(boost::format("%d %s")%count % suffixes[s]);
		else
			rv = str(boost::format("%.1f %s") % count % suffixes[s]);
		return rv;

	}
}