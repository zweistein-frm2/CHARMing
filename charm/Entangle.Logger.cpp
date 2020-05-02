/***************************************************************************
 *   Copyright (C) 2019 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/

#include "Entangle.Logger.hpp"

boost::iostreams::stream< boost::iostreams::null_sink > nullOstream((boost::iostreams::null_sink()));

void Entangle::Init(int fd) {
	try {
		ptrlogger = boost::shared_ptr < Entangle::Logger>(new Logger(fd));

	}
	catch (boost::exception& e) {
		std::cerr << boost::diagnostic_information(e) << std::endl;
	}
}

boost::shared_ptr <Entangle::Logger> Entangle::ptrlogger = nullptr;





