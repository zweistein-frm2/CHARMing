#pragma once
#include <boost/atomic.hpp>
#define COUNTER_MONITOR_COUNT 4
extern boost::atomic<unsigned long long> CounterMonitor[COUNTER_MONITOR_COUNT];