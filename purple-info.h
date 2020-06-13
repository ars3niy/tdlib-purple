#ifndef _PURPLE_INFO_H
#define _PURPLE_INFO_H

#include <purple.h>
#include <td/telegram/td_api.h>

static constexpr int
    GROUP_TYPE_BASIC   = 1,
    GROUP_TYPE_SUPER   = 2,
    GROUP_TYPE_CHANNEL = 3;

class PurpleTdClient;

const char  *getChatNameComponent();
GList       *getChatJoinInfo();
std::string  getPurpleChatName(const td::td_api::chat &chat);
GHashTable  *getChatComponents(const td::td_api::chat &chat);

const char  *getChatName(GHashTable *components);
const char  *getChatInviteLink(GHashTable *components);
const char  *getChatGroupName(GHashTable *components);
int          getChatGroupType(GHashTable *components);

int64_t      getTdlibChatId(const char *chatName);

namespace AccountOptions {
    constexpr const char *AutoDownloadLimit          = "media-size-threshold";
    constexpr const char *AutoDownloadLimitDefault   = "32";
    constexpr const char *BigDownloadHandling        = "media-handling-behavior";
    constexpr const char *BigDownloadHandlingAsk     = "ask";
    constexpr const char *BigDownloadHandlingDiscard = "discard";
    constexpr const char *BigDownloadHandlingDefault = BigDownloadHandlingAsk;
    constexpr const char *AcceptSecretChats          = "accept-secret-chats";
    constexpr const char *AcceptSecretChatsAsk       = "ask";
    constexpr const char *AcceptSecretChatsAlways    = "always";
    constexpr const char *AcceptSecretChatsNever     = "never";
    constexpr const char *AcceptSecretChatsDefault   = AcceptSecretChatsAsk;
    constexpr const char *AnimatedStickers           = "animated-stickers";
    constexpr gboolean    AnimatedStickersDefault    = TRUE;
};

namespace BuddyOptions {
    constexpr const char *ProfilePhotoId = "tdlib-profile-photo-id";
};

unsigned getAutoDownloadLimitKb(PurpleAccount *account);
bool     isSizeWithinLimit(unsigned size, unsigned limit);
bool     ignoreBigDownloads(PurpleAccount *account);
PurpleTdClient *getTdClient(PurpleAccount *account);

#endif
