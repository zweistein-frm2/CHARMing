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
#include <boost/chrono.hpp>
#include <string_view>
#include <magic_enum.hpp>

namespace Mesy {
	enum BufferType {
		DATA = 0,
		COMMAND = 1,
		MDLL = 2
	};
	enum EventType {
		NEUTRON = 0,
		TRIGGER = 1
	};
	enum ModuleId {
		NOMODULE=0,
		MPSD8OLD=1,
		MDLL_TYPE=35,
		MSSD8SADC=102,
		MPSD8=103,
		MPSTD16=104,
		MPSD8P=105,
		MWPCHR=110
	};

	
	class alignas(2) Mpsd8Event {
		unsigned short data[3];
	public:
	//MSB Version
		unsigned char ID() const {
			return (data[2] & 0x8000) >> 15;
		}
		unsigned char MODID() const {
			return (data[2]>>12)  & 0b111;
		}
		unsigned short SLOTID() const {
			return ((data[2] >>7) & 0b11111)&0b111; // Mpsd8 uses only 3 bit others up to 5 bit
		}
		unsigned short AMPLITUDE() const {
			unsigned short rv = (data[2] & 0b1111111) << 3;
			rv +=( data[1] >> 13)&0b111;
			return rv;
		}
		unsigned short POSITION() const {
			unsigned short rv = (data[1] >>3) &0b1111111111;
			return rv;
		}
		unsigned long long TIMESTAMP() const {
			unsigned long long rv = (unsigned long long)(data[1] & 0b111) << 16;
			rv += data[0];
			return rv;
		}
		static void settime19bit(unsigned short data[3],long long nsec) {
			data[0] = nsec/100 & 0xffff;
			data[1] |= (nsec/100 & 0b1110000000000000000) >> 16;

		}
	
		void print(std::ostream & os) const {
			using namespace magic_enum::ostream_operators;
			auto eventtype = magic_enum::enum_cast<Mesy::EventType>(ID());
			os << std::bitset<16>(data[0]) << " " << std::bitset<16>(data[1]) << " " << std::bitset<16>(data[2]) << std::endl;
			os << "ID:" << eventtype << " MODID:" << (short)MODID() << " SLOTID:" << SLOTID() << " AMPLITUDE:" << AMPLITUDE() << " POSITION:" << POSITION() << " TIMESTAMP:" << TIMESTAMP()<<std::endl;
		}
		static const unsigned int sizeSLOTS;
		static const unsigned int sizeMODID;
		static const unsigned int sizeY;
	};
	const unsigned int  Mpsd8Event::sizeSLOTS = 8;
	const unsigned int  Mpsd8Event::sizeMODID = 8;
	const unsigned int  Mpsd8Event::sizeY = 1024;

	class alignas(2) CharmEvent {
		unsigned short buf[3];
	};

	class alignas(2) MdllEvent {
		unsigned short data[3];
	public:
		unsigned char ID() const {
			return (data[2] & 0x8000) >> 15;
		}
		unsigned short AMPLITUDE() const {
			return data[2] & 0x7F80 >> 7;
			/* // code from Qmesydaq measurement.cpp line 1212 ff
			unsigned short slotid = (data[2] >> 7) & 0x1f;
			unsigned short id = (data[2] >> 12) & 0x7;
			unsigned short chan = (id << 5) + slotid;
			return chan;
			*/
		}
		unsigned short YPOSITION() const {
			unsigned short rv = (data[2] & 0x7f) << 3;
			rv += (data[1] >> 13) & 0x7;
			return rv;
		}
		unsigned short XPOSITION() const {
			unsigned short rv = (data[1] >> 3) & 0x3ff;
			return rv;
		}
		unsigned long long TIMESTAMP() const {
			unsigned long long rv = (unsigned long long)(data[1] & 0x7) << 16;
			rv += data[0];
			return rv;
		}
		static MdllEvent* fromMpsd8(Mpsd8Event * mpsd8) {
			return reinterpret_cast<MdllEvent *>(mpsd8);
		}
		void print(std::ostream& os) const {
			using namespace magic_enum::ostream_operators;
			auto eventtype = magic_enum::enum_cast<Mesy::EventType>(ID());
			os << std::bitset<16>(data[0]) << " " << std::bitset<16>(data[1]) << " " << std::bitset<16>(data[2]) << std::endl;
			os << "ID:" << eventtype << ", AMPLITUDE:" << (short)AMPLITUDE() << ", YPOSITION:" << YPOSITION() << ",XPOSITION:" << XPOSITION() << ", TIMESTAMP:" << TIMESTAMP() << std::endl;
		}

		static const unsigned int sizeX;
		static const unsigned int sizeY;
	};
	const unsigned int  MdllEvent::sizeX = 960;
	const unsigned int  MdllEvent::sizeY = 960;
}
namespace Mcpd8 {
	const unsigned short sizeMCPDID = 2;
}