/***************************************************************************
 *   Copyright (C) 2019 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/

#pragma once
#include <string_view>
#include <magic_enum.hpp>

namespace Mesytec {

	enum DataGenerator {
		Undefined = 0,
		Mcpd8 = 1,
		NucleoSimulator = 2,
		Charm = 3,
		CharmSimulator = 4
	};

}

