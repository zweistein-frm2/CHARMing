/***************************************************************************
 *   Copyright (C) 2019 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/
#pragma once
#include "MesytecSystem.Data.hpp"
#include "Mesytec.DeviceParameter.hpp"

namespace Mesytec {
	namespace listmode {
		const  char header_separator[] =	{ '\x00','\x00','\x55','\x55','\xAA','\xAA','\xFF','\xFF' };
		const  char datablock_separator[] = { '\x00','\x00','\xFF','\xFF','\x55','\x55','\xAA','\xAA' };
		const  char closing_signature[] =	{ '\xFF','\xFF','\xAA','\xAA','\x55','\x55','\x00','\x00' };
		
		class Read {
			boost::array< Mcpd8::DataPacket, 1> recv_buf;
		public:
			Read(boost::function<void(Mcpd8::DataPacket &)> abfunc,Mcpd8::Data &_Data, std::map<const unsigned char, Mesytec::DeviceParameter> &_deviceparam):deviceparam(_deviceparam),data(_Data),ab(abfunc),listmoderead_first(true){}
		private:
			Mcpd8::Data& data;
			std::map<const unsigned char, Mesytec::DeviceParameter>& deviceparam;
			bool listmoderead_first ;
			boost::function<void(Mcpd8::DataPacket &)> ab;
			void listmoderead_analyzebuffer(const boost::system::error_code& error,
				std::size_t bytes_transferred, Mcpd8::DataPacket& datapacket) {
				if (listmoderead_first) {
					listmoderead_first = false;
					Mesytec::DeviceParameter mp;
					mp.lastbufnum = datapacket.Number - 1;

					switch (datapacket.GetBuffertype()) {
					case Mesy::BufferType::DATA:
						data.Format = Mcpd8::EventDataFormat::Mpsd8;
						data.widthY = Mesy::Mpsd8Event::sizeY;
						data.widthX = Mesy::Mpsd8Event::sizeMODID * Mesy::Mpsd8Event::sizeSLOTS;

						// one device has maximum 64 (8 mpsd8 with 8 channels each)
						// problem, we don't know how many devices were used in the measurement.
						//  2 solutions: A: use a msmt.json reflecting the real measurement.
						//               B: scan the file for all possible mcpd8 Id[s]. Each Id can handle 64 channels
						break;
					case Mesy::BufferType::MDLL:
						data.Format = Mcpd8::EventDataFormat::Mdll;
						data.widthX = Mesy::MdllEvent::sizeX;
						data.widthY = Mesy::MdllEvent::sizeY;

						break;
					}
					deviceparam.insert(std::pair<unsigned char, Mesytec::DeviceParameter>(Mcpd8::DataPacket::getId(datapacket.deviceStatusdeviceId), mp));
					boost::this_thread::sleep_for(boost::chrono::milliseconds(500));
				}
				ab(datapacket);
			}

		public:
			void files(std::vector<std::string>& filenames, boost::asio::io_service& io_service) {
				for (std::string fname : filenames) {
					asioext::file source(io_service, fname, asioext::open_flags::access_read | asioext::open_flags::open_existing);
					char buffer[0x5ef]; // 0x5ef  is naughty boundary
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
					listmoderead_first = true;
					do {
						const std::size_t bytes_read = source.read_some(boost::asio::buffer(buffer), ec);

						{
							boost::mutex::scoped_lock lock(coutGuard);
							std::cout << "\r" << filenames[0] << ": " << total_bytes_processed << " bytes processed \t";
						}
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
								if (headerfound) std::cout << "Multiple header_separator found at pos:" << total_bytes_processed + nh << std::endl;
							}
							if (n != std::string::npos) n += from;
							if (nc != std::string::npos) {
								nc += from;
								if (closing_sigfound) std::cout << "Multiple closing_sigfound found at pos:" << total_bytes_processed + nc << std::endl;

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
						boost::mutex::scoped_lock lock(coutGuard);
						std::cout << "\r" << filenames[0] << ": " << total_bytes_processed << " bytes processed ,data_packets_found=" << data_packets_found << " " << std::endl;
						if (!headerfound) std::cout << "headerfound=" << std::boolalpha << headerfound << " " << std::endl;
						if (!closing_sigfound) std::cout << "closing_sigfound=" << std::boolalpha << closing_sigfound << " " << std::endl;

					}

				}
			}

			
		};
	}

}
