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
#include <string>
#include <boost/filesystem.hpp>
#include "Zweistein.PrettyBytes.hpp"

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
std::string PROJECT_NAME("CHARMing");

namespace p = boost::python;
namespace np = boost::python::numpy;
using boost::asio::ip::udp;


EXTERN_FUNCDECLTYPE boost::mutex coutGuard;
EXTERN_FUNCDECLTYPE boost::thread_group worker_threads;

extern const char* GIT_REV;
extern const char* GIT_TAG;
extern const char* GIT_LATEST_TAG;
extern const char* GIT_NUMBER_OF_COMMITS_SINCE;
extern const char* GIT_BRANCH;
extern const char* GIT_DATE;

Zweistein::Lock histogramsLock;
std::vector<Histogram> histograms;
static_assert(COUNTER_MONITOR_COUNT >= 4, "Mcpd8 Datapacket can use up to 4 counters: params[4][3]");
boost::atomic<unsigned long long> CounterMonitor[COUNTER_MONITOR_COUNT];
boost::asio::io_service io_service;




void initialize() { LOG_DEBUG << "initialize()" << std::endl;  }
void shutdown() {
    LOG_DEBUG << "shutdown()" << std::endl ;
       io_service.stop();
       boost::this_thread::sleep_for(boost::chrono::milliseconds(250));
       //worker_threads.join_all();
    }



struct StartMsmtParameters {
    StartMsmtParameters() :MaxCount(0), DurationSeconds(0), playlist(std::list<std::string>()),  monitorbusy(false) {}
    boost::atomic<long long> MaxCount;
    boost::atomic<double> DurationSeconds;
    boost::atomic<int> speedmultiplier;
    std::list<std::string> playlist;
    boost::mutex playlistGuard;
    boost::atomic<int> monitorbusy;
};

void startMonitor(boost::shared_ptr < Mesytec::MesytecSystem> ptrmsmtsystem1, boost::shared_ptr <StartMsmtParameters> ptrStartParameters ) {
      
        using namespace magic_enum::bitwise_operators; // out-of-the-box bitwise operators for enums.
        
       
        boost::asio::signal_set signals(io_service, SIGINT, SIGTERM);
        signals.async_wait(boost::bind(&boost::asio::io_service::stop, &io_service));

        std::list<Mcpd8::Parameters> _devlist = std::list<Mcpd8::Parameters>();

        boost::function<void()> t;
     
            ptrmsmtsystem1->inputFromListmodeFile = true;
            t = [&ptrmsmtsystem1,&ptrStartParameters, &_devlist]() {
                using namespace magic_enum::ostream_operators;
                try {
                    std::list<std::string> listmodeinputfiles = std::list<std::string>();
                    int l = 0;
                    do {
                        ptrStartParameters->monitorbusy = false;
                        while (Mesytec::listmode::action  lec = Mesytec::listmode::CheckAction()) {
                            if (lec == Mesytec::listmode::start_reading) {
                                Zweistein::histogram_setup_status hss = Zweistein::setup_status;
                                //hss &= ~(Zweistein::histogram_setup_status::done);
                                hss = Zweistein::histogram_setup_status::not_done;
                                Zweistein::setup_status = hss; // next file is all new life 

                                listmodeinputfiles.clear();
                                ptrmsmtsystem1->data.evntcount = 0;
                                for (int i = 0; i < COUNTER_MONITOR_COUNT; i++) CounterMonitor[i] = 0;
                               
                                LOG_INFO << "data.evntcount = 0, Monitors=0" << std::endl;
                                boost::mutex::scoped_lock lock(ptrStartParameters->playlistGuard);

                                LOG_INFO << __FILE__ << " : " << __LINE__ << lec << std::endl;
                                for (auto& s : ptrStartParameters->playlist) {
                                    LOG_INFO << s << std::endl;
                                    listmodeinputfiles.push_back(s);
                                }
                                LOG_INFO << std::endl;
                                
                                // easy hack to ensure other thread will read the whatnext variable and react on it
                                // but we can happily live with a tiny delay after start
                                Mesytec::listmode::whatnext = Mesytec::listmode::continue_reading;
                                break;
                            }
                            else {
                                char clessidra[8] = { '|', '/' , '-', '\\', '|', '/', '-', '\\' };
                                std::cout << "\r  waiting for action " << clessidra[l%8];
                                l++;
                                boost::this_thread::sleep_for(boost::chrono::milliseconds(300));
                            }
                        }

                      
                        for (std::string& fname : listmodeinputfiles) {
                            LOG_INFO  << " " << fname << std::endl;
                            try { boost::property_tree::read_json(fname + ".json", Mesytec::Config::root); }
                            catch (std::exception& e) {
                                LOG_ERROR << e.what() << " for reading." << std::endl;
                                continue;
                            }
                            _devlist.clear();
                            LOG_INFO << "Config file : " << fname + ".json" << std::endl;
                            bool configok = Mesytec::Config::get(_devlist, "",false);
                            std::stringstream ss_1;
                            boost::property_tree::write_json(ss_1, Mesytec::Config::root);
                            LOG_INFO << ss_1.str() << std::endl;

                            if (!configok) {
                                LOG_INFO << "Configuration error, skipped" << std::endl;
                                continue;
                            }
                            ptrStartParameters->monitorbusy = true;
                            ptrmsmtsystem1->eventdataformat = Mcpd8::EventDataFormat::Undefined;
                            ptrmsmtsystem1->listmode_connect(_devlist, io_service); 
                            // find the .json file for the listmode file
                            // check if Binning file not empty, if not empty wait for
                            try {
                                Zweistein::setupHistograms(io_service, ptrmsmtsystem1, Mesytec::Config::BINNINGFILE.string());
                                bool ok = ptrmsmtsystem1->data.evntqueue.push(Zweistein::Event::Reset());
                                if (!ok) LOG_ERROR << " cannot push Zweistein::Event::Reset()" << std::endl;
                                boost::function<void(Mcpd8::DataPacket&)> abfunc = boost::bind(&Mesytec::MesytecSystem::analyzebuffer, ptrmsmtsystem1, boost::placeholders::_1);
                                boost::shared_ptr <Mesytec::listmode::Read> ptrRead = boost::shared_ptr < Mesytec::listmode::Read>(new Mesytec::listmode::Read(abfunc, ptrmsmtsystem1->data, ptrmsmtsystem1->deviceparam));
                                // pointer to obj needed otherwise exceptions are not propagated properly
                                ptrRead->file(fname, io_service);
                                while (ptrmsmtsystem1->data.evntqueue.read_available()); // wait unitl queue consumed
                                

                           }
                            catch (Mesytec::listmode::read_error& x) {
                                if (int const* mi = boost::get_error_info<Mesytec::listmode::my_info>(x)) {
                                    auto  my_code = magic_enum::enum_cast<Mesytec::listmode::read_errorcode>(*mi);
                                    if (my_code.has_value()) {
                                        auto c1_name = magic_enum::enum_name(my_code.value());
                                        LOG_ERROR << c1_name << std::endl;
                                    }
                                }
                            }
                            catch (std::exception& e) {
                                LOG_ERROR << e.what() << std::endl;
                            }
                        }
                        using namespace magic_enum::ostream_operators;
                        Mesytec::listmode::whatnext = Mesytec::listmode::wait_reading;
                       
                        Mesytec::listmode::action ac = Mesytec::listmode::whatnext;
                       
                        LOG_INFO << " all done: " << ac << std::endl;
                    } while (!io_service.stopped());

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
            histograms = std::vector<Histogram>(2);
            ptrmsmtsystem1->initatomicortime_point();
            worker_threads.create_thread([this] {startMonitor(ptrmsmtsystem1, ptrStartParameters); });
            LOG_DEBUG << "ReplayList(" << loghandle << ")" << std::endl;
            LOG_INFO << get_version() << std::endl;
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
       
        boost::python::list log() {
            boost::python::list l;
            {
                Zweistein::ReadLock r_lock(Entangle::cbLock);

                for (auto iter = Entangle::ptrcb->begin(); iter != Entangle::ptrcb->end();iter++) {
                  
                   std::vector<std::string> strs;
                   boost::split(strs, *iter, boost::is_any_of("\n"));
                   for(auto & s: strs) l.append(s);
                }
            }
            return l;
        }
        boost::python::list monitors_status() {
            boost::python::list l2;
            for (int i = 0; i < COUNTER_MONITOR_COUNT; i++) {
                unsigned long long val = CounterMonitor[i];
                auto t = boost::python::make_tuple("MONITOR" + std::to_string(i), val );
                l2.append(t);
            }
            return l2;
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
            if (directory.empty()) return l;
            LOG_INFO << "files(" << directory << ")" << std::endl;
            
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

                if (boost::filesystem::is_directory(mdat_path)) {
                  
                    for (auto& entry : boost::make_iterator_range(boost::filesystem::directory_iterator(mdat_path), {}))
                      
                         if (entry.path().extension() == LISTMODEEXTENSION) {
                            LOG_DEBUG << __FILE__ << " : " << __LINE__ << " append " << entry.path().string() << std::endl;
                            l.append(entry.path().string());
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
            LOG_INFO <<"addfile("<< file << ")" << std::endl;
            
            boost::python::list l;
            {
                boost::mutex::scoped_lock lock(ptrStartParameters->playlistGuard);
               
                try {
                    bool duplicate = false;
                    for (auto& s : ptrStartParameters->playlist) {
                        if (s == file) {
                            duplicate = true;
                          //  LOG_INFO << s << " == " << file << std::endl;
                        }
                       // else LOG_INFO << s << " != " << file << std::endl;
                       
                    }
                    if (!duplicate) {
                        if (!boost::filesystem::exists(file)) {
                            LOG_WARNING << "Not found: " << file << std::endl;
                        }
                        else {
                            boost::filesystem::path p(file);
                            if (p.extension() != LISTMODEEXTENSION) {
                                LOG_WARNING << "File not added to playlist (wrong extension !=" << LISTMODEEXTENSION << ") : " << file << std::endl;
                            }
                            else {
                                ptrStartParameters->playlist.push_back(file);
                                LOG_INFO << file << " added to playlist" << std::endl;
                            }

                        }

                    }
                    else LOG_WARNING << "File not added to playlist" << "(duplicate)" << " : " << file << std::endl;

                }
                catch (boost::exception& e) { LOG_ERROR << boost::diagnostic_information(e) << std::endl; }
                //LOG_INFO << "list:" << std::endl;
                for (auto& s : ptrStartParameters->playlist) {
                    l.append(s);
                    //LOG_INFO << s << std::endl;
                }
                //LOG_INFO << "end list:" << std::endl;
              
            }
            return l;
            
        }

        boost::python::list removefile(std::string file) {
            LOG_INFO << "removefile(" << file << ")" << std::endl;
            
            boost::python::list l;
            {
                boost::mutex::scoped_lock lock(ptrStartParameters->playlistGuard);
                try {
                    ptrStartParameters->playlist.remove(file);
                }
                catch (boost::exception& e) { LOG_ERROR << boost::diagnostic_information(e) << std::endl; }
                for (auto& s : ptrStartParameters->playlist) l.append(s);
            }
            return l;

        }

        std::string get_version() {
            std::stringstream ss;
            std::string git_latest_tag(GIT_LATEST_TAG);
            git_latest_tag.erase(std::remove_if(git_latest_tag.begin(), git_latest_tag.end(), (int(*)(int)) std::isalpha), git_latest_tag.end());
            ss << PROJECT_NAME << " : " << git_latest_tag << "." << GIT_NUMBER_OF_COMMITS_SINCE << "."<< GIT_REV <<"_"<< GIT_DATE;
            return ss.str();
        }
        void start() {
            using namespace magic_enum::ostream_operators;
            // we have to start from beginning of playlist
            ptrmsmtsystem1->started = boost::chrono::system_clock::now();
            Mesytec::listmode::action ac = Mesytec::listmode::whatnext;
            LOG_INFO << "ReplayList::start() " << ac << std::endl;
            Mesytec::listmode::action action= Mesytec::listmode::whatnext;
            
            // so we stop an angoing reading.
            if (ptrStartParameters->monitorbusy) {
                Mesytec::listmode::whatnext = Mesytec::listmode::start_reading;
                //hence finally it will exit the reading and go into wait_reading state.
                int i = 0;
                for (i = 0; i < 10; i++) {
                    boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
                    if (ptrStartParameters->monitorbusy == false) break;
                    if (i >= 10) { LOG_WARNING << "start(): " << " Monitorbusy waiting timeout" << std::endl; }
                  
                }
               
            }
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
            //LOG_INFO << "getHistogram()" << std::endl;
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
            .def("monitors_status", &ReplayList::monitors_status)
            .def("log", &ReplayList::log)
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



