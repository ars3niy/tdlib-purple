#ifndef _TD_CLIENT_H
#define _TD_CLIENT_H

#include <td/telegram/Client.h>
#include <td/telegram/td_api.h>
#include <td/telegram/td_api.hpp>

#include <memory>
#include <thread>
#include <atomic>

class UpdateHandler;
class AuthUpdateHandler;

class PurpleTdClient {
public:
    PurpleTdClient();
    ~PurpleTdClient();

    static void setLogLevel(int level);
    void startLogin();
private:
    friend class UpdateHandler;
    friend class AuthUpdateHandler;
    using TdObjectPtr = td::td_api::object_ptr<td::td_api::Object>;

    void pollThreadLoop();
    void processResponse(td::Client::Response response);
    void sendTdlibParameters();

    std::unique_ptr<UpdateHandler>     m_updateHandler;
    std::unique_ptr<AuthUpdateHandler> m_authUpdateHandler;
    std::unique_ptr<td::Client>        m_client;
    std::thread                        m_pollThread;
    std::atomic_bool                   m_stopThread;
};

#endif
