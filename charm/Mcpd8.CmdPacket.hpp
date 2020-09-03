/***************************************************************************
 *   Copyright (C) 2019 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License v3 as published  *
 *   by the Free Software Foundation;                                      *
 ***************************************************************************/

#pragma once
#include "stdafx.h"
#include <boost/asio.hpp>
#include <memory>
#include <boost/shared_ptr.hpp>
#include "Mcpd8.DataPacket.hpp"
#include "Zweistein.Logger.hpp"

namespace Mcpd8 {
	extern  unsigned short sendcounter;
	struct alignas(2) CmdPacket
	{
		uint16_t Length;	//!< length of the buffer
		uint16_t Type;
		uint16_t headerLength;	//!< the length of the buffer header
		uint16_t Number;	//!< number of the packet
		uint16_t cmd; 		//!< the run ID
		uint16_t deviceStatusdeviceId;	//!< the device state
		uint16_t time[3];
		uint16_t headerchksum;
		uint16_t data[750];
		static const uint16_t defaultLength;
		CmdPacket() :Type(Mesy::BufferType::COMMAND), Length(defaultLength),
			headerLength(defaultLength), Number(0), deviceStatusdeviceId(0) {

		}
		CmdPacket(const DataPacket & p)
		{
			Length = p.Length;
			Type = p.Type;
			headerLength = p.headerLength;
			Number = p.Number;
			cmd = p.runID;
			deviceStatusdeviceId = p.deviceStatusdeviceId;
			time[0] = p.time[0];
			time[1] = p.time[1];
			time[2] = p.time[2];
			headerchksum = p.param[0][0];
			unsigned short items = (Length - headerLength)+1; // we want trailing 0xffff also
			if (items > 750) {
				LOG_ERROR << "ERROR in CmdPacket, " << items << " > 750" << std::endl;
				items = 750;
			}
			memcpy(data, &p.param[0][1], items < 750 ? sizeof(unsigned short)*items : sizeof(unsigned short) * 750);

		}
		static void setparameter(unsigned short param[3],long long value) {
			param[0] = value & 0xffff;
			param[1] = (value >> 16) & 0xffff;
			param[2] = (value >> 32) & 0xffff;
		}
		static size_t Send(boost::asio::ip::udp::socket *socket, CmdPacket& cmdpacket, boost::asio::ip::udp::endpoint &mcpd_endpoint) {
			unsigned short items = (cmdpacket.Length - cmdpacket.headerLength);
			cmdpacket.data[items] = 0xffff;
			cmdpacket.headerchksum = 0;
			unsigned short chksum = cmdpacket.headerchksum;
			const unsigned short* p = reinterpret_cast<const unsigned short*>(&cmdpacket);
			for (int i = 0; i < cmdpacket.Length; i++)	chksum ^= p[i];
			cmdpacket.headerchksum = chksum;
			short* sp = (short*)&cmdpacket;
			size_t len = cmdpacket.Length;
			//for (int i = 0; i <len; i++) 	boost::endian::big_to_native_inplace(sp[i]);
			size_t bytessent=socket->send_to(boost::asio::buffer(reinterpret_cast<unsigned short*>(&cmdpacket), (cmdpacket.Length)*sizeof(short)), mcpd_endpoint);
			boost::this_thread::sleep_for(boost::chrono::milliseconds(10)); // needed for multiple sends , otherwise SEND_TIMEOUT
			return bytessent;

		}


		void print(std::stringstream& os) const {
			using namespace magic_enum::ostream_operators;
			using namespace magic_enum::bitwise_operators;
			auto buffertype = magic_enum::enum_cast<Mesy::BufferType>(Zweistein::reverse_u16(Type));
			auto status =Mcpd8::DataPacket::getStatus(deviceStatusdeviceId);
			std::stringstream ss_status;
			for (auto s : magic_enum::enum_values<Status>()) {
				if (s & status) ss_status << s<<" ";
			}

			bool understood = true;
			unsigned short realcmd = cmd;
			if (realcmd!= Mcpd8::Internal_Cmd::SETNUCLEORATEEVENTSPERSECOND && realcmd & 0x8000 ) {
				understood = false;
				realcmd-=0x8000;

			}
			auto cmd_1 = magic_enum::enum_cast<Mcpd8::Cmd>(realcmd);
			auto cmd_2 = magic_enum::enum_cast<Mcpd8::Internal_Cmd>(realcmd);

			std::stringstream ss_cmd;
			if (cmd_1.has_value()) 	ss_cmd << cmd_1 ;
			else {
				ss_cmd << cmd_2;
				if (!cmd_2.has_value()) {
					if (realcmd == Mcpd8::Internal_Cmd::SETNUCLEORATEEVENTSPERSECOND) {
						ss_cmd << "(SETNUCLEORATEEVENTSPERSECOND)";
					}
					else ss_cmd << hexfmt(cmd);
				}
				else ss_cmd ;
			}
			if (!understood) {
				ss_cmd << "( NOT UNDERSTOOD)";
			}



			os << buffertype << " " << ss_cmd.str() << ", ";
			os << "Buffer Number:" << Number<< ", ";
			os<< "Status:"<<ss_status.str() << hexfmt((unsigned char) status)  << ", ";
			os << "Id:" <<(unsigned short) Mcpd8::DataPacket::getId(deviceStatusdeviceId) << ", ";
			os << "Timestamp:" << boost::chrono::duration_cast<boost::chrono::milliseconds>(Mcpd8::DataPacket::timeStamp(time)) << std::endl;

			//os << " ITEMS:" << Length - headerLength << std::endl;
			for (int d = 0; d < Length - headerLength; d++) {
				if (d!=0 && d % 5 == 0) os << std::endl;
				os << "\tdata["<<d<<"]=0x" << std::setfill('0') << std::setw(2) << std::hex << data[d] <<std::dec<< " ";
				}
			if(Length - headerLength > 0)os << std::endl;
		}

	};


}

inline std::ostream& operator<<(std::ostream& p, Mcpd8::CmdPacket& cp) {
	std::stringstream ss;
	cp.print(ss);
	p << ss.str();
	return p;
}
#ifdef simpleLogger_hpp__
inline boost::log::BOOST_LOG_VERSION_NAMESPACE::basic_record_ostream<char>& operator<<(boost::log::BOOST_LOG_VERSION_NAMESPACE::basic_record_ostream<char>& p, Mcpd8::CmdPacket& cp) {
	std::stringstream ss;
	cp.print(ss);
	p << ss.str();
	return p;
}
#endif
