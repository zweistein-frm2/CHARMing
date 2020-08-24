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

Entangle::severity_level Entangle::SEVERITY_THRESHOLD = Entangle::severity_level::trace;
EXTERN_FUNCDECLTYPE boost::mutex coutGuard;
EXTERN_FUNCDECLTYPE boost::thread_group worker_threads;

Zweistein::Lock histogramsLock;
std::vector<Histogram> histograms;
static_assert(COUNTER_MONITOR_COUNT >= 4, "Mcpd8 Datapacket can use up to 4 counters: params[4][3]");
boost::atomic<unsigned long long> CounterMonitor[COUNTER_MONITOR_COUNT];

boost::scoped_ptr<boost::asio::io_context> ptr_ctx(new boost::asio::io_context);

boost::atomic<bool> startMonitorRunning = false;


void initialize() { LOG_INFO << "initialize()" << std::endl; }
void shutdown() {
    LOG_INFO << "shutdown()" << std::endl;
    ptr_ctx->stop();
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

