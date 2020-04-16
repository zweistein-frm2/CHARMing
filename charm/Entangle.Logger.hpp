#pragma once

#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <sstream>
#include <ostream>
#include <iostream>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/device/null.hpp>
#include <boost/exception/diagnostic_information.hpp>
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
	struct Logger : public std::ostream{

		class _StreamBuf : public std::stringbuf {
			boost::iostreams::file_descriptor_sink output; 
			std::string prefix;
		public:
			_StreamBuf(boost::iostreams::file_descriptor_sink &sink):output(sink),prefix(" : INFO : "){}
			~_StreamBuf() {
				if (pbase() != pptr()) {
					putOutput();
				}
			}
			void setPrefix(std::string pre) {
				// we should flush before
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
					output.write(prefix.c_str(), prefix.length());
					output.write(str().c_str(), str().length());
					
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

	boost::shared_ptr <Entangle::Logger> ptrlogger=nullptr;
	void Init(int fd) {
		try {
			ptrlogger = boost::shared_ptr < Entangle::Logger>(new Logger(fd));
			
		}
		catch (boost::exception &e) {
			std::cerr << boost::diagnostic_information(e) << std::endl;
		}
	}
	extern severity_level SEVERITY_THRESHOLD;

}

boost::iostreams::stream< boost::iostreams::null_sink > nullOstream((boost::iostreams::null_sink()));

// all this works properly only when we use std::endl to terminate a log message ?


#define LOG_DEBUG ((Entangle::SEVERITY_THRESHOLD<=Entangle::debug)? ((Entangle::ptrlogger?	Entangle::ptrlogger->setPrefix(" : DEBUG : "):	std::cout<<"DEBUG:")): nullOstream)
#define LOG_INFO  ((Entangle::SEVERITY_THRESHOLD<=Entangle::info)? ((Entangle::ptrlogger?		Entangle::ptrlogger->setPrefix(" : INFO : "):	std::cout<<"INFO:")): nullOstream)
#define LOG_WARNING ((Entangle::SEVERITY_THRESHOLD<=Entangle::warning)? ((Entangle::ptrlogger?	Entangle::ptrlogger->setPrefix(" : WARN : "):	std::cout<<"WARN:")): nullOstream)
#define LOG_ERROR ((Entangle::SEVERITY_THRESHOLD<=Entangle::error)? ((Entangle::ptrlogger?		Entangle::ptrlogger->setPrefix(" : ERROR : ")	:std::cout<<"ERROR:")): nullOstream)




