#pragma once
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include "boost/multi_array.hpp"
namespace Zweistein {

	namespace Binning {
        namespace pt = boost::property_tree;
        typedef boost::multi_array<unsigned short, 2> array_type;
        typedef array_type::index index;
        array_type BINNING;
		void GenerateSimple(unsigned short newrows, int rows,int cols) {
            BINNING.resize(boost::extents[cols][rows]);

			for (int i = 0; i < rows; i++) {
                           
                for (int j = 0; j < cols; j++) {
                    BINNING[j][i]= i * newrows / rows;
					
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
                }
                int x = 0;
                for (pt::ptree::value_type& row : iroot.get_child("binning"))
                {
                    int y = 0;
                    for (pt::ptree::value_type& cell : row.second)
                    {
                       
                        if (n != 0) BINNING[x][y] = cell.second.get_value<unsigned short>();
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
            int cols = s[0];
			int rows=s[1];
      
           
            
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
