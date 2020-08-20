#include "format.h"
#include <fmt/format.h>

std::string formatMessage(const char *fmt, std::initializer_list<std::string> args)
{
    fmt::dynamic_format_arg_store<fmt::format_context> fa;

    for (const std::string &arg: args)
        fa.push_back(arg);

    return fmt::vformat(fmt, fa);
}

std::string formatMessage(const char *fmt, const std::string &s)
{
    return formatMessage(fmt, {s});
}

std::string formatDuration(int32_t seconds)
{
    int32_t hours = seconds / 3600;
    seconds -= hours * 3600;
    int32_t minutes = seconds / 60;
    seconds -= minutes * 60;
    // TRANSLATOR: Time format. For HH:MM:SS use "{:02}:{:02}:{:02}" without the quotes.
    return fmt::format(_("{:02}:{:02}:{:02}"), hours, minutes, seconds);
}
