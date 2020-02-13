/***************************************************************************
 *   Copyright (C) 2019 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/

// stdafx.h : Include file for standard system include files,
// or project specific include files.

#pragma once
#define BOOST_CHRONO_VERSION 2
#include <iostream>
#include <boost/thread.hpp>
#include <boost/atomic.hpp>
template<class T>
auto operator<<(std::ostream& os, const T& t) -> decltype(t.print(os), os)
{
	t.print(os);
	return os;
}

#ifdef _WIN32
#ifndef EXTERN_FUNCDECLTYPE
#define EXTERN_FUNCDECLTYPE
#endif
#else
#define EXTERN_FUNCDECLTYPE
#endif

EXTERN_FUNCDECLTYPE  extern boost::thread_group worker_threads;
EXTERN_FUNCDECLTYPE extern boost::mutex coutGuard;
// TODO: Reference additional headers your program requires here.
