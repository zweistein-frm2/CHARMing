/***************************************************************************
 *   Copyright (C) 2019 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/
#pragma once
#include <asioext/file.hpp>
#include <boost/endian/conversion.hpp>
#include <bitset>
#include <boost/atomic.hpp>
#include "MesytecSystem.Data.hpp"
#include "Mesytec.DeviceParameter.hpp"
#include "Zweistein.Logger.hpp"

namespace Mesytec {
	namespace listmode {
		typedef enum _action {
			continue_reading=0,
			wait_reading=1,
			start_reading=2
		} action;
		extern boost::atomic<bool> stopwriting;
		extern boost::atomic<action> whatnext ;
		const  char header_separator[] =	{ '\x00','\x00','\x55','\x55','\xAA','\xAA','\xFF','\xFF' };
		const  char datablock_separator[] = { '\x00','\x00','\xFF','\xFF','\x55','\x55','\xAA','\xAA' };
		const  char closing_signature[] =	{ '\xFF','\xFF','\xAA','\xAA','\x55','\x55','\x00','\x00' };
		
		inline listmode::action CheckAction() {
			action r = whatnext;
			return  r;
		}

		class Read {
			boost::array< Mcpd8::DataPacket, 1> recv_buf;
		public:
			Read(boost::function<void(Mcpd8::DataPacket &)> abfunc,Mcpd8::Data &_Data, std::map<const unsigned char, Mesytec::DeviceParameter> &_deviceparam):deviceparam(_deviceparam),
				data(_Data),ab(abfunc){
				int n=(int)deviceparam.size();
				for (int i = 0; i < n; i++) listmoderead_first.set(i);
			}
		private:
			Mcpd8::Data& data;
			std::map<const unsigned char, Mesytec::DeviceParameter>& deviceparam;
			std::bitset<8> listmoderead_first;
			
			boost::chrono::system_clock::time_point tp_start;
			boost::chrono::nanoseconds start;
			boost::function<void(Mcpd8::DataPacket &)> ab;

			
			void listmoderead_analyzebuffer(const boost::system::error_code& error,
				std::size_t bytes_transferred, Mcpd8::DataPacket& datapacket) {
				if (listmoderead_first!=0) {
					
					start = Mcpd8::DataPacket::timeStamp(datapacket.time);
					tp_start = boost::chrono::system_clock::now();
					unsigned char id = Mcpd8::DataPacket::getId(datapacket.deviceStatusdeviceId);
					auto& params = deviceparam[id];
					params.lastbufnum = datapacket.Number - 1;

					int i = 0;
					for (std::map<const unsigned char, Mesytec::DeviceParameter>::iterator it = deviceparam.begin(); it != deviceparam.end(); ++it) {
						if ((*it).first == id) listmoderead_first.reset(i);
						i++;
					}
					
					
				}
				ab(datapacket);

				boost::chrono::milliseconds elapsed = boost::chrono::duration_cast<boost::chrono::milliseconds>(Mcpd8::DataPacket::timeStamp(datapacket.time) - start);
				if (elapsed.count() > 300) {
					int replayspeedmultiplier = 1;
					start = Mcpd8::DataPacket::timeStamp(datapacket.time);
					boost::chrono::milliseconds ms = boost::chrono::duration_cast<boost::chrono::milliseconds>(boost::chrono::system_clock::now() - tp_start);
					boost::chrono::milliseconds towait = elapsed/ replayspeedmultiplier - ms;
					if (towait.count() > 0) {
						boost::this_thread::sleep_for(boost::chrono::milliseconds(towait));
					}
					tp_start = boost::chrono::system_clock::now();

				}
				
				
			}

		public:
			void file(std::string fname,boost::asio::io_service& io_service) {
					LOG_DEBUG << __FILE__ << " : " << __LINE__ << " " << fname << std::endl;
					asioext::file source(io_service, fname, asioext::open_flags::access_read | asioext::open_flags::open_existing);
					char buffer[0x5ef]; // 0x5ef  is naughty boundary, used for debugging, can use any size really
					boost::system::error_code ec;
					boost::system::error_code error;
					size_t bufnum = 0;
					size_t bytesWrittenbypreviousBuffer = 0;
					size_t possible_remaining_n = std::string::npos;
					size_t possible_nc = std::string::npos;
					bool headerfound = false;
					bool closing_sigfound = false;
					size_t total_bytes_processed = 0;
					size_t transferred = 0;
					size_t data_packets_found = 0;

					int n = (int)deviceparam.size();
					for (int i = 0; i < n; i++) listmoderead_first.set(i);
					
					do {
						const std::size_t bytes_read = source.read_some(boost::asio::buffer(buffer), ec);
						//std::cout << "\r" << fname << " : " << total_bytes_processed << " bytes processed \t";
						
						size_t from = 0;
						size_t to = bytes_read;
						std::string nhneedle(Mesytec::listmode::header_separator, Mesytec::listmode::header_separator + sizeof(Mesytec::listmode::header_separator));
						std::string nneedle(Mesytec::listmode::datablock_separator, Mesytec::listmode::datablock_separator + sizeof(Mesytec::listmode::datablock_separator));
						std::string ncneedle(Mesytec::listmode::closing_signature, Mesytec::listmode::closing_signature + sizeof(Mesytec::listmode::closing_signature));
						do {
							std::string_view  haystack(buffer + from, bytes_read - from);
							size_t nh = haystack.find(nhneedle);
							size_t n = haystack.find(nneedle);
							size_t nc = haystack.find(ncneedle);
							if (nh != std::string::npos) {
								nh += from;
								if (headerfound) LOG_ERROR << "Multiple header_separator found at pos:" << total_bytes_processed + nh << std::endl;
							}
							if (n != std::string::npos) n += from;
							if (nc != std::string::npos) {
								nc += from;
								if (closing_sigfound) LOG_ERROR << "Multiple closing_sigfound found at pos:" << total_bytes_processed + nc << std::endl;

							}

							if (possible_remaining_n != std::string::npos) {
								size_t n_advance = 0;
								// now find needle for size_of(datablock_separator) - possible_remaining_n
								std::string_view  haystackEndOnly(buffer, possible_remaining_n);
								std::string n_restofneedle(Mesytec::listmode::datablock_separator + (sizeof(Mesytec::listmode::datablock_separator) - possible_remaining_n)
									, Mesytec::listmode::datablock_separator + sizeof(Mesytec::listmode::datablock_separator));
								if (haystackEndOnly.find(n_restofneedle) != std::string::npos) {
									// needle confirmed, so remove possible_n from 
									bytesWrittenbypreviousBuffer -= sizeof(Mesytec::listmode::datablock_separator) - possible_remaining_n;
									transferred -= sizeof(Mesytec::listmode::datablock_separator) - possible_remaining_n;
									short* sp = (short*)&recv_buf[0];
									for (int i = 0; i < transferred / sizeof(short); i++) 	boost::endian::big_to_native_inplace(sp[i]);
									listmoderead_analyzebuffer(error, transferred, recv_buf[0]);
									data_packets_found++;
									bytesWrittenbypreviousBuffer = 0;
									transferred = 0;
									from = possible_remaining_n;
								}
								possible_remaining_n = std::string::npos;
							}

							if (!headerfound && (nh != std::string::npos)) {
								from = nh + sizeof(Mesytec::listmode::header_separator);
								headerfound = true;
							}


							// we assume header_separator will be within the first read_buffer
							// have to check for closing separator also in the future

							if (n == std::string::npos && nc == std::string::npos) {

								//nothing found, so check end of buffer for cut of f signatures
								for (int i = 1; i < sizeof(Mesytec::listmode::datablock_separator); i++) {
									//i from 1 to 7
									size_t sep_size = sizeof(Mesytec::listmode::datablock_separator) - i;
									std::string_view  haystackBeginOnly(buffer + (bytes_read - sep_size), sep_size);
									std::string n_possibleneedle(Mesytec::listmode::datablock_separator, Mesytec::listmode::datablock_separator + sep_size);
									if (haystackBeginOnly.find(n_possibleneedle) != std::string::npos) {
										possible_remaining_n = i;
										break;
									};


								}
							}
							to = n;
							if (nc != std::string::npos) {
								closing_sigfound = true;
								to = nc;
							}
							if (headerfound && (from != std::string::npos)) {
								char* dest = ((char*)&recv_buf[0]) + bytesWrittenbypreviousBuffer;
								size_t n_advance = 0;
								if (to == std::string::npos) {
									to = bytes_read;
									bytesWrittenbypreviousBuffer += to - from;
									if (to - from > sizeof(Mcpd8::DataPacket)) {
										BOOST_THROW_EXCEPTION(std::runtime_error("DataPacket not recognized"));
									}
									memcpy(dest, buffer + from, to - from);
									transferred += to - from;
									n_advance = to;
								}
								else {
									// dann ist wohl recv_buf[0] gefüllt
									if (to - from > sizeof(Mcpd8::DataPacket)) {
										BOOST_THROW_EXCEPTION(std::runtime_error("DataPacket not recognized"));
									}
									memcpy(dest, buffer + from, to - from);
									transferred += to - from;
									short* sp = (short*)&recv_buf[0];
									for (int i = 0; i < transferred / sizeof(short); i++) 	boost::endian::big_to_native_inplace(sp[i]);

									listmoderead_analyzebuffer(error, transferred, recv_buf[0]);
									data_packets_found++;
									transferred = 0;
									bytesWrittenbypreviousBuffer = 0;
									n_advance = to + sizeof(Mesytec::listmode::datablock_separator);

								}
								if (n_advance > bytes_read) {
									n_advance = bytes_read;
									to = n_advance;
								}
								from = n_advance;
								to = bytes_read;
							}
							if (from == bytes_read) break;
							if (closing_sigfound) break;

							while(action lec=CheckAction()) {
								if (lec == action::start_reading) return;
								if (lec == action::wait_reading) boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
							}

							if (ec) {
								if (ec == boost::asio::error::eof || ec == boost::asio::error::broken_pipe) {
									from = std::string::npos;
									break;
								}

								else
									throw std::system_error(ec);
							}

						} while (from != std::string::npos);
						bufnum++;
						total_bytes_processed += bytes_read;
					} while (!ec);
					{
						LOG_INFO << fname << ": " << total_bytes_processed << " bytes processed ,data_packets_found=" << data_packets_found << " " << std::endl;
						if (!headerfound) LOG_ERROR << "headerfound=" << std::boolalpha << headerfound << " " << std::endl;
						if (!closing_sigfound) LOG_ERROR << "closing_sigfound=" << std::boolalpha << closing_sigfound << " " << std::endl;

					}

				}
			

			
		};
	}

}
