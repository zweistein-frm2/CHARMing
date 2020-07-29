/***************************************************************************
 *   Copyright (C) 2019 - 2020 by Andreas Langhoff
 *                                          <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/
#pragma once
#include <boost/asio.hpp>
#include <boost/atomic.hpp>
#include "Zweistein.Data.hpp"

namespace Zweistein {

	class XYDetectorSystem {
	public:
		boost::asio::io_service* pio_service;
		boost::atomic<bool> connected;
		boost::atomic<long> simulatordatarate;
		boost::atomic<bool> daq_running;
	private:
		boost::atomic<boost::chrono::system_clock::time_point> started;
	public:
		virtual void setStart(boost::chrono::system_clock::time_point & t) {
			started = t;
		}
		boost::chrono::system_clock::time_point getStart() {
			boost::chrono::system_clock::time_point t = started; // neded because atomic
			return t;
		}

		boost::atomic<boost::chrono::system_clock::time_point> stopped;
		boost::chrono::system_clock::time_point lasteventqueuefull;
		boost::atomic<bool> write2disk;
		size_t lasteventqueuefull_missedcount;
		Zweistein::Data evdata;
		virtual void initatomicortime_point();
		XYDetectorSystem() :lasteventqueuefull_missedcount(0),pio_service(nullptr) {}
	};
}