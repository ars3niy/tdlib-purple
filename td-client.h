#ifndef _TD_CLIENT_H
#define _TD_CLIENT_H

#include <td/telegram/Client.h>
#include <td/telegram/td_api.h>
#include <td/telegram/td_api.hpp>

#include <memory>

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

    void processResponse(td::Client::Response response);

    std::unique_ptr<UpdateHandler>     m_updateHandler;
    std::unique_ptr<AuthUpdateHandler> m_authUpdateHandler;
    std::unique_ptr<td::Client>        m_client;
};

#endif
