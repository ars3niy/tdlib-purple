#ifndef _TD_CLIENT_H
#define _TD_CLIENT_H

#include "account-data.h"
#include "transceiver.h"
#include "client-utils.h"
#include <purple.h>

enum class BasicGroupMembership: uint8_t {
    Invalid,
    Creator,
    NonCreator,
};

class PurpleTdClient {
public:
    PurpleTdClient(PurpleAccount *acct, ITransceiverBackend *testBackend);
    ~PurpleTdClient();

    static void setLogLevel(int level);
    int  sendMessage(const char *buddyName, const char *message);
    void sendTyping(const char *buddyName, bool isTyping);

    void addContact(const std::string &purpleName, const std::string &alias, const std::string &groupName);
    void renameContact(const char *buddyName, const char *newAlias);
    void removeContactAndPrivateChat(const std::string &buddyName);
    void getUsers(const char *username, std::vector<const td::td_api::user *> &users);

    bool joinChat(const char *chatName);
    void joinChatByLink(const char *inviteLink);
    void createGroup(const char *name, int type, const std::vector<std::string> &basicGroupMembers);
    BasicGroupMembership getBasicGroupMembership(const char *purpleChatName);
    void leaveGroup(const std::string &purpleChatName, bool deleteSupergroup);
    int  sendGroupMessage(int purpleChatId, const char *message);
private:
    using TdObjectPtr   = td::td_api::object_ptr<td::td_api::Object>;
    using ResponseCb    = void (PurpleTdClient::*)(uint64_t requestId, TdObjectPtr object);

    void       processUpdate(td::td_api::Object &object);
    void       processAuthorizationState(td::td_api::AuthorizationState &authState);

    // Login sequence start
    bool       addProxy();
    void       addProxyResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    void       getProxiesResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    void       removeOldProxies();
    void       sendTdlibParameters();
    void       sendPhoneNumber();
    void       requestAuthCode(const td::td_api::authenticationCodeInfo *authCodeInfo);
    void       registerUser();
    static void requestCodeEntered(PurpleTdClient *self, const gchar *code);
    static void requestCodeCancelled(PurpleTdClient *self);
    static void displayNameEntered(PurpleTdClient *self, const gchar *name);
    static void displayNameCancelled(PurpleTdClient *self);
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

    void       showMessage(const td::td_api::chat &chat, int64_t messageId);
    void       showTextMessage(const td::td_api::chat &chat, const TgMessageInfo &message,const td::td_api::messageText &text);
    void       showPhotoMessage(const td::td_api::chat &chat, const TgMessageInfo &message, const td::td_api::messagePhoto &photo);
    void       showDocument(const td::td_api::chat &chat, const TgMessageInfo &message, const td::td_api::messageDocument &document);
    void       showVideo(const td::td_api::chat &chat, const TgMessageInfo &message, const td::td_api::messageVideo &video);
    void       showSticker(const td::td_api::chat &chat, const TgMessageInfo &message, td::td_api::messageSticker &sticker);
    void       onIncomingMessage(td::td_api::object_ptr<td::td_api::message> message);
    void       findMessageResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);

    void       updateUserStatus(uint32_t userId, td::td_api::object_ptr<td::td_api::UserStatus> status);
    void       updateUser(td::td_api::object_ptr<td::td_api::user> user);
    void       updateGroup(td::td_api::object_ptr<td::td_api::basicGroup> group);
    void       updateSupergroup(td::td_api::object_ptr<td::td_api::supergroup> group);
    void       updateChat(const td::td_api::chat *chat);
    void       requestBasicGroupMembers(int32_t groupId);
    void       groupInfoResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);

    void       addContactById(int32_t userId, const std::string &phoneNumber, const std::string &alias,
                              const std::string &groupName);
    void       addChat(td::td_api::object_ptr<td::td_api::chat> chat);
    void       handleUserChatAction(const td::td_api::updateUserChatAction &updateChatAction);
    void       showUserChatAction(int32_t userId, bool isTyping);
    void       importContactResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    void       addContactResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    void       addContactCreatePrivateChatResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    void       notifyFailedContact(const std::string &errorMessage);
    void       joinChatByLinkResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    void       deleteSupergroupResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);

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
    void       sendMessageResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    void       removeTempFile(int64_t messageId);

    PurpleAccount        *m_account;
    TdTransceiver         m_transceiver;
    TdAccountData         m_data;
    int32_t               m_lastAuthState = 0;
    bool                  m_connectionReady = false;
    std::vector<int32_t>  m_usersForNewPrivateChats;
    bool                  m_isProxyAdded = false;
    td::td_api::object_ptr<td::td_api::proxy>   m_addedProxy;
    td::td_api::object_ptr<td::td_api::proxies> m_proxies;
};

#endif
