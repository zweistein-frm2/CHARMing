/***************************************************************************
 *   Copyright (C) 2019 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/
#pragma once
namespace Mesytec {

	namespace listmode {
		const  char header_separator[] =	{ '\x00','\x00','\x55','\x55','\xAA','\xAA','\xFF','\xFF' };
		const  char datablock_separator[] = { '\x00','\x00','\xFF','\xFF','\x55','\x55','\xAA','\xAA' };
		const  char closing_signature[] =	{ '\xFF','\xFF','\xAA','\xAA','\x55','\x55','\x00','\x00' };
	}

}
