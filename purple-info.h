#ifndef _PURPLE_INFO_H
#define _PURPLE_INFO_H

#include <purple.h>
#include <td/telegram/td_api.h>

static constexpr int
    GROUP_TYPE_BASIC   = 1,
    GROUP_TYPE_SUPER   = 2,
    GROUP_TYPE_CHANNEL = 3;

GList       *getChatJoinInfo();
std::string  getChatName(const td::td_api::chat &chat);
GHashTable  *getChatComponents(const td::td_api::chat &chat);

const char  *getChatName(GHashTable *components);
const char  *getChatInviteLink(GHashTable *components);
const char  *getChatGroupName(GHashTable *components);
int          getChatGroupType(GHashTable *components);

int64_t      getTdlibChatId(const char *chatName);

#endif
