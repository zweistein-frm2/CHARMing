#ifndef simpleLogger_h__
#define simpleLogger_h__
#pragma once



#define BOOST_LOG_DYN_LINK // necessary when linking the boost_log library dynamically
#include <boost/log/trivial.hpp>
#include <boost/log/sources/global_logger_storage.hpp>

#include <magic_enum.hpp>

namespace magic_enum {
    namespace ostream_operators {

        template <typename Char, typename Traits, typename E>
        auto operator<<(boost::log::formatting_ostream& os, E value) -> detail::enable_if_enum_t<E, boost::log::formatting_ostream&> {
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
        auto operator<<(boost::log::formatting_ostream& os, std::optional<E> value) -> detail::enable_if_enum_t<E, boost::log::formatting_ostream&> {
            if (value.has_value()) {
                os << value.value();
            }

            return os;
        }

    } // namespace magic_enum::ostream_operators
}



// just log messages with severity >= SEVERITY_THRESHOLD are written
#define SEVERITY_THRESHOLD logging::trivial::info

// register a global logger
BOOST_LOG_GLOBAL_LOGGER(logger, boost::log::sources::severity_logger_mt<boost::log::trivial::severity_level>)

void Add_File_Sink(std::string logfile);

// just a helper macro used by the macros below - don't use it in your code
#define LOG(severity) BOOST_LOG_SEV(logger::get(),boost::log::trivial::severity)

// ===== log macros =====
#define LOG_TRACE   LOG(trace)
#define LOG_DEBUG   LOG(debug)
#define LOG_INFO    LOG(info)
#define LOG_WARNING LOG(warning)
#define LOG_ERROR   LOG(error)
#define LOG_FATAL   LOG(fatal)

#endif
