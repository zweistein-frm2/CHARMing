/***************************************************************************
 *   Copyright (C) 2020 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
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
#include <list>
#include <filesystem>
#include <string>
#include <boost/filesystem.hpp>
#include "Zweistein.PrettyBytes.hpp"
#include "Mesytec.RandomData.hpp"
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
std::vector<Histogram> histograms = std::vector<Histogram>(2);

boost::asio::io_service io_service;




void initialize() { LOG_DEBUG << "initialize()" << std::endl;  }
void shutdown() {
    LOG_DEBUG << "shutdown()" << std::endl ;
       io_service.stop();
       boost::this_thread::sleep_for(boost::chrono::milliseconds(250));
       //worker_threads.join_all();
    }



struct StartMsmtParameters {
    StartMsmtParameters() :MaxCount(0), DurationSeconds(0), playlist(std::list<std::string>()) {}
    boost::atomic<long long> MaxCount;
    boost::atomic<double> DurationSeconds;
    boost::atomic<int> speedmultiplier;
    std::list<std::string> playlist;
    boost::mutex playlistGuard;
};

void startMonitor(boost::shared_ptr < Mesytec::MesytecSystem> ptrmsmtsystem1, boost::shared_ptr <StartMsmtParameters> ptrStartParameters ) {
      

        boost::asio::signal_set signals(io_service, SIGINT, SIGTERM);
        signals.async_wait(boost::bind(&boost::asio::io_service::stop, &io_service));

        std::list<Mcpd8::Parameters> _devlist = std::list<Mcpd8::Parameters>();

        boost::function<void()> t;
     
            ptrmsmtsystem1->inputFromListmodeFile = true;
            t = [&ptrmsmtsystem1,&ptrStartParameters, &_devlist]() {
                try {
                    std::list<std::string> listmodeinputfiles = std::list<std::string>();
                    while (Mesytec::listmode::action  lec = Mesytec::listmode::CheckAction()) {
                        if (lec == Mesytec::listmode::start_reading) {
                            listmodeinputfiles.clear();
                            boost::mutex::scoped_lock lock(ptrStartParameters->playlistGuard);
                             for (auto& s : ptrStartParameters->playlist) listmodeinputfiles.push_back(s);
                             LOG_DEBUG << __FILE__ << " : " << __LINE__ << "  Mesytec::listmode::start_reading" << std::endl;
                             Mesytec::listmode::whatnext = Mesytec::listmode::continue_reading;

                        }
                        else boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
                    }

                    LOG_DEBUG << __FILE__ << " : " << __LINE__ << std::endl;
                    
                    for (std::string& fname : listmodeinputfiles) {
                        LOG_DEBUG << __FILE__ << " : " << __LINE__ << " " << fname << std::endl;
                        try { boost::property_tree::read_json(fname + ".json", Mesytec::Config::root); }
                        catch (std::exception& e) {
                            LOG_ERROR << e.what() << " for reading." << std::endl;
                            continue;
                        }
                        _devlist.clear();
                        LOG_INFO << "Config file : " << fname + ".json" << std::endl;
                        bool configok = Mesytec::Config::get(_devlist, "");
                        std::stringstream ss_1;
                        boost::property_tree::write_json(ss_1, Mesytec::Config::root);
                        LOG_INFO << ss_1.str() << std::endl;

                        if (!configok)	return ;

                        ptrmsmtsystem1->listmode_connect(_devlist, io_service);
                        // find the .json file for the listmode file
                        // check if Binning file not empty, if not empty wait for

                        Zweistein::setupHistograms(io_service, ptrmsmtsystem1, Mesytec::Config::BINNINGFILE.string());
                        
                        auto abfunc = boost::bind(&Mesytec::MesytecSystem::analyzebuffer, ptrmsmtsystem1, _1);
                        auto read = Mesytec::listmode::Read(abfunc, ptrmsmtsystem1->data, ptrmsmtsystem1->deviceparam);

                        read.file(fname, io_service);
                        Zweistein::setup_status = Zweistein::histogram_setup_status::not_done; // next file is all new life 
                    }
                    // alldone, so 
                    Mesytec::listmode::whatnext = Mesytec::listmode::wait_reading;


                }
                catch (boost::exception& e) {
                    LOG_ERROR << boost::diagnostic_information(e) << std::endl;
                }

            };

        

        auto pt = new boost::thread(boost::bind(t));
        worker_threads.add_thread(pt);
        
        worker_threads.create_thread([&ptrmsmtsystem1] {Zweistein::populateHistograms(io_service, ptrmsmtsystem1); });
       
        // nothing to do really,
        while (!io_service.stopped()) {

            long long currcount = ptrmsmtsystem1->data.evntcount;
            long long maxcount = ptrStartParameters->MaxCount;
            boost::chrono::system_clock::time_point tps = ptrmsmtsystem1->started;
            boost::chrono::duration<double> secs = boost::chrono::system_clock::now() - tps;
            boost::chrono::duration<double> Maxsecs(ptrStartParameters->DurationSeconds);

            bool sendstop = false;
            if (maxcount != 0 && currcount > maxcount) {
                LOG_INFO << "stopped by counter maxval:" << currcount << ">max:" << maxcount << std::endl;
                sendstop = true;
            }
            if (secs >= Maxsecs && Maxsecs != boost::chrono::duration<double>::zero()) {
                LOG_INFO << "stopped by timer:" << secs << "> max:" << Maxsecs << std::endl;
                sendstop = true;
            }
            boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
            std::chrono::milliseconds maxblocking(100);
            io_service.run_one_for(maxblocking);
            
        }
        
        boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
        LOG_DEBUG << "startMonitor() exiting..." << std::endl;

    }

struct ReplayList {
     public:
       
        boost::shared_ptr<StartMsmtParameters> ptrStartParameters;
        boost::shared_ptr < Mesytec::MesytecSystem> ptrmsmtsystem1;
        ReplayList(long loghandle):ptrStartParameters(boost::shared_ptr < StartMsmtParameters>(new StartMsmtParameters())),
            ptrmsmtsystem1(boost::shared_ptr < Mesytec::MesytecSystem>(new Mesytec::MesytecSystem()))
        {

            Entangle::Init(loghandle);
            ptrmsmtsystem1->initatomicortime_point();
            worker_threads.create_thread([this] {startMonitor(ptrmsmtsystem1, ptrStartParameters); });
            LOG_DEBUG << "ReplayList(" << loghandle << ")" << std::endl;
        }
        ~ReplayList() {
            LOG_DEBUG << "~ReplayList()" << std::endl;
        
        }

        int get_speedmultiplier() {
            return ptrStartParameters->speedmultiplier;
        }
        void set_speedmultiplier(int val) {
            ptrStartParameters->speedmultiplier = val;
        }
        
        void stopafter(uint64 counts, double seconds) {
            ptrStartParameters->MaxCount = counts;
            ptrStartParameters->DurationSeconds = seconds;
            LOG_INFO << "ReplayList.stopafter(" << counts << ", " << seconds << ")" << std::endl;
        }
       
        boost::python::tuple status() {

            boost::chrono::system_clock::time_point tps=ptrmsmtsystem1->started;
            boost::chrono::duration<double> secs = boost::chrono::system_clock::now() - tps;
            long long count = ptrmsmtsystem1->data.evntcount;
            unsigned short tmp = ptrmsmtsystem1->data.last_deviceStatusdeviceId;
            unsigned char devstatus = Mcpd8::DataPacket::getStatus(tmp);
            std::string msg = Mcpd8::DataPacket::deviceStatus(devstatus);
           // LOG_INFO << "started:"<<tps<<", now="<< boost::chrono::system_clock::now()<< ":"<<secs<<std::endl;
           // LOG_INFO <<"count="<<count<<", elapsed="<<secs << ",devstatus=" << (int)devstatus << ", " << msg<<std::endl;
            return boost::python::make_tuple(count, secs.count(), devstatus,msg);

        }
        std::string LISTMODEEXTENSION = ".mdat";
        boost::python::list files(std::string directory) {
          
            boost::python::list l;
            LOG_INFO << "files(" << directory << ")" << std::endl;
            //
            try {
                if (directory.length() >= 1) {
                    if (directory[0] == '~') {
                        directory= directory.substr(1, directory.size() - 1);
                        boost::filesystem::path p = Zweistein::GetHomePath().remove_trailing_separator();
                        p /= directory;
                        directory = p.string();
                    }
                }
               
                boost::filesystem::path mdat_path(directory);
                mdat_path.make_preferred();

                LOG_INFO << "files(" << mdat_path.string() << ")" << std::endl;

                boost::filesystem::recursive_directory_iterator end;
                for (boost::filesystem::recursive_directory_iterator i(mdat_path); i != end; ++i)
                {
                    const boost::filesystem::path cp = (*i);
                    if (cp.extension() == LISTMODEEXTENSION) {
                        LOG_DEBUG <<__FILE__<<" : "<<__LINE__<< " append " << cp.string() << std::endl;
                        l.append(cp.string());
                    }
                }
            }
            catch (boost::exception& e) {
                LOG_ERROR << boost::diagnostic_information(e) << std::endl;
                LOG_ERROR <<"directory["<<directory<<"] :" << std::endl;

            }
            
            return l;
        }

      

        boost::python::list addfile(std::string file) {
            
            boost::mutex::scoped_lock lock(ptrStartParameters->playlistGuard);
            boost::python::list l;
            try {
                if (boost::filesystem::exists(file)) {
                    boost::filesystem::path p(file);
                    if (p.extension() != LISTMODEEXTENSION) {
                        LOG_WARNING << "File not added to playlist (wrong extension !="<< LISTMODEEXTENSION<<") : " << file << std::endl;
                    }
                    else ptrStartParameters->playlist.push_back(file);
                }
                else LOG_WARNING << "File not added to playlist (not found) : " << file << std::endl;
            
            }
            catch (boost::exception& e) {LOG_ERROR << boost::diagnostic_information(e)<<std::endl;}
            for (auto& s : ptrStartParameters->playlist) l.append(s);
            return l;
            
        }

        boost::python::list removefile(std::string file) {

            boost::mutex::scoped_lock lock(ptrStartParameters->playlistGuard);
            boost::python::list l;
            try {
                ptrStartParameters->playlist.remove(file);
            }
            catch (boost::exception& e) {LOG_ERROR << boost::diagnostic_information(e) << std::endl;}
            for (auto& s : ptrStartParameters->playlist) l.append(s);
            return l;

        }

        std::string get_version() {
            std::stringstream ss;
            ss << "BRANCH: " << GIT_BRANCH << " TAG:" << GIT_TAG << " REV: " << GIT_REV << " " << GIT_DATE;
            return ss.str();
        }
        void start() {
            // we have to start from beginning of playlist
            LOG_INFO << "ReplayList::start()" << std::endl;
            Mesytec::listmode::whatnext = Mesytec::listmode::start_reading;
        }
        void stop() {
            LOG_INFO << "ReplayList::stop()" << std::endl;
            Mesytec::listmode::whatnext = Mesytec::listmode::wait_reading;
        }
        void resume() {
            LOG_INFO << "ReplayList::resume()" << std::endl;
            Mesytec::listmode::whatnext = Mesytec::listmode::continue_reading;
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
    catch(std::exception &){}
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



BOOST_PYTHON_MODULE(listmodereplay)
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
        class_< ReplayList>("ReplayList",init<long>())
            .add_property("speedmultiplier", &ReplayList::get_speedmultiplier, &ReplayList::set_speedmultiplier)
            .add_property("version", &ReplayList::get_version)
            .def("addfile", &ReplayList::addfile)
            .def("removefile", &ReplayList::removefile)
            .def("files", &ReplayList::files)
            .def("stopafter", &ReplayList::stopafter)
            .def("start", &ReplayList::start)
            .def("resume", &ReplayList::resume)
            .def("status", &ReplayList::status)
            .def("stop", &ReplayList::stop)
            .def("getHistogram", &ReplayList::getHistogram, return_internal_reference<1, with_custodian_and_ward_postcall<1, 0>>())
            ;
      
        import("atexit").attr("register")(make_function(&::release_guard));


    }
    catch (const error_already_set&)
    {
        ::release_guard();
        throw;
    }

   
   
}



