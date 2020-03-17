/***************************************************************************
 *   Copyright (C) 2019 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/

#pragma once
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/chrono.hpp>
#include "Zweistein.Event.hpp"
#include "Mesytec.Mcpd8.hpp"
#include <boost/format.hpp>
#include <boost/signals2.hpp>
#include <boost/thread.hpp>
#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include <opencv2/core.hpp>
#include <opencv2/core/mat.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <boost/asio.hpp>
#include "Zweistein.Histogram.hpp"
#include "Zweistein.Binning.hpp"
#include "simpleLogger.h"
namespace Zweistein {
	void populateHistograms(boost::asio::io_service & io_service, boost::shared_ptr < Mesytec::MesytecSystem> pmsmtsystem1,std::string binningfile1,std::string wktroi1) {
	
		unsigned short maxX = pmsmtsystem1->data.widthX;
		unsigned short maxY = pmsmtsystem1->data.widthY;
		int left = 0;
		int bottom = 0;
		{
			boost::mutex::scoped_lock lock(histogramsGuard); 
			histograms[0].resize(maxY, maxX);
			histograms[0].setRoiRect(left, bottom, maxX, maxY);
		}
		

		try {
			Zweistein::Binning::Read(binningfile1);
			auto s = Zweistein::Binning::BINNING.shape();
			if(s[0]!=maxX || s[1]!=maxY) BOOST_THROW_EXCEPTION(std::range_error("Binning dims != detector size"));
		}
		catch (std::exception& e) {
			LOG_WARNING << e.what()<<" for reading.";
			int desty = 128;
			if (maxY >= desty) { 
				Zweistein::Binning::GenerateSimple(desty, maxY, maxX); 
				LOG_WARNING << "Zweistein::Binning::GenerateSimple(" << desty << "," << maxY << "," << maxX << ")" << "=> " << binningfile1 << std::endl;
				try {
					Zweistein::Binning::Write(binningfile1);
				}
				catch (std::exception& e) {
					LOG_ERROR << e.what() << " for writing.";
				}
			}
			
		}
		auto s = Zweistein::Binning::BINNING.shape();
		Zweistein::Binning::array_type::element* itr = Zweistein::Binning::BINNING.data();
		
		unsigned short max_value = 0;
		for (int i = 0; i < s[0] * s[1]; i++) {
			if (*itr > max_value) max_value = *itr;
			itr++;
		}
		{
			boost::mutex::scoped_lock lock(histogramsGuard);
			histograms[1].resize( max_value + 1, maxX);
			histograms[1].setRoi(wktroi1);
		}
		
		boost::chrono::system_clock::time_point start = boost::chrono::system_clock::now();
		try {
			Zweistein::Event ev;
			int i = 0;
			
			long long nloop = 0;
			boost::chrono::system_clock::time_point current = boost::chrono::system_clock::now();
			boost::chrono::nanoseconds elapsed;
			do {
									
				boost::mutex::scoped_lock lock(histogramsGuard);
				long evntspopped = 0;
				while (pmsmtsystem1->data.evntqueue.pop(ev)) {
					evntspopped++;
					if (evntspopped > Mcpd8::Data::EVENTQUEUESIZE/2) {
						evntspopped = 0;
						boost::mutex::scoped_lock lock(coutGuard);
						LOG_WARNING << "evntspopped > "<< Mcpd8::Data::EVENTQUEUESIZE / 2 << std::endl;
						break;
					}
					if (ev.type == Zweistein::Event::EventTypeOther::RESET) {
						for (auto& h : histograms) {
							h.count = 0;
							h.resize(h.histogram.size[0], h.histogram.size[1]);
						}
						continue;
					}
					if ((ev.X < 0 || ev.X >= maxX) || (ev.Y < 0 || ev.Y >= maxY)) {
						boost::mutex::scoped_lock lock(coutGuard);
						LOG_ERROR << " Event.X or Event.Y ouside bounds" << std::endl;
						continue;
					}
					// this is our raw histograms[0]
					point_type p(ev.X, ev.Y);
					if (boost::geometry::covered_by(p, histograms[0].roi)) {
							auto size=histograms[0].histogram.size;
							histograms[0].histogram.at<int32_t>(p.y() ,p.x()) += 1;
							histograms[0].count += 1;
					}
					// this is our binned histograms[1]
					unsigned short binnedY = Zweistein::Binning::BINNING[ev.X][ev.Y];
					point_type pb(ev.X, binnedY);
					if (boost::geometry::covered_by(pb, histograms[1].roi)) {
							histograms[1].histogram.at<int32_t>(pb.y(),pb.x()) += 1;
							histograms[1].count += 1;
					}

				}
				nloop++;
			} while (!io_service.stopped());
		}
		catch (boost::exception & e) {
			boost::mutex::scoped_lock lock(coutGuard);
			LOG_ERROR<<boost::diagnostic_information(e);
			
		}
		LOG_INFO << "populateHistograms() exiting..." << std::endl;
	}


}