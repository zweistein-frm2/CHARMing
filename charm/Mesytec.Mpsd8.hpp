/***************************************************************************
  *   Copyright (C) 2019  by Andreas Langhoff
  *                                          <andreas.langhoff@frm2.tum.de> *
  *   This program is free software; you can redistribute it and/or modify  *
  *   it under the terms of the GNU General Public License as published by  *
  *   the Free Software Foundation;                                         *
  ***************************************************************************/


#pragma once
#include "Mcpd8.enums.hpp"
namespace Mesytec {
	namespace Mpsd8 {
		enum PulserPosition {
			left = 0,
			right = 1,
			middle = 2
		};
		enum Mode {
			Position = 0,
			Amplitude=1
		};

		class Module {

		public:
			static Mcpd8::TX_CAP tx_cap_default;
			unsigned char gain[8];
			unsigned char threshold;
			unsigned char pulseramplitude;
			unsigned char pulserchannel;
			PulserPosition position;
			unsigned char pulseron;
			Mode mode;
			unsigned short reg;
			unsigned short tx_caps;
			Mcpd8::TX_CAP tx_cap_setting;
			unsigned short firmware;
			Module():threshold(128),firmware(0x0102),
				tx_caps(Mcpd8::TX_CAP::P | Mcpd8::TX_CAP::TP | Mcpd8::TX_CAP::TPA),
				tx_cap_setting(tx_cap_default),mode(Mode::Position){}
		};


	}

}