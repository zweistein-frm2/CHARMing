/***************************************************************************
 *   Copyright (C) 2019 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/
#pragma once
#include "stdafx.h"
#include <iostream>
#include <boost/exception/all.hpp>
#include <boost/chrono.hpp>
#include "Mesytec.hpp"
#include "Mcpd8.enums.hpp"
#include "Zweistein.bitreverse.hpp"


namespace Mcpd8
{
	class alignas(2) DataPacket
	{
	public:
		// we define all struct elements as short, so that endianness can be converted easier
		unsigned short Length;	//!< length of the buffer in words
		unsigned short Type;	//!< the buffer type
		unsigned short headerLength;	//!< the length of the header in words
		unsigned short Number;	//!< number of the packet 
		unsigned short runID; 		//!< the run ID
		unsigned short  deviceStatusdeviceId;	//!< the device state
		unsigned short time[3];
	private:
		unsigned short param[4][3];	//!< the values of the parameters (belong to the header)
		//unsigned short data[750];	//!< the events, length of the data = length of the buffer - length of the header
	public:
		Mesy::Mpsd8Event events[250];
		DataPacket() : Type(Zweistein::reverse_u16(Mesy::BufferType::DATA)), Length(21),
			headerLength(21), Number(0), runID(0), deviceStatusdeviceId(0) {}

		static long long tstamp_started;
		static void settimeNow48bit(unsigned short time[3]) {

			auto Now = boost::chrono::duration_cast<boost::chrono::nanoseconds>( boost::chrono::system_clock::now().time_since_epoch());
			long long c=Now.count()/100- tstamp_started;
			time[0] = c & 0xffff;
			time[1] = (c >> 16) & 0xffff;
			time[2] = (c >> 32) & 0xffff;
			
			
		}
		friend struct CmdPacket;
		static unsigned char getId(unsigned short deviceStatusdeviceId){
			return deviceStatusdeviceId >> 8;
		}
		static unsigned char getStatus(unsigned short deviceStatusdeviceId) {
			return deviceStatusdeviceId & 0xff;
		}
		unsigned short BytesUsed() {
			return Length * sizeof(unsigned short);
		}
		unsigned short numEvents() const {
			int numEvents = (Length - headerLength) / 3;
			BOOST_ASSERT(numEvents >= 0);
			return (unsigned short)numEvents;
		}

		static_assert(sizeof(Mesy::Mpsd8Event) == 6, "Event must be sizeof(6)");
		Mesy::BufferType GetBuffertype() {
			auto bt = magic_enum::enum_cast<Mesy::BufferType>(Zweistein::reverse_u16(Type));
			if (bt.has_value()) return bt.value();
			else {
				boost::mutex::scoped_lock lock(coutGuard);
				std::cout << " BufferType undefined" << std::endl;
				return Mesy::BufferType::COMMAND;
			}
		}
		static long long timeStamp(const unsigned short time[3]){
			/*
			 DONT USE:  *U reads past time[3]
			   typedef union {
				 unsigned short time[3];
				 long long timeStamp : 48;
			 } U;
			 int t=sizeof(U);
			const U* ptStamp= reinterpret_cast<const U*>(time) ;

			return ptStamp->timeStamp;
			 */
			long long tstamp = time[0] + (((long long)time[1]) << 16) + (((long long)time[2]) << 32);
			return tstamp;
		}

		static void setTimeStamp(unsigned short time[3],unsigned long long tstamp) {

			time[0] = tstamp & 0xffff;
			time[1] = (tstamp >> 16) & 0xffff;
			time[2] = (tstamp >> 32) & 0xffff;
		}

		void print(std::ostream & os) const {
			using namespace magic_enum::ostream_operators;
			auto buffertype = magic_enum::enum_cast<Mesy::BufferType>(Zweistein::reverse_u16(Type));
			auto status = Mcpd8::DataPacket::getStatus(deviceStatusdeviceId);
			
			std::stringstream ss_status;
			for (auto s : magic_enum::enum_values<Status>()) {
				if (s & status) ss_status << s << " ";
			}

			long long timestamp = Mcpd8::DataPacket::timeStamp(&time[0]); // nanoseconds 
			boost::chrono::microseconds micros(timestamp / 10);
			long fractionalseconds = timestamp % 10000000;

			boost::chrono::system_clock::time_point t2(micros);
			std::time_t tt = boost::chrono::system_clock::to_time_t(t2);
			os << buffertype << ", ";
			os << "Buffer Number:" << hexfmt(Number) << ", ";
			os << "Status:" << ss_status.str() << hexfmt((unsigned short)status);
			os << "RunID:" << runID << ", " << numEvents() << " Events ";
			os << "Timestamp:"<< std::put_time(std::localtime(&tt), "%Y-%b-%d %X") << "." << fractionalseconds  << std::endl;
			
			int rest = (Length - headerLength) % 3;
			if (rest) { os << std::endl << "WARNING: trailing " << rest << " Bytes"; }
		}
	};

	long long DataPacket::tstamp_started = 0;


}