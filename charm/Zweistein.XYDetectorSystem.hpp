#pragma once
#include <boost/asio.hpp>
#include <boost/atomic.hpp>
#include "Zweistein.Data.hpp"

namespace Zweistein {

	class XYDetectorSystem {
	public:
		boost::asio::io_service* pio_service;
		boost::atomic<bool> connected;
		Zweistein::Data evdata;
	};
}