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
#include "Module.Globals.hpp"

std::string PROJECT_NAME("CHARMing");
namespace p = boost::python;
namespace np = boost::python::numpy;
using boost::asio::ip::udp;




struct StartMsmtParameters {
    StartMsmtParameters() :MaxCount(0), DurationSeconds(0), playlist(std::list<std::string>()),  monitorbusy(false) {}
    boost::atomic<long long> MaxCount;
    boost::atomic<double> DurationSeconds;
    boost::atomic<int> speedmultiplier;
    std::list<std::string> playlist;
    boost::mutex playlistGuard;
    boost::atomic<int> monitorbusy;
    boost::atomic<unsigned long long> CurrentCounts;
    boost::atomic<double> SecondsAcquiring;
};

void startMonitor(boost::shared_ptr < Mesytec::MesytecSystem> ptrmsmtsystem1, boost::shared_ptr <StartMsmtParameters> ptrStartParameters ) {

        using namespace magic_enum::bitwise_operators; // out-of-the-box bitwise operators for enums.


        boost::asio::signal_set signals(*ptr_ctx, SIGINT, SIGTERM);
        signals.async_wait(boost::bind(&boost::asio::io_service::stop, & *ptr_ctx));

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
                                ptrmsmtsystem1->evdata.evntcount = 0;
                                for (int i = 0; i < COUNTER_MONITOR_COUNT; i++) CounterMonitor[i] = 0;

                                LOG_INFO << "data.evntcount = 0, Monitors=0" << std::endl;
                                ptrmsmtsystem1->daq_running = true;

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
                                ptrmsmtsystem1->daq_running = false;
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
                            ptrmsmtsystem1->eventdataformat = Zweistein::Format::EventData::Undefined;
                            ptrmsmtsystem1->listmode_connect(_devlist, *ptr_ctx);
                            // find the .json file for the listmode file
                            // check if Binning file not empty, if not empty wait for
                            try {
                                Zweistein::setupHistograms(*ptr_ctx, ptrmsmtsystem1, Mesytec::Config::BINNINGFILE.string());
                                bool ok = ptrmsmtsystem1->evdata.evntqueue.push(Zweistein::Event::Reset());
                                if (!ok) LOG_ERROR << " cannot push Zweistein::Event::Reset()" << std::endl;
                                boost::function<void(Mcpd8::DataPacket&)> abfunc = boost::bind(&Mesytec::MesytecSystem::analyzebuffer, ptrmsmtsystem1, boost::placeholders::_1);
                                boost::shared_ptr <Mesytec::listmode::Read> ptrRead = boost::shared_ptr < Mesytec::listmode::Read>(new Mesytec::listmode::Read(abfunc, ptrmsmtsystem1->data, ptrmsmtsystem1->deviceparam));
                                // pointer to obj needed otherwise exceptions are not propagated properly

                                ptrRead->file(fname, *ptr_ctx);
                                while (ptrmsmtsystem1->evdata.evntqueue.read_available()); // wait unitl queue consumed


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
                    } while (!ptr_ctx->stopped());

                }
                catch (boost::exception& e) {
                    LOG_ERROR << boost::diagnostic_information(e) << std::endl;
                }

            };



        auto pt = new boost::thread(boost::bind(t));
        worker_threads.add_thread(pt);

        worker_threads.create_thread([&ptrmsmtsystem1] {Zweistein::populateHistograms(*ptr_ctx, ptrmsmtsystem1); });

        // nothing to do really,
        while (!ptr_ctx->stopped()) {

            long long currcount = ptrmsmtsystem1->evdata.evntcount;
            long long maxcount = ptrStartParameters->MaxCount;
            boost::chrono::system_clock::time_point tps = ptrmsmtsystem1->getStart();
            boost::chrono::duration<double> secs = boost::chrono::system_clock::now() - tps;
            boost::chrono::duration<double> Maxsecs(ptrStartParameters->DurationSeconds);
            bool running = ptrmsmtsystem1->daq_running;
            if (running) {
                bool sendstop = false;
                if (maxcount != 0 && currcount > maxcount) {
                    LOG_INFO << "stopped by counter maxval:" << currcount << ">max:" << maxcount << std::endl;
                    sendstop = true;
                    //ptrStartParameters->MaxCount = 0; // we disarm it
                }
                if (secs >= Maxsecs && Maxsecs != boost::chrono::duration<double>::zero()) {
                    LOG_INFO << "stopped by timer:" << secs << "> max:" << Maxsecs << std::endl;
                    sendstop = true;
                    //ptrStartParameters->DurationSeconds = 0;

                }

                if (sendstop) {
                    Mesytec::listmode::whatnext = Mesytec::listmode::action::wait_reading;
                    ptrmsmtsystem1->daq_running = false;
                }
            }
            boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
            std::chrono::milliseconds maxblocking(100);
            ptr_ctx->run_one_for(maxblocking);

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
            ptrmsmtsystem1->evdata.evntcount = 0;
            auto now = boost::chrono::system_clock::now();
            ptrmsmtsystem1->setStart(now);
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

        void clear_monitor(int i) {

            if (i < 0 || i >= COUNTER_MONITOR_COUNT) {
                LOG_INFO << "clear_monitor: i (" << i << ") outside range." << std::endl;
                return;
            }
            CounterMonitor[i] = 0;
        }

        boost::python::tuple status() {

            boost::chrono::system_clock::time_point tps=ptrmsmtsystem1->getStart();
            boost::chrono::duration<double> secs = boost::chrono::system_clock::now() - tps;
            long long count = ptrmsmtsystem1->evdata.evntcount;
            unsigned short tmp = ptrmsmtsystem1->data.last_deviceStatusdeviceId;
            bool running = ptrmsmtsystem1->daq_running;
            if (running) {
                //LOG_INFO << "Running" << std::endl;
                tmp |= Mcpd8::Status::DAQ_Running;
            }
            unsigned char devstatus = Mcpd8::DataPacket::getStatus(tmp);
            std::string msg = Mcpd8::DataPacket::deviceStatus(devstatus);
            //LOG_INFO << "started:"<<tps<<", now="<< boost::chrono::system_clock::now()<< ":"<<secs<<std::endl;
            //LOG_INFO <<"count="<<count<<", elapsed="<<secs << ",devstatus=" << (int)devstatus << ", " << msg<<std::endl;
            return boost::python::make_tuple(count, secs.count(), devstatus,msg);

        }
        std::string LISTMODEEXTENSION = ".mdat";
        boost::python::list files(std::string directory) {

            boost::python::list l;
            if (directory.empty()) return l;

            boost::replace_all(directory, "\\\\", "\\");
            boost::replace_all(directory, "\\", "/");
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
                            std::string file = entry.path().string();
                            LOG_DEBUG << __FILE__ << " : " << __LINE__ << " append " << file << std::endl;
                            boost::replace_all(file, "\\\\", "\\");
                            boost::replace_all(file, "\\", "/");
                            LOG_INFO << file  <<"("<< file.length() <<")"<< std::endl;
                            l.append(file);
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

            if (!file.empty()) {
                boost::replace_all(file, "\\\\", "\\");
                boost::replace_all(file, "\\", "/");

                LOG_INFO << "addfile(" << file << ")" << std::endl;
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
                }
            }
            boost::python::list l;

            {
                boost::mutex::scoped_lock lock(ptrStartParameters->playlistGuard);
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
            boost::replace_all(file, "\\\\", "\\");
            boost::replace_all(file, "\\", "/");

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

        void clear_counter() {
            LOG_INFO << "ReplayList::clear_counter() " << std::endl;
            ptrmsmtsystem1->evdata.evntcount = 0;
        }

        void clear_timer() {
            LOG_INFO << "ReplayList::clear_timer() " << std::endl;
            auto now = boost::chrono::system_clock::now();
            ptrmsmtsystem1->setStart(now);
        }

        void start() {
            using namespace magic_enum::ostream_operators;
            // we have to start from beginning of playlist
            auto now = boost::chrono::system_clock::now();
            ptrmsmtsystem1->setStart(now);
            Mesytec::listmode::action ac = Mesytec::listmode::whatnext;
            LOG_INFO << "ReplayList::start() " << ac << std::endl;

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
            ptrmsmtsystem1->daq_running = false;
        }
        void resume() {
            LOG_INFO << "ReplayList::resume()" << std::endl;
            ptrmsmtsystem1->daq_running = true;
            Mesytec::listmode::whatnext = Mesytec::listmode::continue_reading;
        }

        Histogram* getHistogram(int index = 0) {
            using namespace magic_enum::bitwise_operators; // out-of-the-box bitwise operators for enums.
            //LOG_DEBUG << "getHistogram()" << std::endl;
            if (index < 0 || index>1) index = 0;
            if (index == 1) return &histograms[0];
            Zweistein::histogram_setup_status hss = Zweistein::setup_status;
            if (magic_enum::enum_integer(hss & Zweistein::histogram_setup_status::has_binning)) {
                return &histograms[1];
            }
            return &histograms[0];
        }


    };

    BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(ReplayList_overloads, getHistogram, 0, 1)
#include "Module.legacy_api_guard.hpp"


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
            .def("getRoiData", &Histogram::getRoiData)
            .def("setRoi", &Histogram::setRoi)
            .def("getRoi", &Histogram::getRoi)
            .def("clear", &Histogram::clear)
            .add_property("Size", &Histogram::getSize)
            .add_property("nextRAW", &Histogram::get_nextRAW, &Histogram::set_nextRAW)
            ;
        class_< ReplayList>("ReplayList",init<long>())
            .add_property("speedmultiplier", &ReplayList::get_speedmultiplier, &ReplayList::set_speedmultiplier)
            .add_property("version", &ReplayList::get_version)
            .def("addfile", &ReplayList::addfile)
            .def("removefile", &ReplayList::removefile)
            .def("files", &ReplayList::files)
            .def("clear_counter", &ReplayList::clear_counter)
            .def("clear_timer", &ReplayList::clear_timer)
            .def("stopafter", &ReplayList::stopafter)
            .def("start", &ReplayList::start)
            .def("resume", &ReplayList::resume)
            .def("status", &ReplayList::status)
            .def("monitors_status", &ReplayList::monitors_status)
            .def("clear_monitor", &ReplayList::clear_monitor)
            .def("log", &ReplayList::log)
            .def("stop", &ReplayList::stop)
            .def("getHistogram", &ReplayList::getHistogram, ReplayList_overloads(args("index") = 0)[return_internal_reference<1, with_custodian_and_ward_postcall<1, 0>>()])
            ;

        import("atexit").attr("register")(make_function(&::release_guard));


    }
    catch (const error_already_set&)
    {
        ::release_guard();
        throw;
    }



}



