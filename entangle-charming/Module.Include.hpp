#define BOOST_NUMPY_STATIC_LIB
#define BOOST_PYTHON_STATIC_LIB
#ifndef BOOST_BIND_GLOBAL_PLACEHOLDERS
#define BOOST_BIND_GLOBAL_PLACEHOLDERS
#endif

#include <Python.h>
#include <boost/python.hpp>
#include <boost/python/tuple.hpp>
#include <pyboostcvconverter/pyboostcvconverter.hpp>
#include <boost/python/numpy.hpp>
#include <stdio.h>
#include <iostream>
#include <list>
#include <boost/filesystem.hpp>
#include "Zweistein.PrettyBytes.hpp"

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/geometry/io/wkt/wkt.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include "Zweistein.Histogram.hpp"
#include "Zweistein.HomePath.hpp"
#include "Zweistein.GetConfigDir.hpp"
#include "Zweistein.ThreadPriority.hpp"
#include "Zweistein.populateHistograms.hpp"
#include "version.h"
