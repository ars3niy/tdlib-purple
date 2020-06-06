#include "td-client.h"
#include "purple-info.h"
#include "config.h"
#include "format.h"
#include <unistd.h>
#include <stdlib.h>

static char *_(const char *s) { return const_cast<char *>(s); }

enum {
    // Typing notifications seems to be resent every 5-6 seconds, so 10s timeout hould be appropriate
    REMOTE_TYPING_NOTICE_TIMEOUT = 10,
    FILE_DOWNLOAD_PRIORITY       = 1,
};

PurpleTdClient::PurpleTdClient(PurpleAccount *acct, ITransceiverBackend *testBackend)
:   m_transceiver(this, acct, &PurpleTdClient::processUpdate, testBackend),
    m_data(acct)
{
    m_account = acct;
}

PurpleTdClient::~PurpleTdClient()
{
}

void PurpleTdClient::setLogLevel(int level)
{
    // Why not just call setLogVerbosityLevel? No idea!
    td::Client::execute({0, td::td_api::make_object<td::td_api::setLogVerbosityLevel>(level)});
}

void PurpleTdClient::setTdlibFatalErrorCallback(td::Log::FatalErrorCallbackPtr callback)
{
    td::Log::set_fatal_error_callback(callback);
}

void PurpleTdClient::processUpdate(td::td_api::Object &update)
{
    purple_debug_misc(config::pluginId, "Incoming update\n");

    switch (update.get_id()) {
    case td::td_api::updateAuthorizationState::ID: {
        auto &update_authorization_state = static_cast<td::td_api::updateAuthorizationState &>(update);
        purple_debug_misc(config::pluginId, "Incoming update: authorization state\n");
        if (update_authorization_state.authorization_state_) {
            m_lastAuthState = update_authorization_state.authorization_state_->get_id();
            processAuthorizationState(*update_authorization_state.authorization_state_);
        }
        break;
    }

    case td::td_api::updateConnectionState::ID: {
        auto &connectionUpdate = static_cast<td::td_api::updateConnectionState &>(update);
        purple_debug_misc(config::pluginId, "Incoming update: connection state\n");
        if (connectionUpdate.state_) {
            if (connectionUpdate.state_->get_id() == td::td_api::connectionStateReady::ID)
                connectionReady();
            else if (connectionUpdate.state_->get_id() == td::td_api::connectionStateConnecting::ID)
                setPurpleConnectionInProgress();
            else if (connectionUpdate.state_->get_id() == td::td_api::connectionStateUpdating::ID)
                setPurpleConnectionUpdating();
        }
        break;
    }

    case td::td_api::updateUser::ID: {
        auto &userUpdate = static_cast<td::td_api::updateUser &>(update);
        updateUser(std::move(userUpdate.user_));
        break;
    }

    case td::td_api::updateNewChat::ID: {
        auto &newChat = static_cast<td::td_api::updateNewChat &>(update);
        purple_debug_misc(config::pluginId, "Incoming update: new chat\n");
        addChat(std::move(newChat.chat_));
        break;
    }

    case td::td_api::updateNewMessage::ID: {
        auto &newMessageUpdate = static_cast<td::td_api::updateNewMessage &>(update);
        purple_debug_misc(config::pluginId, "Incoming update: new message\n");
        if (newMessageUpdate.message_)
            onIncomingMessage(std::move(newMessageUpdate.message_));
        else
            purple_debug_warning(config::pluginId, "Received null new message\n");
        break;
    }

    case td::td_api::updateUserStatus::ID: {
        auto &updateStatus = static_cast<td::td_api::updateUserStatus &>(update);
        purple_debug_misc(config::pluginId, "Incoming update: user status\n");
        if (updateStatus.status_)
            updateUserStatus(updateStatus.user_id_, std::move(updateStatus.status_));
        break;
    }

    case td::td_api::updateUserChatAction::ID: {
        auto &updateChatAction = static_cast<td::td_api::updateUserChatAction &>(update);
        purple_debug_misc(config::pluginId, "Incoming update: chat action %d\n",
            updateChatAction.action_ ? updateChatAction.action_->get_id() : 0);
        handleUserChatAction(updateChatAction);
        break;
    }

    case td::td_api::updateBasicGroup::ID: {
        auto &groupUpdate = static_cast<td::td_api::updateBasicGroup &>(update);
        updateGroup(std::move(groupUpdate.basic_group_));
        break;
    }

    case td::td_api::updateSupergroup::ID: {
        auto &groupUpdate = static_cast<td::td_api::updateSupergroup &>(update);
        updateSupergroup(std::move(groupUpdate.supergroup_));
        break;
    }

    case td::td_api::updateBasicGroupFullInfo::ID: {
        auto &groupUpdate = static_cast<td::td_api::updateBasicGroupFullInfo &>(update);
        updateGroupFull(groupUpdate.basic_group_id_, std::move(groupUpdate.basic_group_full_info_));
        break;
    };

    case td::td_api::updateSupergroupFullInfo::ID: {
        auto &groupUpdate = static_cast<td::td_api::updateSupergroupFullInfo &>(update);
        updateSupergroupFull(groupUpdate.supergroup_id_, std::move(groupUpdate.supergroup_full_info_));
        break;
    };

    case td::td_api::updateMessageSendSucceeded::ID: {
        auto &sendSucceeded = static_cast<const td::td_api::updateMessageSendSucceeded &>(update);
        purple_debug_misc(config::pluginId, "Incoming update: message %" G_GINT64_FORMAT " send succeeded\n",
                          sendSucceeded.old_message_id_);
        removeTempFile(sendSucceeded.old_message_id_);
        break;
    }

    case td::td_api::updateMessageSendFailed::ID: {
        auto &sendFailed = static_cast<const td::td_api::updateMessageSendFailed &>(update);
        purple_debug_misc(config::pluginId, "Incoming update: message %" G_GINT64_FORMAT " send failed\n",
                          sendFailed.old_message_id_);
        removeTempFile(sendFailed.old_message_id_);
        notifySendFailed(sendFailed, m_data);
        // TODO notify in chat
        break;
    }

    case td::td_api::updateChatChatList::ID: {
        auto &chatListUpdate = static_cast<td::td_api::updateChatChatList &>(update);
        purple_debug_misc(config::pluginId, "Incoming update: update chat list for chat %" G_GINT64_FORMAT "\n",
                          chatListUpdate.chat_id_);
        m_data.updateChatChatList(chatListUpdate.chat_id_, std::move(chatListUpdate.chat_list_));
        updateChat(m_data.getChat(chatListUpdate.chat_id_));
        break;
    }

    case td::td_api::updateChatOrder::ID: {
        auto &chatOrderUpdate = static_cast<td::td_api::updateChatOrder&>(update);
        m_data.updateChatOrder(chatOrderUpdate.chat_id_, chatOrderUpdate.order_);
        break;
    };

    case td::td_api::updateChatTitle::ID: {
        auto &chatTitleUpdate = static_cast<td::td_api::updateChatTitle &>(update);
        purple_debug_misc(config::pluginId, "Incoming update: update chat title for chat %" G_GINT64_FORMAT "\n",
                          chatTitleUpdate.chat_id_);
        m_data.updateChatTitle(chatTitleUpdate.chat_id_, chatTitleUpdate.title_);
        updateChat(m_data.getChat(chatTitleUpdate.chat_id_));
        break;
    }

    case td::td_api::updateOption::ID: {
        const td::td_api::updateOption &option = static_cast<const td::td_api::updateOption &>(update);
        if ((option.name_ == "version") && option.value_ &&
            (option.value_->get_id() == td::td_api::optionValueString::ID))
        {
            purple_debug_misc(config::pluginId, "tdlib version: %s\n",
                              static_cast<const td::td_api::optionValueString &>(*option.value_).value_.c_str());
        } else
            purple_debug_misc(config::pluginId, "Option update %s\n", option.name_.c_str());
        break;
    }

    case td::td_api::updateFile::ID: {
        auto &fileUpdate = static_cast<const td::td_api::updateFile &>(update);
        purple_debug_misc(config::pluginId, "Incoming update: file update, id %d\n",
                          fileUpdate.file_ ? fileUpdate.file_->id_ : 0);
        if (fileUpdate.file_)
            updateFileTransferProgress(*fileUpdate.file_, m_transceiver, m_data,
                                       &PurpleTdClient::sendMessageResponse);
        break;
    };

    case td::td_api::updateSecretChat::ID: {
        auto &chatUpdate = static_cast<td::td_api::updateSecretChat &>(update);
        purple_debug_misc(config::pluginId, "Incoming update: secret chat, id %d\n",
                          chatUpdate.secret_chat_ ? chatUpdate.secret_chat_->id_ : 0);
        updateSecretChat(std::move(chatUpdate.secret_chat_), m_transceiver, m_data);
        break;
    };

    default:
        purple_debug_misc(config::pluginId, "Incoming update: ignorig ID=%d\n", update.get_id());
        break;
    }
}

void PurpleTdClient::processAuthorizationState(td::td_api::AuthorizationState &authState)
{
    switch (authState.get_id()) {
    case td::td_api::authorizationStateWaitEncryptionKey::ID:
        purple_debug_misc(config::pluginId, "Authorization state update: encriytion key requested\n");
        m_transceiver.sendQuery(td::td_api::make_object<td::td_api::checkDatabaseEncryptionKey>(""),
                                &PurpleTdClient::authResponse);
        break;

    case td::td_api::authorizationStateWaitTdlibParameters::ID: 
        purple_debug_misc(config::pluginId, "Authorization state update: TDLib parameters requested\n");
        m_transceiver.sendQuery(td::td_api::make_object<td::td_api::disableProxy>(), nullptr);
        if (addProxy()) {
            m_transceiver.sendQuery(td::td_api::make_object<td::td_api::getProxies>(),
                                    &PurpleTdClient::getProxiesResponse);
            sendTdlibParameters();
        }
        break;

    case td::td_api::authorizationStateWaitPhoneNumber::ID:
        purple_debug_misc(config::pluginId, "Authorization state update: phone number requested\n");
        sendPhoneNumber();
        break;

    case td::td_api::authorizationStateWaitCode::ID: {
        auto &codeState = static_cast<td::td_api::authorizationStateWaitCode &>(authState);
        purple_debug_misc(config::pluginId, "Authorization state update: authentication code requested\n");
        requestAuthCode(codeState.code_info_.get());
        break;
    }

    case td::td_api::authorizationStateWaitRegistration::ID: {
        purple_debug_misc(config::pluginId, "Authorization state update: new user registration\n");
        registerUser();
        break;
    }

    case td::td_api::authorizationStateWaitPassword::ID: {
        purple_debug_misc(config::pluginId, "Authorization state update: password requested\n");
        auto &pwInfo = static_cast<const td::td_api::authorizationStateWaitPassword &>(authState);
        requestPassword(pwInfo);
        break;
    }

    case td::td_api::authorizationStateReady::ID:
        purple_debug_misc(config::pluginId, "Authorization state update: ready\n");
        if (m_connectionReady)
            onLoggedIn();
        break;
    }
}

bool PurpleTdClient::addProxy()
{
    PurpleProxyInfo *purpleProxy = purple_proxy_get_setup(m_account);
    PurpleProxyType  proxyType   = purpleProxy ? purple_proxy_info_get_type(purpleProxy) : PURPLE_PROXY_NONE;
    const char *     username    = purpleProxy ? purple_proxy_info_get_username(purpleProxy) : "";
    const char *     password    = purpleProxy ? purple_proxy_info_get_password(purpleProxy) : "";
    const char *     host        = purpleProxy ? purple_proxy_info_get_host(purpleProxy) : "";
    int              port        = purpleProxy ? purple_proxy_info_get_port(purpleProxy) : 0;
    if (username == NULL) username = "";
    if (password == NULL) password = "";
    if (host == NULL) host = "";
    std::string errorMessage;

    td::td_api::object_ptr<td::td_api::ProxyType> tdProxyType;
    switch (proxyType) {
    case PURPLE_PROXY_NONE:
        tdProxyType = nullptr;
        break;
    case PURPLE_PROXY_SOCKS5:
        tdProxyType = td::td_api::make_object<td::td_api::proxyTypeSocks5>(username, password);
        break;
    case PURPLE_PROXY_HTTP:
        tdProxyType = td::td_api::make_object<td::td_api::proxyTypeHttp>(username, password, true);
        break;
    default:
        errorMessage = formatMessage(_("Proxy type {} is not supported"), proxyTypeToString(proxyType));
        break;
    }

    if (!errorMessage.empty()) {
        purple_connection_error(purple_account_get_connection(m_account), errorMessage.c_str());
        return false;
    } else if (tdProxyType) {
        auto addProxy = td::td_api::make_object<td::td_api::addProxy>();
        addProxy->server_ = host;
        addProxy->port_ = port;
        addProxy->enable_ = true;
        addProxy->type_ = std::move(tdProxyType);
        m_transceiver.sendQuery(std::move(addProxy), &PurpleTdClient::addProxyResponse);
        m_isProxyAdded = true;
    }

    return true;
}

static std::string getDisplayedError(const td::td_api::object_ptr<td::td_api::Object> &object)
{
    if (!object)
        return _("No response received");
    else if (object->get_id() == td::td_api::error::ID) {
        const td::td_api::error &error = static_cast<const td::td_api::error &>(*object);
        return formatMessage(errorCodeMessage(), {std::to_string(error.code_), error.message_});
    } else
        return _("Unexpected response");
}

void PurpleTdClient::addProxyResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    if (object && (object->get_id() == td::td_api::proxy::ID)) {
        m_addedProxy = td::move_tl_object_as<td::td_api::proxy>(object);
        if (m_proxies)
            removeOldProxies();
    } else {
        std::string message = formatMessage(_("Could not set proxy: {}"), getDisplayedError(object));
        purple_connection_error(purple_account_get_connection(m_account), message.c_str());
    }
}

void PurpleTdClient::getProxiesResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    if (object && (object->get_id() == td::td_api::proxies::ID)) {
        m_proxies = td::move_tl_object_as<td::td_api::proxies>(object);
        if (!m_isProxyAdded || m_addedProxy)
            removeOldProxies();
    } else {
        std::string message = formatMessage(_("Could not get proxies: {}"), getDisplayedError(object));
        purple_connection_error(purple_account_get_connection(m_account), message.c_str());
    }
}

void PurpleTdClient::removeOldProxies()
{
    for (const td::td_api::object_ptr<td::td_api::proxy> &proxy: m_proxies->proxies_)
        if (proxy && (!m_addedProxy || (proxy->id_ != m_addedProxy->id_)))
            m_transceiver.sendQuery(td::td_api::make_object<td::td_api::removeProxy>(proxy->id_), nullptr);
}

std::string PurpleTdClient::getBaseDatabasePath()
{
    return std::string(purple_user_dir()) + G_DIR_SEPARATOR_S + config::configSubdir;
}

void PurpleTdClient::sendTdlibParameters()
{
    auto parameters = td::td_api::make_object<td::td_api::tdlibParameters>();
    const char *username = purple_account_get_username(m_account);
    parameters->database_directory_ = getBaseDatabasePath() + G_DIR_SEPARATOR_S + username;
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
    m_transceiver.sendQuery(td::td_api::make_object<td::td_api::setTdlibParameters>(std::move(parameters)),
                            &PurpleTdClient::authResponse);
}

void PurpleTdClient::sendPhoneNumber()
{
    const char *number = purple_account_get_username(m_account);
    m_transceiver.sendQuery(td::td_api::make_object<td::td_api::setAuthenticationPhoneNumber>(number, nullptr),
                            &PurpleTdClient::authResponse);
}

static std::string getAuthCodeDesc(const td::td_api::AuthenticationCodeType &codeType)
{
    switch (codeType.get_id()) {
    case td::td_api::authenticationCodeTypeTelegramMessage::ID:
        return formatMessage(_("Telegram message (length: {})"),
                             static_cast<const td::td_api::authenticationCodeTypeTelegramMessage &>(codeType).length_);
    case td::td_api::authenticationCodeTypeSms::ID:
        return formatMessage(_("SMS (length: {})"),
                             static_cast<const td::td_api::authenticationCodeTypeSms &>(codeType).length_);
    case td::td_api::authenticationCodeTypeCall::ID:
        return formatMessage(_("Phone call (length: {})"),
                             static_cast<const td::td_api::authenticationCodeTypeCall &>(codeType).length_);
    case td::td_api::authenticationCodeTypeFlashCall::ID:
        return formatMessage(_("Poor man's phone call (pattern: {})"),
                             static_cast<const td::td_api::authenticationCodeTypeFlashCall &>(codeType).pattern_);
    default:
        return "Pigeon post";
    }
}

void PurpleTdClient::requestAuthCode(const td::td_api::authenticationCodeInfo *codeInfo)
{
    std::string message = _("Enter authentication code") + std::string("\n");

    if (codeInfo) {
        if (codeInfo->type_)
            message += formatMessage(_("Code sent via: {}"), getAuthCodeDesc(*codeInfo->type_)) + "\n";
        if (codeInfo->next_type_)
            message += formatMessage(_("Next code will be: {}"), getAuthCodeDesc(*codeInfo->next_type_)) + "\n";
    }

    if (!purple_request_input (purple_account_get_connection(m_account),
                               _("Login code"),
                               message.c_str(),
                               NULL, // secondary message
                               NULL, // default value
                               FALSE, // multiline input
                               FALSE, // masked input
                               _("the code"),
                               _("OK"), G_CALLBACK(requestCodeEntered),
                               _("Cancel"), G_CALLBACK(requestCodeCancelled),
                               m_account,
                               NULL, // buddy
                               NULL, // conversation
                               this))
    {
        purple_connection_error(purple_account_get_connection(m_account),
            "Authentication code is required but this libpurple doesn't support input requests");
    }
}

void PurpleTdClient::requestCodeEntered(PurpleTdClient *self, const gchar *code)
{
    purple_debug_misc(config::pluginId, "Authentication code entered: '%s'\n", code);
    auto checkCode = td::td_api::make_object<td::td_api::checkAuthenticationCode>();
    if (code)
        checkCode->code_ = code;
    self->m_transceiver.sendQuery(std::move(checkCode), &PurpleTdClient::authResponse);
}

void PurpleTdClient::requestCodeCancelled(PurpleTdClient *self)
{
    purple_connection_error(purple_account_get_connection(self->m_account),
                            _("Authentication code required"));
}

void PurpleTdClient::passwordEntered(PurpleTdClient *self, const gchar *password)
{
    purple_debug_misc(config::pluginId, "Password code entered\n");
    auto checkPassword = td::td_api::make_object<td::td_api::checkAuthenticationPassword>();
    if (password)
        checkPassword->password_ = password;
    self->m_transceiver.sendQuery(std::move(checkPassword), &PurpleTdClient::authResponse);
}

void PurpleTdClient::passwordCancelled(PurpleTdClient *self)
{
    purple_connection_error(purple_account_get_connection(self->m_account), _("Password required"));
}

void PurpleTdClient::requestPassword(const td::td_api::authorizationStateWaitPassword &pwInfo)
{
    std::string hints;
    if (!pwInfo.password_hint_.empty())
        hints = formatMessage(_("Hint: {}"), pwInfo.password_hint_);
    if (!pwInfo.recovery_email_address_pattern_.empty()) {
        if (!hints.empty()) hints += '\n';
        hints += formatMessage(_("Recovery e-mail may have been sent to {}"), pwInfo.recovery_email_address_pattern_);
    }
    if (!purple_request_input (purple_account_get_connection(m_account),
                               _("Password"),
                               _("Enter password for two-factor authentication"),
                               hints.empty() ? NULL : hints.c_str(),
                               NULL, // default value
                               FALSE, // multiline input
                               FALSE, // masked input
                               _("password"),
                               _("OK"), G_CALLBACK(passwordEntered),
                               _("Cancel"), G_CALLBACK(passwordCancelled),
                               m_account,
                               NULL, // buddy
                               NULL, // conversation
                               this))
    {
        purple_connection_error(purple_account_get_connection(m_account),
            "Authentication code is required but this libpurple doesn't support input requests");
    }
}

void PurpleTdClient::registerUser()
{
    std::string firstName, lastName;
    getNamesFromAlias(purple_account_get_alias(m_account), firstName, lastName);

    if (firstName.empty() && lastName.empty()) {
        if (!purple_request_input (purple_account_get_connection(m_account),
                                _("Registration"),
                                _("New account is being created. Please enter your display name."),
                                NULL,
                                NULL, // default value
                                FALSE, // multiline input
                                FALSE, // masked input
                                NULL,
                                _("OK"), G_CALLBACK(displayNameEntered),
                                _("Cancel"), G_CALLBACK(displayNameCancelled),
                                m_account,
                                NULL, // buddy
                                NULL, // conversation
                                this))
        {
            purple_connection_error(purple_account_get_connection(m_account),
                "Authentication is required but this libpurple doesn't support input requests");
        }
    } else
        m_transceiver.sendQuery(td::td_api::make_object<td::td_api::registerUser>(firstName, lastName),
                                &PurpleTdClient::authResponse);
}

void PurpleTdClient::displayNameEntered(PurpleTdClient *self, const gchar *name)
{
    std::string firstName, lastName;
    getNamesFromAlias(name, firstName, lastName);
    if (firstName.empty() && lastName.empty())
        purple_connection_error(purple_account_get_connection(self->m_account),
                                _("Display name is required for registration"));
    else
        self->m_transceiver.sendQuery(td::td_api::make_object<td::td_api::registerUser>(firstName, lastName),
                                      &PurpleTdClient::authResponse);
}

void PurpleTdClient::displayNameCancelled(PurpleTdClient *self)
{
    purple_connection_error(purple_account_get_connection(self->m_account),
                            _("Display name is required for registration"));
}

void PurpleTdClient::authResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    if (object && (object->get_id() == td::td_api::ok::ID))
        purple_debug_misc(config::pluginId, "Authentication success on query %lu\n", (unsigned long)requestId);
    else
        notifyAuthError(object);
}

void PurpleTdClient::notifyAuthError(const td::td_api::object_ptr<td::td_api::Object> &response)
{
    std::string message;
    switch (m_lastAuthState) {
    case td::td_api::authorizationStateWaitEncryptionKey::ID:
        message = _("Error applying database encryption key: {}");
        break;
    case td::td_api::authorizationStateWaitPhoneNumber::ID:
        message = _("Authentication error after sending phone number: {}");
        break;
    default:
        message = _("Authentication error: {}");
    }

    message = formatMessage(message.c_str(), getDisplayedError(response));

    purple_connection_error(purple_account_get_connection(m_account), message.c_str());
}

void PurpleTdClient::connectionReady()
{
    purple_debug_misc(config::pluginId, "Connection ready\n");
    m_connectionReady = true;
    if (m_lastAuthState == td::td_api::authorizationStateReady::ID)
        onLoggedIn();
}

void PurpleTdClient::setPurpleConnectionInProgress()
{
    purple_debug_misc(config::pluginId, "Connection in progress\n");
    m_connectionReady = false;
    PurpleConnection *gc = purple_account_get_connection(m_account);

    if (PURPLE_CONNECTION_IS_CONNECTED(gc))
        purple_blist_remove_account(m_account);
    purple_connection_set_state (gc, PURPLE_CONNECTING);
    purple_connection_update_progress(gc, "Connecting", 1, 3);
}

void PurpleTdClient::setPurpleConnectionUpdating()
{
    purple_debug_misc(config::pluginId, "Updating account status\n");
    m_connectionReady = false;
    PurpleConnection *gc = purple_account_get_connection(m_account);

    purple_connection_update_progress(gc, "Updating status", 2, 3);
}

void PurpleTdClient::onLoggedIn()
{
    // This query ensures an updateUser for every contact
    m_transceiver.sendQuery(td::td_api::make_object<td::td_api::getContacts>(),
                            &PurpleTdClient::getContactsResponse);
}

void PurpleTdClient::getContactsResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    purple_debug_misc(config::pluginId, "getContacts response to request %" G_GUINT64_FORMAT "\n", requestId);
    if (object && (object->get_id() == td::td_api::users::ID)) {
        td::td_api::object_ptr<td::td_api::users> users = td::move_tl_object_as<td::td_api::users>(object);
        m_data.setContacts(users->user_ids_);
        // td::td_api::chats response will be preceded by a string of updateNewChat for all chats
        // apparently even if td::td_api::getChats has limit_ of like 1
        auto getChatsRequest = td::td_api::make_object<td::td_api::getChats>();
        getChatsRequest->chat_list_ = td::td_api::make_object<td::td_api::chatListMain>();
        getChatsRequest->offset_order_ = INT64_MAX;
        getChatsRequest->offset_chat_id_ = 0;
        getChatsRequest->limit_ = 200;
        m_lastChatOrderOffset = INT64_MAX;
        m_transceiver.sendQuery(std::move(getChatsRequest), &PurpleTdClient::getChatsResponse);
    } else
        notifyAuthError(object);
}

void PurpleTdClient::getChatsResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    purple_debug_misc(config::pluginId, "getChats response to request %" G_GUINT64_FORMAT "\n", requestId);
    if (object && (object->get_id() == td::td_api::chats::ID)) {
        td::td_api::object_ptr<td::td_api::chats> chats = td::move_tl_object_as<td::td_api::chats>(object);
        if (chats->chat_ids_.empty()) {
            m_data.getContactsWithNoChat(m_usersForNewPrivateChats);
            requestMissingPrivateChats();
        } else {
            auto getChatsRequest = td::td_api::make_object<td::td_api::getChats>();
            getChatsRequest->chat_list_ = td::td_api::make_object<td::td_api::chatListMain>();
            int64_t chatId;
            int64_t chatOrder;
            m_data.getSmallestOrderChat(*getChatsRequest->chat_list_, chatId, chatOrder);
            fprintf(stderr, "%ld vs %ld\n", chatOrder, m_lastChatOrderOffset);
            if (chatOrder < m_lastChatOrderOffset) {
                getChatsRequest->offset_order_ = chatOrder;
                getChatsRequest->offset_chat_id_ = chatId;
                getChatsRequest->limit_ = 200;
                m_lastChatOrderOffset = chatOrder;
                m_transceiver.sendQuery(std::move(getChatsRequest), &PurpleTdClient::getChatsResponse);
            } else {
                m_data.getContactsWithNoChat(m_usersForNewPrivateChats);
                requestMissingPrivateChats();
            }
        }
    } else
        notifyAuthError(object);
}

void PurpleTdClient::requestMissingPrivateChats()
{
    if (m_usersForNewPrivateChats.empty()) {
        purple_debug_misc(config::pluginId, "Login sequence complete\n");
        updatePurpleChatListAndReportConnected();
    } else {
        int32_t userId = m_usersForNewPrivateChats.back();
        m_usersForNewPrivateChats.pop_back();
        purple_debug_misc(config::pluginId, "Requesting private chat for user id %d\n", (int)userId);
        td::td_api::object_ptr<td::td_api::createPrivateChat> createChat =
            td::td_api::make_object<td::td_api::createPrivateChat>(userId, false);
        m_transceiver.sendQuery(std::move(createChat), &PurpleTdClient::loginCreatePrivateChatResponse);
    }
}

void PurpleTdClient::loginCreatePrivateChatResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    if (object && (object->get_id() == td::td_api::chat::ID)) {
        td::td_api::object_ptr<td::td_api::chat> chat = td::move_tl_object_as<td::td_api::chat>(object);
        purple_debug_misc(config::pluginId, "Requested private chat received: id %" G_GINT64_FORMAT "\n",
                          chat->id_);
        // Here the "new" chat already exists in AccountData because there has just been
        // updateNewChat about this same chat. But do addChat anyway, just in case.
        m_data.addChat(std::move(chat));
    } else
        purple_debug_misc(config::pluginId, "Failed to get requested private chat\n");
    requestMissingPrivateChats();
}

void PurpleTdClient::requestBasicGroupFullInfo(int32_t groupId)
{
    if (!m_data.isBasicGroupInfoRequested(groupId)) {
        m_data.setBasicGroupInfoRequested(groupId);
        uint64_t requestId = m_transceiver.sendQuery(td::td_api::make_object<td::td_api::getBasicGroupFullInfo>(groupId),
                                                     &PurpleTdClient::groupInfoResponse);
        m_data.addPendingRequest<GroupInfoRequest>(requestId, groupId);
    }
}

void PurpleTdClient::requestSupergroupFullInfo(int32_t groupId)
{
    if (!m_data.isSupergroupInfoRequested(groupId)) {
        m_data.setSupergroupInfoRequested(groupId);
        uint64_t requestId = m_transceiver.sendQuery(td::td_api::make_object<td::td_api::getSupergroupFullInfo>(groupId),
                                                     &PurpleTdClient::supergroupInfoResponse);
        m_data.addPendingRequest<GroupInfoRequest>(requestId, groupId);
    }
}

// TODO process messageChatAddMembers and messageChatDeleteMember
// TODO process messageChatUpgradeTo and messageChatUpgradeFrom
void PurpleTdClient::groupInfoResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    std::unique_ptr<GroupInfoRequest> request = m_data.getPendingRequest<GroupInfoRequest>(requestId);

    if (request && object && (object->get_id() == td::td_api::basicGroupFullInfo::ID)) {
        td::td_api::object_ptr<td::td_api::basicGroupFullInfo> groupInfo =
            td::move_tl_object_as<td::td_api::basicGroupFullInfo>(object);
        updateGroupFull(request->groupId, std::move(groupInfo));
    }
}

void PurpleTdClient::supergroupInfoResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    std::unique_ptr<GroupInfoRequest> request = m_data.getPendingRequest<GroupInfoRequest>(requestId);

    if (request && object && (object->get_id() == td::td_api::supergroupFullInfo::ID)) {
        td::td_api::object_ptr<td::td_api::supergroupFullInfo> groupInfo =
            td::move_tl_object_as<td::td_api::supergroupFullInfo>(object);
        updateSupergroupFull(request->groupId, std::move(groupInfo));
    }
}

void PurpleTdClient::updateGroupFull(int32_t groupId, td::td_api::object_ptr<td::td_api::basicGroupFullInfo> groupInfo)
{
    const td::td_api::chat *chat = m_data.getBasicGroupChatByGroup(groupId);

    if (chat) {
        PurpleConvChat *purpleChat = findChatConversation(m_account, *chat);
        if (purpleChat)
            updateChatConversation(purpleChat, *groupInfo, m_data);
    }

    m_data.updateBasicGroupInfo(groupId, std::move(groupInfo));
}

void PurpleTdClient::updateSupergroupFull(int32_t groupId, td::td_api::object_ptr<td::td_api::supergroupFullInfo> groupInfo)
{
    const td::td_api::chat *chat = m_data.getSupergroupChatByGroup(groupId);

    if (chat) {
        PurpleConvChat *purpleChat = findChatConversation(m_account, *chat);
        if (purpleChat)
            updateChatConversation(purpleChat, *groupInfo, m_data);
    }

    m_data.updateSupergroupInfo(groupId, std::move(groupInfo));
}

void PurpleTdClient::updatePurpleChatListAndReportConnected()
{
    purple_connection_set_state (purple_account_get_connection(m_account), PURPLE_CONNECTED);

    std::vector<const td::td_api::chat *> chats;
    m_data.getChats(chats);

    for (const td::td_api::chat *chat: chats) {
        updateChat(chat);

        const td::td_api::user *user = m_data.getUserByPrivateChat(*chat);
        if (user && isChatInContactList(*chat, user)) {
            std::string userName = getPurpleBuddyName(*user);
            purple_prpl_got_user_status(m_account, userName.c_str(),
                                        getPurpleStatusId(*user->status_), NULL);
        }
    }

    // Here we could remove buddies for which no private chat exists, meaning they have been remove
    // from the contact list perhaps in another client

    const td::td_api::user *selfInfo = m_data.getUserByPhone(purple_account_get_username(m_account));
    if (selfInfo != nullptr) {
        std::string alias = selfInfo->first_name_ + " " + selfInfo->last_name_;
        purple_debug_misc(config::pluginId, "Setting own alias to '%s'\n", alias.c_str());
        purple_account_set_alias(m_account, alias.c_str());
    } else
        purple_debug_warning(config::pluginId, "Did not receive user information for self (%s) at login\n",
            purple_account_get_username(m_account));

    purple_blist_add_account(m_account);
}

void PurpleTdClient::showTextMessage(const td::td_api::chat &chat, const TgMessageInfo &message,
                                     const td::td_api::messageText &text)
{
    if (text.text_) {
        std::string displayText = getMessageText(*text.text_);
        showMessageText(m_data, chat, message, displayText.c_str(), NULL);
    }
}

static const td::td_api::file *selectPhotoSize(PurpleAccount *account, const td::td_api::messagePhoto &photo)
{
    unsigned                     sizeLimit        = getAutoDownloadLimitKb(account);
    const td::td_api::photoSize *selectedSize     = nullptr;
    bool                         selectedFileSize = 0;
    if (photo.photo_)
        for (const auto &newSize: photo.photo_->sizes_)
            if (newSize && newSize->photo_) {
                unsigned fileSize            = getFileSizeKb(*newSize->photo_);
                bool     isWithinLimit       = isSizeWithinLimit(fileSize, sizeLimit);
                bool     selectedWithinLimit = isSizeWithinLimit(selectedFileSize, sizeLimit);
                if (!selectedSize ||
                    (!selectedWithinLimit && (isWithinLimit || (fileSize < selectedFileSize))) ||
                    (selectedWithinLimit && isWithinLimit && (newSize->width_ > selectedSize->width_)))
                {
                    selectedSize = newSize.get();
                    selectedFileSize = fileSize;
                }
            }

    if (selectedSize)
        purple_debug_misc(config::pluginId, "Selected size %dx%d for photo\n",
                          (int)selectedSize->width_, (int)selectedSize->height_);
    else
        purple_debug_warning(config::pluginId, "No file found for a photo\n");

    return selectedSize ? selectedSize->photo_.get() : nullptr;
}

void PurpleTdClient::showFile(const td::td_api::chat &chat, TgMessageInfo &message,
                              const td::td_api::file &file, const char *caption,
                              const std::string &fileDesc,
                              td::td_api::object_ptr<td::td_api::file> thumbnail,
                              FileDownloadCb downloadCallback)
{
    std::string notice;
    bool        askDownload  = false;
    bool        autoDownload = false;
    unsigned    fileSize     = getFileSizeKb(file);

    if (caption && (*caption == '\0'))
        caption = NULL;

    if (file.local_ && file.local_->is_downloading_completed_) {
        autoDownload = true;
        notice.clear();
    } else if (isSizeWithinLimit(fileSize, getAutoDownloadLimitKb(m_account))) {
        if (!((message.type == TgMessageInfo::Type::Sticker) && !caption))
            notice = formatMessage(_("Downloading {}"), std::string(fileDesc));
        autoDownload = true;
    } else if (!ignoreBigDownloads(m_account)) {
        notice = formatMessage(_("Requesting {} download"), std::string(fileDesc));
        askDownload = true;
    } else {
        char *fileSizeStr = fileSize ? purple_str_size_to_units(fileSize) : NULL;
        notice = formatMessage(_("Ignoring {} download ({})"),
                               {std::string(fileDesc), fileSizeStr ? std::string(fileSizeStr) :
                                                                     std::string(_("unknown size"))});
        g_free(fileSizeStr);
    }

    if (!notice.empty()) {
        notice = makeNoticeWithSender(chat, message, notice.c_str(), m_account);
        showMessageText(m_data, chat, message, caption, notice.c_str());
    }

    if (autoDownload || askDownload) {
        if (file.local_ && file.local_->is_downloading_completed_)
            (this->*downloadCallback)(chat.id_, message, file.local_->path_, caption, fileDesc,
                                      std::move(thumbnail));
        else if (autoDownload) {
            purple_debug_misc(config::pluginId, "Downloading %s (file id %d)\n", fileDesc.c_str(),
                              (int)file.id_);
            downloadFile(file.id_, chat.id_, message, fileDesc, std::move(thumbnail), downloadCallback);
        } else if (askDownload) {
            std::string sender = getSenderDisplayName(chat, message, m_account);
            requestDownload(sender.c_str(), file, fileDesc, chat, message, downloadCallback);
        }
    }

}

void PurpleTdClient::showPhotoMessage(const td::td_api::chat &chat, TgMessageInfo &message,
                                      const td::td_api::messagePhoto &photo)
{
    const td::td_api::file *file         = selectPhotoSize(m_account, photo);
    const char *            caption      = photo.caption_ ? photo.caption_->text_.c_str() : NULL;
    const char *            errorMessage = NULL;

    if (!file)
        // Unlikely message not worth translating
        errorMessage = "Faulty image";
    else if (photo.is_secret_)
        errorMessage = _("Ignoring secret photo");

    if (errorMessage) {
        std::string notice = makeNoticeWithSender(chat, message, errorMessage, m_account);
        showMessageText(m_data, chat, message, caption, notice.c_str());
    } else
        showFile(chat, message, *file, caption, _("photo"), nullptr, &PurpleTdClient::showDownloadedImage);
}

struct DownloadInfo {
    int32_t         fileId;
    int64_t         chatId;
    TgMessageInfo   message;
    std::string     fileDescription;
    PurpleTdClient *tdClient;
    FileDownloadCb  callback;
};

void PurpleTdClient::startDownload(void *user_data)
{
    std::unique_ptr<DownloadInfo> info(static_cast<DownloadInfo *>(user_data));
    info->tdClient->downloadFile(info->fileId, info->chatId, info->message, info->fileDescription,
                                 nullptr, info->callback);
}

static void ignoreDownload(DownloadRequest *info)
{
    delete info;
}

void PurpleTdClient::downloadFile(int32_t fileId, int64_t chatId, TgMessageInfo &message,
                                  const std::string &fileDescription,
                                  td::td_api::object_ptr<td::td_api::file> thumbnail,
                                  FileDownloadCb callback)
{
    td::td_api::object_ptr<td::td_api::downloadFile> downloadReq =
        td::td_api::make_object<td::td_api::downloadFile>();
    downloadReq->file_id_     = fileId;
    downloadReq->priority_    = FILE_DOWNLOAD_PRIORITY;
    downloadReq->offset_      = 0;
    downloadReq->limit_       = 0;
    downloadReq->synchronous_ = true;

    uint64_t requestId = m_transceiver.sendQuery(std::move(downloadReq), &PurpleTdClient::downloadResponse);
    std::unique_ptr<DownloadRequest> request = std::make_unique<DownloadRequest>(requestId, chatId,
                                               message, fileId, 0, fileDescription, thumbnail.release(),
                                               callback);
    m_data.addPendingRequest<DownloadRequest>(requestId, std::move(request));
    m_transceiver.setQueryTimer(requestId, &PurpleTdClient::startDownloadProgress, 1);
}

void PurpleTdClient::requestDownload(const char *sender, const td::td_api::file &file,
                                     const std::string &fileDesc, const td::td_api::chat &chat,
                                     TgMessageInfo &message, FileDownloadCb callback)
{
    std::string question = formatMessage(_("Download file from {}?"),
                                         getSenderDisplayName(chat, message, m_account));
    unsigned    size     = getFileSize(file);
    char *      sizeStr  = size ? purple_str_size_to_units(size) : g_strdup(_("unknown"));
    std::string chatName = isPrivateChat(chat) ? _("private chat") : chat.title_;
    std::string fileInfo = formatMessage(_("{} posted in {}, size: {}"), {fileDesc,
                                         chatName, std::string(sizeStr)});
    g_free(sizeStr);

    DownloadInfo *info = new DownloadInfo;
    info->fileId = file.id_;
    info->chatId = chat.id_;
    info->message = std::move(message);
    info->fileDescription = fileDesc;
    info->tdClient = this;
    info->callback = callback;

    purple_request_yes_no(purple_account_get_connection(m_account), _("Download"), question.c_str(),
                          fileInfo.c_str(), 0, m_account, NULL, NULL,
                          info, G_CALLBACK(startDownload), G_CALLBACK(ignoreDownload));
}

void PurpleTdClient::startDownloadProgress(uint64_t requestId)
{
    DownloadRequest *request = m_data.findPendingRequest<DownloadRequest>(requestId);
    if (request)
        ::startDownloadProgress(*request, m_transceiver, m_data);
}

static std::string getDownloadPath(const td::td_api::Object *object)
{
    if (!object)
        purple_debug_misc(config::pluginId, "No response after downloading file\n");
    else if (object->get_id() == td::td_api::file::ID) {
        const td::td_api::file &file = static_cast<const td::td_api::file &>(*object);
        if (!file.local_)
            purple_debug_misc(config::pluginId, "No local file info after downloading\n");
        else if (!file.local_->is_downloading_completed_)
            purple_debug_misc(config::pluginId, "File not completely downloaded\n");
        else
            return file.local_->path_;
    } else
        purple_debug_misc(config::pluginId, "Unexpected response to downloading file: id %d\n",
                          (int)object->get_id());

    return "";
}

void PurpleTdClient::downloadResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    std::unique_ptr<DownloadRequest> request = m_data.getPendingRequest<DownloadRequest>(requestId);
    std::string                      path    = getDownloadPath(object.get());
    if (request) {
        finishDownloadProgress(*request, m_data);

        if (!path.empty())
            (this->*(request->callback))(request->chatId, request->message, path, NULL,
                                        request->fileDescription, std::move(request->thumbnail));
    }
}

void PurpleTdClient::showDownloadedImage(int64_t chatId, TgMessageInfo &message,
                                         const std::string &filePath, const char *caption,
                                         const std::string &fileDesc,
                                         td::td_api::object_ptr<td::td_api::file> thumbnail)
{
    const td::td_api::chat *chat = m_data.getChat(chatId);
    if (chat) {
        std::string  text;
        std::string notice;
        gchar       *data   = NULL;
        size_t       len    = 0;

        if (g_file_get_contents (filePath.c_str(), &data, &len, NULL)) {
            int id = purple_imgstore_add_with_id (data, len, NULL);
            text = "\n<img id=\"" + std::to_string(id) + "\">";
        } else if (filePath.find('"') == std::string::npos)
            text = "<img src=\"file://" + filePath + "\">";
        else
            notice = makeNoticeWithSender(*chat, message,  "Cannot show photo: file path contains quotes", m_account);

        if (caption && *caption) {
            if (!text.empty())
                text += "\n";
            text += caption;
        }

        showMessageText(m_data, *chat, message, text.empty() ? NULL : text.c_str(),
                        notice.empty() ? NULL : notice.c_str(), PURPLE_MESSAGE_IMAGES);
    }
}

void PurpleTdClient::showFileMessage(const td::td_api::chat &chat, TgMessageInfo &message,
                                     td::td_api::object_ptr<td::td_api::file> file,
                                     td::td_api::object_ptr<td::td_api::formattedText> caption,
                                     const std::string &fileDescription)
{
    const char *captionStr = caption ? caption->text_.c_str() : NULL;
    if (!file) {
        // Unlikely message not worth translating
        std::string notice = formatMessage("Faulty file: {}", fileDescription);
        notice = makeNoticeWithSender(chat, message, notice.c_str(), m_account);
        showMessageText(m_data, chat, message, captionStr, notice.c_str());
    } else
        showFile(chat, message, *file, captionStr, fileDescription, nullptr,
                 &PurpleTdClient::showDownloadedFile);
}

void PurpleTdClient::showStickerMessage(const td::td_api::chat &chat, TgMessageInfo &message,
                                        td::td_api::messageSticker &stickerContent)
{
    if (!stickerContent.sticker_) return;
    td::td_api::sticker &sticker = *stickerContent.sticker_;

    if (sticker.sticker_) {
        auto thumbnail = sticker.thumbnail_ ? std::move(sticker.thumbnail_->photo_) : nullptr;

        showFile(chat, message, *sticker.sticker_, NULL, _("sticker"), std::move(thumbnail),
                 &PurpleTdClient::showDownloadedSticker);
    }
}

static bool isTgs(const std::string &path)
{
    size_t dot = path.rfind('.');
    if (dot != std::string::npos)
        return !strcmp(path.c_str() + dot + 1, "tgs");

    return false;
}


void PurpleTdClient::showDownloadedSticker(int64_t chatId, TgMessageInfo &message,
                                           const std::string &filePath, const char *caption,
                                           const std::string &fileDescription,
                                           td::td_api::object_ptr<td::td_api::file> thumbnail)
{
    if (isTgs(filePath) && thumbnail) {
        // Avoid message like "Downloading sticker thumbnail...
        // Also ignore size limits, but only determined testers and crazy people would notice.
        if (thumbnail->local_ && thumbnail->local_->is_downloading_completed_)
            showDownloadedSticker(chatId, message, thumbnail->local_->path_, caption,
                                     fileDescription, nullptr);
        else
            downloadFile(thumbnail->id_, chatId, message, fileDescription, nullptr,
                         &PurpleTdClient::showDownloadedSticker);
    } else {
        const td::td_api::chat *chat = m_data.getChat(chatId);
        if (chat)
            showWebpSticker(*chat, message, filePath, fileDescription, m_data);
    }
}


void PurpleTdClient::showDownloadedFile(int64_t chatId, TgMessageInfo &message,
                                        const std::string &filePath, const char *caption,
                                        const std::string &fileDescription,
                                        td::td_api::object_ptr<td::td_api::file> thumbnail)
{
    const td::td_api::chat *chat = m_data.getChat(chatId);
    if (chat)
        showGenericFile(*chat, message, filePath, fileDescription, m_data);
}

void PurpleTdClient::showMessage(const td::td_api::chat &chat, td::td_api::message &message,
                                 td::td_api::object_ptr<td::td_api::message> repliedMessage)
{
    if (!message.content_)
        return;
    purple_debug_misc(config::pluginId, "Displaying message %" G_GINT64_FORMAT "\n", message.id_);

    TgMessageInfo messageInfo;
    messageInfo.type             = TgMessageInfo::Type::Other;
    messageInfo.sender           = getSenderPurpleName(chat, message, m_data);
    messageInfo.timestamp        = message.date_;
    messageInfo.outgoing         = message.is_outgoing_;
    messageInfo.repliedMessageId = message.reply_to_message_id_;
    messageInfo.repliedMessage   = std::move(repliedMessage);

    if (message.forward_info_)
        messageInfo.forwardedFrom = getForwardSource(*message.forward_info_, m_data);

    if (message.ttl_ != 0) {
        const char *text   = _("Received self-destructing message, not displayed due to lack of support");
        std::string notice = makeNoticeWithSender(chat, messageInfo, text, m_account);
        showMessageText(m_data, chat, messageInfo, NULL, notice.c_str());
        return;
    }

    switch (message.content_->get_id()) {
        case td::td_api::messageText::ID:
            showTextMessage(chat, messageInfo, static_cast<const td::td_api::messageText &>(*message.content_));
            break;
        case td::td_api::messagePhoto::ID:
            showPhotoMessage(chat, messageInfo, static_cast<const td::td_api::messagePhoto &>(*message.content_));
            break;
        case td::td_api::messageDocument::ID: {
            td::td_api::messageDocument &document = static_cast<td::td_api::messageDocument &>(*message.content_);
            showFileMessage(chat, messageInfo, document.document_ ? std::move(document.document_->document_) : nullptr,
                            std::move(document.caption_), makeDocumentDescription(document.document_.get()));
            break;
        }
        case td::td_api::messageVideo::ID: {
            td::td_api::messageVideo &video = static_cast<td::td_api::messageVideo &>(*message.content_);
            showFileMessage(chat, messageInfo, video.video_ ? std::move(video.video_->video_) : nullptr,
                            std::move(video.caption_), makeDocumentDescription(video.video_.get()));
            break;
        }
        case td::td_api::messageAnimation::ID: {
            td::td_api::messageAnimation &animation = static_cast<td::td_api::messageAnimation &>(*message.content_);
            showFileMessage(chat, messageInfo, animation.animation_ ? std::move(animation.animation_->animation_) : nullptr,
                            std::move(animation.caption_), makeDocumentDescription(animation.animation_.get()));
            break;
        }
        case td::td_api::messageAudio::ID: {
            td::td_api::messageAudio &audio = static_cast<td::td_api::messageAudio &>(*message.content_);
            showFileMessage(chat, messageInfo, audio.audio_ ? std::move(audio.audio_->audio_) : nullptr,
                            std::move(audio.caption_), makeDocumentDescription(audio.audio_.get()));
            break;
        }
        case td::td_api::messageVoiceNote::ID: {
            td::td_api::messageVoiceNote &audio = static_cast<td::td_api::messageVoiceNote &>(*message.content_);
            showFileMessage(chat, messageInfo, audio.voice_note_ ? std::move(audio.voice_note_->voice_) : nullptr,
                            std::move(audio.caption_), makeDocumentDescription(audio.voice_note_.get()));
            break;
        }
        case td::td_api::messageVideoNote::ID: {
            td::td_api::messageVideoNote &video = static_cast<td::td_api::messageVideoNote &>(*message.content_);
            showFileMessage(chat, messageInfo, video.video_note_ ? std::move(video.video_note_->video_) : nullptr,
                            nullptr, makeDocumentDescription(video.video_note_.get()));
            break;
        }
        case td::td_api::messageSticker::ID:
            messageInfo.type = TgMessageInfo::Type::Sticker;
            showStickerMessage(chat, messageInfo, static_cast<td::td_api::messageSticker &>(*message.content_));
            break;
        case td::td_api::messageChatChangeTitle::ID: {
            const auto &titleChange = static_cast<const td::td_api::messageChatChangeTitle &>(*message.content_);
            std::string notice = formatMessage(_("{} changed group name to {}"),
                                               {getSenderDisplayName(chat, messageInfo, m_account),
                                                titleChange.title_});
            showMessageText(m_data, chat, messageInfo, NULL, notice.c_str());
            break;
        }
        default: {
            std::string notice = formatMessage(_("Received unsupported message type {}"),
                                               messageTypeToString(*message.content_));
            notice = makeNoticeWithSender(chat, messageInfo, notice.c_str(), m_account);
            showMessageText(m_data, chat, messageInfo, NULL, notice.c_str());
        }
    }
}

void PurpleTdClient::onIncomingMessage(td::td_api::object_ptr<td::td_api::message> message)
{
    if (!message)
        return;

    const td::td_api::chat *chat = m_data.getChat(message->chat_id_);
    if (!chat) {
        purple_debug_warning(config::pluginId, "Received message with unknown chat id %" G_GINT64_FORMAT "\n",
                            message->chat_id_);
        return;
    }

    td::td_api::object_ptr<td::td_api::viewMessages> viewMessagesReq = td::td_api::make_object<td::td_api::viewMessages>();
    viewMessagesReq->chat_id_ = chat->id_;
    viewMessagesReq->force_read_ = true; // no idea what "closed chats" are at this point
    viewMessagesReq->message_ids_.push_back(message->id_);
    m_transceiver.sendQuery(std::move(viewMessagesReq), nullptr);

    int64_t messageId      = message->id_;
    int64_t replyMessageId = message->reply_to_message_id_;

    if (replyMessageId) {
        purple_debug_misc(config::pluginId, "Fetching message %" G_GINT64_FORMAT " which message %" G_GINT64_FORMAT " replies to\n",
                          replyMessageId, messageId);
        td::td_api::object_ptr<td::td_api::getMessage> getMessageReq = td::td_api::make_object<td::td_api::getMessage>();
        getMessageReq->chat_id_    = chat->id_;
        getMessageReq->message_id_ = replyMessageId;
        uint64_t requestId = m_transceiver.sendQueryWithTimeout(std::move(getMessageReq),
                                                                &PurpleTdClient::findMessageResponse, 1);
        m_data.addPendingRequest<PendingMessage>(requestId, message.release());
    } else
        showMessage(*chat, *message, nullptr);
}

void PurpleTdClient::findMessageResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    std::unique_ptr<PendingMessage> messageInfo = m_data.getPendingRequest<PendingMessage>(requestId);
    if (!messageInfo || !messageInfo->message) return;
    td::td_api::object_ptr<td::td_api::message> repliedMessage;

    if (object && (object->get_id() == td::td_api::message::ID))
        repliedMessage = td::move_tl_object_as<td::td_api::message>(object);
    else
        purple_debug_misc(config::pluginId, "Failed to fetch reply source for message %" G_GINT64_FORMAT "\n",
                          messageInfo->message->id_);

    const td::td_api::chat *chat = m_data.getChat(messageInfo->message->chat_id_);
    if (chat)
        showMessage(*chat, *messageInfo->message, std::move(repliedMessage));
}

int PurpleTdClient::sendMessage(const char *buddyName, const char *message)
{
    int64_t chatId = getPrivateChatIdByPurpleName(buddyName, m_data, "send message");
    if (chatId != 0) {
        transmitMessage(chatId, message, m_transceiver, m_data, &PurpleTdClient::sendMessageResponse);
        // Message shall not be echoed: tdlib will shortly present it as a new message and it will be displayed then
        return 0;
    }
    return -1;
}

void PurpleTdClient::sendMessageResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    std::unique_ptr<SendMessageRequest> request = m_data.getPendingRequest<SendMessageRequest>(requestId);
    if (!request)
        return;
    if (object && (object->get_id() == td::td_api::message::ID)) {
        if (!request->tempFile.empty()) {
            const td::td_api::message &message = static_cast<td::td_api::message &>(*object);
            m_data.addTempFileUpload(message.id_, request->tempFile);
        }
    } else {
        std::string errorMessage = formatMessage(_("Failed to send message: {}"), getDisplayedError(object));
        const td::td_api::chat *chat = m_data.getChat(request->chatId);
        if (chat) {
            TgMessageInfo messageInfo;
            messageInfo.type = TgMessageInfo::Type::Other;
            messageInfo.outgoing = true;
            messageInfo.timestamp = time(NULL);
            showMessageText(m_data, *chat, messageInfo, NULL, errorMessage.c_str());
        }
    }
}

void PurpleTdClient::sendTyping(const char *buddyName, bool isTyping)
{
    int64_t chatId = getPrivateChatIdByPurpleName(buddyName, m_data, "send message");

    if (chatId != 0) {
        auto sendAction = td::td_api::make_object<td::td_api::sendChatAction>();
        sendAction->chat_id_ = chatId;
        if (isTyping)
            sendAction->action_ = td::td_api::make_object<td::td_api::chatActionTyping>();
        else
            sendAction->action_ = td::td_api::make_object<td::td_api::chatActionCancel>();
        m_transceiver.sendQuery(std::move(sendAction), nullptr);
    }
}

void PurpleTdClient::updateUserStatus(uint32_t userId, td::td_api::object_ptr<td::td_api::UserStatus> status)
{
    const td::td_api::user *user = m_data.getUser(userId);
    if (user) {
        std::string userName = getPurpleBuddyName(*user);
        purple_prpl_got_user_status(m_account, userName.c_str(), getPurpleStatusId(*status), NULL);
        m_data.setUserStatus(userId, std::move(status));
    }
}

void PurpleTdClient::updateUser(td::td_api::object_ptr<td::td_api::user> userInfo)
{
    if (!userInfo) {
        purple_debug_warning(config::pluginId, "updateUser with null user info\n");
        return;
    }

    int32_t userId = userInfo->id_;
    purple_debug_misc(config::pluginId, "Update user: %d '%s' '%s'\n", (int)userId,
                      userInfo->first_name_.c_str(), userInfo->last_name_.c_str());

    m_data.updateUser(std::move(userInfo));

    // For chats, find_chat doesn't work if account is not yet connected, so just in case, don't
    // user find_buddy either
    if (purple_account_is_connected(m_account)) {
        const td::td_api::user *user = m_data.getUser(userId);
        const td::td_api::chat *chat = m_data.getPrivateChatByUserId(userId);

        if (user && chat && isChatInContactList(*chat, user)) {
            downloadProfilePhoto(*user);
            updatePrivateChat(m_data, *chat, *user);
        }
    }
}

static bool shouldDownloadAvatar(const td::td_api::file &file)
{
    return (file.local_ && !file.local_->is_downloading_completed_ &&
            !file.local_->is_downloading_active_ && file.remote_ && file.remote_->is_uploading_completed_ &&
            file.local_->can_be_downloaded_);
}

void PurpleTdClient::downloadProfilePhoto(const td::td_api::user &user)
{
    if (user.profile_photo_ && user.profile_photo_->small_ &&
        shouldDownloadAvatar(*user.profile_photo_->small_))
    {
        auto downloadReq = td::td_api::make_object<td::td_api::downloadFile>();
        downloadReq->file_id_ = user.profile_photo_->small_->id_;
        downloadReq->priority_ = FILE_DOWNLOAD_PRIORITY;
        downloadReq->synchronous_ = true;
        uint64_t queryId = m_transceiver.sendQuery(std::move(downloadReq), &PurpleTdClient::avatarDownloadResponse);
        m_data.addPendingRequest<AvatarDownloadRequest>(queryId, &user);
    }
}

void PurpleTdClient::avatarDownloadResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    std::unique_ptr<AvatarDownloadRequest> request = m_data.getPendingRequest<AvatarDownloadRequest>(requestId);
    if (request && object && (object->get_id() == td::td_api::file::ID)) {
        auto file = td::move_tl_object_as<td::td_api::file>(object);
        if (file->local_ && file->local_->is_downloading_completed_) {
            if (request->userId) {
                m_data.updateSmallProfilePhoto(request->userId, std::move(file));
                const td::td_api::user *user = m_data.getUser(request->userId);
                const td::td_api::chat *chat = m_data.getPrivateChatByUserId(request->userId);
                if (user && chat && isChatInContactList(*chat, user))
                    updatePrivateChat(m_data, *chat, *user);
            } else if (request->chatId) {
                m_data.updateSmallChatPhoto(request->chatId, std::move(file));
                const td::td_api::chat *chat = m_data.getPrivateChatByUserId(request->userId);
                if (chat && isChatInContactList(*chat, nullptr)) {
                    int32_t basicGroupId = getBasicGroupId(*chat);
                    int32_t supergroupId = getSupergroupId(*chat);
                    if (basicGroupId)
                        updateBasicGroupChat(m_data, basicGroupId);
                    if (supergroupId)
                        updateSupergroupChat(m_data, supergroupId);
                }
            }
        }
    }
}

void PurpleTdClient::updateGroup(td::td_api::object_ptr<td::td_api::basicGroup> group)
{
    if (!group) {
        purple_debug_warning(config::pluginId, "updateBasicGroup with null group\n");
        return;
    }
    purple_debug_misc(config::pluginId, "updateBasicGroup id=%d\n", group->id_);

    int32_t id       = group->id_;
    m_data.updateBasicGroup(std::move(group));

    // purple_blist_find_chat doesn't work if account is not connected
    if (purple_account_is_connected(m_account))
        updateBasicGroupChat(m_data, id);
}

void PurpleTdClient::updateSupergroup(td::td_api::object_ptr<td::td_api::supergroup> group)
{
    if (!group) {
        purple_debug_warning(config::pluginId, "updateSupergroup with null group\n");
        return;
    }
    purple_debug_misc(config::pluginId, "updateSupergroup id=%d\n", group->id_);

    int32_t id       = group->id_;
    m_data.updateSupergroup(std::move(group));

    // purple_blist_find_chat doesn't work if account is not connected
    if (purple_account_is_connected(m_account))
        updateSupergroupChat(m_data, id);
}

void PurpleTdClient::updateChat(const td::td_api::chat *chat)
{
    if (!chat) return;

    const td::td_api::user *privateChatUser = m_data.getUserByPrivateChat(*chat);
    int32_t                 basicGroupId    = getBasicGroupId(*chat);
    int32_t                 supergroupId    = getSupergroupId(*chat);
    purple_debug_misc(config::pluginId, "Update chat: %" G_GINT64_FORMAT " private user=%d basic group=%d supergroup=%d\n",
                      chat->id_, privateChatUser ? privateChatUser->id_ : 0, basicGroupId, supergroupId);

    if (!privateChatUser)
        downloadChatPhoto(*chat);

    // For chats, find_chat doesn't work if account is not yet connected, so just in case, don't
    // user find_buddy either
    if (!purple_account_is_connected(m_account))
        return;

    if (isChatInContactList(*chat, privateChatUser)) {
        if (privateChatUser) {
            downloadProfilePhoto(*privateChatUser);
            updatePrivateChat(m_data, *chat, *privateChatUser);
        }

        // purple_blist_find_chat doesn't work if account is not connected
        if (basicGroupId) {
            requestBasicGroupFullInfo(basicGroupId);
            updateBasicGroupChat(m_data, basicGroupId);
        }
        if (supergroupId) {
            requestSupergroupFullInfo(supergroupId);
            updateSupergroupChat(m_data, supergroupId);
        }
    } else {
        removeGroupChat(m_account, *chat);
    }
}

void PurpleTdClient::downloadChatPhoto(const td::td_api::chat &chat)
{
    if (chat.photo_ && chat.photo_->small_ && shouldDownloadAvatar(*chat.photo_->small_)) {
        auto downloadReq = td::td_api::make_object<td::td_api::downloadFile>();
        downloadReq->file_id_ = chat.photo_->small_->id_;
        downloadReq->priority_ = FILE_DOWNLOAD_PRIORITY;
        downloadReq->synchronous_ = true;
        uint64_t queryId = m_transceiver.sendQuery(std::move(downloadReq), &PurpleTdClient::avatarDownloadResponse);
        m_data.addPendingRequest<AvatarDownloadRequest>(queryId, &chat);
    }
}

void PurpleTdClient::addChat(td::td_api::object_ptr<td::td_api::chat> chat)
{
    if (!chat) {
        purple_debug_warning(config::pluginId, "updateNewChat with null chat info\n");
        return;
    }

    purple_debug_misc(config::pluginId, "Add chat: '%s'\n", chat->title_.c_str());
    int64_t chatId = chat->id_;
    m_data.addChat(std::move(chat));
    updateChat(m_data.getChat(chatId));
}

void PurpleTdClient::handleUserChatAction(const td::td_api::updateUserChatAction &updateChatAction)
{
    const td::td_api::chat *chat = m_data.getChat(updateChatAction.chat_id_);
    if (!chat) {
        purple_debug_warning(config::pluginId, "Got user chat action for unknown chat %" G_GINT64_FORMAT "\n",
                             updateChatAction.chat_id_);
        return;
    }

    int32_t chatUserId = getUserIdByPrivateChat(*chat);
    if (chatUserId == 0) {
        purple_debug_misc(config::pluginId, "Ignoring user chat action for non-private chat %" G_GINT64_FORMAT "\n",
                          updateChatAction.chat_id_);
        return;
    }

    if (chatUserId != updateChatAction.user_id_)
        purple_debug_warning(config::pluginId, "Got user action for private chat %" G_GINT64_FORMAT " (with user %d) for another user %d\n",
                                updateChatAction.chat_id_, chatUserId, updateChatAction.user_id_);
    else if (updateChatAction.action_) {
        if (updateChatAction.action_->get_id() == td::td_api::chatActionCancel::ID) {
            purple_debug_misc(config::pluginId, "User (id %d) stopped chat action\n",
                                updateChatAction.user_id_);
            showUserChatAction(updateChatAction.user_id_, false);
        } else if (updateChatAction.action_->get_id() == td::td_api::chatActionStartPlayingGame::ID) {
            purple_debug_misc(config::pluginId, "User (id %d): treating chatActionStartPlayingGame as cancel\n",
                                updateChatAction.user_id_);
            showUserChatAction(updateChatAction.user_id_, false);
        } else {
            purple_debug_misc(config::pluginId, "User (id %d) started chat action (id %d)\n",
                                updateChatAction.user_id_, updateChatAction.action_->get_id());
            showUserChatAction(updateChatAction.user_id_, true);
        }
    }
}

void PurpleTdClient::showUserChatAction(int32_t userId, bool isTyping)
{
    const td::td_api::user *user = m_data.getUser(userId);
    if (user) {
        std::string userName = getPurpleBuddyName(*user);
        if (isTyping)
            serv_got_typing(purple_account_get_connection(m_account),
                            userName.c_str(), REMOTE_TYPING_NOTICE_TIMEOUT,
                            PURPLE_TYPING);
        else
            serv_got_typing_stopped(purple_account_get_connection(m_account),
                                    userName.c_str());
    }
}

static void showFailedContactMessage(void *handle, const std::string &errorMessage)
{
    std::string message = formatMessage(_("Failed to add contact: {}"), errorMessage);
    purple_notify_error(handle, _("Failed to add contact"), message.c_str(), NULL);
}

static int failedContactIdle(gpointer userdata)
{
    char *message = static_cast<char *>(userdata);
    showFailedContactMessage(NULL, message);
    free(message);
    return FALSE; // This idle callback will not be called again
}

static void notifyFailedContactDeferred(const std::string &message)
{
    g_idle_add(failedContactIdle, strdup(message.c_str()));
}

void PurpleTdClient::addContact(const std::string &purpleName, const std::string &alias,
                                const std::string &groupName)
{
    if (m_data.getUserByPhone(purpleName.c_str())) {
        purple_debug_info(config::pluginId, "User with phone number %s already exists\n", purpleName.c_str());
        return;
    }

    std::vector<const td::td_api::user *> users;
    m_data.getUsersByDisplayName(purpleName.c_str(), users);
    if (users.size() > 1) {
        notifyFailedContactDeferred("More than one user known with name '" + purpleName + "'");
        return;
    }

    if (users.size() == 1)
        addContactById(users[0]->id_, "", purpleName, groupName);
    else {
        td::td_api::object_ptr<td::td_api::contact> contact =
            td::td_api::make_object<td::td_api::contact>(purpleName, "", "", "", 0);
        td::td_api::object_ptr<td::td_api::importContacts> importReq =
            td::td_api::make_object<td::td_api::importContacts>();
        importReq->contacts_.push_back(std::move(contact));
        uint64_t requestId = m_transceiver.sendQuery(std::move(importReq),
                                                     &PurpleTdClient::importContactResponse);

        m_data.addPendingRequest<ContactRequest>(requestId, purpleName, alias, groupName, 0);
    }
}

void PurpleTdClient::addContactById(int32_t userId, const std::string &phoneNumber, const std::string &alias,
                                    const std::string &groupName)
{
    purple_debug_misc(config::pluginId, "Adding contact: id=%d alias=%s\n", userId, alias.c_str());
    std::string firstName, lastName;
    getNamesFromAlias(alias.c_str(), firstName, lastName);

    td::td_api::object_ptr<td::td_api::contact> contact =
        td::td_api::make_object<td::td_api::contact>(phoneNumber, firstName, lastName, "", userId);
    td::td_api::object_ptr<td::td_api::addContact> addContact =
        td::td_api::make_object<td::td_api::addContact>(std::move(contact), true);
    uint64_t newRequestId = m_transceiver.sendQuery(std::move(addContact),
                                                    &PurpleTdClient::addContactResponse);
    m_data.addPendingRequest<ContactRequest>(newRequestId, phoneNumber, alias, groupName, userId);
}

void PurpleTdClient::importContactResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    std::unique_ptr<ContactRequest> request = m_data.getPendingRequest<ContactRequest>(requestId);
    if (!request)
        return;

    int32_t userId = 0;
    if (object && (object->get_id() == td::td_api::importedContacts::ID)) {
        td::td_api::object_ptr<td::td_api::importedContacts> reply =
            td::move_tl_object_as<td::td_api::importedContacts>(object);
        if (!reply->user_ids_.empty())
            userId = reply->user_ids_[0];
    }

    // For whatever reason, complaining at an earlier stage leads to error message not being shown in pidgin
    if (!isPhoneNumber(request->phoneNumber.c_str()))
        notifyFailedContact(formatMessage(_("{} is not a valid phone number"), request->phoneNumber));
    else if (userId)
        addContactById(userId, request->phoneNumber, request->alias, request->groupName);
    else
        notifyFailedContact(formatMessage(_("No user found with phone number '{}'"), request->phoneNumber));
}

void PurpleTdClient::addContactResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    std::unique_ptr<ContactRequest> request = m_data.getPendingRequest<ContactRequest>(requestId);
    if (!request)
        return;

    if (object && (object->get_id() == td::td_api::ok::ID)) {
        td::td_api::object_ptr<td::td_api::createPrivateChat> createChat =
            td::td_api::make_object<td::td_api::createPrivateChat>(request->userId, false);
        uint64_t newRequestId = m_transceiver.sendQuery(std::move(createChat),
                                                        &PurpleTdClient::addContactCreatePrivateChatResponse);
        m_data.addPendingRequest(newRequestId, std::move(request));
    } else
        notifyFailedContact(getDisplayedError(object));
}

void PurpleTdClient::addContactCreatePrivateChatResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    std::unique_ptr<ContactRequest> request = m_data.getPendingRequest<ContactRequest>(requestId);
    if (!request)
        return;

    if (!object || (object->get_id() != td::td_api::chat::ID)) {
        purple_debug_misc(config::pluginId, "Failed to create private chat to %s\n",
                          request->phoneNumber.c_str());
        notifyFailedContact(getDisplayedError(object));
    }
}

void PurpleTdClient::notifyFailedContact(const std::string &errorMessage)
{
    showFailedContactMessage(purple_account_get_connection(m_account), errorMessage);
}

void PurpleTdClient::renameContact(const char *buddyName, const char *newAlias)
{
    int32_t userId = stringToUserId(buddyName);
    if (userId == 0) {
        purple_debug_warning(config::pluginId, "Cannot rename %s: not a valid id\n", buddyName);
        return;
    }

    std::string firstName, lastName;
    getNamesFromAlias(newAlias, firstName, lastName);
    auto contact    = td::td_api::make_object<td::td_api::contact>("", firstName, lastName, "", userId);
    auto addContact = td::td_api::make_object<td::td_api::addContact>(std::move(contact), true);
    m_transceiver.sendQuery(std::move(addContact), nullptr);
}

void PurpleTdClient::removeContactAndPrivateChat(const std::string &buddyName)
{
    int32_t userId = stringToUserId(buddyName.c_str());
    if (userId != 0) {
        const td::td_api::chat *chat   = m_data.getPrivateChatByUserId(userId);
        if (chat) {
            int64_t chatId = chat->id_;
            chat = nullptr;
            m_data.deleteChat(chatId); // Prevent re-creating buddy if any updateChat* or updateUser arrives

            auto deleteChat = td::td_api::make_object<td::td_api::deleteChatHistory>();
            deleteChat->chat_id_ = chatId;
            deleteChat->remove_from_chat_list_ = true;
            deleteChat->revoke_ = false;
            m_transceiver.sendQuery(std::move(deleteChat), nullptr);
        }

        auto removeContact = td::td_api::make_object<td::td_api::removeContacts>();
        removeContact->user_ids_.push_back(userId);
        m_transceiver.sendQuery(std::move(removeContact), nullptr);
    }
}

void PurpleTdClient::getUsers(const char *username, std::vector<const td::td_api::user *> &users)
{
    getUsersByPurpleName(username, users, m_data);
}

bool PurpleTdClient::joinChat(const char *chatName)
{
    int64_t                 id       = getTdlibChatId(chatName);
    const td::td_api::chat *chat     = m_data.getChat(id);
    int32_t                 purpleId = m_data.getPurpleChatId(id);
    PurpleConvChat         *conv     = NULL;

    if (!chat)
        purple_debug_warning(config::pluginId, "No telegram chat found for purple name %s\n", chatName);
    else if (!m_data.isGroupChatWithMembership(*chat))
        purple_debug_warning(config::pluginId, "Chat %s (%s) is not a group we a member of\n",
                             chatName, chat->title_.c_str());
    else if (purpleId) {
        conv = getChatConversation(m_data, *chat, purpleId);
        if (conv)
            purple_conversation_present(purple_conv_chat_get_conversation(conv));
    }

    return conv ? true : false;
}

int PurpleTdClient::sendGroupMessage(int purpleChatId, const char *message)
{
    const td::td_api::chat *chat = m_data.getChatByPurpleId(purpleChatId);

    if (!chat)
        purple_debug_warning(config::pluginId, "No chat found for purple id %d\n", purpleChatId);
    else if (!m_data.isGroupChatWithMembership(*chat))
        purple_debug_misc(config::pluginId, "purple id %d (chat %s) is not a group we a member of\n",
                             purpleChatId, chat->title_.c_str());
    else {
        transmitMessage(chat->id_, message, m_transceiver, m_data, &PurpleTdClient::sendMessageResponse);
        return 0;
    }

    return -1;
}

void PurpleTdClient::joinChatByLink(const char *inviteLink)
{
    auto     request   = td::td_api::make_object<td::td_api::joinChatByInviteLink>(inviteLink);
    uint64_t requestId = m_transceiver.sendQuery(std::move(request), &PurpleTdClient::joinChatByLinkResponse);
    m_data.addPendingRequest<GroupJoinRequest>(requestId, inviteLink);
}

void PurpleTdClient::joinChatByLinkResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    std::unique_ptr<GroupJoinRequest> request = m_data.getPendingRequest<GroupJoinRequest>(requestId);
    if (object && (object->get_id() == td::td_api::chat::ID)) {
        // If the chat was added with something like "Add chat" function from Pidgin, the chat in
        // contact list was created without id component (for if there was the id component,
        // tgprpl_chat_join would not have called PurpleTdClient::joinChatByLink).

        // So when updateNewChat came prior to this response (as it must have), a new chat with
        // correct id component (but without invite link component) was added to the contact list
        // by PurpleTdClient::addChat calling updateBasicGroupChat, or whatever happens for
        // supergroups.

        // Therefore, remove the original manually added chat, and keep the auto-added one.
        // Furthermore, user could have added same chat like that multiple times, in which case
        // remove all of them.
        if (request) {
            std::vector<PurpleChat *> obsoleteChats = findChatsByInviteLink(request->inviteLink);
            for (PurpleChat *chat: obsoleteChats)
                purple_blist_remove_chat(chat);
        }
    } else {
        std::string message = formatMessage(_("Failed to join chat: {}"), getDisplayedError(object));
        purple_notify_error(purple_account_get_connection(m_account), _("Failed to join chat"),
                            message.c_str(), NULL);
    }
}

void PurpleTdClient::createGroup(const char *name, int type,
                                 const std::vector<std::string> &basicGroupMembers)
{
    td::td_api::object_ptr<td::td_api::Function> request;
    if (type == GROUP_TYPE_BASIC) {
        auto createRequest = td::td_api::make_object<td::td_api::createNewBasicGroupChat>();
        createRequest->title_ = name;

        std::string errorMessage;
        if (basicGroupMembers.empty())
            errorMessage = _("Cannot create basic group without additional members");
        for (const std::string &memberName: basicGroupMembers) {
            int32_t userId = stringToUserId(memberName.c_str());
            if (userId != 0) {
                if (!m_data.getUser(userId))
                    errorMessage = formatMessage(_("No known user with id {}"), userId);
            } else {
                std::vector<const td::td_api::user*> users;
                m_data.getUsersByDisplayName(memberName.c_str(), users);
                if (users.size() == 1)
                    userId = users[0]->id_;
                else if (users.empty())
                    errorMessage = formatMessage(_("No known user by the name '{}'"), memberName);
                else
                    errorMessage = "More than one user known with name '" + memberName + "'";
            }
            if (!errorMessage.empty())
                break;
            createRequest->user_ids_.push_back(userId);
        }

        if (!errorMessage.empty())
            purple_notify_error(purple_account_get_connection(m_account),
                                _("Failed to create group"), _("Invalid group members"),
                                errorMessage.c_str());
        else
            request = std::move(createRequest);
    } else if ((type == GROUP_TYPE_SUPER) || (type == GROUP_TYPE_CHANNEL)) {
        auto createRequest = td::td_api::make_object<td::td_api::createNewSupergroupChat>();
        createRequest->title_ = name;
        createRequest->is_channel_ = (type == GROUP_TYPE_CHANNEL);
        request = std::move(createRequest);
    }

    if (request) {
        // Same as for joining by invite link
        std::vector<PurpleChat *> obsoleteChats = findChatsByNewGroup(name, type);
        for (PurpleChat *chat: obsoleteChats)
            purple_blist_remove_chat(chat);

        m_transceiver.sendQuery(std::move(request), nullptr);
    }
}

BasicGroupMembership PurpleTdClient::getBasicGroupMembership(const char *purpleChatName)
{
    int64_t                       chatId     = getTdlibChatId(purpleChatName);
    const td::td_api::chat       *chat       = chatId ? m_data.getChat(chatId) : nullptr;
    int32_t                       groupId    = chat ? getBasicGroupId(*chat) : 0;
    const td::td_api::basicGroup *basicGroup = groupId ? m_data.getBasicGroup(groupId) : nullptr;

    if (basicGroup) {
        if (basicGroup->status_ && (basicGroup->status_->get_id() == td::td_api::chatMemberStatusCreator::ID))
            return BasicGroupMembership::Creator;
        else
            return BasicGroupMembership::NonCreator;
    }
    return BasicGroupMembership::Invalid;
}

void PurpleTdClient::leaveGroup(const std::string &purpleChatName, bool deleteSupergroup)
{
    int64_t                 chatId = getTdlibChatId(purpleChatName.c_str());
    const td::td_api::chat *chat   = chatId ? m_data.getChat(chatId) : nullptr;
    if (!chat) return;

    int32_t supergroupId = getSupergroupId(*chat);
    if (deleteSupergroup && (supergroupId != 0)) {
        m_transceiver.sendQuery(td::td_api::make_object<td::td_api::deleteSupergroup>(supergroupId),
                                &PurpleTdClient::deleteSupergroupResponse);
    } else {
        m_transceiver.sendQuery(td::td_api::make_object<td::td_api::leaveChat>(chatId), nullptr);
        auto deleteChatRequest = td::td_api::make_object<td::td_api::deleteChatHistory>();
        deleteChatRequest->chat_id_ = chatId;
        deleteChatRequest->remove_from_chat_list_ = true;
        deleteChatRequest->revoke_ = false;
        m_transceiver.sendQuery(std::move(deleteChatRequest), nullptr);
    }
}

void PurpleTdClient::deleteSupergroupResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    if (!object || (object->get_id() != td::td_api::ok::ID)) {
        std::string errorMessage = getDisplayedError(object).c_str();
        purple_notify_error(m_account, _("Error"), _("Failed to delete group or channel"),
                            errorMessage.c_str());
    }
}

void PurpleTdClient::setGroupDescription(int purpleChatId, const char *description)
{
    const td::td_api::chat *chat = m_data.getChatByPurpleId(purpleChatId);
    if (!chat) {
        purple_debug_warning(config::pluginId, "Unknown libpurple chat id %d\n", purpleChatId);
        return;
    }

    if (getBasicGroupId(*chat) || getSupergroupId(*chat)) {
        auto request = td::td_api::make_object<td::td_api::setChatDescription>();
        request->chat_id_ = chat->id_;
        request->description_ =  description ? description : "";
        m_transceiver.sendQuery(std::move(request), &PurpleTdClient::setGroupDescriptionResponse);
    }
}

void PurpleTdClient::setGroupDescriptionResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    if (!object || (object->get_id() != td::td_api::ok::ID)) {
        std::string message = getDisplayedError(object);
        purple_notify_error(m_account, _("Error"), _("Failed to set group description"), message.c_str());
    }
}

void PurpleTdClient::removeTempFile(int64_t messageId)
{
    std::string path = m_data.extractTempFileUpload(messageId);
    if (!path.empty()) {
        purple_debug_misc(config::pluginId, "Removing temporary file %s\n", path.c_str());
        remove(path.c_str());
    }
}

void PurpleTdClient::setTwoFactorAuth(const char *oldPassword, const char *newPassword,
                                    const char *hint, const char *email)
{
    auto setPassword = td::td_api::make_object<td::td_api::setPassword>();
    if (oldPassword)
        setPassword->old_password_ = oldPassword;
    if (newPassword)
        setPassword->new_password_ = newPassword;
    if (hint)
        setPassword->new_hint_ = hint;
    setPassword->set_recovery_email_address_ = (email && *email);
    if (email)
        setPassword->new_recovery_email_address_ = email;

    m_transceiver.sendQuery(std::move(setPassword), &PurpleTdClient::setTwoFactorAuthResponse);
}

static void inputCancelled(void *data)
{
}

void PurpleTdClient::requestRecoveryEmailConfirmation(const std::string &emailInfo)
{
    std::string secondary = "Password will be changed after new e-mail is confirmed\n" + emailInfo;
    PurpleConnection *gc = purple_account_get_connection(m_account);
    purple_request_input(gc, _("Two-factor authentication"),
                         _("Enter verification code received in the e-mail"), secondary.c_str(),
                         NULL,  // default value
                         FALSE, // multiline input
                         FALSE, // masked input
                         NULL,
                         _("OK"), G_CALLBACK(PurpleTdClient::verifyRecoveryEmail),
                         _("Cancel"), G_CALLBACK(inputCancelled),
                         purple_connection_get_account(gc),
                         NULL, // buddy
                         NULL, // conversation
                         this);
}

static void notifyPasswordChangeSuccess(PurpleAccount *account, const td::td_api::passwordState &passwordState)
{
    purple_notify_info(account, _("Two-factor authentication"),
                        passwordState.has_password_ ? _("Password set") : _("Password cleared"),
                        passwordState.has_recovery_email_address_ ? _("Recovery e-mail is configured") :
                                                                    _("No recovery e-mail configured"));
}

void PurpleTdClient::setTwoFactorAuthResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    if (object && (object->get_id() == td::td_api::passwordState::ID)) {
        const td::td_api::passwordState &passwordState = static_cast<const td::td_api::passwordState &>(*object);
        if (passwordState.recovery_email_address_code_info_) {
            std::string emailInfo = formatMessage(_("Code sent to {} (length: {})"),
                                                  {passwordState.recovery_email_address_code_info_->email_address_pattern_,
                                                   std::to_string(passwordState.recovery_email_address_code_info_->length_)});
            requestRecoveryEmailConfirmation(emailInfo);
        } else
            notifyPasswordChangeSuccess(m_account, passwordState);
    } else {
        std::string errorMessage = getDisplayedError(object);
        purple_notify_error(m_account, _("Two-factor authentication"), _("Failed to set password"), errorMessage.c_str());
    }
}

void PurpleTdClient::verifyRecoveryEmail(PurpleTdClient *self, const char *code)
{
    auto checkCode = td::td_api::make_object<td::td_api::checkRecoveryEmailAddressCode>();
    if (code)
        checkCode->code_ = code;
    self->m_transceiver.sendQuery(std::move(checkCode), &PurpleTdClient::verifyRecoveryEmailResponse);
}

void PurpleTdClient::verifyRecoveryEmailResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    if (object && (object->get_id() == td::td_api::passwordState::ID)) {
        const td::td_api::passwordState &passwordState = static_cast<const td::td_api::passwordState &>(*object);
        if (passwordState.recovery_email_address_code_info_) {
            if (passwordState.recovery_email_address_code_info_->length_ > 0) {
                std::string emailInfo = formatMessage(_("E-mail address: {}"),
                                                      passwordState.recovery_email_address_code_info_->email_address_pattern_);
                purple_notify_info(m_account, _("Two-factor authentication"),
                                   _("For some reason, new confirmation code was sent"), emailInfo.c_str());
            } else
                purple_notify_error(m_account, _("Two-factor authentication"), _("Looks like the code was wrong"), NULL);
        } else
            notifyPasswordChangeSuccess(m_account, passwordState);
    } else {
        std::string errorMessage = getDisplayedError(object);
        purple_notify_error(m_account, _("Two-factor authentication"), _("Failed to verify recovery e-mail"), errorMessage.c_str());
    }
}

void PurpleTdClient::sendFileToChat(PurpleXfer *xfer, const char *purpleName, PurpleConversationType type)
{
    const char *filename = purple_xfer_get_local_filename(xfer);
    int64_t     chatId   = 0;
    if (type == PURPLE_CONV_TYPE_IM)
        chatId = getPrivateChatIdByPurpleName(purpleName, m_data, "send file");

    if (filename && (chatId != 0))
        startDocumentUpload(chatId, filename, xfer, m_transceiver, m_data, &PurpleTdClient::uploadResponse);
    else {
        if (!filename)
            purple_debug_warning(config::pluginId, "Failed to send file, no file name\n");
        else if (chatId == 0)
            purple_debug_warning(config::pluginId, "Failed to send file %s, chat not found\n", filename);
        purple_xfer_cancel_remote(xfer);
    }
}

void PurpleTdClient::uploadResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    std::unique_ptr<UploadRequest> request = m_data.getPendingRequest<UploadRequest>(requestId);
    const td::td_api::file        *file    = nullptr;

    if (object && (object->get_id() == td::td_api::file::ID))
        file = static_cast<const td::td_api::file *>(object.get());

    if (request) {
        if (file)
            startDocumentUploadProgress(request->chatId, request->xfer, *file, m_transceiver, m_data,
                                        &PurpleTdClient::sendMessageResponse);
        else
            uploadResponseError(request->xfer, getDisplayedError(object), m_data);
    }
}

void PurpleTdClient::cancelUpload(PurpleXfer *xfer)
{
    int32_t fileId;
    if (m_data.getFileIdForTransfer(xfer, fileId)) {
        purple_debug_misc(config::pluginId, "Cancelling upload of %s (file id %d)\n",
                          purple_xfer_get_local_filename(xfer), fileId);
        auto cancelRequest = td::td_api::make_object<td::td_api::cancelUploadFile>(fileId);
        m_transceiver.sendQuery(std::move(cancelRequest), nullptr);
        m_data.removeFileTransfer(fileId);
        purple_xfer_unref(xfer);
    } else {
        // This could mean that response to upload request has not come yet - when it does,
        // uploadResponse will notice that the transfer is cancelled and act accordingly.
        // Or it could just be that the upload got cancelled programmatically due to some error,
        // in which case nothing more should be done.
    }
}
