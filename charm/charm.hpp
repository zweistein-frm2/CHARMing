/***************************************************************************
 *   Copyright (C) 2019 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/
#pragma once
#include "stdafx.h"
#include <iostream>
#include <bitset>
#include <string_view>
#include <magic_enum.hpp>


#define Charm_sizeX  960
#define Charm_sizeY  960

namespace Charm {

	class alignas(2) CharmEvent {
		unsigned short data[3];
	public:
		inline unsigned char ID() const {
			return (data[2] & 0x8000) >> 15;
		}
		inline unsigned short AMPLITUDE() const {
			return data[2] & 0x7F80 >> 7;
			/* // code from Qmesydaq measurement.cpp line 1212 ff
			unsigned short slotid = (data[2] >> 7) & 0x1f;
			unsigned short id = (data[2] >> 12) & 0x7;
			unsigned short chan = (id << 5) + slotid;
			return chan;
			*/
		}
		inline unsigned short YPOSITION() const {
			unsigned short rv = (data[2] & 0b1111111) << 3;
			rv += (data[1] >> 13) & 0b111;
			return rv;
		}
		inline unsigned short XPOSITION() const {
			unsigned short rv = (data[1] >> 3) & 0b1111111111;
			return rv;
		}
		inline boost::chrono::nanoseconds TIMESTAMP() const {
			unsigned long long rv = (unsigned long long)(data[1] & 0b111) << 16;
			rv += data[0];
			boost::chrono::nanoseconds ns(rv * 100);
			return ns;
		}
		inline static CharmEvent* fromMpsd8(Mpsd8Event* mpsd8) {
			return reinterpret_cast<CharmEvent*>(mpsd8);
		}
		void print(std::ostream& os) const {
			using namespace magic_enum::ostream_operators;
			auto eventtype = magic_enum::enum_cast<Mesy::EventType>(ID());
			os << std::bitset<16>(data[0]) << " " << std::bitset<16>(data[1]) << " " << std::bitset<16>(data[2]) << std::endl;
			os << "ID:" << eventtype << ", AMPLITUDE:" << (short)AMPLITUDE() << ", YPOSITION:" << YPOSITION() << ",XPOSITION:" << XPOSITION() << ", TIMESTAMP:" << TIMESTAMP() << std::endl;
		}


	};

	

}