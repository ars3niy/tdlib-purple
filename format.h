#ifndef _FORMAT_H
#define _FORMAT_H

#include "buildopt.h"

#include <string>

#ifndef NoTranslations
#  define GETTEXT_PACKAGE "tdlib-purple"
#  include <glib/gi18n-lib.h>
#  define P_(Singular,Plural,N) ((char *) g_dngettext (GETTEXT_PACKAGE, Singular, Plural, N))
#else
// The correct way to handle it is:
//#  define _(String) String
//#  define P_(Singular,Plural,N) Plural
//#  define N_(String) String
//#  define gettext(String) String
// But glib isn't const-correct, and gcc treats this as a fatal error.
#  define _(String) const_cast<char *>(String)
#  define P_(Singular,Plural,N) const_cast<char *>(Plural)
#  define N_(String) const_cast<char *>(String)
#  define gettext(String) const_cast<char *>(String)
#endif

std::string formatMessage(const char *fmt, std::initializer_list<std::string> args);
std::string formatMessage(const char *fmt, const std::string &s);

template<typename T>
std::string formatMessage(const char *fmt, T arg)
{
    return formatMessage(fmt, {std::to_string(arg)});
}

#endif
