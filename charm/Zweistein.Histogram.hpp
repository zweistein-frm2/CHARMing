/***************************************************************************
 *   Copyright (C) 2019 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/
#pragma once
#ifdef BOOST_PYTHON_MODULE
#include <boost/python.hpp>
#include <boost/python/tuple.hpp>
#include <boost/python/numpy.hpp>
namespace p = boost::python;
namespace np = boost::python::numpy;


#endif

#include <iostream>
#include <list>
#include <vector>
#include <opencv2/core.hpp>
#include <opencv2/core/mat.hpp>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>

#include <boost/geometry/io/wkt/wkt.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string.hpp>
#include <magic_enum.hpp>
#include "Zweistein.Locks.hpp"
#include "Zweistein.Logger.hpp"
#include "Zweistein.Binning.hpp"


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
        Histogram():roidata(std::vector<RoiData>()) {
                RoiData rd;
                roidata.push_back(rd);
                resize(1, 1);
                std::string wkt=WKTRoiRect(0,0,1,1);
                _setRoi(wkt, 0);
        }
        void resize(int rows, int cols) {

            histogram = cv::Mat_<int32_t>::zeros(rows,cols);
           
        }

        
        void _delRoi(std::string roi) {
            LOG_INFO << "_delRoi(" << roi<<")" << std::endl;
            for (int i = 0;i< roidata.size();i++){
                if (roi == _getRoi(i)) {
                    roidata.erase(roidata.begin()+i);
                    break;
                }
            }
            // we never delete first roi
            if (!roidata.size()) setRoi("", 0);
        }
        std::string getRoi(int index) {
            int rsize = 0;
            {
                Zweistein::ReadLock r_lock(histogramsLock);
                rsize = (int) roidata.size();
            }

            //LOG_INFO << "getRoi(" << index << ")" << std::endl;
          
            if (rsize == index) {
                LOG_DEBUG << "roidata.size()="<<rsize <<", index does not exist, create new : "  << std::endl;
                LOG_DEBUG << "roidata.push_back(rd), old size:" << rsize << std::endl;
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
            LOG_INFO << "setRoi(" << wkt << ", " << index << ")" << std::endl;
            if (roidata.size() - 1 < index) {
                LOG_ERROR << "index out of range, max=" << roidata.size() - 1 << std::endl;
                return;
            }
           
            if (wkt.empty() && index!=0){
                     std::string currwkt = _getRoi(index);
                    _delRoi(currwkt);
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
        boost::python::tuple update(cv::Mat mat) {
            using namespace magic_enum::bitwise_operators; // out-of-the-box bitwise operators for enums.
            LOG_INFO << __FILE__ << " : " << __LINE__ << std::endl;
            std::vector<RoiData> rdc = std::vector<RoiData>();
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
                width=histogram.cols;
                height = histogram.rows;
            }
            return boost::python::make_tuple(width,height);
                     
           
        }
#endif
    };
        extern std::vector<Histogram> histograms;