// NumpyCpp.cpp : Diese Datei enthält die Funktion "main". Hier beginnt und endet die Ausführung des Programms.
//
#define PY_ARRAY_UNIQUE_SYMBOL pbcvt_ARRAY_API
#include <boost/python.hpp>
#include <pyboostcvconverter/pyboostcvconverter.hpp>
#include <boost/python/numpy.hpp>
#include <iostream>
#include "../charm/PacketSender/Mesytec.RandomData.hpp"
#include <opencv2/core.hpp>
#include <opencv2/core/mat.hpp>
namespace p = boost::python;
namespace np = boost::python::numpy;


np::ndarray GetHistogram() {

    int maxX = 3;
    int maxY = 4;
    p::tuple shape = p::make_tuple(maxY,maxX);
    np::dtype dtype = np::dtype::get_builtin<float>();
    np::ndarray a = np::zeros(shape, dtype);
    int z = 0;

    unsigned  short x_pos;
    unsigned short position_y;
    for (int i = 0; i < 1000; i++) {
        unsigned long l = Zweistein::Random::RandomData(x_pos, position_y, i, maxY, maxX);
        a[x_pos,position_y] += 1;
    }
            
    return a;
}

cv::Mat GetHistogram_cv() {
    int maxX = 64;
    int maxY = 1024;
    cv::Mat_<float> histogram;
    histogram = cv::Mat_<float>::zeros(maxY,maxX);

    unsigned  short x_pos;
    unsigned short position_y;
    long imax = maxX * maxY;
    for (int i = 0; i < imax; i++) {
        unsigned long l = Zweistein::Random::RandomData(x_pos, position_y, i, maxY, maxX);
        histogram.at<float>(position_y, x_pos) += 1.0;
    }
    
   
    return histogram;

}

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
    def("GetHistogram", GetHistogram);
    def("GetHistogram_cv", GetHistogram_cv);
}



