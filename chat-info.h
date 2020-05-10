#include <purple.h>
#include <td/telegram/td_api.h>

GList       *getChatJoinInfo();
std::string  getChatName(const td::td_api::chat &chat);
GHashTable  *getChatComponents(const td::td_api::chat &chat);
const char  *getChatName(GHashTable *components);
int64_t      getTdlibChatId(const char *chatName);
