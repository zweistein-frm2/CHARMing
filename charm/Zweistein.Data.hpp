/***************************************************************************
 *   Copyright (C) 2019 - 2020 by Andreas Langhoff
 *                                          <andreas.langhoff@frm2.tum.de> *
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
#include "Zweistein.Event.hpp"
#include "Zweistein.enums.Generator.hpp"
namespace Zweistein {
	class Data {
	public:
		static const int EVENTQUEUESIZE = 1000 * 1000; // high number needed
		boost::atomic<Zweistein::Format::EventData> Format = Zweistein::Format::EventData::Undefined;
		boost::atomic<unsigned long long> evntcount = 0;
		boost::lockfree::spsc_queue<Zweistein::Event, boost::lockfree::capacity<EVENTQUEUESIZE>> evntqueue;
		boost::atomic<unsigned short> widthX;
		boost::atomic<unsigned short> widthY;
		/* Using blocking queue:
* std::queue<Zweistein::Event> evntqueue;
* std::mutex mutex;
*/
	};
}