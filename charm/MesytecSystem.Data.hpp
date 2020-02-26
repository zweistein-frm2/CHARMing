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
		static const int EVENTQUEUESIZE = 1000*1000; // high number needed
		boost::atomic<long long> evntcount = 0;
		boost::lockfree::spsc_queue<Zweistein::Event, boost::lockfree::capacity<EVENTQUEUESIZE>> evntqueue;
		boost::atomic<EventDataFormat> Format = EventDataFormat::Undefined;
		boost::atomic<long long> listmodepacketcount = 0;
		static const int LISTMODEWRITEQUEUESIZE = 10000;
		boost::lockfree::spsc_queue<DataPacket, boost::lockfree::capacity<LISTMODEWRITEQUEUESIZE>> listmodequeue;
		boost::atomic<long long> packetqueuecount = 0;
		static const int PACKETQUEUESIZE = 10000;
		boost::lockfree::spsc_queue<DataPacket, boost::lockfree::capacity<PACKETQUEUESIZE>> packetqueue;
		unsigned short widthX;
		unsigned short widthY;
		/* Using blocking queue:
 * std::queue<int> queue;
 * std::mutex mutex;
 */
	};
}
