#ifndef _TD_CLIENT_H
#define _TD_CLIENT_H

#include "account-data.h"
#include "transceiver.h"
#include <purple.h>

class PurpleTdClient {
public:
    PurpleTdClient(PurpleAccount *acct, ITransceiverBackend *testBackend);
    ~PurpleTdClient();

    static void setLogLevel(int level);
    int  sendMessage(const char *buddyName, const char *message);
    void addContact(const char *phoneNumber, const char *alias);
    bool joinChat(const char *chatName);
    int  sendGroupMessage(int purpleChatId, const char *message);
private:
    using TdObjectPtr   = td::td_api::object_ptr<td::td_api::Object>;
    using ResponseCb    = void (PurpleTdClient::*)(uint64_t requestId, TdObjectPtr object);
    using TdAuthCodePtr = td::td_api::object_ptr<td::td_api::authenticationCodeInfo>;

    void       processUpdate(td::td_api::Object &object);
    void       processAuthorizationState(td::td_api::AuthorizationState &authState);

    // Login sequence start
    void       sendTdlibParameters();
    void       sendPhoneNumber();
    void       requestAuthCode();
    static void requestCodeEntered(PurpleTdClient *self, const gchar *code);
    static void requestCodeCancelled(PurpleTdClient *self);
    void       authResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    void       notifyAuthError(td::td_api::object_ptr<td::td_api::error> error);
    void       connectionReady();
    void       setPurpleConnectionInProgress();
    void       setPurpleConnectionUpdating();
    void       getContactsResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    void       getChatsResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    void       requestMissingPrivateChats();
    void       loginCreatePrivateChatResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    // List of chats is requested after connection is ready, and when response is received,
    // then we report to libpurple that we are connected
    void       updatePurpleChatListAndReportConnected();
    // Login sequence end

    std::string getSenderPurpleName(const td::td_api::chat &chat, const td::td_api::message &message);
    void       showMessage(const td::td_api::chat &chat, const td::td_api::message &message);
    void       showMessageText(const td::td_api::chat &chat, const std::string &sender, const char *text,
                               const char *notification, time_t timestamp, bool outgoing);
    void       showMessageTextChat(const td::td_api::chat &chat, const std::string &sender, const char *text,
                                   const char *notification, time_t timestamp, bool outgoing);
    void       showTextMessage(const td::td_api::chat &chat, const td::td_api::message &message,const td::td_api::messageText &text);
    void       showPhotoMessage(const td::td_api::chat &chat, const td::td_api::message &message, const td::td_api::messagePhoto &photo);
    void       showDocument(const td::td_api::chat &chat, const td::td_api::message &message, const td::td_api::messageDocument &document);
    void       showVideo(const td::td_api::chat &chat, const td::td_api::message &message, const td::td_api::messageVideo &video);
    void       onIncomingMessage(td::td_api::object_ptr<td::td_api::message> message);

    void       updateUserStatus(uint32_t userId, td::td_api::object_ptr<td::td_api::UserStatus> status);
    void       updateUser(td::td_api::object_ptr<td::td_api::user> user);
    void       updateGroup(td::td_api::object_ptr<td::td_api::basicGroup> group);
    void       updateSupergroup(td::td_api::object_ptr<td::td_api::supergroup> group);
    void       updatePrivateChat(const td::td_api::chat &chat, const td::td_api::user &user);
    void       updateBasicGroupChat(int32_t groupId);
    void       updateSupergroupChat(int32_t groupId);

    void       addChat(td::td_api::object_ptr<td::td_api::chat> chat);
    void       handleUserChatAction(const td::td_api::updateUserChatAction &updateChatAction);
    void       showUserChatAction(int32_t userId, bool isTyping);
    void       importContactResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    void       addContactResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    void       addContactCreatePrivateChatResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    void       notifyFailedContact(const std::string &phoneNumber, const std::string &errorMessage);

    void       messagePhotoDownloadResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    void       showPhoto(int64_t chatId, const std::string &sender, int32_t timestamp, bool outgoing,
                         const std::string &filePath);

    PurpleAccount                      *m_account;
    TdTransceiver                       m_transceiver;
    TdAccountData                       m_data;

    // These data structures are only used in poll thread and therefore don't need to be thread-safe
    int32_t                             m_lastAuthState = 0;
    TdAuthCodePtr                       m_authCodeInfo;
    std::vector<int32_t>                m_usersForNewPrivateChats;
};

#endif
