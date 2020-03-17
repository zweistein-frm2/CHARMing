#pragma once
#include <boost/function.hpp>
#include <boost/chrono.hpp>
#include <boost/shared_ptr.hpp>
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
#include "simpleLogger.h"

namespace Zweistein {
	void displayHistogram(boost::asio::io_service& io_service, boost::shared_ptr<Mesytec::MesytecSystem> pmsmtsystem1) {

		
		boost::this_thread::sleep_for(boost::chrono::milliseconds(2000));

		// cc::Mat is row, column which corresponds to y, x !!!!
		auto imageUpdate = [](cv::Mat& image) {
			double minVal, maxVal;
			cv::Point minLoc, maxLoc;
			boost::mutex::scoped_lock lock(histogramsGuard);
			cv::minMaxLoc(histograms[0].histogram, &minVal, &maxVal, &minLoc, &maxLoc);
			{
			//	boost::mutex::scoped_lock lock(coutGuard);
			//	std::cout << "histogram: minVal=" << minVal <<"("<<minLoc.x<<","<<minLoc.y<<")" << ", maxVal=" << maxVal << "(" << maxLoc.x << "," << maxLoc.y << ")" << std::endl;
			}
			histograms[0].histogram.convertTo(image, CV_8U, maxVal != 0 ? 255.0 / maxVal : 0, 0);
		};
		boost::function<void()> display = [ &io_service, &imageUpdate]() {
			sigslot::signal<cv::Mat&> sig;
			bool bshow = true;
#ifndef _WIN32
			if (NULL == getenv("DISPLAY")) bshow = false;
#endif
			if (bshow) cv::namedWindow("histogram");

			cv::Mat image = cv::Mat_<unsigned char>::zeros(1,1);
			cv::Mat colormappedimage;

			sig.connect(imageUpdate);
			do {
				cv::waitKey(500);

				sig(image);
				if (!image.empty()) {
					cv::applyColorMap(image, colormappedimage, cv::COLORMAP_JET);
					if (bshow) cv::imshow("histogram", colormappedimage);
				}
			} while (!io_service.stopped());
			{
				boost::mutex::scoped_lock lock(coutGuard);
				LOG_DEBUG << "display histogram exiting..." << std::endl;
			}


		};
		worker_threads.create_thread(boost::bind(display));



	}
}