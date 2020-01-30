/***************************************************************************
 *   Copyright (C) 2019 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/

#pragma once
#include <boost/function.hpp>
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

namespace Zweistein {
	static cv::Mat_<float> histogram;
	static boost::atomic<bool> initdonehistogramsize = false;
	void populateHistogram(boost::asio::io_service &io_service,Mesytec::MesytecSystem& msmtsystem1) {
		
		boost::this_thread::sleep_for(boost::chrono::milliseconds(2000));
		unsigned short x_default = msmtsystem1.data.widthX; 
		unsigned short y_default = msmtsystem1.data.widthY; 
		// cc::Mat is row, column which corresponds to y, x !!!!
		histogram = cv::Mat_<float>::zeros(y_default, x_default);
		auto imageUpdate = [](cv::Mat& image) {
			double minVal, maxVal;
			cv::Point minLoc, maxLoc;
			cv::minMaxLoc(histogram, &minVal, &maxVal, &minLoc, &maxLoc);
			{
				//boost::mutex::scoped_lock lock(coutGuard);
				//std::cout << "histogram: minVal=" << minVal <<"("<<minLoc.x<<","<<minLoc.y<<")" << ", maxVal=" << maxVal << "(" << maxLoc.x << "," << maxLoc.y << ")" << std::endl;
			}
			histogram.convertTo(image, CV_8U, maxVal != 0 ? 255.0 / maxVal : 0, 0);
		};
		boost::function<void()> display = [&x_default,&y_default,&io_service, &imageUpdate]() {
			sigslot::signal<cv::Mat&> sig;
			bool bshow = true;
#ifndef _WIN32
			if (NULL == getenv("DISPLAY")) bshow = false;
#endif
				if(bshow) cv::namedWindow("histogram");
				
				cv::Mat image = cv::Mat_<unsigned char>::zeros(y_default, x_default);
				cv::Mat colormappedimage;

				sig.connect(imageUpdate);
				do {
					cv::waitKey(500);
					
					{
						//if (!initdonehistogramsize) continue;
					}
					sig(image);
					if (!image.empty()) {
						//int y = 955;
						//int x = 54;
						//int val=image.at<unsigned char>(y, x);
						cv::applyColorMap(image, colormappedimage, cv::COLORMAP_JET);
						if(bshow) cv::imshow("histogram", colormappedimage);
					}
				} while (!io_service.stopped());
				{
					boost::mutex::scoped_lock lock(coutGuard);
					std::cout << std::endl << "display histogram exiting..." << std::endl;
				}
			
			
		};
		worker_threads.create_thread(boost::bind(display));
		boost::chrono::system_clock::time_point start = boost::chrono::system_clock::now();
		try {
			Zweistein::Event ev;
			int i = 0;
			size_t max_inqueue = 0;
			unsigned short maxX= x_default;
			unsigned short maxY= y_default;
			bool initdone = false;
			long long nloop = 0;
			boost::chrono::system_clock::time_point current = boost::chrono::system_clock::now();
			boost::chrono::nanoseconds elapsed;
			do {
				if (true) { // for speed reasons we do check only every 50th loop
					current = boost::chrono::system_clock::now();
					elapsed = current - start;
					if (elapsed.count() > 500L * 1000L * 1000L) { // we update every 500 ms
						start = boost::chrono::system_clock::now();
						if (max_inqueue == Mcpd8::Data::EVENTQUEUESIZE) {
							boost::mutex::scoped_lock lock(coutGuard);
							std::cout << "EVENT QUEUE FULL (" << max_inqueue << "), dropped excess from histogram input " << std::endl;
							max_inqueue = 0;
						}
					}
				}
				if (!initdone) {
					size_t inqueue = msmtsystem1.data.evntqueue.read_available();
					if (inqueue > 0) {
						if (!initdone)
						{
							if (!initdonehistogramsize) {
								maxX = msmtsystem1.data.widthX;
								maxY = msmtsystem1.data.widthY;

								
								histogram = cv::Mat_<float>::zeros(maxY, maxX);
								{
									boost::mutex::scoped_lock lock(coutGuard);
									std::cout << "Histogram size intialized to: rows("<<maxY<<") x columns("<<maxX<<") " << std::endl;
								}
								initdonehistogramsize = true;
								initdone = true;
							}
						}
					}
					continue;
				}
				long evntspopped = 0;
				while (msmtsystem1.data.evntqueue.pop(ev)) {
					if ((ev.X < 0 || ev.X >= maxX) || (ev.Y < 0 || ev.Y >= maxY)) {
						boost::mutex::scoped_lock lock(coutGuard);
						std::cout << " Event.X or Event.Y ouside bounds" << std::endl;
					}
					evntspopped++;
					if (evntspopped > Mcpd8::Data::EVENTQUEUESIZE*3/4) {
						size_t inqueue = msmtsystem1.data.evntqueue.read_available();
						if (inqueue >= max_inqueue) max_inqueue = inqueue;
					}
					//histogram.at<float>(ev.Y, ev.X) += ev.Amplitude;
					//if(ev.X>=8 && ev.X<16)	
					histogram.at<float>(maxY-ev.Y-1, ev.X) += 1.0;  // event is event and amplitude secondary

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
			std::cout<<std::endl << "populateHistogram() exiting..." << std::endl;
		}
	}


}