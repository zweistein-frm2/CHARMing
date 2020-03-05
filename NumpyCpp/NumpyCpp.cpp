// NumpyCpp.cpp : Diese Datei enthält die Funktion "main". Hier beginnt und endet die Ausführung des Programms.
//
#define PY_ARRAY_UNIQUE_SYMBOL pbcvt_ARRAY_API
#include <boost/python.hpp>
#include <boost/python/tuple.hpp>
#include <pyboostcvconverter/pyboostcvconverter.hpp>
#include <boost/python/numpy.hpp>
#include <iostream>
#include "Mesytec.RandomData.hpp"
#include <opencv2/core.hpp>
#include <opencv2/core/mat.hpp>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>

#include <boost/geometry/io/wkt/wkt.hpp>
#include "../charm/Zweistein.Histogram.hpp"


EXTERN_FUNCDECLTYPE boost::mutex coutGuard;
EXTERN_FUNCDECLTYPE boost::thread_group worker_threads;

namespace p = boost::python;
namespace np = boost::python::numpy;

typedef boost::geometry::model::d2::point_xy<int> point_type;
typedef boost::geometry::model::polygon<point_type> polygon_type;



np::ndarray GetHistogramNumpy() {

    int maxX = 3;
    int maxY = 40;
    p::tuple shape = p::make_tuple(maxY, maxX);
    np::dtype dtype = np::dtype::get_builtin<float>();
    np::ndarray histogram = np::zeros(shape, dtype);
    int z = 0;

    unsigned  short x_pos;
    unsigned short position_y;
    long imax = 10 * maxX * maxY;
    for (int i = 0; i < imax; i++) {
        unsigned long l = Zweistein::Random::RandomData(x_pos, position_y, i, maxY, maxX);
        histogram[x_pos, maxY - position_y - 1] += 1;
    }

    return histogram;
}
/*
void GetHistogramOpenCV(cv::Mat &histogram) {
   
       
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
    polygon_type roi;
    boost::geometry::model::box<point_type> box;
    long long count;
    cv::Mat histogram;
    Histogram() {
        std::string wkt = "";
        setRoi(wkt);
    }
    void setRoi(std::string &wkt) {
        boost::geometry::read_wkt(
            "POLYGON((0 0,64 0,64 1024, 0 0),())", roi);
     
        boost::geometry::envelope(roi, box);
        int width=box.max_corner().get<0>() - box.min_corner().get<0>();
        int height = box.max_corner().get<1>() - box.min_corner().get<1>();
        histogram = cv::Mat_<int32_t>::zeros(height,width);
        std::cout << "setRoi:" << width << "," << height << std::endl;
        
    }
    boost::python::tuple get() {

        GetHistogramOpenCV(histogram);
        return boost::python::make_tuple(count, &histogram);
    }
};

*/

std::vector<Histogram> histograms = std::vector<Histogram>(1);

struct NeutronMeasurement {
    NeutronMeasurement() {
     
    }
    ~NeutronMeasurement() {
        std::cout << "~NeutronMeasurement()" << std::endl;
    }

    void connect(std::string config){}
    void start(){}
    void stop() {}
    void _continue() {}
  
   Histogram* getHistogram(){
       //auto a = histogram[0].get();
            return &histograms[0];
    }

private:
    long long MaxCount;
    long long DurationNanoSeconds;
    
    

};



#if (PY_VERSION_HEX >= 0x03000000)

static void* init_ar() {
#else
static void init_ar() {
#endif
    Py_Initialize();

    import_array();
    return NUMPY_IMPORT_ARRAY_RETVAL;
}



BOOST_PYTHON_MODULE(numpycpp)
{
    init_ar();
    using namespace boost::python;
    np::initialize();

      to_python_converter<cv::Mat,
        pbcvt::matToNDArrayBoostConverter>();
    pbcvt::matFromNDArrayBoostConverter();
    class_<Histogram, boost::noncopyable>("Histogram", boost::python::no_init)
        .def("update", &Histogram::update)
        .add_property("Roi", &Histogram::getRoi, &Histogram::setRoi)
        .add_property("Size", &Histogram::getSize)
        ;
    class_< NeutronMeasurement>("NeutronMeasurement")
        .def("connect",&NeutronMeasurement::connect)
        .def("start", &NeutronMeasurement::start)
        .def("stop", &NeutronMeasurement::stop)
        .def("getHistogram", &NeutronMeasurement::getHistogram, return_internal_reference<1, with_custodian_and_ward_postcall<1,0>>())
        ;
    def("GetHistogramNumpy", GetHistogramNumpy);
   
}



