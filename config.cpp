namespace config {

#define PACKAGE_VERSION "0.3.0"

char pluginId[]      = "telegram-tdlib";
char pluginName[]    = "Telegram (tdlib)";
char versionString[] = PACKAGE_VERSION GIT_COMMIT ", tdlib " TD_VERSION;
char pluginSummary[] = "Telegram using tdlib";
char pluginDesc[]    = "Telegram Protocol Plugin using tdlib";
char pluginAuthor[]  = "Arseniy Lartsev";
char projectUrl[]    = "https://github.com/ars3niy/tdlib-purple";

const char *configSubdir = "tdlib";
}
