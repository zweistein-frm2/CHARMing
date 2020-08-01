/***************************************************************************
  *   Copyright (C) 2019 - 2020 by Andreas Langhoff
  *                                          <andreas.langhoff@frm2.tum.de> *
  *   This program is free software; you can redistribute it and/or modify  *
  *   it under the terms of the GNU General Public License as published by  *
  *   the Free Software Foundation;                                         *
  ***************************************************************************/

#pragma once



#ifdef ENTANGLE_LOGGER
#include "Entangle.Logger.hpp"
#else
#ifdef SIMPLE_LOGGER
#include "simpleLogger.h"
#else
#include <iostream>
#define LOG_DEBUG std::cout<<":DEBUG:"
#define LOG_INFO std::cout<<":INFO:"
#define LOG_WARNING std::cout<<":WARN:"
#define LOG_ERROR std::cerr<<":ERROR:"

#endif

#endif
