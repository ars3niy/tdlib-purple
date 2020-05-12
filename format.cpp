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
