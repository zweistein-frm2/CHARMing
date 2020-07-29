/***************************************************************************
  *   Copyright (C) 2019 - 2020 by Andreas Langhoff
  *                                          <andreas.langhoff@frm2.tum.de> *
  *   This program is free software; you can redistribute it and/or modify  *
  *   it under the terms of the GNU General Public License as published by  *
  *   the Free Software Foundation;                                         *
  ***************************************************************************/

#pragma once
#include <boost/thread.hpp>
#include <iostream>
#include <boost/process.hpp>
#include <boost/algorithm/string.hpp>
#include "Zweistein.Logger.hpp"

#ifndef WIN32
#include <sched.h>
#if defined(__cplusplus)
extern "C"
{
#endif
#include <unistd.h>
#include <sys/syscall.h>
#include "libdl/dl_syscalls.h"
#include "libdl/dl_syscalls.c"
#if defined(__cplusplus)
}
#endif

#endif

namespace Zweistein {
	namespace Thread {
		enum PRIORITY {
			REALTIME = 0,
			HIGH = 1,
			ABOVE_NORMAL = 2,
			NORMAL = 3,
			BELOW_NORMAL = 4,
			IDLE = 5
		};
		enum POLICY_TYPE {
			FIFO = 0,
			RR = 1,
			DEADLINE = 3
		};

#ifdef WIN32
#else  // LINUX
		int th_mapPriority[] = { 98, 80, 60, 50, 40, 20 };
		int th_mapPolicy[] = { SCHED_FIFO, SCHED_RR, SCHED_DEADLINE };
#endif
		bool CheckSetUdpKernelBufferSize() {
			namespace bp = boost::process;
#ifdef WIN32
			return true;
#else
			bp::ipstream is;
			bp::ipstream ierr;
			std::string result;
			// Linux			sysctl -w net.core.rmem_max=26214400
			// Solaris			ndd -set /dev/udp udp_max_buf 26214400
			// FreeBSD, Darwin	sysctl -w kern.ipc.maxsockbuf=26214400
			// AIX				no -o sb_max=8388608 (AIX will only allow 1, 4 or 8 MB)
			try {
				std::vector<std::string> data;
				std::string line;
				std::error_code ec;
				boost::filesystem::path p = bp::search_path("sysctl"); //
				std::string cmdline = p.string() + " -a";
				bp::child c(cmdline,bp::std_out>is,bp::std_err>ierr);
				c.wait_for(std::chrono::seconds(2), ec);
				if (c.running()) c.terminate();
				while (std::getline(is, line) && !line.empty()) {
					if(line.find("net.core.rmem_max")!=std::string::npos)	data.push_back(line);
				}

				size_t t=data[0].find_first_of("=");
				long buffersize = std::atol(data[0].substr(t + 1).c_str());

				if (buffersize < 26214400L) {

					bp::ipstream is2;
					bp::ipstream ierr2;
					std::string cmdline = p.string() + " -w net.core.rmem_max=26214400";
					bp::child c(cmdline, bp::std_out > is2,bp::std_err>ierr2);
					bool bdenied = false;
					while (std::getline(ierr2, line) && !line.empty()) {
						if (line.find("permission denied") != std::string::npos) bdenied = true;
					//	std::cout << line << std::endl;
					}
					c.wait_for(std::chrono::seconds(1), ec);
					if (c.running()) c.terminate();

					if (bdenied) {
						LOG_ERROR << " cannot change udp buffer size from " << buffersize << " to 26214400  => check permissions (sudo needed)" << std::endl;
						return false;
					}

				}
				return true;
			}
			catch (boost::exception& e) {

				LOG_ERROR << boost::diagnostic_information(e) << std::endl;
			}
			return false;
#endif
		}

		bool SetThreadPriority(boost::thread::native_handle_type nh, enum PRIORITY priority) {
			bool rv = false;
#ifdef WIN32
			// gets the thread's native handle to increase the thread priority on Windows
			if (priority == Zweistein::Thread::PRIORITY::NORMAL) rv = ::SetThreadPriority(nh, THREAD_PRIORITY_NORMAL);
			if (priority == Zweistein::Thread::PRIORITY::REALTIME) rv = ::SetThreadPriority(nh, THREAD_PRIORITY_TIME_CRITICAL);
			if (priority == Zweistein::Thread::PRIORITY::ABOVE_NORMAL) rv = ::SetThreadPriority(nh, THREAD_PRIORITY_ABOVE_NORMAL);
			if (priority == Zweistein::Thread::PRIORITY::HIGH) rv = ::SetThreadPriority(nh, THREAD_PRIORITY_HIGHEST);
			if (priority == Zweistein::Thread::PRIORITY::BELOW_NORMAL) rv = ::SetThreadPriority(nh, THREAD_PRIORITY_BELOW_NORMAL);
			if (priority == Zweistein::Thread::PRIORITY::IDLE) rv = ::SetThreadPriority(nh, THREAD_PRIORITY_IDLE);
#else	 //LINUX
			sched_param my_param;
			my_param.sched_priority = th_mapPriority[priority];
			pthread_t threadID = (pthread_t)nh;
			int retval = pthread_setschedparam(threadID, th_mapPolicy[POLICY_TYPE::FIFO], &my_param);
			if (retval != 0) {
				LOG_ERROR<< "Cannot set thread priority => check permissions (sudo needed)" << std::endl;
			}
			if (retval == 0) rv = true;
#endif
			return rv;
		}
	}

}
