 /***************************************************************************
  *   Copyright (C) 2019 - 2020 by Andreas Langhoff
  *                                          <andreas.langhoff@frm2.tum.de> *
  *   This program is free software; you can redistribute it and/or modify  *
  *   it under the terms of the GNU General Public License v3 as published  *
  *   by the Free Software Foundation;                                      *
  ***************************************************************************/

#pragma once
#include "stdafx.h"
#include <boost/exception/all.hpp>
#include <boost/array.hpp>
#include <boost/circular_buffer.hpp>
#include <vector>
#include <map>
#include <boost/asio.hpp>
#include <boost/foreach.hpp>
#include <boost/chrono.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/endian/conversion.hpp>
#include "Mcpd8.enums.hpp"
#include <csignal>
#include <asioext/unique_file_handle.hpp>
#include <asioext/file_handle.hpp>
#include <asioext/open.hpp>
#include <asioext/standard_streams.hpp>

#include <asioext/file.hpp>
#include <asioext/thread_pool_file_service.hpp>
#include <asioext/open_flags.hpp>

#include <asioext/linear_buffer.hpp>

#include "Mesytec.listmode.hpp"
#include "MesytecSystem.Data.hpp"
#include "Mcpd8.CmdPacket.hpp"
#include "Mcpd8.Parameters.hpp"
#include "Mesytec.DeviceParameter.hpp"
#include "Zweistein.Locks.hpp"
#include "Zweistein.Logger.hpp"
#include "Zweistein.ping.hpp"
#include "Zweistein.XYDetectorSystem.hpp"

using boost::asio::ip::udp;



namespace Mesytec {
	typedef boost::error_info<struct tag_my_info, int> my_info;
	struct cmd_error : virtual boost::exception, virtual std::exception { };
	enum cmd_errorcode {
		OK=0,
		SEND_CMD_TIMEOUT=1,
		CMD_NOT_UNDERSTOOD=2,
		CMD_RESPONSE_WRONG_LENGTH=3,
		CMD_RESPONSE_ZERO_LENGTH=4
	};

	std::string cmd_errorstring(Mesytec::cmd_error& x);

	class MesytecSystem : public Zweistein::XYDetectorSystem {
	public:
		static std::map<const unsigned char, Mesytec::DeviceParameter> deviceparam;
		MesytecSystem();
		~MesytecSystem();

		virtual void initatomicortime_point() {
			XYDetectorSystem::initatomicortime_point();
			auto now = boost::chrono::system_clock::now();
			auto min = now - boost::chrono::minutes(60);
			lastpacketqueuefull = min;
			lastlistmodequeuefull = min;
			wait_response = false;
			currentrunid = 0;
			internalerror = Mesytec::cmd_errorcode::OK;
			firstneutrondiscarded = false;
			warning_notified_bufnum_8bit = false;
			allow8bitbufnums = false;
			inputFromListmodeFile = false;
			eventdataformat = Zweistein::Format::EventData::Undefined;
		}

		void closeConnection();

		boost::chrono::system_clock::time_point lastpacketqueuefull;
		boost::chrono::system_clock::time_point lastlistmodequeuefull;


		unsigned short currentrunid;

		cmd_errorcode internalerror;

		Zweistein::Format::EventData eventdataformat;

		virtual bool singleModuleXYSize(Zweistein::Format::EventData eventdataformat, unsigned short& x, unsigned short& y) {
			switch (eventdataformat) {
			case Zweistein::Format::EventData::Mpsd8:
				y = (unsigned short)Mpsd8_sizeY;
				x = (unsigned short)(Mpsd8_sizeMODID * Mpsd8_sizeSLOTS);
				return true;

			case Zweistein::Format::EventData::Mdll:
				x = (unsigned short)Mdll_sizeX;
				y = (unsigned short)Mdll_sizeY;
				return true;
			}
			return false;

		}
		size_t lastpacketqueuefull_missedcount;
		size_t lastlistmodequeuefull_missedcount;

		bool b_bufnums_8BIT ;
		Mcpd8::Data data;
		bool inputFromListmodeFile;
		virtual void setStart(boost::chrono::system_clock::time_point& t);
		virtual bool listmode_connect(std::list<Mcpd8::Parameters>& _devlist, boost::asio::io_service& io_service);

		virtual bool connect(std::list<Mcpd8::Parameters>& _devlist, boost::asio::io_service& io_service);

		virtual void Send(std::pair<const unsigned char, Mesytec::DeviceParameter>& kvp, Mcpd8::Internal_Cmd cmd, unsigned long param = 0);

		void Send(std::pair<const unsigned char, Mesytec::DeviceParameter>& kvp, Mcpd8::Cmd cmd, unsigned long lparam = 0);
		void SendAll(Mcpd8::Cmd cmd);

	private:
		boost::atomic<bool> wait_response;
		Mcpd8::CmdPacket& Send(std::pair<const unsigned char, Mesytec::DeviceParameter>& kvp, Mcpd8::CmdPacket& cmdpacket, bool waitresponse = true);

		void start_receive(const Mesytec::DeviceParameter &mp,unsigned char devid) {
			mp.socket->async_receive(boost::asio::buffer(recv_buf), mp.pstrand->wrap(boost::bind(&MesytecSystem::handle_receive, this,
				boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, devid)));

		}
		void watch_incoming_packets() {

			try {
				Mcpd8::DataPacket dp;
				boost::function<void(Mcpd8::DataPacket&)> abfunc;
				if (systype == Zweistein::XYDetector::Systemtype::Mesytec ) abfunc= boost::bind(&Mesytec::MesytecSystem::analyzebuffer, this, boost::placeholders::_1);
				else  abfunc = boost::bind(&Mesytec::MesytecSystem::charm_analyzebuffer, this, boost::placeholders::_1);
				do {
					while (data.packetqueue.pop(dp)) {
						abfunc(dp);
					}
				} while (!pio_service->stopped());
			}
			catch (boost::exception & e) {	LOG_ERROR << boost::diagnostic_information(e) << std::endl;	}
			LOG_INFO << "watch_incoming_packets() exiting..." << std::endl;

		}

		bool firstneutrondiscarded = false;
		bool warning_notified_bufnum_8bit = false;
		bool allow8bitbufnums = false;
		void handle_receive(const boost::system::error_code& error,
			std::size_t bytes_transferred,unsigned char devid) {
			Mcpd8::DataPacket& dp = recv_buf[0];
			data.last_deviceStatusdeviceId = dp.deviceStatusdeviceId;
			if (error) {
				LOG_ERROR  << " handle_receive(" <<error.message() << " ,bytes_transferred=" << bytes_transferred << ", DataPacket=" << dp <<")"<< std::endl;
				LOG_ERROR << "PACKET BUFNUM: " << (unsigned short)(dp.Number) << std::endl;
				//memset(&recv_buf[0], 0, sizeof(Mcpd8::DataPacket));
				if (pio_service->stopped()) {
					LOG_INFO << "handle_receive: return. " << std::endl;
					return;
				}
			}

			auto &mp = deviceparam.at(devid);

			if (dp.Type == Mesy::BufferType::COMMAND) {

				Mcpd8::CmdPacket p = dp;
				{
					Zweistein::WriteLock w_lock(cmdLock);
					cmd_recv_queue.push_front(p);
				}

			}
			else {
				if (daq_running) {
					// Mesy::BufferType::DATA
					if (!firstneutrondiscarded) {
						LOG_INFO << "TO CHECK: stray Neutron discarded (from startup)" << std::endl;
						firstneutrondiscarded = true;
					}
					bool ok = data.packetqueue.push(dp);

					if (ok)	data.packetqueuecount++;
					else {
						boost::chrono::duration<double> sec = boost::chrono::system_clock::now() - lastpacketqueuefull;
						lastpacketqueuefull_missedcount += 1;
						if (sec.count() > 1) {
							LOG_ERROR << ": PACKETQUEUE FULL , SKIPPED " << lastpacketqueuefull_missedcount << " Event(s)" << std::endl;

							//			LOG_ERROR << std::endl << boost::chrono::time_fmt(boost::chrono::timezone::local) << boost::chrono::system_clock::now() << ": PACKETQUEUE FULL , SKIPPED " << lastpacketqueuefull_missedcount << " Event(s)" << std::endl;
							lastpacketqueuefull = boost::chrono::system_clock::now();
							lastpacketqueuefull_missedcount = 0;
						}
					}
					if (write2disk) {
						bool ok2 = data.listmodequeue.push(dp);
						if (ok2) data.listmodepacketcount++;
						else {
							boost::chrono::duration<double> sec = boost::chrono::system_clock::now() - lastlistmodequeuefull;
							lastlistmodequeuefull_missedcount += 1;
							if (sec.count() > 1) {

								//LOG_ERROR << std::endl << boost::chrono::time_fmt(boost::chrono::timezone::local) << boost::chrono::system_clock::now() << ": LISTMODEWRITEQUEUE FULL , SKIPPED " << lastlistmodequeuefull_missedcount << " Event(s)" << std::endl;
								LOG_ERROR << ": LISTMODEWRITEQUEUE FULL , SKIPPED " << lastlistmodequeuefull_missedcount << " Event(s)" << std::endl;

								lastlistmodequeuefull = boost::chrono::system_clock::now();
								lastlistmodequeuefull_missedcount = 0;
							}
						}
					}
				}
			}
			start_receive(mp,devid);
		}
		protected:

		void PushNeutronEventOnly_HandleOther(Zweistein::Event& Ev);



		public:
			virtual void analyzebuffer(Mcpd8::DataPacket& datapacket);
			virtual void charm_analyzebuffer(Mcpd8::DataPacket& datapacket);
		private:
		udp::endpoint local_endpoint;


		boost::circular_buffer<Mcpd8::CmdPacket> cmd_recv_queue;
		Zweistein::Lock cmdLock;

		boost::array< Mcpd8::DataPacket, 1> recv_buf;
		std::map<const unsigned char, unsigned char> oldidnewid = std::map<const unsigned char, unsigned char>();

	};

}




