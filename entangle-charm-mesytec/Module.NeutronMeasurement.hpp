struct NeutronMeasurement {
public:

    boost::shared_ptr<StartMsmtParameters> ptrStartParameters;
    boost::shared_ptr < MSMTSYSTEM> ptrmsmtsystem1;
    NeutronMeasurement(long loghandle) :ptrStartParameters(boost::shared_ptr < StartMsmtParameters>(new StartMsmtParameters())),
        ptrmsmtsystem1(boost::shared_ptr < MSMTSYSTEM>(new MSMTSYSTEM()))
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

    void stopafter(uint64 counts, double seconds) {
        ptrStartParameters->MaxCount = counts;
        ptrStartParameters->DurationSeconds = seconds;
        LOG_INFO << "NeutronMeasurement.stopafter(" << counts << ", " << seconds << ")" << std::endl;
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

        boost::chrono::system_clock::time_point tps = ptrmsmtsystem1->started;
        boost::chrono::duration<double> secs = boost::chrono::system_clock::now() - tps;
        long long count = ptrmsmtsystem1->evdata.evntcount;
        unsigned short tmp = ptrmsmtsystem1->data.last_deviceStatusdeviceId;
        unsigned char devstatus = Mcpd8::DataPacket::getStatus(tmp);
        std::string msg = Mcpd8::DataPacket::deviceStatus(devstatus);
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
                worker_threads.create_thread([this] {Mesytec::writeListmode(io_service, ptrmsmtsystem1); });
            }
            ptrmsmtsystem1->SendAll(Mcpd8::Cmd::START);
            set_simulatorRate(get_simulatorRate());
        }
        catch (boost::exception& e) { LOG_ERROR << boost::diagnostic_information(e) << std::endl; }

    }
    void stop() {
        try { ptrmsmtsystem1->SendAll(Mcpd8::Cmd::STOP); }
        catch (boost::exception& e) { LOG_ERROR << boost::diagnostic_information(e) << std::endl; }
    }
    void resume() {
        try { ptrmsmtsystem1->SendAll(Mcpd8::Cmd::CONTINUE); }
        catch (boost::exception& e) { LOG_ERROR << boost::diagnostic_information(e) << std::endl; }
    }

    Histogram* getHistogram() {
        using namespace magic_enum::bitwise_operators; // out-of-the-box bitwise operators for enums.
        //LOG_DEBUG << "getHistogram()" << std::endl;
        Zweistein::histogram_setup_status hss = Zweistein::setup_status;
        if (magic_enum::enum_integer(hss & Zweistein::histogram_setup_status::has_binning)) {
            return &histograms[1];
        }
        return &histograms[0];
    }

};