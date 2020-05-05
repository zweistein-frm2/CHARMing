
#include "Mesytec.listmode.hpp"

namespace Mesytec {
	namespace listmode {
		 boost::atomic<bool> stopwriting = false;
		 boost::atomic<action> whatnext = action::wait_reading;
	}

	

}