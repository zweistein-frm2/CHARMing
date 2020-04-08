#pragma once
#include <boost/asio.hpp>
#include <boost/chrono.hpp>
#include "Mesytec.enums.Generator.hpp"
#include "Mesytec.Mpsd8.hpp"
using boost::asio::ip::udp;
namespace  Mesytec {

	struct DeviceParameter {


		unsigned long lastmissed_count;
		boost::chrono::system_clock::time_point lastmissed_time;
		boost::chrono::nanoseconds starttime;
		udp::endpoint mcpd_endpoint;
		udp::endpoint charm_cmd_endpoint;
		udp::socket* socket;
		bool bNewSocket;
		unsigned short lastbufnum;
		DataGenerator datagenerator;
		unsigned short firmware_major;
		unsigned short firmware_minor;
		int offset;
		Mesytec::Mpsd8::Module module[Mpsd8_sizeSLOTS];
		Mesy::ModuleId module_id[Mpsd8_sizeSLOTS];
		DeviceParameter():offset(0),bNewSocket(false), lastmissed_count(0), lastmissed_time(boost::chrono::system_clock::now()) {
			for (int i = 0; i < Mpsd8_sizeSLOTS; i++) module_id[i] = Mesy::ModuleId::NOMODULE;
		}
		
	};
	

}