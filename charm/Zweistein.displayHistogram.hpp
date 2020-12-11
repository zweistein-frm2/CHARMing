/*                          _              _                _
	___  __ __ __  ___     (_)     ___    | |_     ___     (_)    _ _
   |_ /  \ V  V / / -_)    | |    (_-<    |  _|   / -_)    | |   | ' \
  _/__|   \_/\_/  \___|   _|_|_   /__/_   _\__|   \___|   _|_|_  |_||_|
	   .
	   |\       Copyright (C) 2019 - 2020 by Andreas Langhoff
	 _/]_\_                            <andreas.langhoff@frm2.tum.de>
 ~~~"~~~~~^~~   This program is free software; you can redistribute it
 and/or modify it under the terms of the GNU General Public License as
 published by the Free Software Foundation;*/

#pragma once

#ifdef __GNUC__
#include <cxxabi.h>
#endif

#include <boost/function.hpp>
#include <boost/chrono.hpp>
#include <boost/shared_ptr.hpp>
#include "Zweistein.Event.hpp"
#include <boost/format.hpp>
#include <boost/signals2.hpp>
#include <boost/thread.hpp>
#include "sigslot/include/sigslot/signal.hpp"

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include "opencv2/contrib/contrib.hpp"
#include <opencv2/core/core.hpp>
#include <opencv2/core/mat.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#define CVUI_IMPLEMENTATION
#include "cvui/cvui.h"
#include <boost/asio.hpp>
#include "Zweistein.Histogram.hpp"
#include "Zweistein.Binning.ApplyOcc.hpp"
#include "Zweistein.Logger.hpp"


namespace Zweistein {
	void displayHistogram(boost::asio::io_service& io_service, boost::shared_ptr<Zweistein::XYDetectorSystem> pmsmtsystem1) {
		boost::this_thread::sleep_for(boost::chrono::milliseconds(200));

		std::string classname = typeid(*pmsmtsystem1).name();
#ifdef __GNUC__
		char output[255];
		size_t len = 255;
		int status;
		const char* ptrclearclassname = __cxxabiv1::__cxa_demangle(classname.c_str(), output, &len, &status);
#else
		const char* ptrclearclassname = classname.c_str();
#endif
		bool ischarm = false;
		static double shrinkraw = 1.0;
		if (std::string("class Charm::CharmSystem") == std::string(ptrclearclassname)) { // we don't want header dipendency, hence class name check at runtime only
			ischarm = true;
			shrinkraw = 0.25; // reduce from 1024 to 256
		}

		// cv::Mat is row, column which corresponds to y, x !!!!
		auto imageUpdate = [](cv::Mat& image, cv::Mat& binnedimage) {
			using namespace magic_enum::bitwise_operators; // out-of-the-box bitwise operators for all enums.
			enum class  HISTYPE {
				RAW=1,
				BINNED=2
			};
			static cv::Mat binned_occ_corrected;
			static cv::Mat raw_shrinked;
			static cv::Mat raw_float;
			HISTYPE htype = HISTYPE::RAW|HISTYPE::BINNED;

			histogram_setup_status hss = Zweistein::setup_status;
			if ((bool)(htype & HISTYPE::BINNED) && magic_enum::enum_integer(hss & histogram_setup_status::has_binning)) {
				{
					Zweistein::ReadLock r_lock(histogramsLock);
					histograms[1].histogram.copyTo(binned_occ_corrected);
					Zweistein::Binning::Apply_OCC_Correction(binned_occ_corrected, histograms[1].roidata[0].roi, histograms[1].roidata[0].count);
				}
				double minVal, maxVal;
				cv::Point minLoc, maxLoc;
				cv::minMaxLoc(binned_occ_corrected, &minVal, &maxVal, &minLoc, &maxLoc);
				binned_occ_corrected.convertTo(binnedimage, CV_8U, maxVal != 0 ? 255.0 / maxVal : 0, 0);
			}

			if ((bool)(htype & HISTYPE::RAW)) {
				Zweistein::ReadLock r_lock(histogramsLock);

				if (shrinkraw != 1) {

					histograms[0].histogram.convertTo(raw_float, CV_32F);

					if (raw_shrinked.cols < 2) {
						raw_shrinked = cv::Mat_<float>::zeros(histograms[0].histogram.size[0] * shrinkraw, histograms[0].histogram.size[1] * shrinkraw);
					}
					else {
						raw_shrinked.resize(histograms[0].histogram.size[0] * shrinkraw, histograms[0].histogram.size[1] * shrinkraw);
					}
					cv::resize(raw_float, raw_shrinked, raw_shrinked.size(),0,0,cv::INTER_AREA);
					double minVal, maxVal;
					cv::Point minLoc, maxLoc;
					cv::minMaxLoc(raw_shrinked, &minVal, &maxVal, &minLoc, &maxLoc);
					raw_shrinked.convertTo(image, CV_8U, maxVal != 0 ? 255.0 / maxVal : 0, 0);
				}
				else {
					double minVal, maxVal;
				    cv::Point minLoc, maxLoc;
				    cv::minMaxLoc(histograms[0].histogram, &minVal, &maxVal, &minLoc, &maxLoc);
				    histograms[0].histogram.convertTo(image, CV_8U, maxVal != 0 ? 255.0 / maxVal : 0, 0);
			   }
			}
		};
		boost::function<void()> display = [ &io_service, &imageUpdate]() {
			sigslot::signal<cv::Mat&, cv::Mat&> sig;
			bool bshow = true;
#ifndef _WIN32
			if (NULL == getenv("DISPLAY")) bshow = false;
#endif
			bool bbinningwindow = false;
			std::vector<cv::String> windows = std::vector<cv::String>();
			if (bshow) {
				using namespace magic_enum::bitwise_operators; // out-of-the-box bitwise operators for enums.
				std::string title = "histogram_raw";
				windows.push_back(title);
				cv::namedWindow(title);

				histogram_setup_status hss = Zweistein::setup_status;
				if (magic_enum::enum_integer(hss & histogram_setup_status::has_binning)) {
					std::string title2 = "histogram_binned";
					windows.push_back(title2);
					cv::namedWindow(title2);

					bbinningwindow = true;
				}
				cvui::init(windows[0]);
				if(bbinningwindow) cvui::watch(windows[1]);

			}

			cv::Mat rawimage = cv::Mat_<unsigned char>::zeros(1,1);
			cv::Mat binnedimage = cv::Mat_<unsigned char>::zeros(1, 1);
			cv::Mat colormappedimage;
			cv::Mat colormappedbinnedimage;
			bool use_clahe_raw = false;
			bool use_clahe = false;
			sig.connect(imageUpdate);
			do {
				cv::waitKey(500);

				sig(rawimage,binnedimage);
				if (!rawimage.empty() && rawimage.rows>1) {

					if (bshow ) {
						cvui::context(windows[0]);
						if (use_clahe_raw) {
							cv::Ptr<cv::CLAHE> clahe=cv::createCLAHE();
							clahe->setClipLimit(2.0);
							cv::Mat dst;
							clahe->apply(rawimage, dst);
							rawimage = dst;

						}

						cv::applyColorMap(rawimage, colormappedimage, cv::COLORMAP_JET);
						cvui::checkbox(colormappedimage, 0, 0, "CLAHE", &use_clahe_raw);
						cvui::update(windows[0]);
						cv::imshow(windows[0], colormappedimage);
					}
				}
				if (!binnedimage.empty() &&binnedimage.rows>1) {

					if (bshow) {
						cvui::context(windows[1]);
						if(!bbinningwindow) {
							cv::namedWindow(windows[1]);
							cvui::watch(windows[1]);
								bbinningwindow = true;
						}
						if (use_clahe) {
							cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE();
							clahe->setClipLimit(2.0);
							cv::Mat dst;
							clahe->apply(binnedimage, dst);
							binnedimage = dst;

						}
						cv::applyColorMap(binnedimage, colormappedbinnedimage, cv::COLORMAP_JET);
						cvui::checkbox(colormappedbinnedimage,0,0, "CLAHE", &use_clahe);
						cvui::update(windows[1]);
						cv::imshow(windows[1], colormappedbinnedimage);
					}
				}



			} while (!io_service.stopped());
			{
				LOG_DEBUG << "display histogram exiting..." << std::endl;
			}


		};
		worker_threads.create_thread(boost::bind(display));
	}
}