/***************************************************************************
 *   Copyright (C) 2019 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/

#pragma once

#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <sstream>
#include <ostream>
#include <iostream>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/device/null.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/circular_buffer.hpp>
#include "Zweistein.Locks.hpp"

namespace Entangle{
	enum severity_level
	{
		trace,
		debug,
		info,
		warning,
		error,
		fatal
	};
	extern Zweistein::Lock cbLock;
	extern  boost::shared_ptr<boost::circular_buffer<std::string>> ptrcb;
	struct Logger : public std::ostream{

		class _StreamBuf : public std::stringbuf {
			boost::iostreams::file_descriptor_sink output;
			std::string prefix;
			static std::string msg;
		public:
			_StreamBuf(boost::iostreams::file_descriptor_sink &sink):output(sink){
				setPrefix(" : INFO : ");
			}
			~_StreamBuf() {
				if (pbase() != pptr()) {
					putOutput();
				}
			}
			void setPrefix(std::string pre) {
					prefix = pre;
			}
			virtual int sync() {
				putOutput();
				return 0;
			}
			void putOutput() {

				// Called by destructor.
				// destructor can not call virtual methods.
				try {
					Zweistein::WriteLock w_lock(Entangle::cbLock);
					output.write(prefix.c_str(), prefix.length());
					//output.write(str().c_str(), str().length());
					msg = str(); // we need the write data in a static or variable
					// otherwise heap_corruption in output in certain multithreading  exit thread conditions
					// LOG call just before exit of thread)
					output.write(msg.c_str(), msg.length());
					if (*msg.rbegin() == '\n') *msg.rbegin() = '\0';
					{
						ptrcb->push_back(msg);
					}
					str("");

				}
				catch (boost::exception &e) {
					std::cerr<< boost::diagnostic_information(e)<<std::endl;
				}
			}
		};
		boost::iostreams::file_descriptor_sink sink;
		_StreamBuf buffer;

		Logger(int fd) :sink(fd, boost::iostreams::file_descriptor_flags::never_close_handle),buffer(sink), std::ostream(&buffer) {}
		Logger& setPrefix(std::string s) {
			buffer.setPrefix(s);
			return *this;
		}


	};

	extern boost::shared_ptr <Entangle::Logger> ptrlogger;
	void Init(int fd);
	extern severity_level SEVERITY_THRESHOLD;

}

extern boost::iostreams::stream< boost::iostreams::null_sink > nullOstream;


#define LOG_DEBUG ((Entangle::SEVERITY_THRESHOLD<=Entangle::debug)? ((Entangle::ptrlogger?	Entangle::ptrlogger->setPrefix(" : DEBUG : "):	std::cout<<"DEBUG:")): nullOstream)
#define LOG_INFO  ((Entangle::SEVERITY_THRESHOLD<=Entangle::info)? ((Entangle::ptrlogger?		Entangle::ptrlogger->setPrefix(" : INFO : "):	std::cout<<"INFO:")): nullOstream)
#define LOG_WARNING ((Entangle::SEVERITY_THRESHOLD<=Entangle::warning)? ((Entangle::ptrlogger?	Entangle::ptrlogger->setPrefix(" : WARN : "):	std::cout<<"WARN:")): nullOstream)
#define LOG_ERROR ((Entangle::SEVERITY_THRESHOLD<=Entangle::error)? ((Entangle::ptrlogger?		Entangle::ptrlogger->setPrefix(" : ERROR : ")	:std::cout<<"ERROR:")): nullOstream)




