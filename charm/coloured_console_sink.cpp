#include "coloured_console_sink.h"
int dummy_ = 0;
#ifdef WIN32_PRE10OLDSTYLE

boost::log::sinks::auto_newline_mode coloured_console_sink::auto_newline = boost::log::sinks::auto_newline_mode::insert_if_missing;

inline bool ends_with(std::string const& value, std::string const& ending)
{
    if (ending.size() > value.size()) return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}


void coloured_console_sink::set_auto_newline_mode(boost::log::sinks::auto_newline_mode mode) {
    auto_newline = mode;
}

void coloured_console_sink::consume(boost::log::record_view const& rec, string_type const& formatted_string)
{
    auto level = rec.attribute_values()["Severity"].extract<boost::log::trivial::severity_level>();
    auto hstdout = GetStdHandle(STD_OUTPUT_HANDLE);

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hstdout, &csbi);

    SetConsoleTextAttribute(hstdout, get_colour(level.get()));
    std::cout << formatted_string;
    if (auto_newline == boost::log::sinks::auto_newline_mode::always_insert) std::cout << std::endl;
    if (auto_newline == boost::log::sinks::auto_newline_mode::insert_if_missing) {
       if(!ends_with(formatted_string,"\n")) std::cout << std::endl;
    }
    SetConsoleTextAttribute(hstdout, csbi.wAttributes);
}



WORD get_colour(boost::log::trivial::severity_level level)
{
    switch (level)
    {
    case boost::log::trivial::trace: return FOREGROUND_INTENSITY;
    case boost::log::trivial::debug: return FOREGROUND_GREEN;
    case boost::log::trivial::info: return FOREGROUND_GREEN| FOREGROUND_BLUE | FOREGROUND_INTENSITY;
    case boost::log::trivial::warning: return FOREGROUND_BLUE | FOREGROUND_RED| FOREGROUND_INTENSITY;
    case boost::log::trivial::error: return FOREGROUND_RED  | FOREGROUND_INTENSITY;
    case boost::log::trivial::fatal: return FOREGROUND_RED  | FOREGROUND_INTENSITY;
    default: return 0x0F;
    }
}

#endif
