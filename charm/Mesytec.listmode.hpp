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
#include <boost/array.hpp>
#include "MesytecSystem.Data.hpp"
#include "Mesytec.DeviceParameter.hpp"
#include "Zweistein.Logger.hpp"
#include "Mcpd8.DataPacket.hpp"
#include "CounterMonitor.hpp"

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

		typedef boost::error_info<struct tag_my_info, int> my_info;
		struct read_error : virtual boost::exception, virtual std::exception { };
		enum read_errorcode {
			OK = 0,
			MULTIPLE_HEADER_SEPARATOR = 1,
			MULTIPLE_CLOSING_SIGNATURE = 2,
			DATAPACKET_NOT_RECOGNIZED = 3,
			MAX_DATAPACKET_SIZE_EXCEEDED=4,
			INTERNAL_BUFFER_OVERFLOW=5,
			CLOSING_SIGNATURE_NOT_DIRECTLY_AFTER_DATA_BLOCK_SEPARATOR=6,
			DATAPACKET_LENGTH_ZERO=7,
			DATAPACKET_LENGTH_GREATER_THAN_250 =8,
			DATAPACKET_SIGNATURE_EXPECTED=9,
			PROGRAM_LOGIC_ERROR=10,
		};
		class Read {
			boost::array< Mcpd8::DataPacket, 1> recv_buf;
		public:
			Read(boost::function<void(Mcpd8::DataPacket &)> &abfunc,Mcpd8::Data &_Data, std::map<const unsigned char, Mesytec::DeviceParameter> &_deviceparam):deviceparam(_deviceparam),
				data(_Data),ab(abfunc){
				int n=(int)deviceparam.size();
				for (int i = 0; i < n; i++) listmoderead_first.set(i);
				data.evntcount = 0;
				for (int i = 0; i < COUNTER_MONITOR_COUNT; i++) CounterMonitor[i] = 0;
				bool ok = data.evntqueue.push(Zweistein::Event::Reset());
				if (!ok) LOG_ERROR << " cannot push Zweistein::Event::Reset()" << std::endl;
			}
			
		private:
			Mcpd8::Data& data;
			std::map<const unsigned char, Mesytec::DeviceParameter>& deviceparam;
			std::bitset<8> listmoderead_first;
			
			boost::chrono::system_clock::time_point tp_start;
			boost::chrono::nanoseconds start;
			boost::function<void(Mcpd8::DataPacket &)>  ab;

			
			void listmoderead_analyzebuffer(const boost::system::error_code& error,
				std::size_t bytes_transferred, Mcpd8::DataPacket& datapacket) {
				if (listmoderead_first!=0) {
					

					start = Mcpd8::DataPacket::timeStamp(datapacket.time);
					tp_start = boost::chrono::system_clock::now();
					unsigned char id = Mcpd8::DataPacket::getId(datapacket.deviceStatusdeviceId);
					
					if (listmoderead_first.test(id)) {
						auto& params = deviceparam[id];
						params.lastbufnum = datapacket.Number - 1;
						int i = 0;
						for (std::map<const unsigned char, Mesytec::DeviceParameter>::iterator it = deviceparam.begin(); it != deviceparam.end(); ++it) {
							if ((*it).first == id) listmoderead_first.reset(i);
							i++;
						}
					}
					
					
				}
				
				//unsigned char id = Mcpd8::DataPacket::getId(datapacket.deviceStatusdeviceId);
				//LOG_DEBUG << (id==1?"                           ":"")<<"datapacket.Number=" << datapacket.Number << std::endl;
				ab.operator()(datapacket);
				

				boost::chrono::milliseconds elapsed = boost::chrono::duration_cast<boost::chrono::milliseconds>(Mcpd8::DataPacket::timeStamp(&datapacket.time[0]) - start);
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
					//LOG_DEBUG << __FILE__ << " : " << __LINE__ << " " << fname << std::endl;
					asioext::file source(io_service, fname, asioext::open_flags::access_read | asioext::open_flags::open_existing);
#ifdef _DEBUG
					const int bufsize = 80; // small value to catch all cases of signatures crossing buffer boundary
#else
					const int bufsize = 80000;
#endif
					std::array<char, bufsize> buffer; 
					boost::system::error_code ec;
					boost::system::error_code error;
					size_t bufnum = 0;
					size_t possible_nh = std::string::npos;
					size_t possible_remaining_n = std::string::npos;
					size_t possible_nc = std::string::npos;
					bool never_headerfound = true;
					bool never_closing_sigfound = true;
					bool headerfound = false;
					bool check_closing_signature = false;
					bool closing_sigfound = false;
					size_t total_bytes_processed = 0;
					size_t data_packets_found = 0;
					bool warning_type_wrong_sent = false;

					typedef struct __recvbuf_data {
						__recvbuf_data(char *rb,const std::string  filename, bool & warning_type_wrong_sent):recv_buf(rb),_filename(filename),
							_warning_type_wrong_sent(&warning_type_wrong_sent), bytesWrittenbypreviousBuffer(0), transferred(0){
							packet.Length = 0;
						}

						__recvbuf_data(const __recvbuf_data&) = default;
						std::string _filename;
						size_t missing_to_fill_recbuf() {
							if (packet.Length == 0) return std::string::npos;
							return packet.Length * sizeof(unsigned short) - bytesWrittenbypreviousBuffer;
						}
						void filldest(const char* b, size_t len) {
							char* dest = ((char*)&recv_buf[0]) + bytesWrittenbypreviousBuffer;
							if (bytesWrittenbypreviousBuffer + len > sizeof(Mcpd8::DataPacket)) {
								int g = 0;
								throw read_error() << my_info(read_errorcode::MAX_DATAPACKET_SIZE_EXCEEDED);
							}
							memcpy(dest, b,len);
							transferred += len;
							bytesWrittenbypreviousBuffer += len;

							int nvalid = 2;
							if (packet.Length == 0 && bytesWrittenbypreviousBuffer >= sizeof(short) * nvalid) {
								memcpy(&packet, &recv_buf[0], sizeof(short) * nvalid);
								short* sp = (short*)&packet;
								for (int i = 0; i < nvalid; i++) 	boost::endian::big_to_native_inplace(sp[i]);
								if(packet.Length==0) throw read_error() << my_info(read_errorcode::DATAPACKET_LENGTH_ZERO);
								if (packet.Length - packet.headerLength > 250 * 3 * sizeof(short)) {
									//DebugBreak();
									LOG_ERROR << "bytesWrittenbypreviousBuffer="<< bytesWrittenbypreviousBuffer << ", len=" << len << std::endl;
									throw read_error() << my_info(read_errorcode::DATAPACKET_LENGTH_GREATER_THAN_250);
								}
								if (packet.Length > 24) { // commands return always less than 24 data
									if (packet.Type == Mesy::BufferType::COMMAND) {
										Mcpd8::DataPacket *pbuf = (Mcpd8::DataPacket *)(&recv_buf[0]);
										pbuf->Type = Zweistein::reverse_u16(Mesy::BufferType::DATA);
										if (!*_warning_type_wrong_sent) {
											*_warning_type_wrong_sent = true;
											using namespace magic_enum::ostream_operators;
											std::stringstream ss1;
											ss1  << "DataPacket BufferType corrected to: " << Mesy::BufferType::DATA << " (was " << Mesy::BufferType::COMMAND << " before)";
											LOG_WARNING << _filename<<" : "<< ss1.str() << std::endl;
										}
									}
								}
							}
						}
						size_t bytesWrittenbypreviousBuffer;
						size_t transferred ;

						private:
							char* recv_buf;
							Mcpd8::DataPacket packet;
							bool * _warning_type_wrong_sent;

					} _recvbuf_data;
					_recvbuf_data recvbuf_data((char*)&recv_buf[0],fname, warning_type_wrong_sent);

					for (int i = 0; i < (int)deviceparam.size(); i++) listmoderead_first.set(i);

					std::string nhneedle(Mesytec::listmode::header_separator, Mesytec::listmode::header_separator + sizeof(Mesytec::listmode::header_separator));
					std::string nneedle(Mesytec::listmode::datablock_separator, Mesytec::listmode::datablock_separator + sizeof(Mesytec::listmode::datablock_separator));
					std::string ncneedle(Mesytec::listmode::closing_signature, Mesytec::listmode::closing_signature + sizeof(Mesytec::listmode::closing_signature));

					
					do {
						const std::size_t bytes_read = source.read_some(boost::asio::buffer(buffer), ec);
						size_t from = 0;
						size_t to = bytes_read;
						size_t nh = std::string::npos;
						size_t n = std::string::npos;
						size_t nc = std::string::npos;
						if (ec) {
							if (ec == boost::asio::error::eof || ec == boost::asio::error::broken_pipe) {
								from = std::string::npos;
								break;
							}
							else	throw std::system_error(ec);
						}


						do {
							std::string_view  haystack(buffer.data() + from, bytes_read - from);
							if (bufnum == 58163) {
								int f = 0;

							}
							if (possible_nc != std::string::npos) {
								if (from != 0) { throw read_error() << my_info(read_errorcode::PROGRAM_LOGIC_ERROR); }
								std::string_view  haystackBeginOnly(buffer.data(), possible_nc);
								std::string n_restofneedle(Mesytec::listmode::closing_signature + (sizeof(Mesytec::listmode::closing_signature) - possible_nc)
									, Mesytec::listmode::closing_signature + sizeof(Mesytec::listmode::closing_signature));
								if (haystackBeginOnly.find(n_restofneedle) != std::string::npos) {
									// needle confirmed, so remove possible_n from 
									closing_sigfound = true;
									never_closing_sigfound = false;
									from  = possible_nc;
									headerfound = false;
									check_closing_signature = false;
								}
								else {
									// no closing signature, so data is beginning of next data_block
									recvbuf_data.filldest(closing_signature, sizeof(closing_signature) - possible_nc);
									from = 0; // recheck from beginning of buffer
								}
								possible_nc = std::string::npos;
								continue;
							}
							if (check_closing_signature) {
								nc = haystack.find(ncneedle);
								if (nc == 0) {
									closing_sigfound = true;
									never_closing_sigfound = false;
									from += nc + sizeof(Mesytec::listmode::closing_signature);
									headerfound = false;
								}
								else if (bytes_read - from < sizeof(Mesytec::listmode::closing_signature) && nc == std::string::npos) {
									// check only for closing if directly after data_signature
									int i = (int)(bytes_read - from);
									size_t sep_size = sizeof(Mesytec::listmode::closing_signature) - i;
									std::string_view  haystackBeginOnly(buffer.data() + (bytes_read - i), i);
									std::string n_possibleneedle(Mesytec::listmode::closing_signature, Mesytec::listmode::closing_signature + sep_size);
									if (haystackBeginOnly.find(n_possibleneedle) != std::string::npos) {
										//recvbuf_data.filldest(buffer.data() + from, i); //no fill because closing_sig follows data without gap
										possible_nc = sep_size;
										from = std::string::npos;
										break;
									};
								}
							}
							if (possible_remaining_n != std::string::npos) {
								if (from != 0) {throw read_error() << my_info(read_errorcode::PROGRAM_LOGIC_ERROR);}
								std::string_view  haystackBeginOnly(buffer.data(), possible_remaining_n);
								std::string n_restofneedle(Mesytec::listmode::datablock_separator + (sizeof(Mesytec::listmode::datablock_separator) - possible_remaining_n)
									, Mesytec::listmode::datablock_separator + sizeof(Mesytec::listmode::datablock_separator));
								if (haystackBeginOnly.find(n_restofneedle) != std::string::npos) {
									// we fill known last bytes of previous buffer
									short* sp = (short*)&recv_buf[0];
									for (int i = 0; i < recvbuf_data.transferred / sizeof(short); i++) 	boost::endian::big_to_native_inplace(sp[i]);
									listmoderead_analyzebuffer(error, recvbuf_data.transferred, recv_buf[0]);
									recvbuf_data = _recvbuf_data((char*)&recv_buf[0],fname, warning_type_wrong_sent);
									data_packets_found++;
									check_closing_signature = true;
									from = possible_remaining_n;
									// chceck for follwing closing!
								}
								else {
									check_closing_signature = false;
									recvbuf_data.filldest(datablock_separator, sizeof(datablock_separator) - possible_remaining_n);
									from = 0; // recheck buffer for needles
								}
								possible_remaining_n = std::string::npos;
								continue;

							}
							if (possible_nh != std::string::npos) {
								if (from != 0) { throw read_error() << my_info(read_errorcode::PROGRAM_LOGIC_ERROR); }
								std::string_view  haystackBeginOnly(buffer.data(), possible_nh);
								std::string n_restofneedle(Mesytec::listmode::header_separator + (sizeof(Mesytec::listmode::header_separator) - possible_nh)
									, Mesytec::listmode::header_separator + sizeof(Mesytec::listmode::header_separator));
								if (haystackBeginOnly.find(n_restofneedle) != std::string::npos) {
									// needle confirmed, so remove possible_n from 
									headerfound = true;
									never_headerfound = false;
									from += possible_nh; // forces next buffer read
								}
								else {
									// no header found yet so do not consider data?!
									from = 0; // recheck for eventual needles
								}
								possible_nh = std::string::npos;
								continue;
							}
							if (!headerfound) {
								nh = haystack.find(nhneedle);
								bool breakwhile = false;
								if (nh != std::string::npos) {
									headerfound = true;
									never_headerfound = false;
									from += nh + sizeof(Mesytec::listmode::header_separator);
									closing_sigfound = false;
								}
								else {
									// we could look for a partial header
									for (int i = 1; i < sizeof(Mesytec::listmode::header_separator); i++) {
										//i from 1 to 7
										size_t sep_size = sizeof(Mesytec::listmode::header_separator) - i;
										std::string_view  haystackBeginOnly(buffer.data() + (bytes_read - sep_size), sep_size);
										std::string n_possibleneedle(Mesytec::listmode::header_separator, Mesytec::listmode::header_separator + sep_size);
										if (haystackBeginOnly.find(n_possibleneedle) != std::string::npos) {
											possible_nh = i;
											from = std::string::npos; // forces next buffer read
											breakwhile = true;
											break;  // we are looking at buffer end only
										};
									}
								}
								if (breakwhile) break;
							}
							if (headerfound) {
								haystack =std::string_view(buffer.data() + from, bytes_read - from);
								n = haystack.find(nneedle);
								bool breakwhile = false;
								if (n != std::string::npos ) {
									recvbuf_data.filldest(buffer.data()+from , n);
									short* sp = (short*)&recv_buf[0];
									for (int i = 0; i < recvbuf_data.transferred / sizeof(short); i++) 	boost::endian::big_to_native_inplace(sp[i]);
									listmoderead_analyzebuffer(error, recvbuf_data.transferred, recv_buf[0]);
									recvbuf_data = _recvbuf_data((char*)&recv_buf[0],fname, warning_type_wrong_sent);
									data_packets_found++;
									from += n+sizeof(datablock_separator);
									check_closing_signature = true;
									// chceck for follwing closing!
									continue;
								}
								else {
									check_closing_signature = false;
									// could be a possible_n
								    	for (int i = 1; i < std::min(to - from, sizeof(Mesytec::listmode::datablock_separator)); i++) {
										//i from 1 to 7
										size_t sep_size = sizeof(Mesytec::listmode::datablock_separator) - i;
										std::string_view  haystackEndOnly(buffer.data() + (bytes_read - sep_size), sep_size);
										std::string n_possibleneedle(Mesytec::listmode::datablock_separator, Mesytec::listmode::datablock_separator + sep_size);
										if (haystackEndOnly.find(n_possibleneedle) != std::string::npos) {
											possible_remaining_n = i;
											recvbuf_data.filldest(buffer.data() + from, (bytes_read - sep_size) - from);
											from = std::string::npos;
											breakwhile = true;
											break;
										};
									}
								}
								if (breakwhile) break;
								size_t maxn= recvbuf_data.missing_to_fill_recbuf();
								size_t tmp=std::min(maxn, to - from);
								if (tmp == maxn && to-from != tmp) {
									// this means we have recvbuf full but no datablock_separator (or possible) recoginized.
									throw read_error() << my_info(read_errorcode::DATAPACKET_SIGNATURE_EXPECTED);
								}
								recvbuf_data.filldest(buffer.data() + from, to - from);
								from = to;
								check_closing_signature = false;
								
							}
							if (from == bytes_read) break;
							while(action lec=CheckAction()) {
								if (lec == action::start_reading) return;
								if (lec == action::wait_reading) boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
							}
							
						} while (from != std::string::npos);
						bufnum++;
						total_bytes_processed += bytes_read;
					} while (!ec);
					{
						LOG_INFO << fname << ": " << total_bytes_processed << " bytes processed  (" << data_packets_found << " data packets)" << std::endl;
						if (never_headerfound) LOG_ERROR << "never_headerfound=" << std::boolalpha << never_headerfound << " " << std::endl;
						if (never_closing_sigfound) LOG_ERROR << "never_closing_sigfound=" << std::boolalpha << never_closing_sigfound << " " << std::endl;
					}

				}
			

			
		};
	}

}
