#include "td-client.h"
#include "config.h"

class UpdateHandler {
public:
    UpdateHandler(PurpleTdClient *owner) : m_owner(owner) {}

    void operator()(td::td_api::updateAuthorizationState &update_authorization_state) const {
        purple_debug_misc(config::pluginId, "Incoming update: authorization state\n");
        m_owner->m_lastAuthState = update_authorization_state.authorization_state_->get_id();
        td::td_api::downcast_call(*update_authorization_state.authorization_state_, *m_owner->m_authUpdateHandler);
    }

    void operator()(auto &update) const {
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
        m_owner->sendQuery(td::td_api::make_object<td::td_api::checkDatabaseEncryptionKey>(""), &PurpleTdClient::authResponse);
    }

    void operator()(td::td_api::authorizationStateWaitTdlibParameters &) const {
        purple_debug_misc(config::pluginId, "Authorization state update: TDLib parameters requested\n");
        m_owner->sendTdlibParameters();
    }

    void operator()(td::td_api::authorizationStateWaitPhoneNumber &) const {
        purple_debug_misc(config::pluginId, "Authorization state update: phone number requested\n");
        m_owner->sendPhoneNumber();
    }

    void operator()(auto &update) const {
        purple_debug_misc(config::pluginId, "Authorization state update: ignorig ID=%d\n", update.get_id());
    }
private:
    PurpleTdClient *m_owner;
};

PurpleTdClient::PurpleTdClient(PurpleAccount *acct)
{
    m_account           = acct;
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
        m_lastQueryId = 0;
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
            auto it = m_responseHandlers.find(response.id);
            if (it != m_responseHandlers.end()) {
                (this->*(it->second))(response.id, std::move(response.object));
                m_responseHandlers.erase(it);
            }
        }
    } else
        purple_debug_misc(config::pluginId, "Response id %lu timed out or something\n", response.id);
}

void PurpleTdClient::sendTdlibParameters()
{
    auto parameters = td::td_api::make_object<td::td_api::tdlibParameters>();
    const char *username = purple_account_get_username(m_account);
    parameters->database_directory_ = std::string(purple_user_dir()) + G_DIR_SEPARATOR_S +
                                      config::configSubdir + G_DIR_SEPARATOR_S + username;
    purple_debug_misc(config::pluginId, "Account %s using database directory %s\n",
                      username, parameters->database_directory_.c_str());
    parameters->use_message_database_ = true;
    parameters->use_secret_chats_ = true;
    parameters->api_id_ = 94575;
    parameters->api_hash_ = "a3406de8d171bb422bb6ddf3bbd800e2";
    parameters->system_language_code_ = "en";
    parameters->device_model_ = "Desktop";
    parameters->system_version_ = "Unknown";
    parameters->application_version_ = "1.0";
    parameters->enable_storage_optimizer_ = true;
    sendQuery(td::td_api::make_object<td::td_api::setTdlibParameters>(std::move(parameters)),
              &PurpleTdClient::authResponse);
}

void PurpleTdClient::sendPhoneNumber()
{
    const char *number = purple_account_get_username(m_account);
    sendQuery(td::td_api::make_object<td::td_api::setAuthenticationPhoneNumber>(number, nullptr),
              &PurpleTdClient::authResponse);
}

void PurpleTdClient::sendQuery(td::td_api::object_ptr<td::td_api::Function> f, ResponseCb handler)
{
    uint64_t queryId = ++m_lastQueryId;
    purple_debug_misc(config::pluginId, "Sending query id %lu\n", (unsigned long)queryId);
    if (handler)
        m_responseHandlers.emplace(queryId, std::move(handler));
    m_client->send({queryId, std::move(f)});
}

void PurpleTdClient::authResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    if (object->get_id() == td::td_api::error::ID) {
        td::tl::unique_ptr<td::td_api::error> error = td::move_tl_object_as<td::td_api::error>(object);
        purple_debug_misc(config::pluginId, "Authentication error on query %lu (auth step %d): code %d (%s)\n",
                          (unsigned long)requestId, (int)m_lastAuthState, (int)error->code_,
                          error->message_.c_str());
        m_authErrorCode = error->code_;
        m_authError     = std::move(error->message_);
        g_idle_add(notifyAuthError, this);
    } else
        purple_debug_misc(config::pluginId, "Authentication success on query %lu\n", (unsigned long)requestId);
}

int PurpleTdClient::notifyAuthError(gpointer user_data)
{
    PurpleTdClient *self = static_cast<PurpleTdClient *>(user_data);

    std::string message;
    switch (self->m_lastAuthState) {
    case td::td_api::authorizationStateWaitEncryptionKey::ID:
        message = "Error applying database encryption key";
        break;
    case td::td_api::authorizationStateWaitPhoneNumber::ID:
        message = "Authentication error after sending phone number";
        break;
    default:
        message = "Authentication error";
    }

    message += ": code " + std::to_string(self->m_authErrorCode) + " (" + self->m_authError + ")";
    purple_connection_error(purple_account_get_connection(self->m_account), message.c_str());
    return FALSE; // This idle handler will not be called again
}
