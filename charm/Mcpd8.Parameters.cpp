/***************************************************************************
 *   Copyright (C) 2019 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/
#include <string>
#include "Mcpd8.enums.hpp"
#include "Mcpd8.Parameters.hpp"

namespace Mcpd8 {
	const std::string Parameters::defaultIpAddress = "192.168.168.121";
	const unsigned char Parameters::defaultmcpd_id = 0;
	const unsigned short Parameters::defaultUdpPort = 54321;

}

namespace Charm {
	const unsigned short Parameters::defaultUdpPort = 54340;
}
