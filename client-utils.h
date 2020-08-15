#ifndef _CLIENT_UTILS_H
#define _CLIENT_UTILS_H

#include "account-data.h"
#include "transceiver.h"
#include <purple.h>
#include <thread>

const char *errorCodeMessage();

std::string         messageTypeToString(const td::td_api::MessageContent &content);
std::string         proxyTypeToString(PurpleProxyType proxyType);

const char *        getPurpleStatusId(const td::td_api::UserStatus &tdStatus);
std::string         getPurpleBuddyName(const td::td_api::user &user);
std::vector<const td::td_api::user *> getUsersByPurpleName(const char *buddyName, TdAccountData &account,
                                                           const char *action);
PurpleConversation *getImConversation(PurpleAccount *account, const char *username);
PurpleConvChat *    getChatConversation(TdAccountData &account, const td::td_api::chat &chat,
                                        int chatPurpleId);
PurpleConvChat *    findChatConversation(PurpleAccount *account, const td::td_api::chat &chat);
void                updatePrivateChat(TdAccountData &account, const td::td_api::chat &chat, const td::td_api::user &user);
void                updateBasicGroupChat(TdAccountData &account, int32_t groupId);
void                updateSupergroupChat(TdAccountData &account, int32_t groupId);
void                removeGroupChat(PurpleAccount *purpleAccount, const td::td_api::chat &chat);
std::string         makeBasicDisplayName(const td::td_api::user &user);
std::string         getSenderPurpleName(const td::td_api::chat &chat, const td::td_api::message &message,
                                        TdAccountData &account);
std::string         getForwardSource(const td::td_api::messageForwardInfo &forwardInfo,
                                     TdAccountData &accountData);
void                getNamesFromAlias(const char *alias, std::string &firstName, std::string &lastName);
std::vector<PurpleChat *> findChatsByJoinString(const std::string &inviteLink);
std::vector<PurpleChat *> findChatsByNewGroup(const char *name, int type);

std::string getSenderDisplayName(const td::td_api::chat &chat, const TgMessageInfo &message,
                                 PurpleAccount *account);
std::string makeNoticeWithSender(const td::td_api::chat &chat, const TgMessageInfo &message,
                                 const char *noticeText, PurpleAccount *account);
std::string getMessageText(const td::td_api::formattedText &text);
void showMessageText(TdAccountData &account, const td::td_api::chat &chat, const TgMessageInfo &message,
                     const char *text, const char *notification, uint32_t extraFlags = 0);
void showMessageTextIm(TdAccountData &account, const char *purpleUserName, const char *text,
                       const char *notification, time_t timestamp, PurpleMessageFlags flags);
void showChatNotification(TdAccountData &account, const td::td_api::chat &chat,
                          const char *notification, PurpleMessageFlags extraFlags = (PurpleMessageFlags)0);
void showGenericFileInline(const td::td_api::chat &chat, const TgMessageInfo &message,
                           const std::string &filePath, const std::string &fileDescription,
                           TdAccountData &account);
void notifySendFailed(const td::td_api::updateMessageSendFailed &sendFailed, TdAccountData &account);
void updateChatConversation(PurpleConvChat *purpleChat, const td::td_api::basicGroupFullInfo &groupInfo,
                    const TdAccountData &account);
void updateChatConversation(PurpleConvChat *purpleChat, const td::td_api::supergroupFullInfo &groupInfo,
                    const TdAccountData &account);
void updateSupergroupChatMembers(PurpleConvChat *purpleChat, const td::td_api::chatMembers &members,
                                 const TdAccountData &account);

int  transmitMessage(int64_t chatId, const char *message, TdTransceiver &transceiver,
                     TdAccountData &account, TdTransceiver::ResponseCb response);

void requestRecoveryEmailConfirmation(PurpleConnection *gc, const char *emailInfo);

void updateSecretChat(td::td_api::object_ptr<td::td_api::secretChat> secretChat,
                      TdTransceiver &transceiver, TdAccountData &account);
void updateOption(const td::td_api::updateOption &option, TdAccountData &account);
void populateGroupChatList(PurpleRoomlist *roomlist, const std::vector<const td::td_api::chat *> &chats,
                           const TdAccountData &account);

class AccountThread {
public:
    using Callback = void (PurpleTdClient::*)(AccountThread *thread);
    static void setSingleThread();
    static bool isSingleThread();

    AccountThread(PurpleAccount *purpleAccount, Callback callback);
    virtual ~AccountThread() {}
    void startThread();
private:
    std::thread m_thread;
    std::string m_accountUserName;
    std::string m_accountProtocolId;
    Callback    m_callback;

    void            threadFunc();
    static gboolean mainThreadCallback(gpointer data);
protected:
    virtual void run() = 0;
};

#endif
