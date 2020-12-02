/*                          _              _                _
    ___  __ __ __  ___     (_)     ___    | |_     ___     (_)    _ _
   |_ /  \ V  V / / -_)    | |    (_-<    |  _|   / -_)    | |   | ' \
  _/__|   \_/\_/  \___|   _|_|_   /__/_   _\__|   \___|   _|_|_  |_||_|
       .
       |\       Copyright (C) 2019 - 2020 by Andreas Langhoff
     _/]_\_                            <andreas.langhoff@frm2.tum.de>
 ~~~"~~~~~^~~   This program is free software; you can redistribute it
 and/or modify it under the terms of the GNU General Public License v3
 as published by the Free Software Foundation;*/

#define PY_ARRAY_UNIQUE_SYMBOL pbcvt_ARRAY_API
#include "Module.Include.hpp"
#include "Mesytec.Mcpd8.hpp"
#include "Mesytec.config.hpp"
#include "Mesytec.listmode.write.hpp"
#include "Zweistein.Averaging.hpp"

#include "Module.Globals.hpp"


std::string PROJECT_NAME("CHARMing");
std::string CONFIG_FILE("mesytec");
namespace p = boost::python;
namespace np = boost::python::numpy;
using boost::asio::ip::udp;

boost::atomic<bool> cancelrequested = false;

#define MSMTSYSTEM Mesytec::MesytecSystem

struct StartMsmtParameters {
    StartMsmtParameters() :MaxCount(0), DurationSeconds(0), writelistmode(false) {}
    boost::atomic<long long> MaxCount;
    boost::atomic<double> DurationSeconds;
    boost::atomic<bool> writelistmode;

};

#include "Module.startMonitor.hpp"



#include "Module.NeutronMeasurement.hpp"
#include "Module.legacy_api_guard.hpp"




BOOST_PYTHON_MODULE(mesytecsystem)
{

    using namespace boost::python;

    try {
        ::get_api_guard(); // Initialize.
        init_ar();

        np::initialize();

        to_python_converter<cv::Mat,
            pbcvt::matToNDArrayBoostConverter>();
        pbcvt::matFromNDArrayBoostConverter();
        class_<Histogram, boost::noncopyable>("Histogram", boost::python::no_init)
            .def("update", &Histogram::update)
            .def("getRoiData", &Histogram::getRoiData)
            .def("setRoi", &Histogram::setRoi)
            .def("getRoi", &Histogram::getRoi)
            .add_property("Size", &Histogram::getSize)
            ;
        class_< NeutronMeasurement>("NeutronMeasurement",init<long>())
            .add_property("writelistmode", &NeutronMeasurement::get_writelistmode, &NeutronMeasurement::set_writelistmode)
            .add_property("simulatorRate", &NeutronMeasurement::get_simulatorRate, &NeutronMeasurement::set_simulatorRate)
            .add_property("version", &NeutronMeasurement::get_version)
            .def("start", &NeutronMeasurement::start)
            .def("stopafter", &NeutronMeasurement::stopafter)
            .def("resume", &NeutronMeasurement::resume)
            .def("monitors_status", &NeutronMeasurement::monitors_status)
            .def("log", &NeutronMeasurement::log)
            .def("status", &NeutronMeasurement::status)
            .def("stop", &NeutronMeasurement::stop)
            .def("on", &NeutronMeasurement::on)
            .def("off", &NeutronMeasurement::off)
            .def("getHistogram", &NeutronMeasurement::getHistogram, return_internal_reference<1, with_custodian_and_ward_postcall<1, 0>>())
            ;

        import("atexit").attr("register")(make_function(&::release_guard));


    }
    catch (const error_already_set&)
    {
        ::release_guard();
        throw;
    }



}



