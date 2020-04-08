#ifndef simpleLogger_hpp__
#define simpleLogger_hpp__
#pragma once
#include <boost/iostreams/device/file_descriptor.hpp>


//#define BOOST_LOG_DYN_LINK // necessary when linking the boost_log library dynamically
#include <boost/log/trivial.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
#include "stdafx.h"
#include <boost/log/core/core.hpp>
#include <boost/log/expressions/formatters/date_time.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/sinks/auto_newline_mode.hpp>
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
#ifdef WIN32_PRE10OLDSTYLE
#include "coloured_console_sink.h"
#endif
#ifdef __cpp_lib_string_view
#include <magic_enum.hpp>
namespace magic_enum {
    namespace ostream_operators {

        template <typename Char, typename Traits, typename E>
        inline auto operator<<(boost::log::formatting_ostream& os, E value) -> detail::enable_if_enum_t<E, boost::log::formatting_ostream&> {
            if (auto name = enum_name(value); !name.empty()) {
                for (auto c : name) {
                    os.put(c);
                }
            }
            else {
                os << enum_integer(value);
            }

            return os;
        }

        template <typename Char, typename Traits, typename E>
        inline auto operator<<(boost::log::formatting_ostream& os, std::optional<E> value) -> detail::enable_if_enum_t<E, boost::log::formatting_ostream&> {
            if (value.has_value()) {
                os << value.value();
            }

            return os;
        }

    } // namespace magic_enum::ostream_operators
}
#endif
// just log messages with severity >= SEVERITY_THRESHOLD are written
extern EXTERN_FUNCDECLTYPE boost::log::trivial::severity_level SEVERITY_THRESHOLD;

// register a global logger
BOOST_LOG_GLOBAL_LOGGER(logger, boost::log::sources::severity_logger_mt<boost::log::trivial::severity_level>)


// just a helper macro used by the macros below - don't use it in your code
#define LOG(severity) BOOST_LOG_SEV(logger::get(),boost::log::trivial::severity)

// ===== log macros =====
#define LOG_DEBUG   LOG(debug)
#define LOG_INFO    LOG(info)
#define LOG_WARNING LOG(warning)
#define LOG_ERROR   LOG(error)


namespace logging = boost::log;
namespace src = boost::log::sources;
namespace expr = boost::log::expressions;
namespace sinks = boost::log::sinks;
namespace attrs = boost::log::attributes;

BOOST_LOG_ATTRIBUTE_KEYWORD(line_id, "LineID", unsigned int)
BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)
BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", logging::trivial::severity_level)

void coloring_formatter(
    logging::record_view const& rec, logging::formatting_ostream& strm)
{

    auto severity = rec[logging::BOOST_LOG_VERSION_NAMESPACE::trivial::severity];
    if (severity)
    {
        // Set the color
        switch (severity.get())
        {
        case logging::BOOST_LOG_VERSION_NAMESPACE::trivial::info:
            strm << "\033[32m";
            break;
        case logging::BOOST_LOG_VERSION_NAMESPACE::trivial::warning:
            strm << "\033[33m";
            break;
        case logging::BOOST_LOG_VERSION_NAMESPACE::trivial::error:
        case logging::BOOST_LOG_VERSION_NAMESPACE::trivial::fatal:
            strm << "\033[31m";
            break;
        default:
            break;
        }
    }

    // auto a = expr::stream << logging::BOOST_LOG_VERSION_NAMESPACE::expressions::format_date_time(timestamp, "%Y-%m-%d, %H:%M:%S.%f");

    strm << rec[timestamp] << " "
        << "[" << severity << "]"
        << " - " << rec[expr::smessage];

    if (severity)
    {
        // Restore the default color
        strm << "\033[0m";
    }
}

namespace Zweistein {
    namespace Logger {
        void Add_File_Sink(std::string logfile) {
            typedef sinks::synchronous_sink<sinks::text_ostream_backend> text_sink;
            boost::shared_ptr<text_sink> sink = boost::make_shared<text_sink>();

            // add a logfile stream to our sink
            sink->locked_backend()->add_stream(boost::make_shared<std::ofstream>(logfile));


            // specify the format of the log message
            logging::formatter formatter = expr::stream
                //     << std::setw(7) << std::setfill('0') << line_id << std::setfill(' ') << " | "
                << expr::format_date_time(timestamp, "%Y-%m-%d, %H:%M:%S.%f") << " "
                << "[" << logging::trivial::severity << "]"
                << " - " << expr::smessage;
            sink->set_formatter(formatter);

            // only messages with severity >= SEVERITY_THRESHOLD are written
            sink->set_filter(severity >= SEVERITY_THRESHOLD);

            // "register" our sink
            logging::core::get()->add_sink(sink);

        }
    }
}

BOOST_LOG_GLOBAL_LOGGER_INIT(logger, src::severity_logger_mt) {
    src::severity_logger_mt<boost::log::trivial::severity_level> logger;

    // add attributes
 //   logger.add_attribute("LineID", attrs::counter<unsigned int>(1));     // lines are sequentially numbered
    logger.add_attribute("TimeStamp", attrs::local_clock());             // each log line gets a timestamp
#ifdef WIN32_PRE10OLDSTYLE
    // add a text sink
    typedef sinks::synchronous_sink<class coloured_console_sink> coloured_console_sink_t;
    auto sink = boost::make_shared<coloured_console_sink_t>();
    logging::formatter formatter = expr::stream
        //  << std::setw(7) << std::setfill('0') << line_id << std::setfill(' ') << " | "
        << expr::format_date_time(timestamp, "%Y-%m-%d, %H:%M:%S.%f") << " "
        << "[" << logging::trivial::severity << "]"
        << " - " << expr::smessage;
    sink->set_formatter(formatter);

#else
    typedef sinks::synchronous_sink<sinks::text_ostream_backend> text_sink;
    boost::shared_ptr<text_sink> sink = boost::make_shared<text_sink>();
    // add "console" output stream to our sink

    sink->locked_backend().get()->set_auto_newline_mode(boost::log::BOOST_LOG_VERSION_NAMESPACE::sinks::auto_newline_mode::disabled_auto_newline);
    sink->locked_backend()->add_stream(boost::shared_ptr<std::ostream>(&std::clog, boost::null_deleter()));
    sink->set_formatter(&coloring_formatter);

#endif
    // only messages with severity >= SEVERITY_THRESHOLD are written
    sink->set_filter(severity >= SEVERITY_THRESHOLD);
    // "register" our sink
    logging::core::get()->add_sink(sink);


    return logger;
}
#endif


