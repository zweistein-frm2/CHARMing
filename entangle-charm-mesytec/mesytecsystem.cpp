/***************************************************************************
 *   Copyright (C) 2019 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/
#define PY_ARRAY_UNIQUE_SYMBOL pbcvt_ARRAY_API
#include <boost/python.hpp>
#include <boost/python/tuple.hpp>
#include <pyboostcvconverter/pyboostcvconverter.hpp>
#include <boost/python/numpy.hpp>
#include <iostream>
#include "Zweistein.PrettyBytes.hpp"
#include <opencv2/core.hpp>
#include <opencv2/core/mat.hpp>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/geometry/io/wkt/wkt.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include "Zweistein.Histogram.hpp"
#include "Mesytec.Mcpd8.hpp"
#include "Zweistein.HomePath.hpp"
#include "Zweistein.GetConfigDir.hpp"
#include "Mesytec.config.hpp"
#include "Zweistein.ThreadPriority.hpp"
#include "Zweistein.populateHistograms.hpp"
#include "Mesytec.listmode.write.hpp"
#include <stdio.h>
#include "Zweistein.Averaging.hpp"
Entangle::severity_level Entangle::SEVERITY_THRESHOLD =Entangle::severity_level::trace;
std::string PROJECT_NAME("charm");

namespace p = boost::python;
namespace np = boost::python::numpy;
using boost::asio::ip::udp;


EXTERN_FUNCDECLTYPE boost::mutex coutGuard;
EXTERN_FUNCDECLTYPE boost::thread_group worker_threads;

extern const char* GIT_REV;
extern const char* GIT_TAG;
extern const char* GIT_BRANCH;
extern const char* GIT_DATE;

Zweistein::Lock histogramsLock;
std::vector<Histogram> histograms;

boost::asio::io_service io_service;




void initialize() { LOG_DEBUG << "initialize()" << std::endl;  }
void shutdown() {
    LOG_DEBUG << "shutdown()" << std::endl;
       io_service.stop();
       boost::this_thread::sleep_for(boost::chrono::milliseconds(450));
       //worker_threads.join_all();
  
    }



struct StartMsmtParameters {
    StartMsmtParameters() :MaxCount(0), DurationSeconds(0), writelistmode(false) {}
    boost::atomic<long long> MaxCount;
    boost::atomic<double> DurationSeconds;
    boost::atomic<bool> writelistmode;

};

void startMonitor(boost::shared_ptr < Mesytec::MesytecSystem> ptrmsmtsystem1, boost::shared_ptr <StartMsmtParameters> ptrStartParameters ) {
       
        boost::filesystem::path inidirectory = Zweistein::Config::GetConfigDirectory();
        Zweistein::Config::inipath = inidirectory;
        Zweistein::Config::inipath.append(PROJECT_NAME + ".json");
        LOG_INFO << "Using config file:" << Zweistein::Config::inipath << " " << std::endl;
        boost::asio::signal_set signals(io_service, SIGINT, SIGTERM);
        signals.async_wait(boost::bind(&boost::asio::io_service::stop, &io_service));

        try { boost::property_tree::read_json(Zweistein::Config::inipath.string(),Mesytec::Config::root); }
        catch (std::exception& e) {
            LOG_ERROR << e.what() << " for reading."<<std::endl;
        }
        std::list<Mcpd8::Parameters> _devlist = std::list<Mcpd8::Parameters>();

        ptrmsmtsystem1->data.Format = Mcpd8::EventDataFormat::Mpsd8;

        bool configok = Mesytec::Config::get(_devlist, inidirectory.string());

        try { boost::property_tree::read_json(Zweistein::Config::inipath.string(), Mesytec::Config::root); }
        catch (std::exception& e) {
            LOG_ERROR << e.what() << " for reading."<<std::endl;
        }


        std::stringstream ss_1;
        boost::property_tree::write_json(ss_1, Mesytec::Config::root);
        LOG_INFO << ss_1.str()<<std::endl;

        
        

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
            LOG_ERROR << e.what() << " for writing."<<std::endl;
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
            catch (boost::exception& e) {LOG_ERROR << boost::diagnostic_information(e) << std::endl;}
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
        Zweistein::Averaging<double> avg(3);
        while (!io_service.stopped()) {
            boost::this_thread::sleep_for(boost::chrono::milliseconds(500));
             //       if (PyErr_CheckSignals() == -1) {
         //        io_service.stop();
        //      }

            
            boost::chrono::system_clock::time_point tps=ptrmsmtsystem1->started;
            boost::chrono::duration<double> secs = boost::chrono::system_clock::now() - tps;
            boost::chrono::duration<double> Maxsecs(ptrStartParameters->DurationSeconds);
            long long currcount = ptrmsmtsystem1->data.evntcount;
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
            long long currentcount = ptrmsmtsystem1->data.evntcount; 
            double evtspersecond = sec.count() != 0 ? (double)(currentcount - lastcount) / sec.count() : 0;
            avg.addValue(evtspersecond);
            {
              
                std::stringstream ss1;

                unsigned short tmp = ptrmsmtsystem1->data.last_deviceStatusdeviceId;
                std::string tmpstr = Mcpd8::DataPacket::deviceStatus(tmp);
                ss1 << "\r" << std::setprecision(0) << std::fixed << avg.getAverage()<< " Events/s, (" << Zweistein::PrettyBytes((size_t)(evtspersecond * sizeof(Mesy::Mpsd8Event))) << "/s), "<< tmpstr <<" elapsed:"<<secs;// << std::endl;
                std::streampos oldpos = ss1.tellg();
                ss1.seekg(0, std::ios::end);
                std::streampos len = ss1.tellg();
                ss1.seekg(oldpos,std::ios::beg);
                int maxlen = 75;
                int n_ws =int( maxlen - len);
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
            catch (boost::exception& e) {LOG_ERROR << boost::diagnostic_information(e) << std::endl;}
            //	delete ptrmsmtsystem1;
            //	ptrmsmtsystem1 = nullptr;
        }
        boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
        LOG_DEBUG << "startMonitor() exiting..." << std::endl;

    }

struct NeutronMeasurement {
     public:
       
        boost::shared_ptr<StartMsmtParameters> ptrStartParameters;
        boost::shared_ptr < Mesytec::MesytecSystem> ptrmsmtsystem1;
        NeutronMeasurement(long loghandle):ptrStartParameters(boost::shared_ptr < StartMsmtParameters>(new StartMsmtParameters())),
            ptrmsmtsystem1(boost::shared_ptr < Mesytec::MesytecSystem>(new Mesytec::MesytecSystem()))
        {

            Entangle::Init(loghandle);
            histograms = std::vector<Histogram>(2);
            ptrmsmtsystem1->initatomicortime_point();
            worker_threads.create_thread([this] {startMonitor(ptrmsmtsystem1, ptrStartParameters); });
            LOG_DEBUG << "NeutronMeasurement(" << loghandle << ")" << std::endl;
            LOG_INFO << get_version() << std::endl;
        }
        ~NeutronMeasurement() {
            LOG_DEBUG << "~NeutronMeasurement()" << std::endl;
        
        }

        bool get_writelistmode() {
            return ptrStartParameters->writelistmode;
        }
        void set_writelistmode(bool val) {
            ptrStartParameters->writelistmode = val;
        }
         long get_simulatorRate() {
             long rate= ptrmsmtsystem1->simulatordatarate;
             return rate;
        }
        void set_simulatorRate(long rate) {
            for (auto& kvp : ptrmsmtsystem1->deviceparam) {
                if (kvp.second.datagenerator == Mesytec::DataGenerator::NucleoSimulator) {
                    ptrmsmtsystem1->Send(kvp, Mcpd8::Internal_Cmd::SETNUCLEORATEEVENTSPERSECOND, rate);//1650000 is maximum
                }
                if (kvp.second.datagenerator == Mesytec::DataGenerator::CharmSimulator) {
                    ptrmsmtsystem1->Send(kvp, Mcpd8::Internal_Cmd::CHARMSETEVENTRATE, rate); // oder was du willst
                    ptrmsmtsystem1->Send(kvp, Mcpd8::Internal_Cmd::CHARMPATTERNGENERATOR, 1); // oder was du willst
                }
            }
        }

        void stopafter(uint64 counts, double seconds) {
            ptrStartParameters->MaxCount = counts;
            ptrStartParameters->DurationSeconds = seconds;
            LOG_INFO << "NeutronMeasurement.stopafter(" << counts << ", " << seconds << ")" << std::endl;
        }
       
        boost::python::tuple status() {

            boost::chrono::system_clock::time_point tps=ptrmsmtsystem1->started;
            boost::chrono::duration<double> secs = boost::chrono::system_clock::now() - tps;
            long long count = ptrmsmtsystem1->data.evntcount;
            unsigned short tmp = ptrmsmtsystem1->data.last_deviceStatusdeviceId;
            unsigned char devstatus = Mcpd8::DataPacket::getStatus(tmp);
            std::string msg = Mcpd8::DataPacket::deviceStatus(devstatus);
            //LOG_INFO << "started:"<<tps<<", now="<< boost::chrono::system_clock::now()<< ":"<<secs<<std::endl;
           // LOG_INFO <<"count="<<count<<", elapsed="<<secs << ",devstatus=" << (int)devstatus << ", " << msg<<std::endl;
            return boost::python::make_tuple(count, secs.count(), devstatus,msg);

        }

        std::string get_version() {
            std::stringstream ss;
            ss <<PROJECT_NAME << " : BRANCH: " << GIT_BRANCH << " TAG:" << GIT_TAG << " REV: " << GIT_REV << " " << GIT_DATE;
            return ss.str();
        }

        void start() {
            unsigned short tmp = ptrmsmtsystem1->data.last_deviceStatusdeviceId;
            auto status = Mcpd8::DataPacket::getStatus(tmp);
            if (status & Mcpd8::Status::DAQ_Running) {
                LOG_WARNING << "skipped ," << status << std::endl;
                return;

            }
            
            try {
                if (ptrStartParameters->writelistmode) {
                        ptrmsmtsystem1->write2disk = true;
                        worker_threads.create_thread([this] {Mesytec::writeListmode(io_service, ptrmsmtsystem1); });
            }
                ptrmsmtsystem1->SendAll(Mcpd8::Cmd::START);
                set_simulatorRate(get_simulatorRate());
            }
            catch (boost::exception& e) { LOG_ERROR << boost::diagnostic_information(e)<<std::endl;}

        }
        void stop() {
            try { ptrmsmtsystem1->SendAll(Mcpd8::Cmd::STOP); }
            catch (boost::exception& e) { LOG_ERROR << boost::diagnostic_information(e) << std::endl;}
        }
        void resume() {
            try { ptrmsmtsystem1->SendAll(Mcpd8::Cmd::CONTINUE); }
            catch (boost::exception& e) { LOG_ERROR << boost::diagnostic_information(e) << std::endl;}
        }

        Histogram* getHistogram() {
            using namespace magic_enum::bitwise_operators; // out-of-the-box bitwise operators for enums.
            LOG_DEBUG << "getHistogram()" << std::endl;
            Zweistein::histogram_setup_status hss = Zweistein::setup_status;
            if (magic_enum::enum_integer(hss & Zweistein::histogram_setup_status::has_binning)) {
                return &histograms[1];
            }
            return &histograms[0];
        }

    };

namespace {
struct legacy_api_guard
{
    legacy_api_guard() { initialize(); }
    ~legacy_api_guard() { shutdown(); }
};

/// @brief Global shared guard for the legacy API.
boost::shared_ptr<legacy_api_guard> legacy_api_guard_;
/// @brief Get (or create) guard for legacy API.
boost::shared_ptr<legacy_api_guard> get_api_guard()
{
    if (!legacy_api_guard_)
    {
        legacy_api_guard_ = boost::make_shared<legacy_api_guard>();
    }
    return legacy_api_guard_;
}

void release_guard()
{
    try {
        LOG_DEBUG << "release_guard()"<<std::endl;
     }
    catch(std::exception  &){}
    legacy_api_guard_.reset();
}



} // namespace 




#if (PY_VERSION_HEX >= 0x03000000)

static void* init_ar() {
#else
static void init_ar() {
#endif
    Py_Initialize();

    import_array();
    return NUMPY_IMPORT_ARRAY_RETVAL;
}



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



