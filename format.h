#ifndef _FORMAT_H
#define _FORMAT_H

#include <string>
#include <purple.h>
#include "translate.h"
#include "config.h"

std::string formatMessage(const char *fmt, std::initializer_list<std::string> args);
std::string formatMessage(const char *fmt, const std::string &s);

template<typename T>
std::string formatMessage(const char *fmt, T arg)
{
    return formatMessage(fmt, {std::to_string(arg)});
}

std::string formatDuration(int32_t seconds);

void purpleDebug(const char *fmt, std::initializer_list<std::string> args);

template<typename T>
void purpleDebug(const char *fmt, T arg)
{
    std::string message = formatMessage(fmt, arg);
    purple_debug_misc(config::pluginId, "%s\n", message.c_str());
}

#endif
