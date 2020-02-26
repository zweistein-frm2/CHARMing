#pragma once
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>
namespace Zweistein {
	typedef boost::shared_mutex Lock;
	typedef boost::unique_lock< Lock >  WriteLock;
	typedef boost::shared_lock< Lock >  ReadLock;
}

