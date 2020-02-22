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
    using TdObjectPtr = td::td_api::object_ptr<td::td_api::Object>;
    using ResponseCb  = std::function<void(uint64_t requestId, TdObjectPtr object)>;

    void pollThreadLoop();
    void processResponse(td::Client::Response response);
    void sendTdlibParameters();
    void sendPhoneNumber();
    void sendQuery(td::td_api::object_ptr<td::td_api::Function> f, ResponseCb handler);

    PurpleAccount                      *m_account;
    std::unique_ptr<UpdateHandler>      m_updateHandler;
    std::unique_ptr<AuthUpdateHandler>  m_authUpdateHandler;
    std::unique_ptr<td::Client>         m_client;
    std::thread                         m_pollThread;
    std::atomic_bool                    m_stopThread;
    uint64_t                            m_lastQueryId;
    std::map<std::uint64_t, ResponseCb> m_responseHandlers;
};

#endif
