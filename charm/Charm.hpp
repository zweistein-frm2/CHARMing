/*                          _              _                _
	___  __ __ __  ___     (_)     ___    | |_     ___     (_)    _ _
   |_ /  \ V  V / / -_)    | |    (_-<    |  _|   / -_)    | |   | ' \
  _/__|   \_/\_/  \___|   _|_|_   /__/_   _\__|   \___|   _|_|_  |_||_|
	   .
	   |\       Copyright (C) 2019 - 2020 by Andreas Langhoff
	 _/]_\_                            <andreas.langhoff@frm2.tum.de>
 ~~~"~~~~~^~~   This program is free software; you can redistribute it
 and/or modify it under the terms of the GNU General Public License v3
 as published by the Free Software Foundation;*/

#pragma once
#include "stdafx.h"
#include <iostream>
#include <bitset>
#include <string_view>
#include "magic_enum/include/magic_enum.hpp"
#include "Mesytec.hpp"


#define Charm_sizeX  1024
#define Charm_sizeY  1024

namespace Charm {
	class  proposal9june2020_Packet
	{
	public:
		// we define all struct elements as uint16_t, so that endianness can be converted easier
		uint16_t Length;	//!< length of the buffer in words
		uint16_t sessionid;
		uint16_t Type;	//!< the buffer type
		uint16_t headerLength;	//!< the length of the header in words
		int64_t timestamp;
		uint16_t Number;	//!< number of the packet
		uint16_t deviceStatus;	//!< the device state
		uint16_t dummy0;
		uint16_t dummy1 ;
		typedef struct {
			uint16_t data[3];
		} mesytec48;
		union {
			int64_t s64[184];
			mesytec48 u48 [245];
			int32_t s32[368];
			uint16_t u16[736];
		} data;
	};

	class alignas(2) CharmEvent {
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
		inline static CharmEvent* fromMpsd8(Mesy::Mpsd8Event* mpsd8) {
			return reinterpret_cast<CharmEvent*>(mpsd8);
		}
		void print(std::ostream& os) const {
			using namespace magic_enum::ostream_operators;
			auto eventtype = magic_enum::enum_cast<Mesy::EventType>(ID());
			os << std::bitset<16>(data[0]) << " " << std::bitset<16>(data[1]) << " " << std::bitset<16>(data[2]) << std::endl;
			os << "ID:" << eventtype << ", AMPLITUDE:" << (unsigned short)AMPLITUDE() << ", YPOSITION:" << YPOSITION() << ",XPOSITION:" << XPOSITION() << ", TIMESTAMP:" << TIMESTAMP() << std::endl;
		}


	};



}
