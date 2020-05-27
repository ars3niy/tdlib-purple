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

using FileDownloadCb = void (PurpleTdClient::*)(int64_t chatId, const TgMessageInfo &message,
                                                const std::string &filePath, const char *caption,
                                                td::td_api::object_ptr<td::td_api::file> thumbnail);

// Matching completed downloads to chats they belong to
class DownloadRequest: public PendingRequest {
public:
    int64_t        chatId;
    TgMessageInfo  message;
    td::td_api::object_ptr<td::td_api::file> thumbnail;
    FileDownloadCb callback;

    // Could not pass object_ptr through variadic funciton :(
    DownloadRequest(uint64_t requestId, int64_t chatId, const TgMessageInfo &message,
                    td::td_api::file *thumbnail, FileDownloadCb callback)
    : PendingRequest(requestId), chatId(chatId), message(message), thumbnail(thumbnail),
      callback(callback) {}
};

std::string         messageTypeToString(const td::td_api::MessageContent &content);
std::string         proxyTypeToString(PurpleProxyType proxyType);

const char *        getPurpleStatusId(const td::td_api::UserStatus &tdStatus);
std::string         getPurpleBuddyName(const td::td_api::user &user);
void                getUsersByPurpleName(const char *username, std::vector<const td::td_api::user*> &users,
                                         TdAccountData &account);
int64_t             getPrivateChatIdByPurpleName(const char *buddyName, TdAccountData &account,
                                                 const char *action);
PurpleConversation *getImConversation(PurpleAccount *account, const char *username);
PurpleConvChat *    getChatConversation(TdAccountData &account, const td::td_api::chat &chat,
                                        int chatPurpleId);
PurpleConvChat *    findChatConversation(PurpleAccount *account, const td::td_api::chat &chat);
void                updatePrivateChat(TdAccountData &account, const td::td_api::chat &chat, const td::td_api::user &user);
void                updateBasicGroupChat(TdAccountData &account, int32_t groupId);
void                updateSupergroupChat(TdAccountData &account, int32_t groupId);
void                removeGroupChat(PurpleAccount *purpleAccount, const td::td_api::chat &chat);
std::string         getSenderPurpleName(const td::td_api::chat &chat, const td::td_api::message &message,
                                        TdAccountData &account);
std::string         getForwardSource(const td::td_api::messageForwardInfo &forwardInfo,
                                     TdAccountData &accountData);
void                getNamesFromAlias(const char *alias, std::string &firstName, std::string &lastName);
std::vector<PurpleChat *> findChatsByInviteLink(const std::string &inviteLink);
std::vector<PurpleChat *> findChatsByNewGroup(const char *name, int type);

void showMessageText(TdAccountData &account, const td::td_api::chat &chat, const TgMessageInfo &message,
                     const char *text, const char *notification, uint32_t extraFlags = 0);
void setChatMembers(PurpleConvChat *purpleChat, const td::td_api::basicGroupFullInfo &groupInfo,
                    const TdAccountData &account);

void transmitMessage(int64_t chatId, const char *message, TdTransceiver &transceiver,
                     TdAccountData &account, TdTransceiver::ResponseCb response);

void requestRecoveryEmailConfirmation(PurpleConnection *gc, const char *emailInfo);

unsigned getFileSize(const td::td_api::file &file);
unsigned getFileSizeKb(const td::td_api::file &file);

#endif
