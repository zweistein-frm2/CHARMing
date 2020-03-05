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
#include <sigslot/signal.hpp>
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

namespace Zweistein {
	void populateHistograms(boost::asio::io_service & io_service, boost::shared_ptr < Mesytec::MesytecSystem> pmsmtsystem1) {
	
		unsigned short maxX = pmsmtsystem1->data.widthX;
		unsigned short maxY = pmsmtsystem1->data.widthY;
		int left = 0;
		int bottom = 0;
		histograms[0].setRoiRect(left, bottom, maxX,maxY);
		{
			boost::mutex::scoped_lock lock(coutGuard);
		//	std::cout << "Histogram size intialized to: rows(" << maxY << ") x columns(" << maxX << ") " << std::endl;
		}
	
		boost::chrono::system_clock::time_point start = boost::chrono::system_clock::now();
		try {
			Zweistein::Event ev;
			int i = 0;
			
			long long nloop = 0;
			boost::chrono::system_clock::time_point current = boost::chrono::system_clock::now();
			boost::chrono::nanoseconds elapsed;
			do {
									
				
				long evntspopped = 0;
				while (pmsmtsystem1->data.evntqueue.pop(ev)) {
					if ((ev.X < 0 || ev.X >= maxX) || (ev.Y < 0 || ev.Y >= maxY)) {
						boost::mutex::scoped_lock lock(coutGuard);
						std::cout << " Event.X or Event.Y ouside bounds" << std::endl;
					}
					evntspopped++;
					
					//histogram.at<float>(ev.Y, ev.X) += ev.Amplitude;
					//if(ev.X>=8 && ev.X<16)	

					for (auto& h : histograms) {
						//Zweistein::WriteLock w_lock(h.lock); 
						point_type p(ev.X, ev.Y);
						if (boost::geometry::covered_by(p, h.roi)) {
							h.histogram.at<int32_t>(ev.Y - h.box.min_corner().get<1>(), ev.X- h.box.min_corner().get<0>()) += 1;
							h.count += 1;
						}
					}
				

				}
				nloop++;
			} while (!io_service.stopped());
		}
		catch (boost::exception & e) {
			boost::mutex::scoped_lock lock(coutGuard);
			std::cout<<boost::diagnostic_information(e);
			
		}
		{
			boost::mutex::scoped_lock lock(coutGuard);
			std::cout<<std::endl << "populateHistograms() exiting..." << std::endl;
		}
	}


}