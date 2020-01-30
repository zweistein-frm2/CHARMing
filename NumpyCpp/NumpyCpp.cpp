// NumpyCpp.cpp : Diese Datei enthält die Funktion "main". Hier beginnt und endet die Ausführung des Programms.
//
#include <boost/python/numpy.hpp>
#include <iostream>
#include "../charm/PacketSender/Mesytec.RandomData.hpp"
namespace p = boost::python;
namespace np = boost::python::numpy;


np::ndarray GetHistogram() {

    int X = 8;
    int Y = 128;
    p::tuple shape = p::make_tuple(X,Y);
    np::dtype dtype = np::dtype::get_builtin<float>();
    np::ndarray a = np::zeros(shape, dtype);
    int z = 0;
    for (int x = 0; x < X; x++) {
        for (int y = 0; y < Y; y++) {
            a[x, y] = z++;
        }

    }

    return a;
}

int main()
{
    Py_Initialize();
    np::initialize();
    std::cout << "Hello World!\n";
    np::ndarray hist = GetHistogram();
    
    std::cout << hist.get_shape() << std::endl;
}



