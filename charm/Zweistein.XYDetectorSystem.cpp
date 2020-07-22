#include "Zweistein.XYDetectorSystem.hpp"

namespace Zweistein {

	void XYDetectorSystem::initatomicortime_point() {
		auto now = boost::chrono::system_clock::now();
		started = now;
		stopped = now;
		simulatordatarate = 51;
		connected = false;
		daq_running = false;
		write2disk = false;
		auto max = boost::chrono::system_clock::time_point::max();
		lasteventqueuefull = max;


	}
}