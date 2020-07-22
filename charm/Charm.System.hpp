#pragma once
#include "Mesytec.Mcpd8.hpp"
namespace Charm {
	class CharmSystem : public Mesytec::MesytecSystem {
	public:
		int icharmid;
		CharmSystem():icharmid(0) {};
		void start_receive_charm() {

			deviceparam.at(icharmid).socket->async_receive_from(boost::asio::buffer(charm_buf), deviceparam.at(icharmid).charm_cmd_endpoint, boost::bind(&CharmSystem::handle__charm_receive, this,
				boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
		}

		void handle__charm_receive(const boost::system::error_code& error,
			std::size_t bytes_transferred) {
			LOG_DEBUG << "handle__charm_receive(" << error.message() << " ,bytes_transferred=" << bytes_transferred << std::endl;
			for (int i = 0; i < bytes_transferred; i++) {
				LOG_DEBUG << std::hex << charm_buf[i] << " ";
				if (i % 8 == 0) LOG_DEBUG << std::endl;
			}
			LOG_DEBUG << std::dec << std::endl;
			start_receive_charm();
		}
		bool connect(std::list<Mcpd8::Parameters>& _devlist, boost::asio::io_service& io_service) {
			bool rv = Mesytec::MesytecSystem::connect(_devlist, io_service);
			for (const auto& [key, value] : deviceparam) {
				if (value.datagenerator == Zweistein::DataGenerator::Charm || value.datagenerator == Zweistein::DataGenerator::CharmSimulator) {
					icharmid = key;
				}
			}
			return rv;
		}
		boost::array< unsigned char, 64> charm_buf;

	};

}