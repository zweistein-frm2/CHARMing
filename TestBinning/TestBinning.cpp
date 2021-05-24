#include <fstream>
#include <iostream>
#include <errno.h>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include "boost/multi_array.hpp"
#include <boost/numeric/ublas/matrix.hpp>
#include <boost/exception/all.hpp>
#include "Zweistein.Binning.hpp"
#include "Zweistein.Logger.hpp"
#include "Mcpd8.enums.hpp"
#include <stdio.h>
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>

#include <boost/geometry/io/wkt/wkt.hpp>

namespace pt = boost::property_tree;

typedef boost::geometry::model::d2::point_xy<int> point_type;
typedef boost::geometry::model::polygon<point_type> polygon_type;

typedef boost::multi_array<unsigned short, 2> array_type;
array_type A;

//boost::numeric::ublas::matrix<unsigned char> M;

Entangle::severity_level Entangle::SEVERITY_THRESHOLD =Entangle::severity_level::trace;


int main()
{
    //boost::iostreams::file_descriptor_sink si(_fileno(stdout), boost::iostreams::file_descriptor_flags::never_close_handle);
    //si.write("hi", 2);
#ifdef WIN32
    Entangle::Init(_fileno(stdout)); // _fileno in Windows
#else
    Entangle::Init(fileno(stdout)); // _fileno in Windows
#endif
    LOG_ERROR << "fff" << std::endl << 1 << 2 << std::endl;
    polygon_type roi;
    {
        int height = 2;
        int width = 2;
        std::vector<point_type> coor = { {0, 0}, {0,height}, {width, height}, {width,0},{0,0} };
        for (auto& p : coor) roi.outer().push_back(p);
    }
    using namespace magic_enum::ostream_operators;
    Mcpd8::Status mstar = Mcpd8::Status::DAQ_Running;

    std::cout << mstar << std::endl;

    std::cout << boost::geometry::wkt(roi) << std::endl;
    std::string jsonpath = "C:\\temp\\testbinning2.json";
    std::string txtpath = "C:\\Users\\Public\\Documents\\CHARMing\\TestBinning\\pos_cal_lut_2016_07_13.txt";
    try {

        bool b = false;

        std::cout <<  b << std::endl;

        Zweistein::Binning::ReadTxt(txtpath);
        //Zweistein::Binning::ReadJson(jsonpath);
    }
    catch (boost::exception& e) {
        std::cout << boost::diagnostic_information(e);
    }

    array_type::element* itr =(unsigned short *) Zweistein::Binning::BINNING.data();
    auto s = Zweistein::Binning::BINNING.shape();
    unsigned int max_value = 0;
    for (int i = 0; i < s[0] * s[1]; i++) {
        if (*itr > max_value) max_value = *itr;
        itr++;
    }


    Zweistein::Binning::GenerateSimple(3, 1024, 128);
    Zweistein::Binning::Write(jsonpath);

    pt::ptree iroot;



    int width = 2;
    int height = 5;

    // M = boost::numeric::ublas::matrix<unsigned char>(width, height);
    A.resize(boost::extents[width][height]);
    pt::ptree oroot;
    pt::ptree matrix_node;
    for (int i = 0; i < width; i++)
    {
        pt::ptree row;
        for (int j = 0; j < height; j++)
        {
            // Create an unnamed value
            pt::ptree cell;
            //  cell.put_value(M(i,j));
              row.push_back(std::make_pair("", cell));
        }

        matrix_node.push_back(std::make_pair("", row));
    }
    // Add the node to the root
    oroot.add_child("binning", matrix_node);
    pt::write_json(std::cout, oroot);

}

