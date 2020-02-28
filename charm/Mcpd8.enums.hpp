/***************************************************************************
 *   Copyright (C) 2019 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/
#pragma once
#include <string_view>
#include <magic_enum.hpp>

namespace Mcpd8 {
	enum EventDataFormat {
		Undefined = 0,
		Mpsd8 = 1,
		Mdll = 2,
		Charm = 3
	};
	enum Status {
		DAQ_Running = 1,
		sync_ok = 2,
		
	};
	enum Cmd {
		RESET = 0,
		START = 1,
		STOP = 2,
		CONTINUE = 3,
		SETID = 4,
		SETPROTOCOL = 5,
		SETTIMING = 6,
		SETCLOCK = 7,
		SETRUNID = 8,
		SETCELL = 9,
		SETAUXTIMER = 10,
		SETPARAMETERS = 11,
		GETPARAMETERS = 12,
		SETGAIN_MPSD = 13,
		SETTHRESH = 14,
		SETPULSER = 15,
		SETMODE = 16,
		SETDAC = 17,
		SENDSERIAL = 18,
		READSERIAL = 19,
		SCANPERI = 20,
		GETCAPABILITIES = 22,
		SETCAPABILITIES = 23,
		GETMPSD8PLUSPARAMETERS = 24,
		SETGAIN_MSTD = 26
	};
	enum Internal_Cmd {
		WRITEFPGA = 30,
		WRITEREGISTER = 31,
		READREGISTER = 32,
		READFPGA = 33,
		SETPOTI = 34,
		GETPOTI = 35,
		READID = 36,
		DATAREQUEST = 37,
		QUIET = 50,
		GETVER = 51,
		READPERIREG = 52,
		WRITEPERIREG = 53,
		CHARMPATTERNGENERATOR=70,
		CHARMSETEVENTRATE=71,
		SETNUCLEORATEEVENTSPERSECOND = 0xF1F0

	};

	enum TX_CAP {

		P=1,
		TP=2,
		TPA=4

	};
		
	/*
namespace magic_enum {
	template <>
	struct enum_range<Mcpd8::Internal_Cmd> {
		static constexpr int min = 30;
		static constexpr int max = 0xF1F0;
	};
}
*/
}

