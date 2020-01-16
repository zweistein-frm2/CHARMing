/***************************************************************************
 *   Copyright (C) 2019 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/


#pragma once

namespace Zweistein {

	unsigned char reverse(unsigned char b) {
		return (unsigned char)(b * 0x0202020202ULL & 0x010884422010ULL) % 1023;
	}
	unsigned char reverse_u8(unsigned char x)
	{
		const unsigned char rev[] = "\x0\x8\x4\xC\x2\xA\x6\xE\x1\x9\x5\xD\x3\xB\x7\xF";
		return rev[(x & 0xF0) >> 4] | (rev[x & 0x0F] << 4);
	}

	// reverse a word
	unsigned short reverse_u16(unsigned short x)
	{
		return reverse_u8(x >> 8) | (reverse_u8(x & 0xFF) << 8);
	}

	// reverse a long
	unsigned long reverse_u32(unsigned long x)
	{
		return reverse_u16(x >> 16) | (reverse_u16(x & 0xFFFF) << 16);
	}



}
