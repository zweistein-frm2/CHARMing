/***************************************************************************
 *   Copyright (C) 2019 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/
#define PY_ARRAY_UNIQUE_SYMBOL pbcvt_ARRAY_API
#include "Module.Include.hpp"
#include "Charm.System.hpp"
#include "Mesytec.config.hpp"
#include "Mesytec.listmode.write.hpp"
#include "Zweistein.Averaging.hpp"

#include "Module.Globals.hpp"


std::string PROJECT_NAME("CHARMing");
std::string CONFIG_FILE("charm");
namespace p = boost::python;
namespace np = boost::python::numpy;
using boost::asio::ip::udp;


#define MSMTSYSTEM Charm::CharmSystem

struct StartMsmtParameters {
    StartMsmtParameters() :MaxCount(0), DurationSeconds(0), writelistmode(false) {}
    boost::atomic<long long> MaxCount;
    boost::atomic<double> DurationSeconds;
    boost::atomic<bool> writelistmode;

};

void startMonitor(boost::shared_ptr < MSMTSYSTEM> ptrmsmtsystem1, boost::shared_ptr <StartMsmtParameters> ptrStartParameters) {

    boost::filesystem::path inidirectory = Zweistein::Config::GetConfigDirectory();
    Zweistein::Config::inipath = inidirectory;
    Zweistein::Config::inipath.append(CONFIG_FILE + ".json");
    LOG_INFO << "Using config file:" << Zweistein::Config::inipath << " " << std::endl;
    boost::asio::signal_set signals(io_service, SIGINT, SIGTERM);
    signals.async_wait(boost::bind(&boost::asio::io_service::stop, &io_service));

    try { boost::property_tree::read_json(Zweistein::Config::inipath.string(), Mesytec::Config::root); }
    catch (std::exception& e) {
        LOG_ERROR << e.what() << " for reading." << std::endl;
    }
    std::list<Mcpd8::Parameters> _devlist = std::list<Mcpd8::Parameters>();

    ptrmsmtsystem1->evdata.Format = Zweistein::Format::EventData::Charm;

    bool configok = Mesytec::Config::get(_devlist, inidirectory.string());

    try { boost::property_tree::read_json(Zweistein::Config::inipath.string(), Mesytec::Config::root); }
    catch (std::exception& e) {
        LOG_ERROR << e.what() << " for reading." << std::endl;
    }


    std::stringstream ss_1;
    boost::property_tree::write_json(ss_1, Mesytec::Config::root);
    LOG_INFO << ss_1.str() << std::endl;




    try {
        std::string fp = Zweistein::Config::inipath.string();
        std::ofstream outfile;
        outfile.open(fp, std::ios::out | std::ios::trunc);
        outfile << ss_1.str();
        outfile.close();
        // boost::property_tree::write_json(Zweistein::Config::inipath.string(), Mesytec::Config::root);
        if (!configok) LOG_ERROR << "Please edit to fix configuration error : " << fp << std::endl;
    }
    catch (std::exception& e) { // exception expected, //std::cout << boost::diagnostic_information(e);
        LOG_ERROR << e.what() << " for writing." << std::endl;
    }
    if (!configok)      return;


    for (Mcpd8::Parameters& p : _devlist) {
        Zweistein::ping(p.mcpd_ip); // sometime
    }

    boost::function<void()> t;
    t = [&ptrmsmtsystem1, &_devlist]() {
        try {
            ptrmsmtsystem1->connect(_devlist, io_service);
            io_service.run();
        }
        catch (Mesytec::cmd_error& x) {
            if (int const* mi = boost::get_error_info<Mesytec::my_info>(x)) {
                auto  my_code = magic_enum::enum_cast<Mesytec::cmd_errorcode>(*mi);
                if (my_code.has_value()) {
                    auto c1_name = magic_enum::enum_name(my_code.value());
                    LOG_ERROR << c1_name << std::endl;
                }
            }
        }
        catch (boost::exception& e) { LOG_ERROR << boost::diagnostic_information(e) << std::endl; }
        io_service.stop();

    };

    auto pt = new boost::thread(boost::bind(t));

    Zweistein::Thread::CheckSetUdpKernelBufferSize();
    Zweistein::Thread::SetThreadPriority(pt->native_handle(), Zweistein::Thread::PRIORITY::HIGH);
    worker_threads.add_thread(pt);
    boost::this_thread::sleep_for(boost::chrono::milliseconds(3000));// so msmtsystem1 should be connected

    for (int i = 0; i < 10; i++) {
        // we try to connect for 10 seconds
        if (ptrmsmtsystem1->connected) {
            Zweistein::setupHistograms(io_service, ptrmsmtsystem1, Mesytec::Config::BINNINGFILE.string());
            worker_threads.create_thread([&ptrmsmtsystem1] {Zweistein::populateHistograms(io_service, ptrmsmtsystem1); });
            boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
            break;
        }
        boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));
    }

    boost::chrono::system_clock::time_point lastsec = boost::chrono::system_clock::now();
    long long lastcount = 0;
    Zweistein::Averaging<double> avg(4);
    while (!io_service.stopped()) {
        boost::this_thread::sleep_for(boost::chrono::milliseconds(500));

        boost::chrono::duration<double> secs = boost::chrono::system_clock::now() - ptrmsmtsystem1->getStart();
        boost::chrono::duration<double> Maxsecs(ptrStartParameters->DurationSeconds);
        long long currcount = ptrmsmtsystem1->evdata.evntcount;
        long long maxcount = ptrStartParameters->MaxCount;
        if (ptrmsmtsystem1->data.last_deviceStatusdeviceId & Mcpd8::Status::DAQ_Running) {
            bool sendstop = false;
            if (maxcount != 0 && currcount > maxcount) {
                LOG_INFO << "stopped by counter maxval:" << currcount << ">max:" << maxcount << std::endl;
                sendstop = true;
            }
            if (secs >= Maxsecs && Maxsecs != boost::chrono::duration<double>::zero()) {

                LOG_INFO << "stopped by timer:" << secs << "> max:" << Maxsecs << std::endl;

                sendstop = true;

            }

            if (sendstop) ptrmsmtsystem1->SendAll(Mcpd8::Cmd::STOP);
        }

        boost::chrono::duration<double> sec = boost::chrono::system_clock::now() - lastsec;
        lastsec = boost::chrono::system_clock::now();
        long long currentcount = ptrmsmtsystem1->evdata.evntcount;
        double evtspersecond = sec.count() != 0 ? (double)(currentcount - lastcount) / sec.count() : 0;
        avg.addValue(evtspersecond);
        {

            std::stringstream ss1;

            unsigned short tmp = ptrmsmtsystem1->data.last_deviceStatusdeviceId;
            std::string tmpstr = Mcpd8::DataPacket::deviceStatus(tmp);
            ss1 << "\r" << std::setprecision(0) << std::fixed << avg.getAverage() << " Events/s, (" << Zweistein::PrettyBytes((size_t)(evtspersecond * sizeof(Mesy::Mpsd8Event))) << "/s), " << tmpstr << " elapsed:" << secs;// << std::endl;
            std::streampos oldpos = ss1.tellg();
            ss1.seekg(0, std::ios::end);
            std::streampos len = ss1.tellg();
            ss1.seekg(oldpos, std::ios::beg);
            int maxlen = 75;
            int n_ws = int(maxlen - len);
            for (; n_ws > 0; n_ws--) ss1 << " ";
            {
                boost::mutex::scoped_lock lock(coutGuard);
                std::cout << ss1.str();
            }
            lastcount = currentcount;
#ifndef _WIN32
            std::cout << std::flush;
#endif
        }
        std::chrono::milliseconds maxblocking(100);
        io_service.run_one_for(maxblocking);
    };
    if (ptrmsmtsystem1) {
        try { ptrmsmtsystem1->SendAll(Mcpd8::Cmd::STOP_UNCHECKED); }
        catch (boost::exception& e) { LOG_ERROR << boost::diagnostic_information(e) << std::endl; }
        //	delete ptrmsmtsystem1;
        //	ptrmsmtsystem1 = nullptr;
    }
    boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
    LOG_DEBUG << "startMonitor() exiting..." << std::endl;

}



#include "Module.NeutronMeasurement.hpp"
#include "Module.legacy_api_guard.hpp"






BOOST_PYTHON_MODULE(charmsystem)
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
            .def("setRoi", &Histogram::setRoi)
            .def("getRoi", &Histogram::getRoi)
            .add_property("Size", &Histogram::getSize)
            ;
        class_< NeutronMeasurement>("NeutronMeasurement", init<long>())
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