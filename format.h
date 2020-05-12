#ifndef _FORMAT_H
#define _FORMAT_H

#include <string>

std::string formatMessage(const char *fmt, std::initializer_list<std::string> args);
std::string formatMessage(const char *fmt, const std::string &s);

template<typename T>
std::string formatMessage(const char *fmt, T arg)
{
    return formatMessage(fmt, {std::to_string(arg)});
}

#endif

