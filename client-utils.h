#ifndef _CLIENT_UTILS_H
#define _CLIENT_UTILS_H

#include "account-data.h"
#include "transceiver.h"
#include <purple.h>

struct TgMessageInfo {
    std::string sender;
    time_t      timestamp;
    bool        outgoing;
    int64_t     repliedMessageId;
    std::string forwardedFrom;
};

// Matching completed downloads to chats they belong to
class DownloadRequest: public PendingRequest {
public:
    int64_t       chatId;
    TgMessageInfo message;
    td::td_api::object_ptr<td::td_api::file> thumbnail;

    // Could not pass object_ptr through variadic funciton :(
    DownloadRequest(uint64_t requestId, int64_t chatId, const TgMessageInfo &message, td::td_api::file *thumbnail)
    : PendingRequest(requestId), chatId(chatId), message(message), thumbnail(thumbnail) {}
};

std::string         messageTypeToString(const td::td_api::MessageContent &content);
const char         *getPurpleStatusId(const td::td_api::UserStatus &tdStatus);
std::string         getPurpleBuddyName(const td::td_api::user &user);
void                getUsersByPurpleName(const char *username, std::vector<const td::td_api::user*> &users,
                                         TdAccountData &accountData);
int64_t             getPrivateChatIdByPurpleName(const char *buddyName, TdAccountData &accountData,
                                                 const char *action);
PurpleConversation *getImConversation(PurpleAccount *account, const char *username);
PurpleConvChat     *getChatConversation(PurpleAccount *account, const td::td_api::chat &chat,
                                        int chatPurpleId, TdAccountData &accountData);
PurpleConvChat     *findChatConversation(PurpleAccount *account, const td::td_api::chat &chat);
void                updateChatConversationTitle(PurpleAccount *account, const td::td_api::chat &chat);
std::string         getSenderPurpleName(const td::td_api::chat &chat, const td::td_api::message &message,
                                        TdAccountData &accountData);
std::string         getForwardSource(const td::td_api::messageForwardInfo &forwardInfo,
                                     TdAccountData &accountData);
void                getNamesFromAlias(const char *alias, std::string &firstName, std::string &lastName);
std::vector<PurpleChat *> findChatsByInviteLink(const std::string &inviteLink);

void showMessageText(PurpleAccount *account, const td::td_api::chat &chat, const TgMessageInfo &message,
                     const char *text, const char *notification, TdAccountData &accountData,
                     uint32_t extraFlags = 0);
void setChatMembers(PurpleConvChat *purpleChat, const td::td_api::basicGroupFullInfo &groupInfo,
                    const TdAccountData &accountData);

void transmitMessage(int64_t chatId, const char *message, TdTransceiver &transceiver,
                     TdAccountData &accountData, TdTransceiver::ResponseCb response);

#endif
