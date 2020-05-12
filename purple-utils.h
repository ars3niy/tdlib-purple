#ifndef _PURPLE_UTILS_H
#define _PURPLE_UTILS_H

#include "account-data.h"
#include <purple.h>

std::string         messageTypeToString(const td::td_api::MessageContent &content);
const char         *getPurpleStatusId(const td::td_api::UserStatus &tdStatus);
const char         *getPurpleUserName(const td::td_api::user &user);
PurpleConversation *getImConversation(PurpleAccount *account, const char *username);
PurpleConvChat     *getChatConversation(PurpleAccount *account, const td::td_api::chat &chat,
                                        int chatPurpleId, TdAccountData &accountData);
std::string         getSenderPurpleName(const td::td_api::chat &chat, const td::td_api::message &message,
                                        TdAccountData &accountData);
std::vector<PurpleChat *> findChatsByInviteLink(const std::string &inviteLink);

void showMessageTextIm(PurpleAccount *account, const char *purpleUserName, const char *text,
                       const char *notification, time_t timestamp, bool outgoing);
void showMessageTextChat(PurpleAccount *account, const td::td_api::chat &chat,
                         const std::string &sender, const char *text,
                         const char *notification, time_t timestamp, bool outgoing,
                         TdAccountData &accountData);
void showMessageText(PurpleAccount *account, const td::td_api::chat &chat, const std::string &sender,
                     const char *text, const char *notification, time_t timestamp, bool outgoing,
                     TdAccountData &accountData);
void setChatMembers(PurpleConvChat *purpleChat, const td::td_api::basicGroupFullInfo &groupInfo,
                    const TdAccountData &accountData);

#endif
