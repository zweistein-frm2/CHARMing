/***************************************************************************
 *   Copyright (C) 2019 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/
#pragma once
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>
namespace Zweistein {
	typedef boost::shared_mutex Lock;
	typedef boost::unique_lock< Lock >  WriteLock;
	typedef boost::shared_lock< Lock >  ReadLock;
}

