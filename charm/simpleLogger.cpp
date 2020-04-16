#include "simpleLogger.h"


/*
 colorcodes = (
    'black' = > "\033[30m",
    'red' = > "\033[31m",
    'green' = > "\033[32m",
    'yellow' = > "\033[33m",
    'blue' = > "\033[34m",
    'magenta' = > "\033[35m",
    'cyan' = > "\033[36m",
    'white' = > "\033[37m",
    'brightblack' = > "\033[01;30m",
    'brightred' = > "\033[01;31m",
    'brightgreen' = > "\033[01;32m",
    'brightyellow' = > "\033[01;33m",
    'brightblue' = > "\033[01;34m",
    'brightmagenta' = > "\033[01;35m",
    'brightcyan' = > "\033[01;36m",
    'brightwhite' = > "\033[01;37m",
    'underlineblack' = > "\033[04;30m",
    'underlinered' = > "\033[04;31m",
    'underlinegreen' = > "\033[04;32m",
    'underlineyellow' = > "\033[04;33m",
    'underlineblue' = > "\033[04;34m",
    'underlinemagenta' = > "\033[04;35m",
    'underlinecyan' = > "\033[04;36m",
    'underlinewhite' = > "\033[04;37m",
    'blinkingblack' = > "\033[05;30m",
    'blinkingred' = > "\033[05;31m",
    'blinkinggreen' = > "\033[05;32m",
    'blinkingyellow' = > "\033[05;33m",
    'blinkingblue' = > "\033[05;34m",
    'blinkingmagenta' = > "\033[05;35m",
    'blinkingcyan' = > "\033[05;36m",
    'blinkingwhite' = > "\033[05;37m",
    'backgroundblack' = > "\033[07;30m",
    'backgroundred' = > "\033[07;31m",
    'backgroundgreen' = > "\033[07;32m",
    'backgroundyellow' = > "\033[07;33m",
    'backgroundblue' = > "\033[07;34m",
    'backgroundmagenta' = > "\033[07;35m",
    'backgroundcyan' = > "\033[07;36m",
    'backgroundwhite' = > "\033[07;37m",
    'default' = > "\033[0m"
    );
    */
void coloring_formatter(
    logging::record_view const& rec, logging::formatting_ostream& strm)
{

    auto severity = rec[logging::BOOST_LOG_VERSION_NAMESPACE::trivial::severity];
    if (severity)
    {
        // Set the color
        switch (severity.get())
        {
        case logging::BOOST_LOG_VERSION_NAMESPACE::trivial::debug:
            strm << "\033[32m";
            break;
        case logging::BOOST_LOG_VERSION_NAMESPACE::trivial::info:
            strm << "\033[01;36m";
            break;
        case logging::BOOST_LOG_VERSION_NAMESPACE::trivial::warning:
            strm << "\033[01;35m";
            break;
        case logging::BOOST_LOG_VERSION_NAMESPACE::trivial::error:
        case logging::BOOST_LOG_VERSION_NAMESPACE::trivial::fatal:
            strm << "\033[01;31m";
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
    sink->locked_backend().get()->set_auto_newline_mode(boost::log::BOOST_LOG_VERSION_NAMESPACE::sinks::auto_newline_mode::disabled_auto_newline);

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