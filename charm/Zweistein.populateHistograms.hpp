/*                          _              _                _
	___  __ __ __  ___     (_)     ___    | |_     ___     (_)    _ _
   |_ /  \ V  V / / -_)    | |    (_-<    |  _|   / -_)    | |   | ' \
  _/__|   \_/\_/  \___|   _|_|_   /__/_   _\__|   \___|   _|_|_  |_||_|
	   .
	   |\       Copyright (C) 2019 - 2020 by Andreas Langhoff
	 _/]_\_                            <andreas.langhoff@frm2.tum.de>
 ~~~"~~~~~^~~   This program is free software;
 original site:  https://github.com/zweistein-frm2/CHARMing
 you can redistribute it
 and/or modify it under the terms of the GNU General Public License v3
 as published by the Free Software Foundation;
 */

#pragma once
#ifdef __GNUC__
#include <cxxabi.h>
#endif
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/chrono.hpp>
#include "Zweistein.Event.hpp"
#include "Zweistein.XYDetectorSystem.hpp"
#include <boost/format.hpp>
#include <boost/signals2.hpp>
#include <boost/thread.hpp>
#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include <boost/asio.hpp>
#include "Zweistein.Histogram.hpp"
#include "Zweistein.Binning.hpp"
#include "Zweistein.Logger.hpp"
#include <boost/filesystem.hpp>
#include <cctype>
#include <sstream>
#include "Zweistein.GetConfigDir.hpp"

namespace Zweistein {

	void setupHistograms(boost::asio::io_service& io_service, boost::shared_ptr < Zweistein::XYDetectorSystem> pmsmtsystem1, std::string binningfile1) {
		using namespace magic_enum::bitwise_operators; // out-of-the-box bitwise operators for enums.

		setup_status = histogram_setup_status::not_done;

		unsigned short maxX = pmsmtsystem1->evdata.widthX;
		unsigned short maxY = pmsmtsystem1->evdata.widthY;




		if (maxX == 0 || maxY == 0) {
			LOG_ERROR << __FILE__ << " : " << __LINE__ << " maxX=" << maxX << ", maxY=" << maxY << std::endl;
			return;
		}


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
		}


		if (pmsmtsystem1->systype == Zweistein::XYDetector::Systemtype::Charm) ischarm = true;

		if (pmsmtsystem1->systype == Zweistein::XYDetector::Systemtype::Mesytec) ischarm = false;


		//LOG_DEBUG << "pmsmtsystem1->data.widthX=" << maxX << ", pmsmtsystem1->data.widthY=" << maxY << std::endl;
		int left = 0;
		int bottom = 0;
		{
			Zweistein::WriteLock w_lock(histogramsLock);
			histograms[0].resize(maxY, maxX);
			std::string wkt = histograms[0].WKTRoiRect(left, bottom, maxX, maxY);
			histograms[0]._setRoi(wkt, 0);
		}
		histogram_setup_status hss = setup_status;
		setup_status = hss | histogram_setup_status::histogram0_resized;
		if (ischarm) {
			if (!binningfile1.empty() || binningfile1.length() != 0) {
				LOG_ERROR << "skipped: " << binningfile1 << std::endl;
				LOG_ERROR << classname << " does not support Binning files." << std::endl;
				binningfile1 = "";
			}

		}

		if (!binningfile1.empty() || binningfile1.length() != 0) {
			bool readfileproblem = true;
			try {

				boost::filesystem::path p(binningfile1);

				std::string ext = boost::algorithm::to_lower_copy(p.extension().string());
				if (ext == ".txt") {
					Zweistein::Binning::ReadTxt(binningfile1);
				}
				else if (ext == ".json") {
					Zweistein::Binning::Read(binningfile1);
				}

				else {
					LOG_ERROR << "Binning not handled:" << binningfile1 << std::endl;
				}


				auto s = Zweistein::Binning::BINNING.shape();

				if (s[0] > maxX) {
					LOG_ERROR << "BINNING.shape()[0]=" << s[0] << " greater than detector sizeX(" << maxX << ")" << std::endl;
					io_service.stop();

				}

				if (s[1] > maxY) {
					LOG_ERROR << "BINNING.shape()[1]=" << s[1] << " greater than detector sizeY(" << maxY << ")" << std::endl;
					io_service.stop();

				}

				if (s[1] < maxY) {
					LOG_WARNING << "BINNING.shape()[1]=" << s[1] << " smaller than detector sizeY(" << maxY << "), detector partially unused." << std::endl;
				}
				hss = setup_status;
				setup_status = hss | histogram_setup_status::has_binning;
				readfileproblem = false;
			}

			catch(const boost::property_tree::json_parser_error& e1){
				LOG_ERROR << e1.what() << " " << e1.filename() << std::endl ;
			}

			catch (std::exception& e) {
				LOG_ERROR << e.what() << " for reading." << std::endl;
			}

			if(readfileproblem){
				int newy = maxY / 4;
				Zweistein::Binning::GenerateSimple(newy, maxY, maxX);
				std::stringstream ss;
				ss << std::endl << "Zweistein::Binning::GenerateSimple(" << newy << "," << maxY << "," << maxX << ") ";

				try {
					boost::filesystem::path p(binningfile1);
					std::string f = "y" + std::to_string(newy) + "-" + std::to_string(maxY) + "_x" + std::to_string(maxX) + "_binning.json";
					p = p.remove_filename();
					std::string file = p.append(f).string();
					Zweistein::Binning::Write(file);
					ss << "=> " << file << " " << std::endl;
					LOG_WARNING << ss.str();
					histogram_setup_status hss = setup_status;
					setup_status = hss | histogram_setup_status::has_binning;

				}
				catch (std::exception& e) {
					ss << e.what() << " for writing." << std::endl;
					LOG_ERROR << ss.str();

				}

			}

			auto s = Zweistein::Binning::BINNING.shape();

			Zweistein::Binning::array_type::element* itr = Zweistein::Binning::BINNING.data();

			short max_value = 0;
			for (int i = 0; i < s[0] * s[1]; i++) {
				if (*itr > max_value) max_value = *itr;
				itr++;
			}
			int power = 2;
			while (power < max_value) power *= 2;

			{

				Zweistein::WriteLock w_lock(histogramsLock);
				histograms[1].resize(power, maxX);
				std::string wkt = histograms[1].WKTRoiRect(0, 0, maxX, power);
				histograms[1]._setRoi(wkt, 0);
				LOG_DEBUG << "histograms[1] :rows=" << histograms[1].histogram.rows << ", cols=" << histograms[1].histogram.cols << std::endl;
				LOG_INFO << "Zweistein::Binning::BINNING.shape(" << s[0] << "," << s[1] << ")" << std::endl;
			}
			hss = setup_status;
			setup_status = hss | histogram_setup_status::histogram1_resized ;
		}
		hss = setup_status;
		setup_status = hss | histogram_setup_status::done;
	}


	void populateHistograms(boost::asio::io_service & io_service, boost::shared_ptr < Zweistein::XYDetectorSystem> pmsmtsystem1) {
		using namespace magic_enum::bitwise_operators; // out-of-the-box bitwise operators for enums.
		boost::chrono::system_clock::time_point start = boost::chrono::system_clock::now();
		try {
			Zweistein::Event ev;
			int i = 0;
			long long nloop = 0;
			do {
				boost::chrono::system_clock::time_point current = boost::chrono::system_clock::now();
				histogram_setup_status hss = setup_status;
				if (!magic_enum::enum_integer(hss & histogram_setup_status::done)) {
					boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
					continue;
				}
				unsigned short maxX = pmsmtsystem1->evdata.widthX;
				unsigned short maxY = pmsmtsystem1->evdata.widthY;

				unsigned short binningMaxY = 0;
				if (magic_enum::enum_integer(hss & histogram_setup_status::has_binning)) {
					binningMaxY = (unsigned short) Zweistein::Binning::BINNING.shape()[1];
				}

				{

					Zweistein::WriteLock w_lock(histogramsLock);
					long evntspopped = 0;
					while (pmsmtsystem1->evdata.evntqueue.pop(ev)) {
					evntspopped++;
					if (evntspopped > 2 * Zweistein::Data::EVENTQUEUESIZE / 3) {
						evntspopped = 0;
						LOG_WARNING << "evntspopped > " << 2 * Zweistein::Data::EVENTQUEUESIZE / 3 << std::endl;
						break;
					}

					if (evntspopped > 1000) { // to save cpu time in this critical loop
						// we have to give free histogramsGuard once a while (at least every 200 milliseconds)
						// so that other code can change Roi data.
						auto  elapsed = boost::chrono::duration_cast<boost::chrono::milliseconds>(boost::chrono::system_clock::now() - current);
						if (elapsed.count() > 200) {
							current = boost::chrono::system_clock::now();
							break;
						}
					}



					if (ev.type == Zweistein::Event::EventTypeOther::RESET) {
						maxX = pmsmtsystem1->evdata.widthX;
						maxY = pmsmtsystem1->evdata.widthY;
						for (auto& h : histograms) {
							for (auto& r : h.roidata) 	r.count = 0;
							h.resize(h.histogram.size[0], h.histogram.size[1]);
						}
						break;
					}
					if (ev.X < 0 || ev.X >= maxX) {
						LOG_WARNING << "0 < Event.X=" << ev.X << " < " << maxX << " Event.X ouside bounds" << std::endl;
						continue;
					}
					if (ev.Y < 0 || ev.Y >= maxY) {
						LOG_WARNING << "0< Event.Y=" << ev.Y << " < " << maxY << " Event.Y ouside bounds" << std::endl;
						continue;

					}

					// this is our raw histograms[0]
					point_type p(ev.X, ev.Y);
					histograms[0].histogram.at<HISTOGRAMTYPE>(p.y(), p.x()) += ev.Amplitude;
					for (auto& r : histograms[0].roidata) {
						if (boost::geometry::covered_by(p, r.roi)) {
							auto size = histograms[0].histogram.size;
							r.count += ev.Amplitude;

						}
					}

					if (magic_enum::enum_integer(hss & histogram_setup_status::has_binning)) {
						// this is our binned histograms[1]
						if (ev.Y >= binningMaxY) continue; // skip it
						short binnedY = Zweistein::Binning::BINNING[ev.X][ev.Y];

						if (binnedY < 0) continue; // skip also

						point_type pb(ev.X, binnedY);
						for (auto& r : histograms[1].roidata) {
							histograms[1].histogram.at<HISTOGRAMTYPE>(pb.y(), pb.x()) += ev.Amplitude;
							if (boost::geometry::covered_by(pb, r.roi)) {
								r.count += ev.Amplitude;

							}
						}

					}
				}
					nloop++;
				}
			} while (!io_service.stopped());
		}
		catch (boost::exception & e) {
				LOG_ERROR<<boost::diagnostic_information(e ) << std::endl;
		}
		LOG_DEBUG << "populateHistograms() exiting..." << std::endl;
	}


}