/***************************************************************************
 *   Copyright (C) 2019 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/

#pragma once
#include <boost/lockfree/spsc_queue.hpp>
/* Using blocking queue:
 * #include <mutex>
 * #include <queue>
 */

#include "Mcpd8.DataPacket.hpp"
#include "Zweistein.Event.hpp"
namespace Mcpd8 {
	class Data {
	public:
		static const int EVENTQUEUESIZE = 1000 * 1000; // high number needed
		boost::atomic<long long> evntcount = 0;
		boost::atomic<unsigned short> last_deviceStatusdeviceId = Mcpd8::Status::sync_ok;
		boost::lockfree::spsc_queue<Zweistein::Event, boost::lockfree::capacity<EVENTQUEUESIZE>> evntqueue;
		boost::atomic<EventDataFormat> Format = EventDataFormat::Undefined;
		boost::atomic<long long> listmodepacketcount = 0;
		static const int LISTMODEWRITEQUEUESIZE = 50000;
		boost::lockfree::spsc_queue<DataPacket, boost::lockfree::capacity<LISTMODEWRITEQUEUESIZE>> listmodequeue;
		boost::atomic<long long> packetqueuecount = 0;
		static const int PACKETQUEUESIZE = 20000;
		boost::lockfree::spsc_queue<DataPacket, boost::lockfree::capacity<PACKETQUEUESIZE>> packetqueue;
		boost::atomic<unsigned short> widthX;
		boost::atomic<unsigned short> widthY;
		/* Using blocking queue:
 * std::queue<int> queue;
 * std::mutex mutex;
 */
	};
}
