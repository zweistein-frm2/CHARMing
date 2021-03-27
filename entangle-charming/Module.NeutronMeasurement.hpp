/*                          _              _                _
    ___  __ __ __  ___     (_)     ___    | |_     ___     (_)    _ _
   |_ /  \ V  V / / -_)    | |    (_-<    |  _|   / -_)    | |   | ' \
  _/__|   \_/\_/  \___|   _|_|_   /__/_   _\__|   \___|   _|_|_  |_||_|
       .
       |\       Copyright (C) 2019 - 2020 by Andreas Langhoff
     _/]_\_                            <andreas.langhoff@frm2.tum.de>
 ~~~"~~~~~^~~   This program is free software; you can redistribute it
 and/or modify it under the terms of the GNU General Public License as
 published by the Free Software Foundation;*/

boost::thread_group control_threads;

struct NeutronMeasurement {
public:

    boost::shared_ptr<StartMsmtParameters> ptrStartParameters;
    boost::shared_ptr < MSMTSYSTEM> ptrmsmtsystem1;
    std::string cmderror;
    NeutronMeasurement(long loghandle): cmderror(""),ptrStartParameters(boost::shared_ptr < StartMsmtParameters>(new StartMsmtParameters())),
        ptrmsmtsystem1(boost::shared_ptr < MSMTSYSTEM>(new MSMTSYSTEM()))
    {
        Entangle::Init(loghandle);
        histograms = std::vector<Histogram>(2);
        LOG_INFO << "NeutronMeasurement(" << loghandle << ")" << std::endl;
        LOG_INFO << get_version() << std::endl;
    }
    ~NeutronMeasurement() {
        LOG_INFO << "~NeutronMeasurement()" << std::endl;

    }

    void off() {
        LOG_DEBUG << std::endl << "off(): begin." << std::endl;
        cancelrequested = true;
        control_threads.join_all();
        LOG_INFO << "off(): end." << "io_service.stopped() = " << ptr_ctx->stopped() << std::endl;

    }

    void on(){
        LOG_INFO << "on(): " << std::endl;
       /* if (control_threads.size()) {
            LOG_WARNING << "on(): startMonitor aready on." << std::endl;
            return;
        }
        */
        bool bwait = true;
        for (int i = 0; i < 20; i++) {
            bwait = startMonitorRunning;
            if (bwait) break;
            boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
        }
        if (bwait) {
            LOG_WARNING << "on(): startMonitor aready on." << std::endl;
            return;
        }
        cmderror = "";
        control_threads.create_thread([this] {startMonitor(ptrmsmtsystem1, ptrStartParameters); });

    }
    bool get_writelistmode() {
        return ptrStartParameters->writelistmode;
    }
    void set_writelistmode(bool val) {
        ptrStartParameters->writelistmode = val;
    }
    long get_simulatorRate() {
        long rate = ptrmsmtsystem1->simulatordatarate;
        return rate;
    }
    void set_simulatorRate(long rate) {
        for (auto& kvp : ptrmsmtsystem1->deviceparam) {
            if (kvp.second.datagenerator == Zweistein::DataGenerator::NucleoSimulator) {
                ptrmsmtsystem1->Send(kvp, Mcpd8::Internal_Cmd::SETNUCLEORATEEVENTSPERSECOND, rate);//1650000 is maximum
            }
            if (kvp.second.datagenerator == Zweistein::DataGenerator::CharmSimulator) {
                ptrmsmtsystem1->Send(kvp, Mcpd8::Internal_Cmd::CHARMSETEVENTRATE, rate); // oder was du willst
                ptrmsmtsystem1->Send(kvp, Mcpd8::Internal_Cmd::CHARMPATTERNGENERATOR, 1); // oder was du willst
            }
        }
    }

    void clear_counter() {
        LOG_INFO << "NeutronMeasurement::clear_counter() " << std::endl;
        ptrmsmtsystem1->evdata.evntcount = 0;
    }

    void clear_timer() {
        LOG_INFO << "NeutronMeasurement::clear_timer() " << std::endl;
        auto now = boost::chrono::system_clock::now();
        ptrmsmtsystem1->setStart(now);
    }

    void stopafter(uint64 counts, double seconds) {
        ptrStartParameters->MaxCount = counts;
        ptrStartParameters->DurationSeconds = seconds;
        LOG_INFO << "NeutronMeasurement.stopafter(" << counts << " cts, " << seconds << " seconds)" << std::endl;
    }

    boost::python::list log() {
        boost::python::list l;
        {
           Zweistein::ReadLock r_lock(Entangle::cbLock); //circular buffer lock

            for (auto iter = Entangle::ptrcb->begin(); iter != Entangle::ptrcb->end(); iter++) {

                std::vector<std::string> strs;
                boost::split(strs, *iter, boost::is_any_of("\n"));
                for (auto& s : strs) l.append(s);
            }

        }
        return l;
    }

    void clear_monitor(int i) {

        if (i < 0 || i >= COUNTER_MONITOR_COUNT) {
            LOG_INFO << "clear_monitor: i (" << i << ") outside range." << std::endl;
            return;
        }
        CounterMonitor[i] = 0;
    }

    boost::python::list monitors_status() {
        boost::python::list l2;
        for (int i = 0; i < COUNTER_MONITOR_COUNT; i++) {
            unsigned long long val = CounterMonitor[i];
            auto t = boost::python::make_tuple("MONITOR" + std::to_string(i), val);
            l2.append(t);
        }
        return l2;
    }
    boost::python::tuple status() {
        boost::chrono::duration<double> secs;
        boost::chrono::system_clock::time_point tps = ptrmsmtsystem1->getStart();
        long long count = ptrmsmtsystem1->evdata.evntcount;
        unsigned short tmp = ptrmsmtsystem1->data.last_deviceStatusdeviceId;
        unsigned char devstatus = Mcpd8::DataPacket::getStatus(tmp);
        std::string msg = Mcpd8::DataPacket::deviceStatus(devstatus);


        bool running = ptrmsmtsystem1->daq_running;

        if (running) {
            //LOG_INFO << "Running" << std::endl;
                secs = boost::chrono::system_clock::now() - tps ;
        }
        else {
            boost::chrono::system_clock::time_point tpe = ptrmsmtsystem1->stopped;
            secs = tpe - tps;
        }


        if (!cmderror.empty()) {
            return boost::python::make_tuple(count, secs.count(), devstatus, msg,cmderror);
        }
        //LOG_INFO << "started:"<<tps<<", now="<< boost::chrono::system_clock::now()<< ":"<<secs<<std::endl;
       // LOG_INFO <<"count="<<count<<", elapsed="<<secs << ",devstatus=" << (int)devstatus << ", " << msg<<std::endl;
        return boost::python::make_tuple(count, secs.count(), devstatus, msg);

    }

    std::string get_version() {
        std::stringstream ss;
        //ss << PROJECT_NAME << " : BRANCH: " << GIT_BRANCH << " LATEST TAG:" << GIT_LATEST_TAG << " commits since:" << GIT_NUMBER_OF_COMMITS_SINCE << " " << GIT_DATE << std::endl;
        std::string git_latest_tag(GIT_LATEST_TAG);
        git_latest_tag.erase(std::remove_if(git_latest_tag.begin(), git_latest_tag.end(), (int(*)(int)) std::isalpha), git_latest_tag.end());
        ss << PROJECT_NAME << " : " << git_latest_tag << "." << GIT_NUMBER_OF_COMMITS_SINCE << "." << GIT_REV << "_" << GIT_DATE;
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
                worker_threads.create_thread([this] {Mesytec::writeListmode(*ptr_ctx, ptrmsmtsystem1); });
            }
            ptrmsmtsystem1->SendAll(Mcpd8::Cmd::START);
            set_simulatorRate(get_simulatorRate());
        }
        catch (Mesytec::cmd_error& x) {
            cmderror = Mesytec::cmd_errorstring(x);
            LOG_ERROR << cmderror << std::endl;
        }
        catch (boost::exception& e) {
            LOG_ERROR << boost::diagnostic_information(e) << std::endl;
        }

    }
    void stop() {
        try { ptrmsmtsystem1->SendAll(Mcpd8::Cmd::STOP); }
        catch (Mesytec::cmd_error& x) {
                cmderror = Mesytec::cmd_errorstring(x);
                LOG_ERROR << cmderror << std::endl;
        }
        catch (boost::exception& e) { LOG_ERROR << boost::diagnostic_information(e) << std::endl; }
    }
    void resume() {
        try { ptrmsmtsystem1->SendAll(Mcpd8::Cmd::CONTINUE); }
        catch (Mesytec::cmd_error& x) {
            cmderror = Mesytec::cmd_errorstring(x);
            LOG_ERROR << cmderror << std::endl;
        }
        catch (boost::exception& e) { LOG_ERROR << boost::diagnostic_information(e) << std::endl; }
    }

    Histogram* getHistogram(int index=0) {
        using namespace magic_enum::bitwise_operators; // out-of-the-box bitwise operators for enums.
        //LOG_DEBUG << "getHistogram()" << std::endl;
        if (index < 0 || index>1) index = 0;
        if(index==1) return &histograms[0];
        Zweistein::histogram_setup_status hss = Zweistein::setup_status;
        if (magic_enum::enum_integer(hss & Zweistein::histogram_setup_status::has_binning)) {
            return &histograms[1];
        }
        return &histograms[0];
    }


};
BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(NeutronMeasurements_overloads, getHistogram, 0, 1)
