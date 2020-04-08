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
#include "Zweistein.Logger.hpp"
#include <magic_enum.hpp>

namespace Zweistein {
	void displayHistogram(boost::asio::io_service& io_service, boost::shared_ptr<Mesytec::MesytecSystem> pmsmtsystem1) {

		
		boost::this_thread::sleep_for(boost::chrono::milliseconds(200));

		// cc::Mat is row, column which corresponds to y, x !!!!
		auto imageUpdate = [](cv::Mat& image, cv::Mat& binnedimage) {
			using namespace magic_enum::bitwise_operators; // out-of-the-box bitwise operators for all enums.
			enum class  HISTYPE {
				RAW=1,
				BINNED=2
			};
			static cv::Mat binned_occ_corrected;
			
			HISTYPE htype = HISTYPE::RAW|HISTYPE::BINNED;
			boost::mutex::scoped_lock lock(histogramsGuard);
			
			
			if ((bool)(htype & HISTYPE::BINNED) && Zweistein::Binning::loaded==true) {
				auto s = Zweistein::Binning::OCC.shape();
			//	LOG_DEBUG << "Zweistein::Binning::OCC.shape()" << s[0] << "," << s[1] << std::endl;
				histograms[1].histogram.copyTo(binned_occ_corrected);
				for (int r = 0; r < s[1]; r++) {
					for (int c = 0; c < s[0]; c++) {
						short occ = Zweistein::Binning::OCC[c][r];
						//short occ = 950;
						if (occ <= 0) continue;
					//	LOG_DEBUG << "occ=" << occ << ", " << " [" << c << "," << r << "]" << std::endl;
						if (occ == Zweistein::Binning::occmultiplier) continue; //  occ == 1
						if (occ < -Zweistein::Binning::occmultiplier || occ>Zweistein::Binning::occmultiplier) {
								LOG_ERROR << "occ=" << occ << ", " << " [" << c << "," << r << "]" << std::endl;
								continue;
						}
						if (r < s[1] - 1) {
						// occ is stored as short, and float 0.94 is mapped to 940 (=> multiplier 1000)
							//point_type pb0(c, r);
							//unsigned long nextpixelcount=histograms[0].histogram.at<int32_t>(p0.y(), p0.x());
							short binnedY = Zweistein::Binning::BINNING[c][r];
						//	LOG_DEBUG << "occ=" << occ << ", binnedY=" << binnedY << " [" << c << "," << r << "]" << std::endl;
							if (binnedY < 0) continue;
							point_type pb(c, binnedY);
							if (pb.x() >= binned_occ_corrected.cols) {
								LOG_ERROR << pb.x() << " <binned_occ_corrected.cols" << std::endl;
							}
							if (pb.y() >= binned_occ_corrected.rows) {
								LOG_ERROR << pb.y() << " <binned_occ_corrected.rows" << std::endl;
							}
							unsigned long nextpixelcount= binned_occ_corrected.at<int32_t>(pb.y(), pb.x())  *(Zweistein::Binning::occmultiplier-occ)/ Zweistein::Binning::occmultiplier;
							if (nextpixelcount != 0) {
								LOG_DEBUG << "pixel corrected:" << pb.x() << ", " << pb.y() << " count -=" << nextpixelcount << std::endl;
							}

							binned_occ_corrected.at<int32_t>(pb.y(), pb.x()) -= nextpixelcount;
							point_type pbNext(c, binnedY + 1);
							binned_occ_corrected.at<int32_t>(pbNext.y(), pbNext.x()) += nextpixelcount;
							
							bool origInside = false;
							bool nextInside = false;

							if (boost::geometry::covered_by(pb, histograms[1].roi)) {
								origInside = true;
							}
							if (boost::geometry::covered_by(pbNext, histograms[1].roi)) {
								nextInside = true;
							}
							
							if (origInside != nextInside) {
								// we have to correct counts:
								if (origInside) histograms[1].count -= nextpixelcount; // so next Y  is outside counting area, so we have to correct origY counts
								if(nextInside) histograms[1].count += nextpixelcount; // original was not counted since outside
							}
							
							
						}
						
					}
				}

				double minVal, maxVal;
				cv::Point minLoc, maxLoc;
				cv::minMaxLoc(binned_occ_corrected, &minVal, &maxVal, &minLoc, &maxLoc);
				binned_occ_corrected.convertTo(binnedimage, CV_8U, maxVal != 0 ? 255.0 / maxVal : 0, 0);
			}
			
			if ((bool)(htype & HISTYPE::RAW)) {
				double minVal, maxVal;
				cv::Point minLoc, maxLoc;
				cv::minMaxLoc(histograms[0].histogram, &minVal, &maxVal, &minLoc, &maxLoc);
				histograms[0].histogram.convertTo(image, CV_8U, maxVal != 0 ? 255.0 / maxVal : 0, 0);
			}

		

		};
		boost::function<void()> display = [ &io_service, &imageUpdate]() {
			sigslot::signal<cv::Mat&, cv::Mat&> sig;
			bool bshow = true;
#ifndef _WIN32
			if (NULL == getenv("DISPLAY")) bshow = false;
#endif
			bool bbinningwindow = false;
			if (bshow) {
				cv::namedWindow("histogram_raw");
				if (Zweistein::Binning::loaded==true) {
					cv::namedWindow("histogram_binned");
					bbinningwindow = true;
				}
			}

			cv::Mat rawimage = cv::Mat_<unsigned char>::zeros(1,1);
			cv::Mat binnedimage = cv::Mat_<unsigned char>::zeros(1, 1);
			cv::Mat colormappedimage;
			cv::Mat colormappedbinnedimage;
			sig.connect(imageUpdate);
			do {
				cv::waitKey(500);

				sig(rawimage,binnedimage);
				if (!rawimage.empty() && rawimage.rows>1) {
					cv::applyColorMap(rawimage, colormappedimage, cv::COLORMAP_JET);
					if (bshow) {
						cv::imshow("histogram_raw", colormappedimage);
					}
				}
				if (!binnedimage.empty() &&binnedimage.rows>1) {
					cv::applyColorMap(binnedimage, colormappedbinnedimage, cv::COLORMAP_JET);
					if (bshow) {
						if(!bbinningwindow) {
							cv::namedWindow("histogram_binned");
								bbinningwindow = true;
						}
						cv::imshow("histogram_binned", colormappedbinnedimage);
					}
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