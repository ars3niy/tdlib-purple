#include "td-client.h"
#include <purple.h>

class UpdateHandler {
public:
    UpdateHandler(PurpleTdClient *owner) : m_owner(owner) {}

    void operator()(td::td_api::updateAuthorizationState &update_authorization_state) const {
        td::td_api::downcast_call(*update_authorization_state.authorization_state_, *m_owner->m_authUpdateHandler);
    }

    void operator()(auto &update) const {}
private:
    PurpleTdClient *m_owner;
};

class AuthUpdateHandler {
public:
    AuthUpdateHandler(PurpleTdClient *owner) : m_owner(owner) {}

    void operator()(td::td_api::authorizationStateWaitEncryptionKey &) const {
    }

    void operator()(auto &update) const {}
private:
    PurpleTdClient *m_owner;
};

PurpleTdClient::PurpleTdClient()
{
    m_updateHandler     = std::make_unique<UpdateHandler>(this);
    m_authUpdateHandler = std::make_unique<AuthUpdateHandler>(this);
}

PurpleTdClient::~PurpleTdClient()
{
}

void PurpleTdClient::startLogin()
{
    m_client = std::make_unique<td::Client>();
    processResponse(m_client->receive(1));
}

void PurpleTdClient::processResponse(td::Client::Response response)
{
    if (response.object) {
        if (response.id == 0)
            td::td_api::downcast_call(*response.object, *m_updateHandler);
        else {
            // Response to a request
        }
    }
}
