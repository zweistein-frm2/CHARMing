#pragma once



#ifdef ENTANGLE_LOGGER
#include "Entangle.Logger.hpp"
#else
#ifdef SIMPLE_LOGGER
#include "simpleLogger.h"
#else
#define LOG_DEBUG std::cout<<":DEBUG:"
#define LOG_INFO std::cout<<":INFO:"
#define LOG_WARNING std::cout<<":WARN:"
#define LOG_ERROR std::cout<<":ERROR:"

#endif

#endif
