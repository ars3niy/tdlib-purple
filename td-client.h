#ifndef _TD_CLIENT_H
#define _TD_CLIENT_H

#include "account-data.h"
#include "transceiver.h"
#include "client-utils.h"
#include <td/telegram/Log.h>
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
    static void setTdlibFatalErrorCallback(td::Log::FatalErrorCallbackPtr callback);
    static std::string getBaseDatabasePath();
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
    void setGroupDescription(int purpleChatId, const char *description);
    void kickUserFromChat(PurpleConversation *conv, const char *name);
    void addUserToChat(int purpleChatId, const char *name);
    void showInviteLink(const std::string &purpleChatName);

    void setTwoFactorAuth(const char *oldPassword, const char *newPassword, const char *hint,
                        const char *email);

    void sendFileToChat(PurpleXfer *xfer, const char *purpleName, PurpleConversationType type);
    void cancelUpload(PurpleXfer *xfer);
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
    void       requestPassword(const td::td_api::authorizationStateWaitPassword &pwInfo);
    void       registerUser();
    static void requestCodeEntered(PurpleTdClient *self, const gchar *code);
    static void requestCodeCancelled(PurpleTdClient *self);
    static void passwordEntered(PurpleTdClient *self, const gchar *code);
    static void passwordCancelled(PurpleTdClient *self);
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

    void       showMessage(const td::td_api::chat &chat, td::td_api::message &message,
                           td::td_api::object_ptr<td::td_api::message> repliedMessage);
    void       showTextMessage(const td::td_api::chat &chat, const TgMessageInfo &message,const td::td_api::messageText &text);
    void       showPhotoMessage(const td::td_api::chat &chat, TgMessageInfo &message, const td::td_api::messagePhoto &photo);
    void       showFileMessage(const td::td_api::chat &chat, TgMessageInfo &message,
                               td::td_api::object_ptr<td::td_api::file> file,
                               td::td_api::object_ptr<td::td_api::formattedText> caption,
                               const std::string &fileDescription);
    void       showStickerMessage(const td::td_api::chat &chat, TgMessageInfo &message, td::td_api::messageSticker &sticker);
    void       onIncomingMessage(td::td_api::object_ptr<td::td_api::message> message);
    void       findMessageResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);

    void       updateUserStatus(uint32_t userId, td::td_api::object_ptr<td::td_api::UserStatus> status);
    void       updateUser(td::td_api::object_ptr<td::td_api::user> user);
    void       downloadProfilePhoto(const td::td_api::user &user);
    void       avatarDownloadResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    void       updateGroup(td::td_api::object_ptr<td::td_api::basicGroup> group);
    void       updateSupergroup(td::td_api::object_ptr<td::td_api::supergroup> group);
    void       updateChat(const td::td_api::chat *chat);
    void       updateUserInfo(const td::td_api::user &user, const td::td_api::chat *privateChat);
    void       downloadChatPhoto(const td::td_api::chat &chat);
    void       requestBasicGroupFullInfo(int32_t groupId);
    void       requestSupergroupFullInfo(int32_t groupId);
    void       groupInfoResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    void       supergroupInfoResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    void       updateGroupFull(int32_t groupId, td::td_api::object_ptr<td::td_api::basicGroupFullInfo> groupInfo);
    void       updateSupergroupFull(int32_t groupId, td::td_api::object_ptr<td::td_api::supergroupFullInfo> groupInfo);

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
    void       setGroupDescriptionResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    void       chatActionResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);

    void       showFile(const td::td_api::chat &chat, TgMessageInfo &message,
                        const td::td_api::file &file, const char *caption, const std::string &fileDesc,
                        td::td_api::object_ptr<td::td_api::file> thumbnail,
                        FileDownloadCb downloadCallback);
    void       downloadFile(int32_t fileId, int64_t chatId, TgMessageInfo &message,
                            const std::string &fileDescription,
                            td::td_api::object_ptr<td::td_api::file> thumbnail,
                            FileDownloadCb callback);
    void       requestDownload(const char *sender, const td::td_api::file &file,
                               const std::string &fileDesc, const td::td_api::chat &chat,
                               TgMessageInfo &message, FileDownloadCb callback);
    static void startDownload(void *user_data);
    void       startDownloadProgress(uint64_t requestId);
    void       downloadResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    void       showDownloadedImage(int64_t chatId, TgMessageInfo &message,
                                   const std::string &filePath, const char *caption,
                                   const std::string &fileDesc,
                                   td::td_api::object_ptr<td::td_api::file> thumbnail);
    void       showDownloadedFile(int64_t chatId, TgMessageInfo &message,
                                        const std::string &filePath, const char *caption,
                                        const std::string &fileDescription,
                                        td::td_api::object_ptr<td::td_api::file> thumbnail);
    void       showDownloadedSticker(int64_t chatId, TgMessageInfo &message,
                                     const std::string &filePath, const char *caption,
                                     const std::string &fileDescription,
                                     td::td_api::object_ptr<td::td_api::file> thumbnail);
    void       sendMessageCreatePrivateChatResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    void       uploadResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);

    void       sendMessageResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    void       removeTempFile(int64_t messageId);

    void        setTwoFactorAuthResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    void        requestRecoveryEmailConfirmation(const std::string &emailInfo);
    static void verifyRecoveryEmail(PurpleTdClient *self, const char *code);
    void        verifyRecoveryEmailResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);

    PurpleAccount        *m_account;
    TdTransceiver         m_transceiver;
    TdAccountData         m_data;
    int32_t               m_lastAuthState = 0;
    bool                  m_connectionReady = false;
    std::vector<int32_t>  m_usersForNewPrivateChats;
    bool                  m_isProxyAdded = false;
    int64_t               m_lastChatOrderOffset = 0;
    td::td_api::object_ptr<td::td_api::proxy>   m_addedProxy;
    td::td_api::object_ptr<td::td_api::proxies> m_proxies;
};

#endif
