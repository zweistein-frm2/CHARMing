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
#include "Zweistein.Locks.hpp"
#include "Zweistein.Logger.hpp"
#include "Zweistein.Binning.hpp"


typedef boost::geometry::model::d2::point_xy<int> point_type;
typedef boost::geometry::model::polygon<point_type> polygon_type;


extern boost::mutex histogramsGuard;

#ifdef BOOST_PYTHON_MODULE
#include "Zweistein.Binning.ApplyOcc.hpp"
#endif

    struct RoiData {
        polygon_type roi;
        long count;
        boost::geometry::model::box<point_type> box;
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
                setRoiRect(0,0,1,1);
        }
        void resize(int rows, int cols) {

            histogram = cv::Mat_<int32_t>::zeros(rows,cols);
           
        }

        void delRoi(std::string roi) {
            // we never delete first roi
            for (int i = 0;i< roidata.size();i++){
                if (roi == getRoi(i)) {
                    roidata.erase(roidata.begin()+i);
                    break;
                }
            }
            if (!roidata.size()) setRoi("", 0);
        }
        std::string getRoi(int index) {

            std::stringstream ss_wkt;
            ss_wkt << boost::geometry::wkt(roidata[index].roi);
            return ss_wkt.str();
        }
        void setRoiRect(int left, int bottom, int maxX, int maxY) {
            std::stringstream ssroi;
            ssroi << ("POLYGON((") << left << " " << bottom << ",";
            ssroi << left << " " << maxY << ",";
            ssroi << maxX << " " << maxY << ",";
            ssroi << maxX << " " << bottom << ",";
            ssroi << left << " " << bottom << "),())";
            std::string roi = ssroi.str();

            setRoi(roi,0);
        }
        void setRoi(std::string wkt,int index) {
            int width = 1;
            int height = 1; 
            while (roidata.size() <= index) {
                RoiData rd;
                roidata.push_back(rd);
             }
            //LOG_INFO << std::endl << "setRoi(" << wkt << ")" << std::endl;
            boost::algorithm::replace_last(wkt,",())",")");
            std::string reason;
            boost::geometry::validity_failure_type failure= boost::geometry::no_failure;
            try {
                if (!wkt.empty()) {
                    boost::geometry::read_wkt(wkt, roidata[index].roi);
                    boost::geometry::validity_failure_type failure;
                    bool valid = boost::geometry::is_valid(roidata[index].roi, failure);
                    bool could_be_fixed = (failure == boost::geometry::failure_not_closed
                        || boost::geometry::failure_wrong_orientation);
                    if(could_be_fixed)boost::geometry::correct(roidata[index].roi);
                    valid = boost::geometry::is_valid(roidata[index].roi, reason);
                   
                
                }
            }
            catch (std::exception& e) {
                LOG_ERROR << e.what() << std::endl;
                wkt = "";
            }

            
            if (failure!= boost::geometry::no_failure || wkt.empty()) {
                
                if (failure != boost::geometry::no_failure) {
                       LOG_ERROR << "boost::geometry::validity_failure_type=" << failure << " : "<< reason<< std::endl;
                }
               
                
                height =histogram.size[0];
                width=histogram.size[1];

                std::vector<point_type> coor = { {0, 0}, {0,height}, {width, height}, {width,0},{0,0} };
                roidata[index].roi.outer().clear();
                roidata[index].roi.inners().clear();
                for (auto& p : coor) roidata[index].roi.outer().push_back(p);
            }
           // LOG_INFO << "new Roi : " << getRoi() << std::endl;

            boost::geometry::envelope(roidata[index].roi,roidata[index].box);
            int roiwidth = roidata[index].box.max_corner().get<0>() - roidata[index].box.min_corner().get<0>();
            int roiheight = roidata[index].box.max_corner().get<1>() - roidata[index].box.min_corner().get<1>();

           // Zweistein::Config::AddRoi(getRoi());
           
           

        }
#ifdef BOOST_PYTHON_MODULE
        boost::python::tuple update(cv::Mat mat) {
           
            boost::mutex::scoped_lock lock(histogramsGuard);
            histogram.copyTo(mat);
           if (Zweistein::Binning::loaded) {
           //  Zweistein::Binning::Apply_OCC_Correction(mat,roi,count);
            //    LOG_INFO << "update:applyocc_correction" << std::endl;
           }
           
           boost::python::list l;

           for (auto r : roidata) {
               std::stringstream ss_wkt;
               ss_wkt << boost::geometry::wkt(r.roi);
               auto t=boost::python::make_tuple(ss_wkt.str(),r.count);
               l.append(t);
           }
            return boost::python::make_tuple(l, mat);
        }

        boost::python::tuple getSize() {
           
            
            int width = roidata[0].box.max_corner().get<0>() - roidata[0].box.min_corner().get<0>();
            int height = roidata[0].box.max_corner().get<1>() - roidata[0].box.min_corner().get<1>();
            {
                boost::mutex::scoped_lock lock(histogramsGuard);
                width=histogram.cols;
                height = histogram.rows;
            }
            return boost::python::make_tuple(width,height);
                     
           
        }
#endif
    };
        extern std::vector<Histogram> histograms;