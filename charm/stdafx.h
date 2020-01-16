/***************************************************************************
 *   Copyright (C) 2019 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/

// stdafx.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <iostream>
#include <boost/thread.hpp>
#include <boost/atomic.hpp>
template<class T>
auto operator<<(std::ostream& os, const T& t) -> decltype(t.print(os), os)
{
	t.print(os);
	return os;
}

extern boost::mutex coutGuard;
extern boost::thread_group worker_threads;

// TODO: Reference additional headers your program requires here.
