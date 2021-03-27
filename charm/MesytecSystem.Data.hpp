/***************************************************************************
 *   Copyright (C) 2019 - 2020 by Andreas Langhoff
 *                                          <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/

#pragma once

#include "Zweistein.Data.hpp"
#include "Mcpd8.DataPacket.hpp"

namespace Mcpd8 {
	class Data {
	public:

		boost::atomic<unsigned short> last_deviceStatusdeviceId = Mcpd8::Status::sync_ok;
		boost::atomic<long long> listmodepacketcount = 0;
		boost::atomic<boost::chrono::nanoseconds> elapsed = boost::chrono::nanoseconds(0);
		static const int LISTMODEWRITEQUEUESIZE = 50000;
		boost::lockfree::spsc_queue<DataPacket, boost::lockfree::capacity<LISTMODEWRITEQUEUESIZE>> listmodequeue;
		boost::atomic<long long> packetqueuecount = 0;
		static const int PACKETQUEUESIZE = 20000;
		boost::lockfree::spsc_queue<DataPacket, boost::lockfree::capacity<PACKETQUEUESIZE>> packetqueue;



	};
}
