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


std::vector<Histogram> histograms = std::vector<Histogram>(1);

struct NeutronMeasurement {
    NeutronMeasurement() {
        boost::mutex::scoped_lock lock(coutGuard);
        std::cout << "NeutronMeasurement()" << std::endl;
    }
    ~NeutronMeasurement() {
        boost::mutex::scoped_lock lock(coutGuard);
        std::cout << "~NeutronMeasurement()" << std::endl;
    }

    void connect(std::string config){}
    void start(){
        boost::mutex::scoped_lock lock(coutGuard);
        std::cout << "NeutronMeasurement.start()" << std::endl;
    }
    void stop() {
        boost::mutex::scoped_lock lock(coutGuard);
        std::cout << "NeutronMeasurement.stop()" << std::endl;
    }
    void _continue() {}
  
   Histogram* getHistogram(){
       //auto a = histogram[0].get();
       {
           boost::mutex::scoped_lock lock(coutGuard);
           std::cout << "getHistogram()" << std::endl;
       }
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



