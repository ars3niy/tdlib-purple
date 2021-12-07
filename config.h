#ifndef _CONFIG_H
#define _CONFIG_H

namespace config {

// These have to be non-const char* because that's what libpurple accepts
extern char pluginId[];
extern char pluginName[];
extern char versionString[];
extern char pluginSummary[];
extern char pluginDesc[];
extern char pluginAuthor[];
extern char projectUrl[];

extern const char *api_id;
extern const char *api_hash;

extern const char *stuff;
extern const char *configSubdir;
}

#endif
