#ifndef _TD_CLIENT_H
#define _TD_CLIENT_H

#include "account-data.h"
#include "transceiver.h"
#include "client-utils.h"
#include <purple.h>

class PurpleTdClient {
public:
    PurpleTdClient(PurpleAccount *acct, ITransceiverBackend *testBackend);
    ~PurpleTdClient();

    static void setLogLevel(int level);
    int  sendMessage(const char *buddyName, const char *message);
    void addContact(const std::string &phoneNumber, const std::string &alias, const std::string &groupName);
    bool joinChat(const char *chatName);
    int  sendGroupMessage(int purpleChatId, const char *message);
    bool joinChatByLink(const char *inviteLink);
private:
    using TdObjectPtr   = td::td_api::object_ptr<td::td_api::Object>;
    using ResponseCb    = void (PurpleTdClient::*)(uint64_t requestId, TdObjectPtr object);

    void       processUpdate(td::td_api::Object &object);
    void       processAuthorizationState(td::td_api::AuthorizationState &authState);

    // Login sequence start
    void       sendTdlibParameters();
    void       sendPhoneNumber();
    void       requestAuthCode(const td::td_api::authenticationCodeInfo *authCodeInfo);
    void       registerUser();
    static void requestCodeEntered(PurpleTdClient *self, const gchar *code);
    static void requestCodeCancelled(PurpleTdClient *self);
    void       authResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    void       notifyAuthError(const td::td_api::object_ptr<td::td_api::Object> &response);
    void       connectionReady();
    void       setPurpleConnectionInProgress();
    void       setPurpleConnectionUpdating();
    void       onLoggedIn();
    void       getContactsResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    void       getChatsResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    void       requestMissingPrivateChats();
    void       loginCreatePrivateChatResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    // List of chats is requested after connection is ready, and when response is received,
    // then we report to libpurple that we are connected
    void       updatePurpleChatListAndReportConnected();
    // Login sequence end

    void       showMessage(const td::td_api::chat &chat, td::td_api::message &message);
    void       showTextMessage(const td::td_api::chat &chat, const TgMessageInfo &message,const td::td_api::messageText &text);
    void       showPhotoMessage(const td::td_api::chat &chat, const TgMessageInfo &message, const td::td_api::messagePhoto &photo);
    void       showDocument(const td::td_api::chat &chat, const TgMessageInfo &message, const td::td_api::messageDocument &document);
    void       showVideo(const td::td_api::chat &chat, const TgMessageInfo &message, const td::td_api::messageVideo &video);
    void       showSticker(const td::td_api::chat &chat, const TgMessageInfo &message, td::td_api::messageSticker &sticker);
    void       onIncomingMessage(td::td_api::object_ptr<td::td_api::message> message);

    void       updateUserStatus(uint32_t userId, td::td_api::object_ptr<td::td_api::UserStatus> status);
    void       updateUser(td::td_api::object_ptr<td::td_api::user> user);
    void       updateGroup(td::td_api::object_ptr<td::td_api::basicGroup> group);
    void       updateSupergroup(td::td_api::object_ptr<td::td_api::supergroup> group);
    void       updatePrivateChat(const td::td_api::chat &chat, const td::td_api::user &user);
    void       updateBasicGroupChat(int32_t groupId);
    void       updateSupergroupChat(int32_t groupId);
    void       requestBasicGroupMembers(int32_t groupId);
    void       groupInfoResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);

    void       addChat(td::td_api::object_ptr<td::td_api::chat> chat);
    void       handleUserChatAction(const td::td_api::updateUserChatAction &updateChatAction);
    void       showUserChatAction(int32_t userId, bool isTyping);
    void       importContactResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    void       addContactResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    void       addContactCreatePrivateChatResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    void       notifyFailedContact(const std::string &phoneNumber, const std::string &errorMessage);
    void       joinChatByLinkResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);

    void       requestDownload(int32_t fileId, int64_t chatId, const TgMessageInfo &message,
                               td::td_api::object_ptr<td::td_api::file> thumbnail,
                               TdTransceiver::ResponseCb responseCb);
    void       showImage(const td::td_api::chat &chat, const TgMessageInfo &message,
                         const td::td_api::file &file, const char *caption);
    void       imageDownloadResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    void       showDownloadedImage(int64_t chatId, const TgMessageInfo &message,
                                   const std::string &filePath, const char *caption);
    void       showInlineFile(const td::td_api::chat &chat, const TgMessageInfo &message,
                              const td::td_api::file &file);
    void       fileDownloadResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    void       showDownloadedInlineFile(int64_t chatId, const TgMessageInfo &message,
                                        const std::string &filePath, const char *label);
    void       stickerDownloadResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    void       showDownloadedSticker(int64_t chatId, const TgMessageInfo &message,
                                     const std::string &filePath,
                                     td::td_api::object_ptr<td::td_api::file> thumbnail);

    bool       sendMessage(int64_t chatId, const char *message);

    PurpleAccount        *m_account;
    TdTransceiver         m_transceiver;
    TdAccountData         m_data;
    int32_t               m_lastAuthState = 0;
    bool                  m_connectionReady = false;
    std::vector<int32_t>  m_usersForNewPrivateChats;
};

#endif
