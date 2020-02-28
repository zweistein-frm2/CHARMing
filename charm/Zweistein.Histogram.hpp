#pragma once
#ifdef BOOST_PYTHON_MODULE
#include <boost/python.hpp>
#include <boost/python/tuple.hpp>
#include <boost/python/numpy.hpp>
namespace p = boost::python;
namespace np = boost::python::numpy;

template <typename T>
boost::python::object transfer_to_python(T* t)
{
    // Transfer ownership to a smart pointer, allowing for proper cleanup incase Boost.Python throws.
    std::unique_ptr<T> ptr(t);
    // Use the manage_new_object generator to transfer ownership to Python.
    typename p::manage_new_object::apply<T*>::type converter;
    // Transfer ownership to the Python handler and release ownership from C++.
    p::handle<> handle(converter(*ptr));
    ptr.release();
    return p::object(handle);
}


#endif

#include <iostream>
#include "PacketSender/Mesytec.RandomData.hpp"
#include <opencv2/core.hpp>
#include <opencv2/core/mat.hpp>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>

#include <boost/geometry/io/wkt/wkt.hpp>

#include "Zweistein.Locks.hpp"



typedef boost::geometry::model::d2::point_xy<int> point_type;
typedef boost::geometry::model::polygon<point_type> polygon_type;


    void GetHistogramOpenCV(cv::Mat& histogram) {


        unsigned  short x_pos;
        unsigned short position_y;
        long imax = 10 * histogram.cols * histogram.rows;
        for (int i = 0; i < imax; i++) {
            unsigned long l = Zweistein::Random::RandomData(x_pos, position_y, i, histogram.rows, histogram.cols);
            histogram.at<int32_t>(histogram.rows - position_y - 1, x_pos) += 1;
        }




    }

    //https://www.boost.org/doc/libs/1_72_0/libs/geometry/doc/html/geometry/reference/algorithms/within/within_2.html
    // boost::geometry::within(p, poly)
    //https://www.boost.org/doc/libs/1_72_0/libs/geometry/doc/html/geometry/reference/algorithms/envelope/envelope_2.html
    struct Histogram {
        Zweistein::Lock lock;
        polygon_type roi;
        boost::geometry::model::box<point_type> box;
        long long count;
        cv::Mat histogram;
        Histogram() {
            std::stringstream ssroi;
            int left = 0;
            int bottom = 0;
            int maxX = 1;
            int maxY = 1;

         
            ssroi << ("POLYGON((") << left << " " << bottom << ",";
            ssroi << left << " " << maxY << ",";
            ssroi << maxX << " " << maxY << ",";
            ssroi << maxX << " " << bottom << ",";
            ssroi << left << " " << bottom << "),())";
            std::string roi = ssroi.str();

            setRoi(roi);
        }
      //  Histogram(const Histogram& origin) {
 //       }
        void setRoi(std::string& wkt) {
            Zweistein::WriteLock w_lock(lock); // we fill only first histogram
            int width = 1;
            int height = 1;
            bool illformedwkt = true;
            try {
                if (!wkt.empty()) {
                    boost::geometry::read_wkt(wkt, roi);
                    illformedwkt = false;
                }
            }
            catch (boost::exception& e) {
                boost::mutex::scoped_lock lock(coutGuard);
                std::cout << boost::diagnostic_information(e);
            }

            if (illformedwkt) {
                std::vector<point_type> coor = { {0, 0}, {0,height}, {width, height}, {width,0},{0,0} };
                roi.outer().clear();
                for (auto& p : coor) roi.outer().push_back(p);
            }

            boost::geometry::envelope(roi, box);
            width = box.max_corner().get<0>() - box.min_corner().get<0>();
            height = box.max_corner().get<1>() - box.min_corner().get<1>();

            histogram = cv::Mat_<int32_t>::zeros(height, width);
            std::cout << "setRoi:" << width << "," << height << std::endl;

        }
#ifdef BOOST_PYTHON_MODULE
        boost::python::tuple get() {
            {
                Zweistein::WriteLock w_lock(lock);
                GetHistogramOpenCV(histogram);

            }
            auto x1 = std::make_unique<cv::Mat>();
            {
                Zweistein::ReadLock r_lock(lock);
                histogram.copyTo(*x1);
            }
            return boost::python::make_tuple(count, transfer_to_python(x1.release()));
        }
#endif
    };

    extern std::vector<Histogram> histograms;
