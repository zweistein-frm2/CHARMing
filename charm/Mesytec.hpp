/*                          _              _                _
	___  __ __ __  ___     (_)     ___    | |_     ___     (_)    _ _
   |_ /  \ V  V / / -_)    | |    (_-<    |  _|   / -_)    | |   | ' \
  _/__|   \_/\_/  \___|   _|_|_   /__/_   _\__|   \___|   _|_|_  |_||_|
_|"""""|_|"""""|_|"""""|_|"""""|_|"""""|_|"""""|_|"""""|_|"""""|_|"""""|
"`-0-0-'"`-0-0-'"`-0-0-'"`-0-0-'"`-0-0-'"`-0-0-'"`-0-0-'"`-0-0-'"`-0-0-'

   Copyright (C) 2019 - 2020 by Andreas Langhoff
										 <andreas.langhoff@frm2.tum.de>
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation;*/

#pragma once
#include "stdafx.h"
#include <iostream>
#include <bitset>
#include <string_view>
#include "magic_enum/include/magic_enum.hpp"

#define Mpsd8_sizeSLOTS 8
#define Mpsd8_sizeMODID  8
#define Mpsd8_sizeY  1024

#define Mdll_sizeX  960
#define Mdll_sizeY  960

#define Mcpd8_sizeMCPDID 2

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
	enum TimePoint {
		ZERO =0
	};
	class alignas(2) Mpsd8Event {
		uint16_t data[3];
	public:
	//MSB Version
		inline unsigned char ID() const {
			return (data[2] & 0x8000) >> 15;
		}
		inline unsigned char MODID() const {
			return (data[2]>>12)  & 0b111;
		}
		inline unsigned short SLOTID() const {
			return ((data[2] >>7) & 0b11111)&0b111; // Mpsd8 uses only 3 bit others up to 5 bit
		}
		inline unsigned short AMPLITUDE() const {
			unsigned short rv = (data[2] & 0b1111111) << 3;
			rv +=( data[1] >> 13)&0b111;
			return rv;
		}
		inline unsigned short POSITION() const {
			unsigned short rv = (data[1] >>3) &0b1111111111;
			return rv;
		}
		inline boost::chrono::nanoseconds TIMESTAMP() const {
			unsigned long long rv = (unsigned long long)(data[1] & 0b111) << 16;
			rv += data[0];
			boost::chrono::nanoseconds ns(rv * 100);
			return ns;

		}
		static void settime19bit(unsigned short data[3], boost::chrono::nanoseconds nsec) {

			data[0] = nsec.count()/100 & 0xffff;
			data[1] |= (nsec.count()/100 & 0b1110000000000000000) >> 16;

		}

		void print(std::ostream & os) const {
			using namespace magic_enum::ostream_operators;
			auto eventtype = magic_enum::enum_cast<Mesy::EventType>(ID());
			os << std::bitset<16>(data[0]) << " " << std::bitset<16>(data[1]) << " " << std::bitset<16>(data[2]) << std::endl;
			os << "ID:" << eventtype << " MODID:" << (short)MODID() << " SLOTID:" << SLOTID() << " AMPLITUDE:" << AMPLITUDE() << " POSITION:" << POSITION() << " TIMESTAMP:" << TIMESTAMP()<<std::endl;
		}

	};



	class alignas(2) TriggerEvent {
		uint16_t data[3];
	public:
		inline unsigned char ID() const {
			return (data[2] & 0x8000) >> 15;
		}
		inline unsigned char TRIGID() const {
			return (data[2] >> 12) & 0b111;
		}

		inline unsigned char DATAID() const {
			return (data[2] >> 8) & 0b1111;
		}
		inline unsigned int DATA() const {
			unsigned int rv = 0;
			rv = (unsigned int)(data[2]) & 0b11111111 ;
			rv <<= 13;
			rv += data[1] >> 3;
			return rv;
		}
		inline boost::chrono::nanoseconds TIMESTAMP() const {
			unsigned long long rv = (unsigned long long)(data[1] & 0b111) << 16;
			rv += data[0];
			boost::chrono::nanoseconds ns(rv * 100);
			return ns;

		}

		inline static TriggerEvent* fromMpsd8(Mpsd8Event* mpsd8) {
			return reinterpret_cast<TriggerEvent*>(mpsd8);
		}
	};


	class alignas(2) MdllEvent {
		uint16_t data[3];
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
		inline static MdllEvent* fromMpsd8(Mpsd8Event * mpsd8) {
			return reinterpret_cast<MdllEvent *>(mpsd8);
		}
		void print(std::ostream& os) const {
			using namespace magic_enum::ostream_operators;
			auto eventtype = magic_enum::enum_cast<Mesy::EventType>(ID());
			os << std::bitset<16>(data[0]) << " " << std::bitset<16>(data[1]) << " " << std::bitset<16>(data[2]) << std::endl;
			os << "ID:" << eventtype << ", AMPLITUDE:" << (unsigned short)AMPLITUDE() << ", YPOSITION:" << YPOSITION() << ",XPOSITION:" << XPOSITION() << ", TIMESTAMP:" << TIMESTAMP() << std::endl;
		}
	};
}


