#pragma once
#include <boost/thread.hpp>
#include <iostream>
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
		int th_mapPriority[] = { 98, 80, 60, 50, 40, 20 };
		int th_mapPolicy[] = { SCHED_FIFO, SCHED_RR, SCHED_DEADLINE };
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
#else
			sched_param my_param;
			my_param.sched_priority = th_mapPriority[priority];
			pthread_t threadID = (pthread_t)nh;
			int retval = pthread_setschedparam(threadID, th_mapPolicy[POLICY_TYPE::FIFO], &my_param);
			if (retval != 0) {
				std::cout<< std::endl<< "ERROR setting Thread Priority => check permissions (sudo needed)" << std::endl;
			}
			if (retval == 0) rv = true;
#endif
			return rv;
		}
	}

}
