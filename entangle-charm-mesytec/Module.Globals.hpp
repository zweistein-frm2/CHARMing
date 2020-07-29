Entangle::severity_level Entangle::SEVERITY_THRESHOLD = Entangle::severity_level::trace;
EXTERN_FUNCDECLTYPE boost::mutex coutGuard;
EXTERN_FUNCDECLTYPE boost::thread_group worker_threads;

Zweistein::Lock histogramsLock;
std::vector<Histogram> histograms;
static_assert(COUNTER_MONITOR_COUNT >= 4, "Mcpd8 Datapacket can use up to 4 counters: params[4][3]");
boost::atomic<unsigned long long> CounterMonitor[COUNTER_MONITOR_COUNT];

boost::asio::io_service io_service;




void initialize() { LOG_DEBUG << "initialize()" << std::endl; }
void shutdown() {
    LOG_DEBUG << "shutdown()" << std::endl;
    io_service.stop();
    boost::this_thread::sleep_for(boost::chrono::milliseconds(450));
    //worker_threads.join_all();

}

#if (PY_VERSION_HEX >= 0x03000000)

static void* init_ar() {
#else
static void init_ar() {
#endif
    Py_Initialize();

    import_array();
    return NUMPY_IMPORT_ARRAY_RETVAL;
}

