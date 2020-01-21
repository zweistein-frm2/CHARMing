#pragma once
#include <boost/asio.hpp>
#include "Mesytec.enums.Generator.hpp"
#include "Mesytec.Mpsd8.hpp"
using boost::asio::ip::udp;
namespace  Mesytec {

	struct DeviceParameter {
		udp::endpoint mcpd_endpoint;
		udp::endpoint charm_cmd_endpoint;
		udp::socket* socket;
		unsigned short lastbufnum;
		DataGenerator datagenerator;
		unsigned short firmware_major;
		unsigned short firmware_minor;
		unsigned short devid;
		unsigned short runid;
		Mesytec::Mpsd8::Module module[Mesy::Mpsd8Event::sizeSLOTS];
		Mesy::ModuleId module_id[Mesy::Mpsd8Event::sizeSLOTS];
		DeviceParameter() {
			for (int i = 0; i < Mesy::Mpsd8Event::sizeSLOTS; i++) module_id[i] = Mesy::ModuleId::NOMODULE;
		}
		static int minid;
		static int maxid;
	};
	int DeviceParameter::minid = 0;
	int DeviceParameter::maxid = 0;

}