#include "td-client.h"
#include "config.h"
#include <purple.h>

class UpdateHandler {
public:
    UpdateHandler(PurpleTdClient *owner) : m_owner(owner) {}

    void operator() (td::td_api::updateAuthorizationState &update_authorization_state) const {
        purple_debug_misc(config::pluginId, "Incoming update: authorization state\n");
        td::td_api::downcast_call(*update_authorization_state.authorization_state_, *m_owner->m_authUpdateHandler);
    }

    void operator() (auto &update) const {
        purple_debug_misc(config::pluginId, "Incoming update: ignorig ID=%d\n", update.get_id());
    }
private:
    PurpleTdClient *m_owner;
};

class AuthUpdateHandler {
public:
    AuthUpdateHandler(PurpleTdClient *owner) : m_owner(owner) {}

    void operator()(td::td_api::authorizationStateWaitEncryptionKey &) const {
        purple_debug_misc(config::pluginId, "Authorization state update: encriytion key requested\n");
    }

    void operator() (td::td_api::authorizationStateWaitTdlibParameters &) const {
        purple_debug_misc(config::pluginId, "Authorization state update: TDLib parameters requested\n");
        m_owner->sendTdlibParameters();
    }

    void operator()(auto &update) const {
        purple_debug_misc(config::pluginId, "Authorization state update: ignorig ID=%d\n", update.get_id());
    }
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
    m_stopThread = true;
    m_client->send({UINT64_MAX, td::td_api::make_object<td::td_api::close>()});
    m_pollThread.join();
}

void PurpleTdClient::setLogLevel(int level)
{
    // Why not just call setLogVerbosityLevel? No idea!
    td::Client::execute({0, td::td_api::make_object<td::td_api::setLogVerbosityLevel>(level)});
}

void PurpleTdClient::startLogin()
{
#if !GLIB_CHECK_VERSION(2, 32, 0)
    // GLib threading system is automaticaly initialized since 2.32.
    // For earlier versions, it have to be initialized before calling any
    // Glib or GTK+ functions.
    if (!g_thread_supported())
        g_thread_init(NULL);
#endif

    m_client = std::make_unique<td::Client>();
    if (!m_pollThread.joinable()) {
        m_stopThread = false;
        m_pollThread = std::thread([this]() { pollThreadLoop(); });
    }
}

void PurpleTdClient::pollThreadLoop()
{
    while (!m_stopThread)
        processResponse(m_client->receive(1));
}

void PurpleTdClient::processResponse(td::Client::Response response)
{
    if (response.object) {
        if (response.id == 0) {
            purple_debug_misc(config::pluginId, "Incoming update\n");
            td::td_api::downcast_call(*response.object, *m_updateHandler);
        } else {
            // Response to a request
        }
    } else
        purple_debug_misc(config::pluginId, "Response id %lu timed out or something\n", response.id);
}

void PurpleTdClient::sendTdlibParameters()
{
}
