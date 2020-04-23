/***************************************************************************
 *   Copyright (C) 2020 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/
#pragma once
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/foreach.hpp>
#include <boost/multi_array.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <boost/atomic.hpp>
#include "Zweistein.Logger.hpp"
#include <boost/atomic.hpp>


namespace Zweistein {

	namespace Binning {
        namespace pt = boost::property_tree;
        typedef boost::multi_array<short, 2> array_type;
        typedef array_type::index index;
        boost::atomic<bool> loaded=false;
        array_type BINNING;
        array_type OCC;
        short occmultiplier = 10000;
        void OCC_default(int rows, int cols) {
            OCC.resize(boost::extents[cols][rows]);
            for (int i = 0; i < rows; i++) {
                for (int j = 0; j < cols; j++) {
                         OCC[j][i] = occmultiplier;
                }
            }
        }
		void GenerateSimple(unsigned short newrows, int rows,int cols) {
            BINNING.resize(boost::extents[cols][rows]);
            OCC_default(rows,cols);
            for (int i = 0; i < rows; i++) {
                for (int j = 0; j < cols; j++) {
                    BINNING[j][i]= i * newrows / rows;
				}
			}
		}
       
   
        void ReadTxt(std::string txtpath) {
                LOG_INFO << "Zweistein::Binning::ReadTxt:" << txtpath << std::endl;
                std::string firstline = "mesydaq Position Calibration File";

                int rows = 0;
                int cols = 0;

                for (int n = 0; n < 2; n++)
                {
                    std::ifstream infile(txtpath);
                    std::string line;

                    std::getline(infile, line);
                    boost::algorithm::trim_right_if(line,boost::algorithm::is_any_of("\r "));
                    if (line != firstline) {
                        LOG_ERROR << "firstline!=" << firstline << std::endl;
                        return;
                    }
                    std::getline(infile, line);   // comment line
                    std::getline(infile, line);   // bin  pix[0] occ[0]

                    boost::trim(line);

                    typedef std::vector<std::string> Tokens;
                    Tokens tokens;
                    boost::split(tokens, line, boost::is_any_of("\t "));

                    if (n == 0) {
                        tokens.erase(tokens.begin());
                        cols =(int) tokens.size() / 2;
                        int l = 0;
                        while (getline(infile, line)) rows++;
                    }

                    if (n == 1) {
                        short strideY = 0;
                        //if (rows == 960) strideY = 32;
                        OCC.resize(boost::extents[cols][rows+2*strideY]);
                        BINNING.resize(boost::extents[cols][rows+2*strideY]);
                        LOG_DEBUG <<"ReadTxt: "<< txtpath << " BINNING, OCC: cols=" << cols << ", rows=" << rows + 2 * strideY << std::endl;
                        int l = 0;
                        while (getline(infile, line)) {
                            Tokens tokens1;
                            boost::split(tokens1, line, boost::is_any_of("\t "));

                            int bin = std::stoi(tokens1[0]);
                            tokens1.erase(tokens1.begin());

                            for (int c = 0; c < cols*2; c += 2) {
                                BINNING[c/2][l+strideY] = std::stoi(tokens1[c]);
                                float f = std::stof(tokens1[c+1])* (float)Zweistein::Binning::occmultiplier;
                                short occ =(short)((long) f);
                                if (occ <= 0)     continue;
                                if ( occ< Zweistein::Binning::occmultiplier) {
                                 //   LOG_DEBUG << "f[" << c/2 << "," << l + strideY << "]=" << occ <<"("<<tokens[c+1]<<")" << std::endl;
                                }
                                OCC[c / 2][l + strideY] = occ;
                            }
                            l++;
                        }

                    }
                }

            }

        void Read(std::string jsonpath) {
            pt::ptree iroot;

            // Load the json file in this ptree
            pt::read_json(jsonpath, iroot);
            int rows = 0;
            int cols = 0;
            for (int n = 0; n < 2; n++) {
                if (n == 1) {
             
                    BINNING.resize(boost::extents[cols][rows]);
                    OCC_default(rows, cols);
                }
                int x = 0;
                for (pt::ptree::value_type& row : iroot.get_child("binning"))
                {
                    int y = 0;
                    for (pt::ptree::value_type& cell : row.second)
                    {
                       
                        if (n != 0) BINNING[x][y] = cell.second.get_value<short>();
                        y++;
                        if (n == 0) { if (y > rows) rows = y; }
                        
                    }
                    x++;
                    if (n == 0) if (x > cols) cols = x;
                   
                }
            }



        }
		void Write(std::string jsonpath) {
            auto s = BINNING.shape();
            int cols = (int) s[0];
			int rows = (int) s[1];
      
           
            
            pt::ptree oroot;
            // Add a matrix
            pt::ptree matrix_node;
            for (int i = 0; i < cols; i++)
            {
                pt::ptree row;
                for (int j = 0; j < rows; j++)
                {
                    // Create an unnamed value
                    pt::ptree cell;
                    cell.put_value(BINNING[i][j]);
                    // Add the value to our row
                    row.push_back(std::make_pair("", cell));
                }
                // Add the row to our matrix
                matrix_node.push_back(std::make_pair("tube "+std::to_string(i), row));
            }
            // Add the node to the root
            oroot.add_child("binning", matrix_node);

          //  pt::write_json(std::cout, oroot);
            pt::write_json(jsonpath, oroot);

		}
	}

}
