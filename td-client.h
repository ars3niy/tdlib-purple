#ifndef _TD_CLIENT_H
#define _TD_CLIENT_H

#include "account-data.h"
#include <purple.h>
#include <td/telegram/Client.h>
#include <td/telegram/td_api.hpp>

#include <memory>
#include <thread>
#include <atomic>
#include <map>
#include <mutex>
#include <list>

class UpdateHandler;
class AuthUpdateHandler;

class PurpleTdClient {
public:
    PurpleTdClient(PurpleAccount *acct);
    ~PurpleTdClient();

    static void setLogLevel(int level);
    void startLogin();
    int sendMessage(const char *buddyName, const char *message);
    void addContact(const char *phoneNumber, const char *alias);
private:
    friend class UpdateHandler;
    friend class AuthUpdateHandler;
    using TdObjectPtr   = td::td_api::object_ptr<td::td_api::Object>;
    using ResponseCb    = void (PurpleTdClient::*)(uint64_t requestId, TdObjectPtr object);
    using TdErrorPtr    = td::td_api::object_ptr<td::td_api::error>;
    using TdAuthCodePtr = td::td_api::object_ptr<td::td_api::authenticationCodeInfo>;

    // All non-static response handling functions are called from the poll thread
    // Static response handling functions are called from main thread via g_idle_add
    // Functions sending requests can be called from either thread
    void pollThreadLoop();
    void processResponse(td::Client::Response response);
    void sendTdlibParameters();
    void sendPhoneNumber();
    static int  requestAuthCode(gpointer user_data);
    static void requestCodeEntered(PurpleTdClient *self, const gchar *code);
    static void requestCodeCancelled(PurpleTdClient *self);
    uint64_t sendQuery(td::td_api::object_ptr<td::td_api::Function> f, ResponseCb handler);

    void       authResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    static int notifyAuthError(gpointer user_data);
    void       connectionReady();
    static int setPurpleConnectionInProgress(gpointer user_data);
    static int setPurpleConnectionUpdating(gpointer user_data);
    void       getContactsResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    void       getChatsResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    // List of chats is requested after connection is ready, and when response is received,
    // then we report to libpurple that we are connected
    static int updatePurpleChatListAndReportConnected(gpointer user_data);
    static int showUnreadMessages(gpointer user_data);
    void       showMessage(const td::td_api::message &message);
    void       onIncomingMessage(td::td_api::object_ptr<td::td_api::message> message);
    void       updateUserStatus(uint32_t userId, td::td_api::object_ptr<td::td_api::UserStatus> status);
    void       updateUser(td::td_api::object_ptr<td::td_api::user> user);
    static int showUserUpdates(gpointer user_data);
    void       handleUserChatAction(const td::td_api::updateUserChatAction &updateChatAction);
    static int showUserChatActions(gpointer user_data);
    void       importContactResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    void       addContactResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    void       createPrivateChatResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    static int notifyFailedContacts(gpointer user_data);

    PurpleAccount                      *m_account;
    std::unique_ptr<UpdateHandler>      m_updateHandler;
    std::unique_ptr<AuthUpdateHandler>  m_authUpdateHandler;
    std::unique_ptr<td::Client>         m_client;
    std::thread                         m_pollThread;
    std::atomic_bool                    m_stopThread;
    std::atomic<uint64_t>               m_lastQueryId;
    std::map<std::uint64_t, ResponseCb> m_responseHandlers;
    std::mutex                          m_queryMutex;

    TdAccountData                       m_data;

    int32_t                             m_lastAuthState = 0;
    TdErrorPtr                          m_authError;
    TdAuthCodePtr                       m_authCodeInfo;
};

#endif
