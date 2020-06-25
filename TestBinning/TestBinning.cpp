#include <fstream>
#include <iostream>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include "boost/multi_array.hpp"
#include <boost/numeric/ublas/matrix.hpp>
#include <boost/exception/all.hpp>
#include "Zweistein.Binning.hpp"
#include "Zweistein.Logger.hpp"

#include <stdio.h>
namespace pt = boost::property_tree;


typedef boost::multi_array<unsigned short, 2> array_type;
typedef array_type::index index;
array_type A;

//boost::numeric::ublas::matrix<unsigned char> M;

Entangle::severity_level Entangle::SEVERITY_THRESHOLD =Entangle::severity_level::trace;


int main()
{
    //boost::iostreams::file_descriptor_sink si(_fileno(stdout), boost::iostreams::file_descriptor_flags::never_close_handle);
    //si.write("hi", 2);
    Entangle::Init(_fileno(stdout));
    LOG_ERROR << "fff" << std::endl << 1 << 2 << std::endl;

    std::string jsonpath = "C:\\temp\\testbinning2.json";
    std::string txtpath = "C:\\users\\alanghof\\source\\repos\\charming\\TestBinning\\pos_cal_lut_2016_07_13.txt";
    try {
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

