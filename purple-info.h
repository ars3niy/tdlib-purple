#ifndef _PURPLE_INFO_H
#define _PURPLE_INFO_H

#include "identifiers.h"
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
const char  *getChatJoinString(GHashTable *components);
const char  *getChatGroupName(GHashTable *components);
int          getChatGroupType(GHashTable *components);

ChatId       getTdlibChatId(const char *chatName);

namespace AccountOptions {
    constexpr const char *AutoDownloadLimit          = "media-size-threshold";
    constexpr const char *AutoDownloadLimitDefault   = "32";
    constexpr const char *BigDownloadHandling        = "media-handling-behavior";
    constexpr const char *BigDownloadHandlingAsk     = "ask";
    constexpr const char *BigDownloadHandlingDiscard = "discard";
    constexpr const char *BigDownloadHandlingDefault = BigDownloadHandlingAsk;
    constexpr const char *EnableSecretChats          = "enable-secret-chats";
    constexpr gboolean    EnableSecretChatsDefault   = TRUE;
    constexpr const char *AnimatedStickers           = "animated-stickers";
    constexpr gboolean    AnimatedStickersDefault    = TRUE;
    constexpr const char *DownloadBehaviour          = "download-behaviour";
    constexpr const char *DownloadBehaviourHyperlink = "hyperlink";
    constexpr const char *DownloadBehaviourStandard  = "file-transfer";
    const char           *DownloadBehaviourDefault();
};

namespace BuddyOptions {
    constexpr const char *ProfilePhotoId = "tdlib-profile-photo-id";
};

unsigned getAutoDownloadLimitKb(PurpleAccount *account);
bool     isSizeWithinLimit(unsigned size, unsigned limit);
bool     ignoreBigDownloads(PurpleAccount *account);
PurpleTdClient *getTdClient(PurpleAccount *account);
const char *getUiName();

#endif
