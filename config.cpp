namespace config {

#define PACKAGE_VERSION "0.0.0"
#define GIT_COMMIT      "unknown"

char pluginId[]      = "tdlib-purple";
char pluginName[]    = "Telegram (tdlib)";
char versionString[] = PACKAGE_VERSION "\n\t\t\tcommit: " GIT_COMMIT "\n\t\t\ttdlib: " TD_VERSION;
char pluginSummary[] = "Telegram using tdlib";
char pluginDesc[]    = "Telegram Protocol Plugin using tdlib";
char pluginAuthor[]  = "Arseniy Lartsev";
char projectUrl[]    = "https://github.com/ars3niy/tdlib-purple";

}
