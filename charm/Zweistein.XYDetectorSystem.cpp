/***************************************************************************
 *   Copyright (C) 2019 - 2020 by Andreas Langhoff
 *                                          <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/

#include "Zweistein.XYDetectorSystem.hpp"

namespace Zweistein {

	void XYDetectorSystem::initatomicortime_point() {
		auto now = boost::chrono::system_clock::now();
		started = now;
		stopped = now;
		simulatordatarate = 51;
		connected = false;
		daq_running = false;
		write2disk = false;
		auto min = boost::chrono::system_clock::now() - boost::chrono::minutes(60);
		lasteventqueuefull = min;


	}
}