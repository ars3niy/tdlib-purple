#ifndef _TD_CLIENT_H
#define _TD_CLIENT_H

#include <purple.h>
#include <td/telegram/Client.h>
#include <td/telegram/td_api.h>
#include <td/telegram/td_api.hpp>

#include <memory>
#include <thread>
#include <atomic>
#include <map>
#include <mutex>

class UpdateHandler;
class AuthUpdateHandler;

class PurpleTdClient {
public:
    PurpleTdClient(PurpleAccount *acct);
    ~PurpleTdClient();

    static void setLogLevel(int level);
    void startLogin();
private:
    friend class UpdateHandler;
    friend class AuthUpdateHandler;
    using TdObjectPtr   = td::td_api::object_ptr<td::td_api::Object>;
    using ResponseCb    = void (PurpleTdClient::*)(uint64_t requestId, TdObjectPtr object);
    using TdErrorPtr    = td::td_api::object_ptr<td::td_api::error>;
    using TdAuthCodePtr = td::td_api::object_ptr<td::td_api::authenticationCodeInfo>;

    void pollThreadLoop();
    void processResponse(td::Client::Response response);
    void sendTdlibParameters();
    void sendPhoneNumber();
    static int  requestAuthCode(gpointer user_data);
    static void requestCodeEntered(PurpleTdClient *self, const gchar *code);
    static void requestCodeCancelled(PurpleTdClient *self);
    void sendQuery(td::td_api::object_ptr<td::td_api::Function> f, ResponseCb handler);

    void authResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);
    static int notifyAuthError(gpointer user_data);
    static int connectionReady(gpointer user_data);

    PurpleAccount                      *m_account;
    std::unique_ptr<UpdateHandler>      m_updateHandler;
    std::unique_ptr<AuthUpdateHandler>  m_authUpdateHandler;
    std::unique_ptr<td::Client>         m_client;
    std::thread                         m_pollThread;
    std::atomic_bool                    m_stopThread;
    std::atomic<uint64_t>               m_lastQueryId;
    std::map<std::uint64_t, ResponseCb> m_responseHandlers;
    std::mutex                          m_dataMutex;

    int32_t                             m_lastAuthState = 0;
    TdErrorPtr                          m_authError;
    TdAuthCodePtr                       m_authCodeInfo;
};

#endif
