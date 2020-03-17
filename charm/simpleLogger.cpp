#include "stdafx.h"
#include <boost/log/core/core.hpp>
#include <boost/log/expressions/formatters/date_time.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/core/null_deleter.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>
#include <iostream>
#include <fstream>
#include <ostream>
#include "simpleLogger.h"

namespace logging = boost::log;
namespace src = boost::log::sources;
namespace expr = boost::log::expressions;
namespace sinks = boost::log::sinks;
namespace attrs = boost::log::attributes;

BOOST_LOG_ATTRIBUTE_KEYWORD(line_id, "LineID", unsigned int)
BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)
BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", logging::trivial::severity_level)



namespace boost::log::BOOST_LOG_VERSION_NAMESPACE {

        void  console_color_formatter(logging::record_view const& rec, logging::formatting_ostream& strm)

        {
#ifdef WIN32

#else //LINUX

            auto severity = rec[logging::trivial::severity];
            switch (severity.get()) {

            case logging::trivial::info:
                strm << "\033[32m";
            case logging::trivial::warning:
                strm << "\033[33m";
            case logging::trivial::error:
            case logging::trivial::fatal:
                strm << "\033[31m";

            }

#endif

            strm 
                << std::setw(7) << std::setfill('0') << line_id << std::setfill(' ') << " | "
                << expr::format_date_time(timestamp, "%Y-%m-%d, %H:%M:%S.%f") << " "
                << "[" << logging::trivial::severity << "]"
                << " - " << expr::smessage
#ifdef WIN32
                ;
#else
                 << "\033[0m";

#endif
        }


}


BOOST_LOG_GLOBAL_LOGGER_INIT(logger, src::severity_logger_mt) {
    src::severity_logger_mt<boost::log::trivial::severity_level> logger;

    // add attributes
    logger.add_attribute("LineID", attrs::counter<unsigned int>(1));     // lines are sequentially numbered
    logger.add_attribute("TimeStamp", attrs::local_clock());             // each log line gets a timestamp

    // add a text sink
    typedef sinks::synchronous_sink<sinks::text_ostream_backend> text_sink;
    boost::shared_ptr<text_sink> sink = boost::make_shared<text_sink>();

    // add "console" output stream to our sink
    sink->locked_backend()->add_stream(boost::shared_ptr<std::ostream>(&std::clog, boost::null_deleter()));
    // specify the format of the log message
    logging::formatter formatter = expr::stream
        << std::setw(7) << std::setfill('0') << line_id << std::setfill(' ') << " | "
        << expr::format_date_time(timestamp, "%Y-%m-%d, %H:%M:%S.%f") << " "
        << "[" << logging::trivial::severity << "]"
        << " - " << expr::smessage ;
    sink->set_formatter(formatter);
    

   // sink->set_formatter(&boost::log::BOOST_LOG_VERSION_NAMESPACE::console_color_formatter);

    // only messages with severity >= SEVERITY_THRESHOLD are written
    sink->set_filter(severity >= SEVERITY_THRESHOLD);

    // "register" our sink
    logging::core::get()->add_sink(sink);

    
    return logger;
}


void Add_File_Sink(std::string logfile) {
    typedef sinks::synchronous_sink<sinks::text_ostream_backend> text_sink;
    boost::shared_ptr<text_sink> sink = boost::make_shared<text_sink>();

    // add a logfile stream to our sink
    sink->locked_backend()->add_stream(boost::make_shared<std::ofstream>(logfile));

   
    // specify the format of the log message
    logging::formatter formatter = expr::stream
        << std::setw(7) << std::setfill('0') << line_id << std::setfill(' ') << " | "
        << expr::format_date_time(timestamp, "%Y-%m-%d, %H:%M:%S.%f") << " "
        << "[" << logging::trivial::severity << "]"
        << " - " << expr::smessage;
    sink->set_formatter(formatter);

    // only messages with severity >= SEVERITY_THRESHOLD are written
    sink->set_filter(severity >= SEVERITY_THRESHOLD);

    // "register" our sink
    logging::core::get()->add_sink(sink);


}