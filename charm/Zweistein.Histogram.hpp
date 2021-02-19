/*                          _              _                _
    ___  __ __ __  ___     (_)     ___    | |_     ___     (_)    _ _
   |_ /  \ V  V / / -_)    | |    (_-<    |  _|   / -_)    | |   | ' \
  _/__|   \_/\_/  \___|   _|_|_   /__/_   _\__|   \___|   _|_|_  |_||_|
       .
       |\       Copyright (C) 2019 - 2020 by Andreas Langhoff
     _/]_\_                            <andreas.langhoff@frm2.tum.de>
 ~~~"~~~~~^~~   This program is free software;
 original site: https://github.com/zweistein-frm2/CHARMing
 you can redistribute it and/or modify it under the terms of the GNU
 General Public License as published by the Free Software Foundation;*/

#pragma once
#ifdef BOOST_PYTHON_MODULE
#include <boost/python.hpp>
#include <boost/python/tuple.hpp>
#include <boost/python/numpy.hpp>
namespace p = boost::python;
namespace np = boost::python::numpy;
#endif
#define HISTOGRAMTYPE float  // or  int32_t


#include <iostream>
#include <list>
#include <vector>
#include <opencv2/core/core.hpp>
#include <opencv2/core/mat.hpp>
#include <opencv2/imgproc.hpp>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>

#include <boost/geometry/io/wkt/wkt.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string.hpp>
#include "magic_enum/include/magic_enum.hpp"
#include "Zweistein.Locks.hpp"
#include "Zweistein.Logger.hpp"
#include "Zweistein.Binning.hpp"
#include "CounterMonitor.hpp"

typedef boost::geometry::model::d2::point_xy<int> point_type;
typedef boost::geometry::model::polygon<point_type> polygon_type;



extern Zweistein::Lock histogramsLock;

#ifdef BOOST_PYTHON_MODULE
#include "Zweistein.Binning.ApplyOcc.hpp"
#endif

namespace Zweistein {

    enum class histogram_setup_status {
        not_done = 0x0,
        done = 0x1,
        histogram0_resized = 0x2,
        histogram1_resized = 0x4,
        has_binning = 0x8
    };

    boost::atomic<histogram_setup_status> setup_status = histogram_setup_status::not_done;

}

    struct RoiData {
        polygon_type roi;
        unsigned long count;
        boost::geometry::model::box<point_type> box;
        RoiData():count(0){}
    };

    //https://www.boost.org/doc/libs/1_72_0/libs/geometry/doc/html/geometry/reference/algorithms/within/within_2.html
    // boost::geometry::within(p, poly)
    //https://www.boost.org/doc/libs/1_72_0/libs/geometry/doc/html/geometry/reference/algorithms/envelope/envelope_2.html
    struct Histogram {
        std::vector<RoiData> roidata;
        cv::Mat histogram;
        bool nextRAW;
        Histogram():roidata(std::vector<RoiData>()) {
                RoiData rd;
                roidata.push_back(rd);
                resize(1, 1);
                std::string wkt=WKTRoiRect(0,0,4,4);
                _setRoi(wkt, 0);
                nextRAW = false; // used for charm only and RAW histograms
        }
        void resize(int rows, int cols) {

            histogram = cv::Mat_<HISTOGRAMTYPE>::zeros(rows,cols);

        }

        void clear() {
            LOG_INFO << "Histogram::clear()" << std::endl;
            cv::Size dsize = histogram.size();
            resize(dsize.height, dsize.width);
            for (auto& rd : roidata) {
                rd.count = 0;
            }
        }


        std::string getRoi(int index) {
            int rsize = 0;
            {
                Zweistein::ReadLock r_lock(histogramsLock);
                rsize = (int) roidata.size();
            }

            //LOG_INFO << "getRoi(" << index << ")" << std::endl;

            if (rsize == index) {
                LOG_INFO << "roidata.size()="<<rsize <<", index does not exist, create new : "  << std::endl;
                LOG_INFO << "roidata.push_back(rd), old size:" << rsize << std::endl;
                {
                    Zweistein::WriteLock w_lock(histogramsLock);
                    RoiData rd;
                    roidata.push_back(rd);
                    std::string wkt = WKTRoiRect(0, 0, histogram.size[1], histogram.size[0]);
                    _setRoi(wkt, index);
                    return wkt;
                }


            }
            if (rsize - 1 < index) return "";
            std::string tmp;
            {
                Zweistein::ReadLock r_lock(histogramsLock);
                tmp= _getRoi(index);
            }
           // LOG_INFO << tmp << std::endl;
            return tmp;
        }

        std::string _getRoi(int index) {
            try {
                std::stringstream ss_wkt;
                ss_wkt << boost::geometry::wkt(roidata[index].roi);
                return ss_wkt.str();
            }
            catch (std::exception& e) {
                LOG_ERROR << e.what() << std::endl;
                return "";
            }
    }

        const double shrinkraw = 0.25;
        std::string polygon_scaled(std::string wkt, double scalefactor) {
            std::string wktout;

            std::string reason;
            boost::geometry::validity_failure_type failure = boost::geometry::no_failure;
            bool valid = false;
            try {
                if (!wkt.empty()) {
                    polygon_type roi;
                    boost::geometry::read_wkt(wkt, roi);
                    valid = boost::geometry::is_valid(roi, failure);
                    bool could_be_fixed = (failure == boost::geometry::failure_not_closed
                        || failure == boost::geometry::failure_wrong_orientation);

                    if (could_be_fixed)boost::geometry::correct(roi);
                    valid = boost::geometry::is_valid(roi, reason);


                    for (point_type& p : roi.outer()) {

                        p.set<0>((int)(p.x() * scalefactor));
                        p.set<1>((int)(p.y() * scalefactor));

                    }

                    std::stringstream ss_wkt;
                    ss_wkt << boost::geometry::wkt(roi);
                    wktout = ss_wkt.str();
                }
            }
            catch (std::exception& e) {
                LOG_ERROR << e.what() << std::endl;
                wktout = "";
            }
            if (!valid || wkt.empty()) {

                if (failure != boost::geometry::no_failure) {
                    LOG_ERROR << "boost::geometry::validity_failure_type=" << failure << " : " << reason << std::endl;
                }
            }
            return wktout;
        }

        void charm_setRoi(std::string wkt, int index) {

            if (nextRAW) {
                nextRAW = false;
                return setRoi(wkt, index);
            }
            //LOG_INFO << "charm_setRoi(" << wkt << ", " << index << ")" << std::endl;
            boost::algorithm::replace_last(wkt, ",())", ")");


            std::string wktout = polygon_scaled(wkt,1.0/shrinkraw);

            return setRoi(wktout, index);




        }

        std::string charm_getRoi(int index) {

            if (nextRAW) {
                nextRAW = false;
                return getRoi(index);
            }
            int rsize = 0;
            {
                Zweistein::ReadLock r_lock(histogramsLock);
                rsize = (int)roidata.size();
            }

            //LOG_INFO << "charm_getRoi(" << index << ")" << std::endl;

            if (rsize == index) {
                LOG_INFO << "roidata.size()=" << rsize << ", index does not exist, create new : " << std::endl;
                LOG_INFO << "roidata.push_back(rd), old size:" << rsize << std::endl;
                {
                    Zweistein::WriteLock w_lock(histogramsLock);
                    RoiData rd;
                    roidata.push_back(rd);
                    std::string wkt = WKTRoiRect(0, 0, histogram.size[1], histogram.size[0]);
                    _setRoi(wkt, index);

                    std::string wktout = polygon_scaled(wkt, shrinkraw);

                    return wktout;
                }


            }
            if (rsize - 1 < index) return "";
            std::string tmp;
            {
                Zweistein::ReadLock r_lock(histogramsLock);
                tmp = _getRoi(index);
                tmp = polygon_scaled(tmp, shrinkraw);
            }
            // LOG_INFO << tmp << std::endl;
            return tmp;
        }

    std::string WKTRoiRect(int left, int bottom, int maxX, int maxY) {
            std::stringstream ssroi;
            ssroi << ("POLYGON((") << left << " " << bottom << ",";
            ssroi << left << " " << maxY << ",";
            ssroi << maxX << " " << maxY << ",";
            ssroi << maxX << " " << bottom << ",";
            ssroi << left << " " << bottom << "),())";
            std::string roi = ssroi.str();
            //LOG_INFO << __FILE__ << " : " << __LINE__ << " "<<roi << std::endl;
            return roi;

        }

        void setRoi(std::string wkt, int index) {

            Zweistein::WriteLock w_lock(histogramsLock);
            //LOG_INFO << "setRoi(" << wkt << ", " << index << ")" << std::endl;
            if (roidata.size() - 1 < index) {
                LOG_ERROR << "index out of range, max=" << roidata.size() - 1 << std::endl;
                return;
            }

            if (wkt.empty() && index!=0){
                     LOG_INFO << "roidata.erase(" << index << ")" << std::endl;
                     roidata.erase(roidata.begin() + index);
                     LOG_INFO << "roidata.size()=" << roidata.size() << std::endl;
                     return;
            }
            if(index!=0) _setRoi(wkt,index); // index 0 is always full rectangle
        }

        void _setRoi(std::string wkt,int index) {

            //LOG_INFO << "_setRoi(" << wkt <<", "<<index <<")" << std::endl;
            int width = 1;
            int height = 1;

            boost::algorithm::replace_last(wkt,",())",")");
            std::string reason;
            boost::geometry::validity_failure_type failure= boost::geometry::no_failure;
            bool valid = false;
            try {
                if (!wkt.empty()) {
                    boost::geometry::read_wkt(wkt, roidata[index].roi);
                    valid = boost::geometry::is_valid(roidata[index].roi, failure);
                    bool could_be_fixed = (failure == boost::geometry::failure_not_closed
                        || failure == boost::geometry::failure_wrong_orientation);

                    if(could_be_fixed)boost::geometry::correct(roidata[index].roi);
                    valid = boost::geometry::is_valid(roidata[index].roi, reason);


                }
            }
            catch (std::exception& e) {
                LOG_ERROR << e.what() << std::endl;
                wkt = "";
            }
            if (!valid || wkt.empty()) {

                if (failure != boost::geometry::no_failure) {
                       LOG_ERROR << "boost::geometry::validity_failure_type=" << failure << " : "<< reason<< std::endl;
                }
                height = histogram.size[0];
                width = histogram.size[1];

                std::vector<point_type> coor = { {0, 0}, {0,height}, {width, height}, {width,0},{0,0} };
                roidata[index].roi.outer().clear();
                roidata[index].roi.inners().clear();
                for (auto& p : coor) roidata[index].roi.outer().push_back(p);
            }
            boost::geometry::envelope(roidata[index].roi,roidata[index].box);
            int roiboxwidth = roidata[index].box.max_corner().get<0>() - roidata[index].box.min_corner().get<0>();
            int roiboxheight = roidata[index].box.max_corner().get<1>() - roidata[index].box.min_corner().get<1>();

        }
#ifdef BOOST_PYTHON_MODULE

        boost::python::list getRoiData(void) {
            std::vector<RoiData> rdc = std::vector<RoiData>(); // rdc = roi data current
            {
                Zweistein::ReadLock r_lock(histogramsLock);
                for (auto& a : roidata)   rdc.push_back(a);
            }
            boost::python::list l;
            for (auto& r : rdc) {
                std::stringstream ss_wkt;
                ss_wkt << boost::geometry::wkt(r.roi);
                auto t = boost::python::make_tuple(ss_wkt.str(), r.count);
                l.append(t);
            }
            return l;
        }

        boost::python::tuple update(cv::Mat mat) {
            using namespace magic_enum::bitwise_operators; // out-of-the-box bitwise operators for enums.
            std::vector<RoiData> rdc = std::vector<RoiData>(); // rdc = roi data current
            {
                Zweistein::ReadLock r_lock(histogramsLock);
                for (auto& a : roidata)   rdc.push_back(a);
                histogram.copyTo(mat);
            }
            Zweistein::histogram_setup_status hss = Zweistein::setup_status;
            if (magic_enum::enum_integer(hss & Zweistein::histogram_setup_status::has_binning)) {
                Zweistein::Binning::Apply_OCC_Correction(mat, rdc[0].roi, rdc[0].count);
            }


           boost::python::list l;

           for (auto & r : rdc) {
               std::stringstream ss_wkt;
               ss_wkt << boost::geometry::wkt(r.roi);
               auto t=boost::python::make_tuple(ss_wkt.str(),r.count);
               l.append(t);
           }
           return boost::python::make_tuple(l, mat);
        }

        boost::python::tuple getSize() {


            int width = 0;// = roidata[0].box.max_corner().get<0>() - roidata[0].box.min_corner().get<0>();
            int height = 0;// roidata[0].box.max_corner().get<1>() - roidata[0].box.min_corner().get<1>();
            {

                Zweistein::ReadLock r_lock(histogramsLock);
                width = histogram.cols;
                height = histogram.rows;
            }
            return boost::python::make_tuple(width, height);


        }

        cv::Mat raw_shrinked;
        cv::Mat shrinked_int32;



        bool get_nextRAW() {
            return nextRAW;
        }

        void set_nextRAW(bool value) {
            nextRAW = value;
        }
        boost::python::tuple charm_update(cv::Mat mat) {

            if (nextRAW) {
                nextRAW = false;
                return update(mat);
            }
            //LOG_INFO << "charm_update" << std::endl;
            using namespace magic_enum::bitwise_operators; // out-of-the-box bitwise operators for enums.
            std::vector<RoiData> rdc = std::vector<RoiData>(); // rdc = roi data current
            {
                Zweistein::ReadLock r_lock(histogramsLock);
                for (auto& a : roidata)   rdc.push_back(a);
                {
                    cv::Size dsize = histogram.size();
                    dsize.height = (int) ( dsize.height * shrinkraw);
                    dsize.width = (int) (dsize.width * shrinkraw);
                    cv::resize(histogram, raw_shrinked, dsize,0,0, cv::INTER_AREA);
                    if(shrinked_int32.cols<2) shrinked_int32 = cv::Mat_<int32_t>::zeros(4, 4);
                    shrinked_int32.resize((int)(histogram.size[0] * shrinkraw), (int)(histogram.size[1] * shrinkraw));


                    double minVal, maxVal;
                    cv::Point minLoc, maxLoc;
                    cv::minMaxLoc(histogram, &minVal, &maxVal, &minLoc, &maxLoc);


                    double minVals, maxVals;
                    cv::Point minLocs, maxLocs;
                    cv::minMaxLoc(raw_shrinked, &minVals, &maxVals, &minLocs, &maxLocs);

                    LOG_INFO<<std::endl << "histogram " << histogram.size << " min:" << minVal << "(" << minLoc << ")" << " ,max:" << maxVal << "(" << maxLoc << ")" << std::endl
                          << "raw_shrinked " << raw_shrinked.size << " min:" << minVals << "(" << minLocs << ")" << " ,max:" << maxVals << "(" << maxLocs << ")"
                        << std::endl;

                    raw_shrinked.convertTo(shrinked_int32, CV_32S);


                    shrinked_int32.copyTo(mat);
                }
            }


            boost::python::list l;

            for (auto& r : rdc) {
                std::stringstream ss_wkt;
                ss_wkt << boost::geometry::wkt(r.roi);
                std::string wktout = polygon_scaled(ss_wkt.str(),shrinkraw);
                auto t = boost::python::make_tuple(wktout,(unsigned long) (r.count*shrinkraw*shrinkraw));  // counts are relative to scaled image
                l.append(t);
            }

            return boost::python::make_tuple(l, mat);
        }




        boost::python::tuple charm_getSize() {

            if (nextRAW) {
                nextRAW = false;
                return getSize();
            }
            int width = 0;// = roidata[0].box.max_corner().get<0>() - roidata[0].box.min_corner().get<0>();
            int height = 0;// roidata[0].box.max_corner().get<1>() - roidata[0].box.min_corner().get<1>();
            {

                Zweistein::ReadLock r_lock(histogramsLock);
                width = (int) ( histogram.cols * shrinkraw);
                height = (int) (histogram.rows * shrinkraw);
            }
            return boost::python::make_tuple(width, height);


        }



        boost::python::list charm_getRoiData(void) {
            std::vector<RoiData> rdc = std::vector<RoiData>(); // rdc = roi data current
            {
                Zweistein::ReadLock r_lock(histogramsLock);
                for (auto& a : roidata)   rdc.push_back(a);
            }
            boost::python::list l;
            for (auto& r : rdc) {
                std::stringstream ss_wkt;
                ss_wkt << boost::geometry::wkt(r.roi);
                std::string tmp = polygon_scaled(ss_wkt.str(), shrinkraw);
                auto t = boost::python::make_tuple(tmp, (unsigned long) (r.count *shrinkraw*shrinkraw));
                l.append(t);
            }
            return l;
        }
#endif
    };
        extern std::vector<Histogram> histograms;
