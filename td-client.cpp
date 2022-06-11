#include "td-client.h"
#include "purple-info.h"
#include "config.h"
#include "format.h"
#include "receiving.h"
#include "file-transfer.h"
#include "call.h"
#include "secret-chat.h"
#include "sticker.h"
#include "receiving.h"
#include <unistd.h>
#include <stdlib.h>
#include <algorithm>

enum {
    // Typing notifications seems to be resent every 5-6 seconds, so 10s timeout hould be appropriate
    REMOTE_TYPING_NOTICE_TIMEOUT = 10,
    SUPERGROUP_MEMBER_LIMIT      = 200,
};

PurpleTdClient::PurpleTdClient(PurpleAccount *acct, ITransceiverBackend *testBackend)
:   m_transceiver(this, acct, &PurpleTdClient::processUpdate, testBackend),
    m_data(acct, m_transceiver)
{
    StickerConversionThread::setCallback(&PurpleTdClient::onAnimatedStickerConverted);
    m_account = acct;
    setPurpleConnectionInProgress();
}

PurpleTdClient::~PurpleTdClient()
{
    std::vector<PurpleXfer *> transfers;
    m_data.removeAllFileTransfers(transfers);
    for (PurpleXfer *xfer: transfers) {
        // We keep uploads ref'd but not downloads
        if (purple_xfer_get_type(xfer) == PURPLE_XFER_SEND)
            purple_xfer_unref(xfer);
        purple_xfer_cancel_local(xfer);
    }
    m_data.extractFileTransferRequests(transfers);
    for (PurpleXfer *xfer: transfers) {
        purple_xfer_unref(xfer);
        purple_xfer_cancel_local(xfer);
    }

    std::vector<IncomingMessage> messages;
    m_data.pendingMessages.flush(messages);

    // This avoids re-sending download request when displaying message. Doing this for messages
    // that don't involve inline downloads is fine.
    for (IncomingMessage &fullMessage: messages)
        fullMessage.inlineDownloadTimeout = true;

    showMessages(messages, m_data);
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
            updateUserStatus(getUserId(updateStatus), std::move(updateStatus.status_));
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
        updateGroupFull(getBasicGroupId(groupUpdate), std::move(groupUpdate.basic_group_full_info_));
        break;
    };

    case td::td_api::updateSupergroupFullInfo::ID: {
        auto &groupUpdate = static_cast<td::td_api::updateSupergroupFullInfo &>(update);
        updateSupergroupFull(getSupergroupId(groupUpdate), std::move(groupUpdate.supergroup_full_info_));
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

    case td::td_api::updateChatPosition::ID: {
        auto &chatPositionUpdate = static_cast<td::td_api::updateChatPosition &>(update);
        purple_debug_misc(config::pluginId, "Incoming update: update chat position for chat %" G_GINT64_FORMAT "\n",
                          chatPositionUpdate.chat_id_);
        if (chatPositionUpdate.position_)
            m_data.updateChatPosition(getChatId(chatPositionUpdate), std::move(chatPositionUpdate.position_));
        updateChat(m_data.getChat(getChatId(chatPositionUpdate)));
        break;
    }

    case td::td_api::updateChatTitle::ID: {
        auto &chatTitleUpdate = static_cast<td::td_api::updateChatTitle &>(update);
        purple_debug_misc(config::pluginId, "Incoming update: update chat title for chat %" G_GINT64_FORMAT "\n",
                          chatTitleUpdate.chat_id_);
        m_data.updateChatTitle(getChatId(chatTitleUpdate), chatTitleUpdate.title_);
        updateChat(m_data.getChat(getChatId(chatTitleUpdate)));
        break;
    }

    case td::td_api::updateChatLastMessage::ID: {
        auto &lastMessage = static_cast<td::td_api::updateChatLastMessage &>(update);
        updateChatLastMessage(lastMessage);
        break;
    }

    case td::td_api::updateOption::ID: {
        const td::td_api::updateOption &option = static_cast<const td::td_api::updateOption &>(update);
        updateOption(option, m_data);
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

    case td::td_api::updateCall::ID: {
        auto &callUpdate = static_cast<const td::td_api::updateCall &>(update);
        if (callUpdate.call_) {
            purpleDebug("Call update: id {}, outgoing={}, user id {}, state {}", {
                        std::to_string(callUpdate.call_->id_),
                        std::to_string(callUpdate.call_->user_id_),
                        std::to_string((int)callUpdate.call_->is_outgoing_),
                        std::to_string(callUpdate.call_->state_ ? callUpdate.call_->state_->get_id() : 0)});
            updateCall(*callUpdate.call_, m_data, m_transceiver);
        }
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
        // TRANSLATOR: Buddy-window error message, argument will be some kind of proxy-identifier.
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

void PurpleTdClient::addProxyResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    if (object && (object->get_id() == td::td_api::proxy::ID)) {
        m_addedProxy = td::move_tl_object_as<td::td_api::proxy>(object);
        if (m_proxies)
            removeOldProxies();
    } else {
        // TRANSLATOR: Buddy-window error message
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
        // TRANSLATOR: Buddy-window error message
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

static void stuff(td::td_api::tdlibParameters &parameters)
{
    std::string s(config::stuff);
    for (size_t i = 0; i < s.length(); i++)
        s[i] -= 16;
    size_t i = s.find('i');
    if (i == std::string::npos)
        return;
    s[i] = ' ';
    sscanf(s.c_str(), "%" G_GINT32_FORMAT, &parameters.api_id_);
    parameters.api_hash_ = s.c_str()+i+1;
}

void PurpleTdClient::sendTdlibParameters()
{
    auto parameters = td::td_api::make_object<td::td_api::tdlibParameters>();
    const char *username = purple_account_get_username(m_account);
    const char *api_id = purple_account_get_string(m_account, AccountOptions::ApiId, "");
    const char *api_hash = purple_account_get_string(m_account, AccountOptions::ApiHash, "");

    parameters->database_directory_ = getBaseDatabasePath() + G_DIR_SEPARATOR_S + username;
    purple_debug_misc(config::pluginId, "Account %s using database directory %s\n",
                      username, parameters->database_directory_.c_str());
    parameters->use_chat_info_database_ = true;
    parameters->use_message_database_ = true;
    parameters->use_secret_chats_ = (purple_account_get_bool(m_account, AccountOptions::EnableSecretChats,
                                                             AccountOptions::EnableSecretChatsDefault) != FALSE);
    parameters->api_id_ = atoi((api_id == nullptr || strlen(api_id) == 0) ? config::api_id : api_id);
    parameters->api_hash_ = (api_hash == nullptr || strlen(api_hash) == 0) ? config::api_hash : api_hash;
    if (*config::stuff)
        stuff(*parameters);
    parameters->system_language_code_ = "en";
    parameters->device_model_ = "Desktop";
    parameters->system_version_ = "Unknown";
    parameters->application_version_ = "1.0";
    parameters->enable_storage_optimizer_ = (purple_account_get_bool(m_account, AccountOptions::KeepInlineDownloads,
                                                                     AccountOptions::KeepInlineDownloadsDefault) == FALSE);
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
        // TRANSLATOR: Authentication dialog, secondary content. Appears after a colon (':'). Argument is a number.
        return formatMessage(_("Telegram message (length: {})"),
                             static_cast<const td::td_api::authenticationCodeTypeTelegramMessage &>(codeType).length_);
    case td::td_api::authenticationCodeTypeSms::ID:
        // TRANSLATOR: Authentication dialog, secondary content. Appears after a colon (':'). Argument is a number.
        return formatMessage(_("SMS (length: {})"),
                             static_cast<const td::td_api::authenticationCodeTypeSms &>(codeType).length_);
    case td::td_api::authenticationCodeTypeCall::ID:
        // TRANSLATOR: Authentication dialog, secondary content. Appears after a colon (':'). Argument is a number.
        return formatMessage(_("Phone call (length: {})"),
                             static_cast<const td::td_api::authenticationCodeTypeCall &>(codeType).length_);
    case td::td_api::authenticationCodeTypeFlashCall::ID:
        // TRANSLATOR: Authentication dialog, secondary content. Official name "flash call". Appears after a colon (':'). Argument is some text-string-ish.
        return formatMessage(_("Poor man's phone call (pattern: {})"),
                             static_cast<const td::td_api::authenticationCodeTypeFlashCall &>(codeType).pattern_);
    default:
        // Shouldn't happen, so don't translate.
        return "Pigeon post";
    }
}

void PurpleTdClient::requestAuthCode(const td::td_api::authenticationCodeInfo *codeInfo)
{
    // TRANSLATOR: Authentication dialog, primary content. Will be followed by instructions and an input box.
    std::string message = _("Enter authentication code") + std::string("\n");

    if (codeInfo) {
        if (codeInfo->type_) {
            // TRANSLATOR: Authentication dialog, secondary content. Argument will be a term.
            message += formatMessage(_("Code sent via: {}"), getAuthCodeDesc(*codeInfo->type_)) + "\n";
        }
        if (codeInfo->next_type_) {
            // TRANSLATOR: Authentication dialog, secondary content. Argument will be a term.
            message += formatMessage(_("Next code will be: {}"), getAuthCodeDesc(*codeInfo->next_type_)) + "\n";
        }
    }

    purple_request_input (purple_account_get_connection(m_account),
                               // TRANSLATOR: Authentication dialog, title.
                               _("Login code"),
                               message.c_str(),
                               NULL, // secondary message
                               NULL, // default value
                               FALSE, // multiline input
                               FALSE, // masked input
                               NULL,
                               // TRANSLATOR: Authentication dialog, alternative is "_Cancel". The underscore marks accelerator keys, they must be different!
                               _("_OK"), G_CALLBACK(requestCodeEntered),
                               // TRANSLATOR: Authentication dialog, alternative is "_OK". The underscore marks accelerator keys, they must be different!
                               _("_Cancel"), G_CALLBACK(requestCodeCancelled),
                               m_account,
                               NULL, // buddy
                               NULL, // conversation
                               this);
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
                            // TRANSLATOR: Connection failure, error message (title; empty content)
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
    // TRANSLATOR: Connection failure, error message title (title; empty content)
    purple_connection_error(purple_account_get_connection(self->m_account), _("Password required"));
}

void PurpleTdClient::requestPassword(const td::td_api::authorizationStateWaitPassword &pwInfo)
{
    std::string hints;
    if (!pwInfo.password_hint_.empty()) {
        // TRANSLATOR: 2FA dialog, secondary content, appears in new line. Argument is an arbitrary string from Telegram.
        hints = formatMessage(_("Hint: {}"), pwInfo.password_hint_);
    }
    if (!pwInfo.recovery_email_address_pattern_.empty()) {
        if (!hints.empty())
            hints += '\n';
        // TRANSLATOR: 2FA dialog, secondary content, appears in new line. Argument is an e-mail address.
        hints += formatMessage(_("Recovery e-mail may have been sent to {}"), pwInfo.recovery_email_address_pattern_);
    }
    if (!purple_request_input (purple_account_get_connection(m_account),
                               // TRANSLATOR: 2FA dialog, title
                               _("Password"),
                               // TRANSLATOR: 2FA dialog, primary content
                               _("Enter password for two-factor authentication"),
                               hints.empty() ? NULL : hints.c_str(),
                               NULL, // default value
                               FALSE, // multiline input
                               FALSE, // masked input
                               NULL,
                               // TRANSLATOR: 2FA dialog, alternative is "_Cancel". The underscore marks accelerator keys, they must be different!
                               _("_OK"), G_CALLBACK(passwordEntered),
                               // TRANSLATOR: 2FA dialog, alternative is "_OK". The underscore marks accelerator keys, they must be different!
                               _("_Cancel"), G_CALLBACK(passwordCancelled),
                               m_account,
                               NULL, // buddy
                               NULL, // conversation
                               this))
    {
        // Only happens with like empathy, not worth translating
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
                                // TRANSLATOR: Registration dialog, title
                                _("Registration"),
                                // TRANSLATOR: Registration dialog, content
                                _("New account is being created. Please enter your display name."),
                                NULL,
                                NULL, // default value
                                FALSE, // multiline input
                                FALSE, // masked input
                                NULL,
                                // TRANSLATOR: Registration dialog, alternative is "_Cancel". The underscore marks accelerator keys, they must be different!
                                _("_OK"), G_CALLBACK(displayNameEntered),
                                // TRANSLATOR: Registration dialog, alternative is "_OK". The underscore marks accelerator keys, they must be different!
                                _("_Cancel"), G_CALLBACK(displayNameCancelled),
                                m_account,
                                NULL, // buddy
                                NULL, // conversation
                                this))
        {
            // Same as when requesting authentication code - not worth translating
            purple_connection_error(purple_account_get_connection(m_account),
                "Registration is required but this libpurple doesn't support input requests");
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
                                // TRANSLATOR: Connection error message after failed registration.
                                _("Display name is required for registration"));
    else
        self->m_transceiver.sendQuery(td::td_api::make_object<td::td_api::registerUser>(firstName, lastName),
                                      &PurpleTdClient::authResponse);
}

void PurpleTdClient::displayNameCancelled(PurpleTdClient *self)
{
    purple_connection_error(purple_account_get_connection(self->m_account),
                            // TRANSLATOR: Connection error message after failed registration.
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
        // TRANSLATOR: Connection error message, argument is text (a proper reason)
        message = _("Error applying database encryption key: {}");
        break;
    default:
        // TRANSLATOR: Connection error message, argument is text (a proper reason)
        message = _("Authentication error: {}");
        break;
    }

    message = formatMessage(message.c_str(), getDisplayedError(response));

    purple_connection_error(purple_account_get_connection(m_account), message.c_str());
}

void PurpleTdClient::setPurpleConnectionInProgress()
{
    purple_debug_misc(config::pluginId, "Connection in progress\n");
    PurpleConnection *gc = purple_account_get_connection(m_account);

    if (PURPLE_CONNECTION_IS_CONNECTED(gc))
        purple_blist_remove_account(m_account);
    purple_connection_set_state (gc, PURPLE_CONNECTING);
    purple_connection_update_progress(gc, "Connecting", 1, 2);
}

void PurpleTdClient::onLoggedIn()
{
    purple_connection_set_state (purple_account_get_connection(m_account), PURPLE_CONNECTED);

    // This query ensures an updateUser for every contact
    m_transceiver.sendQuery(td::td_api::make_object<td::td_api::getContacts>(),
                            &PurpleTdClient::getContactsResponse);
}

void PurpleTdClient::getContactsResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    purple_debug_misc(config::pluginId, "getContacts response to request %" G_GUINT64_FORMAT "\n", requestId);
    if (object && (object->get_id() == td::td_api::users::ID)) {
        m_data.setContacts(*td::move_tl_object_as<td::td_api::users>(object));
        auto getChatsRequest = td::td_api::make_object<td::td_api::loadChats>();
        getChatsRequest->chat_list_ = td::td_api::make_object<td::td_api::chatListMain>();
        getChatsRequest->limit_ = 200;
        m_transceiver.sendQuery(std::move(getChatsRequest), &PurpleTdClient::getChatsResponse);
    } else
        notifyAuthError(object);
}

void PurpleTdClient::getChatsResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    purple_debug_misc(config::pluginId, "getChats response to request %" G_GUINT64_FORMAT "\n", requestId);
    if (object && (object->get_id() == td::td_api::ok::ID)) {
        auto getChatsRequest = td::td_api::make_object<td::td_api::loadChats>();
        getChatsRequest->chat_list_ = td::td_api::make_object<td::td_api::chatListMain>();
        getChatsRequest->limit_ = 200;
        m_transceiver.sendQuery(std::move(getChatsRequest), &PurpleTdClient::getChatsResponse);
    } else {
        std::string message = getDisplayedError(object);
        purple_debug_misc(config::pluginId, "Got no more chats: %s\n", message.c_str());
            m_data.getContactsWithNoChat(m_usersForNewPrivateChats);
            requestMissingPrivateChats();
    }
}

void PurpleTdClient::requestMissingPrivateChats()
{
    if (m_usersForNewPrivateChats.empty()) {
        purple_debug_misc(config::pluginId, "Login sequence complete\n");
        onChatListReady();
    } else {
        UserId userId = m_usersForNewPrivateChats.back();
        m_usersForNewPrivateChats.pop_back();
        purpleDebug("Requesting private chat for user id {}", userId.value());
        td::td_api::object_ptr<td::td_api::createPrivateChat> createChat =
            td::td_api::make_object<td::td_api::createPrivateChat>(userId.value(), false);
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

void PurpleTdClient::requestBasicGroupFullInfo(BasicGroupId groupId)
{
    if (!m_data.isBasicGroupInfoRequested(groupId)) {
        m_data.setBasicGroupInfoRequested(groupId);
        uint64_t requestId = m_transceiver.sendQuery(td::td_api::make_object<td::td_api::getBasicGroupFullInfo>(groupId.value()),
                                                     &PurpleTdClient::groupInfoResponse);
        m_data.addPendingRequest<GroupInfoRequest>(requestId, groupId);
    }
}

void PurpleTdClient::requestSupergroupFullInfo(SupergroupId groupId)
{
    if (!m_data.isSupergroupInfoRequested(groupId)) {
        m_data.setSupergroupInfoRequested(groupId);
        uint64_t requestId = m_transceiver.sendQuery(td::td_api::make_object<td::td_api::getSupergroupFullInfo>(groupId.value()),
                                                     &PurpleTdClient::supergroupInfoResponse);
        m_data.addPendingRequest<SupergroupInfoRequest>(requestId, groupId);

        auto getMembersReq = td::td_api::make_object<td::td_api::getSupergroupMembers>();
        getMembersReq->supergroup_id_ = groupId.value();
        getMembersReq->filter_ = td::td_api::make_object<td::td_api::supergroupMembersFilterRecent>();
        getMembersReq->limit_ = SUPERGROUP_MEMBER_LIMIT;
        requestId = m_transceiver.sendQuery(std::move(getMembersReq), &PurpleTdClient::supergroupMembersResponse);
        m_data.addPendingRequest<SupergroupInfoRequest>(requestId, groupId);
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
    std::unique_ptr<SupergroupInfoRequest> request = m_data.getPendingRequest<SupergroupInfoRequest>(requestId);

    if (request && object && (object->get_id() == td::td_api::supergroupFullInfo::ID)) {
        td::td_api::object_ptr<td::td_api::supergroupFullInfo> groupInfo =
            td::move_tl_object_as<td::td_api::supergroupFullInfo>(object);
        updateSupergroupFull(request->groupId, std::move(groupInfo));
    }
}

void PurpleTdClient::supergroupMembersResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    std::unique_ptr<SupergroupInfoRequest> request = m_data.getPendingRequest<SupergroupInfoRequest>(requestId);

    if (request && object && (object->get_id() == td::td_api::chatMembers::ID)) {
        td::td_api::object_ptr<td::td_api::chatMembers> members =
            td::move_tl_object_as<td::td_api::chatMembers>(object);

        auto getMembersReq = td::td_api::make_object<td::td_api::getSupergroupMembers>();
        getMembersReq->supergroup_id_ = request->groupId.value();
        getMembersReq->filter_ = td::td_api::make_object<td::td_api::supergroupMembersFilterAdministrators>();
        getMembersReq->limit_ = SUPERGROUP_MEMBER_LIMIT;
        uint64_t newRequestId = m_transceiver.sendQuery(std::move(getMembersReq), &PurpleTdClient::supergroupAdministratorsResponse);
        m_data.addPendingRequest<GroupMembersRequestCont>(newRequestId, request->groupId, members.release());
    }
}

void PurpleTdClient::supergroupAdministratorsResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    std::unique_ptr<GroupMembersRequestCont> request = m_data.getPendingRequest<GroupMembersRequestCont>(requestId);
    if (request) {
        auto members = std::move(request->members);

        if (object && (object->get_id() == td::td_api::chatMembers::ID)) {
            td::td_api::object_ptr<td::td_api::chatMembers> newMembers =
                td::move_tl_object_as<td::td_api::chatMembers>(object);
            for (auto &pNewMember: newMembers->members_) {
                if (! pNewMember || !pNewMember->member_id_) continue;
                const td::td_api::MessageSender *pNewMemberInfo = pNewMember->member_id_.get();
                if (std::find_if(members->members_.begin(), members->members_.end(),
                              [pNewMemberInfo](const td::td_api::object_ptr<td::td_api::chatMember> &pExistingMember) {
                                  return pExistingMember && pExistingMember->member_id_ &&
                                         isSameUser(*pExistingMember->member_id_, *pNewMemberInfo);
                              }) == members->members_.end())
                {
                    members->members_.push_back(std::move(pNewMember));
                }
            }
        }

        const td::td_api::chat *chat = m_data.getSupergroupChatByGroup(request->groupId);
        if (chat) {
            PurpleConvChat *purpleChat = findChatConversation(m_account, *chat);
            if (purpleChat)
                updateSupergroupChatMembers(purpleChat, *members, m_data);
        }

        m_data.updateSupergroupMembers(request->groupId, std::move(members));
    }
}

void PurpleTdClient::updateGroupFull(BasicGroupId groupId, td::td_api::object_ptr<td::td_api::basicGroupFullInfo> groupInfo)
{
    const td::td_api::chat *chat = m_data.getBasicGroupChatByGroup(groupId);

    if (chat) {
        PurpleConvChat *purpleChat = findChatConversation(m_account, *chat);
        if (purpleChat)
            updateChatConversation(purpleChat, *groupInfo, m_data);
    }

    m_data.updateBasicGroupInfo(groupId, std::move(groupInfo));
}

void PurpleTdClient::updateSupergroupFull(SupergroupId groupId, td::td_api::object_ptr<td::td_api::supergroupFullInfo> groupInfo)
{
    const td::td_api::chat *chat = m_data.getSupergroupChatByGroup(groupId);

    if (chat) {
        PurpleConvChat *purpleChat = findChatConversation(m_account, *chat);
        if (purpleChat)
            updateChatConversation(purpleChat, *groupInfo, m_data);
    }

    m_data.updateSupergroupInfo(groupId, std::move(groupInfo));
}

void PurpleTdClient::onChatListReady()
{
    m_chatListReady = true;
    std::vector<const td::td_api::chat *> chats;
    m_data.getChats(chats);

    for (const td::td_api::chat *chat: chats) {
        const td::td_api::user *user = m_data.getUserByPrivateChat(*chat);
        if (user && isChatInContactList(*chat, user)) {
            std::string userName = getPurpleBuddyName(*user);
            purple_prpl_got_user_status(m_account, userName.c_str(),
                                        getPurpleStatusId(*user->status_), NULL);
        }
    }

    for (PurpleRoomlist *roomlist: m_pendingRoomLists) {
        populateGroupChatList(roomlist, chats, m_data);
        purple_roomlist_unref(roomlist);
    }
    m_pendingRoomLists.clear();

    // Here we could remove buddies for which no private chat exists, meaning they have been remove
    // from the contact list perhaps in another client

    const td::td_api::user *selfInfo = m_data.getUserByPhone(purple_account_get_username(m_account));
    if (selfInfo != nullptr) {
        std::string alias = makeBasicDisplayName(*selfInfo);
        purple_debug_misc(config::pluginId, "Setting own alias to '%s'\n", alias.c_str());
        purple_account_set_alias(m_account, alias.c_str());
    } else
        purple_debug_warning(config::pluginId, "Did not receive user information for self (%s) at login\n",
            purple_account_get_username(m_account));

    purple_blist_add_account(m_account);
}

void PurpleTdClient::onAnimatedStickerConverted(AccountThread *arg)
{
    std::unique_ptr<AccountThread> baseThread(arg);
    StickerConversionThread *thread = dynamic_cast<StickerConversionThread *>(arg);
    const td::td_api::chat  *chat   = thread ? m_data.getChat(thread->chatId) : nullptr;
    if (!chat || !thread)
        return;
    IncomingMessage *pendingMessage = m_data.pendingMessages.findPendingMessage(getId(*chat), thread->message().id);

    std::string  errorMessage = thread->getErrorMessage();
    gchar       *imageData    =  NULL;
    gsize        imageSize    = 0;
    bool         success      = false;
    if (errorMessage.empty()) {
        GError *error = NULL;

        g_file_get_contents(thread->getOutputFileName().c_str(), &imageData, &imageSize, &error);
        if (error) {
            // unlikely error message not worth translating
            errorMessage = formatMessage("Could not read converted file {}: {}", {
                                            thread->getOutputFileName(), error->message});
            g_error_free(error);
        } else
            success = true;
        remove(thread->getOutputFileName().c_str());
    }

    if (success) {
        int id = purple_imgstore_add_with_id (imageData, imageSize, NULL);
        if (pendingMessage) {
            pendingMessage->animatedStickerConverted = true;
            pendingMessage->animatedStickerConvertSuccess = true;
            pendingMessage->animatedStickerImageId = id;
            checkMessageReady(pendingMessage, m_transceiver, m_data);
            pendingMessage = nullptr;
        } else {
            std::string text = makeInlineImageText(id);
            showMessageText(m_data, *chat, thread->message(), text.c_str(), NULL, PURPLE_MESSAGE_IMAGES);
        }
    } else {
        if (pendingMessage) {
            pendingMessage->animatedStickerConverted = true;
            pendingMessage->animatedStickerConvertSuccess = false;
            checkMessageReady(pendingMessage, m_transceiver, m_data);
            pendingMessage = nullptr;
        }
        // TRANSLATOR: In-chat error message, arguments will be a file name and a proper reason
        errorMessage = formatMessage(_("Could not read sticker file {0}: {1}"),
                                        {thread->inputFileName, errorMessage});
        errorMessage = makeNoticeWithSender(*chat, thread->message(), errorMessage.c_str(), m_account);
        showMessageText(m_data, *chat, thread->message(), NULL, errorMessage.c_str());
    }
}

void PurpleTdClient::sendReadReceipts(PurpleConversation *conversation)
{
    if (conversation != NULL) {
        sendConversationReadReceipts(m_data, conversation);
        return;
    }
}

void PurpleTdClient::onIncomingMessage(td::td_api::object_ptr<td::td_api::message> message)
{
    if (!message)
        return;
    ChatId chatId = getChatId(*message);

    auto pGap = std::find_if(m_chatGaps.begin(), m_chatGaps.end(),
                             [chatId](const ChatGap &gap) { return (gap.chatId == chatId); });
    if (pGap != m_chatGaps.end()) {
        MessageId lastMessageId = pGap->lastMessage;
        m_chatGaps.erase(pGap);
        purple_debug_misc(config::pluginId,
            "Fetching skipped messages for chat %" G_GINT64_FORMAT
            " between %" G_GINT64_FORMAT " and %" G_GINT64_FORMAT "\n",
            chatId.value(), lastMessageId.value(), getId(*message).value());
        fetchHistory(m_data, chatId, getId(*message), lastMessageId);
    }

    const td::td_api::chat *chat = m_data.getChat(chatId);
    if (!chat) {
        purple_debug_warning(config::pluginId, "Received message with unknown chat id %" G_GINT64_FORMAT "\n",
                            message->chat_id_);
        return;
    }

    handleIncomingMessage(m_data, *chat, std::move(message), PendingMessageQueue::Append);
}

void PurpleTdClient::updateChatLastMessage(td::td_api::updateChatLastMessage &lastMessage)
{
    ChatId chatId = getChatId(lastMessage);
    for (auto &chatPosition: lastMessage.positions_)
        m_data.updateChatPosition(chatId, std::move(chatPosition));
    if (lastMessage.last_message_)
        saveChatLastMessage(m_data, chatId, getId(*lastMessage.last_message_));
    else {
        MessageId lastMessageId = getChatLastMessage(m_data, chatId);
        if (lastMessageId.valid()) {
            purple_debug_misc(config::pluginId,
                "Skipped messages detected for chat %" G_GINT64_FORMAT
                ", last seen message %" G_GINT64_FORMAT "\n",
                chatId.value(), lastMessageId.value());
            if (std::find_if(m_chatGaps.begin(), m_chatGaps.end(),
                             [chatId](const ChatGap &gap) {
                                 return (gap.chatId == chatId);
                             }) == m_chatGaps.end()) {
                m_chatGaps.emplace_back();
                m_chatGaps.back().chatId = chatId;
                m_chatGaps.back().lastMessage = lastMessageId;
            }
        }
    }
}

int PurpleTdClient::sendMessage(const char *buddyName, const char *message)
{
    SecretChatId secretChatId           = purpleBuddyNameToSecretChatId(buddyName);
    const td::td_api::user *privateUser = nullptr;
    const td::td_api::chat *chat        = nullptr;

    if (secretChatId.valid()) {
        chat = m_data.getChatBySecretChat(secretChatId);
        if (!chat) {
            showMessageTextIm(m_data, buddyName, NULL, "Secret chat not found", time(NULL), PURPLE_MESSAGE_ERROR);
            return -1;
        }
    } else {
        std::vector<const td::td_api::user *> users = getUsersByPurpleName(buddyName, m_data, "send message");
        if (users.size() != 1) {
            // Unlikely error messages not worth translating
            std::string errorMessage;
            if (users.empty())
                errorMessage = "User not found";
            else
                errorMessage = formatMessage("More than one user known with name '{}'", std::string(buddyName));
            showMessageTextIm(m_data, buddyName, NULL, errorMessage.c_str(), time(NULL), PURPLE_MESSAGE_ERROR);
            return -1;
        }
        privateUser = users[0];
        chat = m_data.getPrivateChatByUserId(getId(*privateUser));
    }

    if (chat) {
        int ret = transmitMessage(getId(*chat), message, m_transceiver, m_data, &PurpleTdClient::sendMessageResponse);
        if (ret < 0)
            return ret;
        // Message shall not be echoed: tdlib will shortly present it as a new message and it will be displayed then
        return 0;
    } else if (privateUser) {
        purpleDebug("Requesting private chat for user id {}", privateUser->id_);
        td::td_api::object_ptr<td::td_api::createPrivateChat> createChat =
            td::td_api::make_object<td::td_api::createPrivateChat>(privateUser->id_, false);
        uint64_t requestId = m_transceiver.sendQuery(std::move(createChat), &PurpleTdClient::sendMessageCreatePrivateChatResponse);
        m_data.addPendingRequest<NewPrivateChatForMessage>(requestId, buddyName, message);
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
        // TRANSLATOR: In-chat error message, argument will be a user-sent message
        std::string errorMessage = formatMessage(_("Failed to send message: {}"), getDisplayedError(object));
        const td::td_api::chat *chat = m_data.getChat(request->chatId);
        if (chat)
            showChatNotification(m_data, *chat, errorMessage.c_str());
    }
}

void PurpleTdClient::sendTyping(const char *buddyName, bool isTyping)
{
    const td::td_api::chat *chat = nullptr;
    SecretChatId secretChatId = purpleBuddyNameToSecretChatId(buddyName);
    if (secretChatId.valid())
        chat = m_data.getChatBySecretChat(secretChatId);
    else {
        std::vector<const td::td_api::user *> users = getUsersByPurpleName(buddyName, m_data, "send typing notification");
        if (users.size() == 1)
            chat = m_data.getPrivateChatByUserId(getId(*users[0]));
    }

    if (chat) {
        auto sendAction = td::td_api::make_object<td::td_api::sendChatAction>();
        sendAction->chat_id_ = chat->id_;
        if (isTyping)
            sendAction->action_ = td::td_api::make_object<td::td_api::chatActionTyping>();
        else
            sendAction->action_ = td::td_api::make_object<td::td_api::chatActionCancel>();
        m_transceiver.sendQuery(std::move(sendAction), nullptr);
    }
}

void PurpleTdClient::updateUserStatus(UserId userId, td::td_api::object_ptr<td::td_api::UserStatus> status)
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

    UserId userId = getId(*userInfo);
    m_data.updateUser(std::move(userInfo));

    // For chats, find_chat doesn't work if account is not yet connected, so just in case, don't
    // user find_buddy either.
    // Updates are only supposed to come after authorizationStateReady which sets account to connected.
    // But check purple_account_is_connected just in case.
    if (purple_account_is_connected(m_account)) {
        const td::td_api::user *user = m_data.getUser(userId);
        const td::td_api::chat *chat = m_data.getPrivateChatByUserId(userId);

        if (user)
            updateUserInfo(*user, chat);
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
            if (request->userId.valid()) {
                m_data.updateSmallProfilePhoto(request->userId, std::move(file));
                const td::td_api::user *user = m_data.getUser(request->userId);
                const td::td_api::chat *chat = m_data.getPrivateChatByUserId(request->userId);
                if (user && chat && isChatInContactList(*chat, user))
                    updatePrivateChat(m_data, chat, *user);
            } else if (request->chatId.valid()) {
                m_data.updateSmallChatPhoto(request->chatId, std::move(file));
                const td::td_api::chat *chat = m_data.getPrivateChatByUserId(request->userId);
                if (chat && isChatInContactList(*chat, nullptr)) {
                    BasicGroupId basicGroupId = getBasicGroupId(*chat);
                    SupergroupId supergroupId = getSupergroupId(*chat);
                    if (basicGroupId.valid())
                        updateBasicGroupChat(m_data, basicGroupId);
                    if (supergroupId.valid())
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

    BasicGroupId id = getId(*group);
    m_data.updateBasicGroup(std::move(group));

    // purple_blist_find_chat doesn't work if account is not connected.
    // Updates are only supposed to come after authorizationStateReady which sets account to connected.
    // But check purple_account_is_connected just in case.
    if (purple_account_is_connected(m_account))
        updateBasicGroupChat(m_data, id);
}

void PurpleTdClient::updateSupergroup(td::td_api::object_ptr<td::td_api::supergroup> group)
{
    if (!group) {
        purple_debug_warning(config::pluginId, "updateSupergroup with null group\n");
        return;
    }

    SupergroupId id = getId(*group);
    m_data.updateSupergroup(std::move(group));

    // purple_blist_find_chat doesn't work if account is not connected.
    // Updates are only supposed to come after authorizationStateReady which sets account to connected.
    // But check purple_account_is_connected just in case.
    if (purple_account_is_connected(m_account))
        updateSupergroupChat(m_data, id);
}

void PurpleTdClient::updateChat(const td::td_api::chat *chat)
{
    if (!chat) return;

    const td::td_api::user *privateChatUser = m_data.getUserByPrivateChat(*chat);
    BasicGroupId            basicGroupId    = getBasicGroupId(*chat);
    SupergroupId            supergroupId    = getSupergroupId(*chat);
    SecretChatId            secretChatId    = getSecretChatId(*chat);
    purpleDebug("Update chat: {} private user={} basic group={} supergroup={}", {
        std::to_string(chat->id_), std::to_string(privateChatUser ? privateChatUser->id_ : 0),
        std::to_string(basicGroupId.value()), std::to_string(supergroupId.value())
    });

    // For secret chats, chat photo is same as user profile photo, so hopefully already downloaded.
    // But if not (such as when creating secret chat while downloading new photo for the user),
    // then don't bother.
    if (!privateChatUser && !secretChatId.valid())
        downloadChatPhoto(*chat);

    // For chats, find_chat doesn't work if account is not yet connected, so just in case, don't
    // user find_buddy either.
    // Updates are only supposed to come after authorizationStateReady which sets account to connected.
    // But check purple_account_is_connected just in case.
    if (!purple_account_is_connected(m_account))
        return;

    if (privateChatUser)
        updateUserInfo(*privateChatUser, chat);

    if (isChatInContactList(*chat, privateChatUser)) {
        // purple_blist_find_chat doesn't work if account is not connected
        if (basicGroupId.valid()) {
            requestBasicGroupFullInfo(basicGroupId);
            updateBasicGroupChat(m_data, basicGroupId);
        }
        if (supergroupId.valid()) {
            requestSupergroupFullInfo(supergroupId);
            updateSupergroupChat(m_data, supergroupId);
        }
    } else {
        if (basicGroupId.valid() || supergroupId.valid())
            removeGroupChat(m_account, *chat);
    }

    if (secretChatId.valid())
        updateKnownSecretChat(secretChatId, m_transceiver, m_data);
}

void PurpleTdClient::updateUserInfo(const td::td_api::user &user, const td::td_api::chat *privateChat)
{
    if (privateChat) {
        if (isChatInContactList(*privateChat, &user)) {
            downloadProfilePhoto(user);
            updatePrivateChat(m_data, privateChat, user);
        } else
            removePrivateChat(m_data, *privateChat);
    }

    // User could have renamed, or they may have become, or ceased being, libpurple buddy.
    // Update member list in all chat conversation where this user is a member.
    std::vector<std::pair<BasicGroupId, const td::td_api::basicGroupFullInfo *>> groups;
    groups = m_data.getBasicGroupsWithMember(getId(user));
    for (const auto &groupInfo: groups) {
        const td::td_api::chat *groupChat = m_data.getBasicGroupChatByGroup(groupInfo.first);
        PurpleConvChat *purpleChat = groupChat ? findChatConversation(m_account, *groupChat) : nullptr;
        if (purpleChat)
            updateChatConversation(purpleChat, *groupInfo.second, m_data);
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
    ChatId chatId = getId(*chat);
    m_data.addChat(std::move(chat));
    updateChat(m_data.getChat(chatId));
}

void PurpleTdClient::handleUserChatAction(const td::td_api::updateUserChatAction &updateChatAction)
{
    const td::td_api::chat *chat = m_data.getChat(getChatId(updateChatAction));
    if (!chat) {
        purple_debug_warning(config::pluginId, "Got user chat action for unknown chat %" G_GINT64_FORMAT "\n",
                             updateChatAction.chat_id_);
        return;
    }

    UserId chatUserId = getUserIdByPrivateChat(*chat);
    if (!chatUserId.valid()) {
        purple_debug_misc(config::pluginId, "Ignoring user chat action for non-private chat %" G_GINT64_FORMAT "\n",
                          updateChatAction.chat_id_);
        return;
    }

    if (chatUserId != getUserId(updateChatAction)) {
        purpleDebug("Got user action for private chat {} (with user {}) for another user {}", {
            std::to_string(updateChatAction.chat_id_), std::to_string(chatUserId.value()),
            std::to_string(updateChatAction.user_id_)
        });
    } else if (updateChatAction.action_) {
        if (updateChatAction.action_->get_id() == td::td_api::chatActionCancel::ID) {
            purpleDebug("User (id {}) stopped chat action", updateChatAction.user_id_);
            showUserChatAction(getUserId(updateChatAction), false);
        } else if (updateChatAction.action_->get_id() == td::td_api::chatActionStartPlayingGame::ID) {
            purpleDebug("User (id %d): treating chatActionStartPlayingGame as cancel",
                        updateChatAction.user_id_);
            showUserChatAction(getUserId(updateChatAction), false);
        } else {
            purpleDebug("User (id {}) started chat action (id {})", {
                std::to_string(updateChatAction.user_id_), std::to_string(updateChatAction.action_->get_id())
            });
            showUserChatAction(getUserId(updateChatAction), true);
        }
    }
}

void PurpleTdClient::showUserChatAction(UserId userId, bool isTyping)
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
    // TRANSLATOR: Error dialog, content
    std::string message = formatMessage(_("Failed to add contact: {}"), errorMessage);
    // TRANSLATOR: Error dialog, title
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
        notifyFailedContactDeferred(formatMessage("More than one user known with name '{}'", purpleName));
        return;
    }

    if (users.size() == 1)
        addContactById(getId(*users[0]), "", purpleName, groupName);
    else if (isPhoneNumber(purpleName.c_str())) {
        td::td_api::object_ptr<td::td_api::contact> contact =
            td::td_api::make_object<td::td_api::contact>(purpleName, "", "", "", 0);
        td::td_api::object_ptr<td::td_api::importContacts> importReq =
            td::td_api::make_object<td::td_api::importContacts>();
        importReq->contacts_.push_back(std::move(contact));
        uint64_t requestId = m_transceiver.sendQuery(std::move(importReq),
                                                     &PurpleTdClient::importContactResponse);

        m_data.addPendingRequest<ContactRequest>(requestId, purpleName, alias, groupName, UserId::invalid);
    } else {
        auto     request   = td::td_api::make_object<td::td_api::searchPublicChat>(purpleName);
        uint64_t requestId = m_transceiver.sendQuery(std::move(request), &PurpleTdClient::addBuddySearchChatResponse);
        m_data.addPendingRequest<ContactRequest>(requestId, "", alias, groupName, UserId::invalid);
    }
}

void PurpleTdClient::addBuddySearchChatResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    std::unique_ptr<ContactRequest> request = m_data.getPendingRequest<ContactRequest>(requestId);

    if (object && (object->get_id() == td::td_api::chat::ID)) {
        const td::td_api::chat *chat = static_cast<const td::td_api::chat *>(object.get());
        int32_t chatType = chat->type_ ? chat->type_->get_id() : 0;
        if (chatType == td::td_api::chatTypePrivate::ID) {
            if (request)
                addContactById(getUserIdByPrivateChat(*chat), "", request->alias, request->groupName);
        } else if ((chatType == td::td_api::chatTypeBasicGroup::ID) ||
                   (chatType == td::td_api::chatTypeSupergroup::ID))
        {
            // When trying to join a group but finding a user instead, we display an error message.
            // When it's vice versa here, don't make it an error: there are enough error messages to
            // translate as it is.
            joinGroupSearchChatResponse(requestId, std::move(object));
            chat = NULL;
        }
    } else
        notifyFailedContact(getDisplayedError(object));
}

void PurpleTdClient::addContactById(UserId userId, const std::string &phoneNumber, const std::string &alias,
                                    const std::string &groupName)
{
    purpleDebug("Adding contact: id={} alias={}", {std::to_string(userId.value()), alias});
    std::string firstName, lastName;
    getNamesFromAlias(alias.c_str(), firstName, lastName);

    td::td_api::object_ptr<td::td_api::contact> contact =
        td::td_api::make_object<td::td_api::contact>(phoneNumber, firstName, lastName, "", userId.value());
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

    UserId userId = UserId::invalid;
    if (object && (object->get_id() == td::td_api::importedContacts::ID)) {
        td::td_api::object_ptr<td::td_api::importedContacts> reply =
            td::move_tl_object_as<td::td_api::importedContacts>(object);
        if (!reply->user_ids_.empty())
            userId = getUserId(*reply, 0);
    }

    if (userId.valid())
        addContactById(userId, request->phoneNumber, request->alias, request->groupName);
    else {
        // TRANSLATOR: Buddy-window error message, title (no content), argument will be a phone number.
        notifyFailedContact(formatMessage(_("No user found with phone number '{}'"), request->phoneNumber));
    }
}

void PurpleTdClient::addContactResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    std::unique_ptr<ContactRequest> request = m_data.getPendingRequest<ContactRequest>(requestId);
    if (!request)
        return;

    if (object && (object->get_id() == td::td_api::ok::ID)) {
        td::td_api::object_ptr<td::td_api::createPrivateChat> createChat =
            td::td_api::make_object<td::td_api::createPrivateChat>(request->userId.value(), false);
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

    if (object && (object->get_id() == td::td_api::chat::ID)) {
        const td::td_api::chat &chat = static_cast<const td::td_api::chat &>(*object);
        const td::td_api::user *user = m_data.getUserByPrivateChat(chat);
        if (user && !isChatInContactList(chat, user)) {
            // Normally, the user will become a contact and this won't happen. But it does happen
            // when adding BotFather, for example. Nothing will be added to buddy list, so open chat
            // window just to make something happen.
            // Since the user is not in buddy list, we have to use display name as libpurple name,
            // otherwise conversation title will be idXXXXXXX
            std::string displayName = m_data.getDisplayName(*user);
            getImConversation(m_account, displayName.c_str());
        }
    } else {
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
    UserId userId = purpleBuddyNameToUserId(buddyName);
    if (!userId.valid()) {
        purple_debug_warning(config::pluginId, "Cannot rename %s: not a valid id\n", buddyName);
        return;
    }

    std::string firstName, lastName;
    getNamesFromAlias(newAlias, firstName, lastName);
    auto contact    = td::td_api::make_object<td::td_api::contact>("", firstName, lastName, "", userId.value());
    auto addContact = td::td_api::make_object<td::td_api::addContact>(std::move(contact), true);
    m_transceiver.sendQuery(std::move(addContact), nullptr);
}

void PurpleTdClient::removeContactAndPrivateChat(const std::string &buddyName)
{
    const td::td_api::chat *chat         = nullptr;
    UserId                  userId       = purpleBuddyNameToUserId(buddyName.c_str());
    SecretChatId            secretChatId = purpleBuddyNameToSecretChatId(buddyName.c_str());

    if (userId.valid())
        chat = m_data.getPrivateChatByUserId(userId);
    else if (secretChatId.valid())
        chat = m_data.getChatBySecretChat(secretChatId);

    if (chat) {
        ChatId chatId = getId(*chat);
        chat = nullptr;
        // Prevent accidentally re-creating buddy if any updateChat* or updateUser arrives
        m_data.deleteChat(chatId);

        auto deleteChat = td::td_api::make_object<td::td_api::deleteChatHistory>();
        deleteChat->chat_id_ = chatId.value();
        deleteChat->remove_from_chat_list_ = true;
        deleteChat->revoke_ = false;
        m_transceiver.sendQuery(std::move(deleteChat), nullptr);
    }

    if (userId.valid()) {
        auto removeContact = td::td_api::make_object<td::td_api::removeContacts>();
        removeContact->user_ids_.push_back(userId.value());
        m_transceiver.sendQuery(std::move(removeContact), nullptr);
    }

    if (secretChatId.valid()) {
        auto closeChat = td::td_api::make_object<td::td_api::closeSecretChat>(secretChatId.value());
        m_transceiver.sendQuery(std::move(closeChat), nullptr);
    }
}

void PurpleTdClient::getUsers(const char *username, std::vector<const td::td_api::user *> &users)
{
    users = getUsersByPurpleName(username, m_data, NULL);
}

bool PurpleTdClient::joinChat(const char *chatName)
{
    ChatId                  id       = getTdlibChatId(chatName);
    const td::td_api::chat *chat     = m_data.getChat(id);
    int32_t                 purpleId = m_data.getPurpleChatId(id);
    PurpleConvChat         *conv     = NULL;

    if (!chat) {
        // Either the user is actively trying to join non-existent chat (for example by entering
        // a chat ID when joining with pidgin), or this is the pidgin auto-rejoin that usually
        // happens before we get info about telegram chats.
        // Check if the latter is the case and if it is, schedule to rejoin when telegram chat appears
        PurpleConversation *baseConv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT,
                                                                             chatName, m_account);
        if (baseConv && purple_conv_chat_has_left(purple_conversation_get_chat_data(baseConv))) {
            purple_debug_misc(config::pluginId, "Scheduling to rejoin group chat %s - "
                              "no telegram chat found at the moment\n", chatName);
            m_data.addExpectedChat(id);
        } else
            purple_debug_warning(config::pluginId, "No telegram chat found for purple name %s\n", chatName);
    } else if (!m_data.isGroupChatWithMembership(*chat))
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
        int ret = transmitMessage(getId(*chat), message, m_transceiver, m_data, &PurpleTdClient::sendMessageResponse);
        if (ret < 0)
            return ret;
        return 0;
    }

    return -1;
}

void PurpleTdClient::joinChatByInviteLink(const char *inviteLink)
{
    auto     request   = td::td_api::make_object<td::td_api::joinChatByInviteLink>(inviteLink);
    uint64_t requestId = m_transceiver.sendQuery(std::move(request), &PurpleTdClient::joinChatResponse);
    m_data.addPendingRequest<GroupJoinRequest>(requestId, inviteLink, GroupJoinRequest::Type::InviteLink);
}

void PurpleTdClient::joinChatResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    std::unique_ptr<GroupJoinRequest> request = m_data.getPendingRequest<GroupJoinRequest>(requestId);
    if (object && ((object->get_id() == td::td_api::chat::ID) || (object->get_id() == td::td_api::ok::ID))) {
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
            if (!request->joinString.empty()) {
                std::vector<PurpleChat *> obsoleteChats = findChatsByJoinString(request->joinString);
                for (PurpleChat *chat: obsoleteChats)
                    purple_blist_remove_chat(chat);
            }
            // Conversation window for the chat should be presented. If joining by invite link, it
            // will happen automatically due to messageChatJoinByLink message. If joining a public
            // group, conversation window needs to be created explicitly instead
            if (request->type != GroupJoinRequest::Type::InviteLink) {
                const td::td_api::chat *chat     = m_data.getChat(request->chatId);
                int32_t                 purpleId = m_data.getPurpleChatId(request->chatId);
                if (chat)
                    getChatConversation(m_data, *chat, purpleId);
            }
        }
    } else {
        // TRANSLATOR: Error dialog, content
        std::string message = formatMessage(_("Failed to join chat: {}"), getDisplayedError(object));
        // TRANSLATOR: Error dialog, title
        purple_notify_error(purple_account_get_connection(m_account), _("Failed to join chat"),
                            message.c_str(), NULL);
    }
}

void PurpleTdClient::joinChatByGroupName(const char *joinString, const char *groupName)
{
    auto     request   = td::td_api::make_object<td::td_api::searchPublicChat>(groupName);
    uint64_t requestId = m_transceiver.sendQuery(std::move(request), &PurpleTdClient::joinGroupSearchChatResponse);
    m_data.addPendingRequest<GroupJoinRequest>(requestId, joinString, GroupJoinRequest::Type::Username);
}

void PurpleTdClient::joinGroupSearchChatResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    std::unique_ptr<GroupJoinRequest> request = m_data.getPendingRequest<GroupJoinRequest>(requestId);
    if (object && (object->get_id() == td::td_api::chat::ID)) {
        const td::td_api::chat &chat = static_cast<const td::td_api::chat &>(*object);
        if (chat.type_ && ((chat.type_->get_id() == td::td_api::chatTypeBasicGroup::ID) ||
                           (chat.type_->get_id() == td::td_api::chatTypeSupergroup::ID))) {
            auto     joinRequest = td::td_api::make_object<td::td_api::joinChat>(chat.id_);
            uint64_t requestId   = m_transceiver.sendQuery(std::move(joinRequest), &PurpleTdClient::joinChatResponse);
            m_data.addPendingRequest<GroupJoinRequest>(requestId, request ? request->joinString : std::string(),
                                                       GroupJoinRequest::Type::Username, getId(chat));
        } else
            // TRANSLATOR: Error dialog, title
            purple_notify_error(purple_account_get_connection(m_account), _("Failed to join chat"),
                                // TRANSLATOR: Error dialog, content
                                _("The name belongs to a user, not a group"), NULL);
    } else {
        // TRANSLATOR: Error dialog, content, argument is a reason (text)
        std::string message = formatMessage(_("Could not find group: {}"), getDisplayedError(object));
        // TRANSLATOR: Error dialog, title
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
        if (basicGroupMembers.empty()) {
            // TRANSLATOR: Error dialog, secondary content
            errorMessage = _("Cannot create basic group without additional members");
        }
        for (const std::string &memberName: basicGroupMembers) {
            UserId userId = purpleBuddyNameToUserId(memberName.c_str());
            if (userId.valid()) {
                if (!m_data.getUser(userId)) {
                    errorMessage = formatMessage(_("No known user with id {}"), userId);
                }
            } else {
                std::vector<const td::td_api::user*> users;
                m_data.getUsersByDisplayName(memberName.c_str(), users);
                if (users.size() == 1)
                    userId = getId(*users[0]);
                else if (users.empty()) {
                    // TRANSLATOR: Error dialog, secondary content, argument is a username
                    errorMessage = formatMessage(_("No known user by the name '{}'"), memberName);
                } else {
                    // Unlikely error message not worth translating
                    errorMessage = formatMessage("More than one user known with name '{}'", memberName);
                }
            }
            if (!errorMessage.empty())
                break;
            createRequest->user_ids_.push_back(userId.value());
        }

        if (!errorMessage.empty())
            purple_notify_error(purple_account_get_connection(m_account),
                                // TRANSLATOR: Error dialog, title
                                _("Failed to create group"),
                                // TRANSLATOR: Error dialog, primary content
                                _("Invalid group members"),
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
    ChatId                        chatId     = getTdlibChatId(purpleChatName);
    const td::td_api::chat       *chat       = chatId.valid() ? m_data.getChat(chatId) : nullptr;
    BasicGroupId                  groupId    = chat ? getBasicGroupId(*chat) : BasicGroupId::invalid;
    const td::td_api::basicGroup *basicGroup = groupId.valid() ? m_data.getBasicGroup(groupId) : nullptr;

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
    ChatId                  chatId = getTdlibChatId(purpleChatName.c_str());
    const td::td_api::chat *chat   = chatId.valid() ? m_data.getChat(chatId) : nullptr;
    if (!chat) return;

    SupergroupId supergroupId = getSupergroupId(*chat);
    if (deleteSupergroup && supergroupId.valid()) {
        m_transceiver.sendQuery(td::td_api::make_object<td::td_api::deleteChat>(supergroupId.value()),
                                &PurpleTdClient::deleteSupergroupResponse);
    } else {
        m_transceiver.sendQuery(td::td_api::make_object<td::td_api::leaveChat>(chatId.value()), nullptr);
        auto deleteChatRequest = td::td_api::make_object<td::td_api::deleteChatHistory>();
        deleteChatRequest->chat_id_ = chatId.value();
        deleteChatRequest->remove_from_chat_list_ = true;
        deleteChatRequest->revoke_ = false;
        m_transceiver.sendQuery(std::move(deleteChatRequest), nullptr);
    }
}

void PurpleTdClient::deleteSupergroupResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    if (!object || (object->get_id() != td::td_api::ok::ID)) {
        std::string errorMessage = getDisplayedError(object).c_str();
        purple_notify_error(m_account,
                            // TRANSLATOR: Error dialog, title
                            _("Failed to delete group or channel"),
                            errorMessage.c_str(), NULL);
    }
}

void PurpleTdClient::setGroupDescription(int purpleChatId, const char *description)
{
    const td::td_api::chat *chat = m_data.getChatByPurpleId(purpleChatId);
    if (!chat) {
        purple_debug_warning(config::pluginId, "Unknown libpurple chat id %d\n", purpleChatId);
        return;
    }

    if (getBasicGroupId(*chat).valid() || getSupergroupId(*chat).valid()) {
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
        purple_notify_error(m_account,
                            // TRANSLATOR: Error dialog, title
                            _("Failed to set group description"),
                            message.c_str(), NULL);
    }
}

void PurpleTdClient::kickUserFromChat(PurpleConversation *conv, const char *name)
{
    int purpleChatId = purple_conv_chat_get_id(PURPLE_CONV_CHAT(conv));
    const td::td_api::chat *chat = m_data.getChatByPurpleId(purpleChatId);

    if (!chat) {
        // Unlikely error message not worth translating
        purple_conversation_write(conv, "", "Chat not found", PURPLE_MESSAGE_NO_LOG, time(NULL));
        return;
    }

    std::vector<const td::td_api::user *> users = getUsersByPurpleName(name, m_data, "kick user");
    if (users.size() != 1) {
        // TRANSLATOR: In-chat error message, appears after a colon (':')
        const char *reason = users.empty() ? _("User not found") :
                                             // Unlikely error message not worth translating
                                             "More than one user found with this name";
        // TRANSLATOR: In-chat error message, argument is a reason (text)
        std::string message = formatMessage(_("Cannot kick user: {}"), std::string(reason));
        purple_conversation_write(conv, "", message.c_str(), PURPLE_MESSAGE_NO_LOG, 0);
        return;
    }

    auto setStatusRequest = td::td_api::make_object<td::td_api::setChatMemberStatus>();
    setStatusRequest->chat_id_ = chat->id_;
    setStatusRequest->member_id_ = td::td_api::make_object<td::td_api::messageSenderUser>(users[0]->id_);
    setStatusRequest->status_ = td::td_api::make_object<td::td_api::chatMemberStatusLeft>();

    uint64_t requestId = m_transceiver.sendQuery(std::move(setStatusRequest), &PurpleTdClient::chatActionResponse);
    m_data.addPendingRequest<ChatActionRequest>(requestId, ChatActionRequest::Type::Kick, getId(*chat));
}

void PurpleTdClient::chatActionResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    std::unique_ptr<ChatActionRequest> request = m_data.getPendingRequest<ChatActionRequest>(requestId);
    if (!request) return;

    int32_t expectedId = 0;
    switch (request->type) {
        case ChatActionRequest::Type::Kick:
        case ChatActionRequest::Type::Invite:
            expectedId = td::td_api::ok::ID;
            break;
        case ChatActionRequest::Type::GenerateInviteLink:
            expectedId = td::td_api::chatInviteLink::ID;
            break;
    }

    if (!object || (object->get_id() != expectedId)) {
        const td::td_api::chat *chat = request ? m_data.getChat(request->chatId) : nullptr;
        if (chat) {
            std::string message = getDisplayedError(object);
            switch (request->type) {
                case ChatActionRequest::Type::Kick:
                    // TRANSLATOR: In-chat error message, argument is a reason (text)
                    message = formatMessage(_("Cannot kick user: {}"), message);
                    break;
                case ChatActionRequest::Type::Invite:
                    // TRANSLATOR: In-chat error message, argument is a reason (text)
                    message = formatMessage(_("Cannot add user to group: {}"), message);
                    break;
                case ChatActionRequest::Type::GenerateInviteLink:
                    // TRANSLATOR: In-chat error message, argument is a reason (text)
                    message = formatMessage(_("Cannot generate invite link: {}"), message);
                    break;
            }
            showChatNotification(m_data, *chat, message.c_str());
        }
    } else {
        if (request->type == ChatActionRequest::Type::GenerateInviteLink) {
            const td::td_api::chatInviteLink &inviteLink = static_cast<const td::td_api::chatInviteLink &>(*object);
            const td::td_api::chat *chat = request ? m_data.getChat(request->chatId) : nullptr;
            if (chat)
                showChatNotification(m_data, *chat, inviteLink.invite_link_.c_str());
        }
    }
}

void PurpleTdClient::addUserToChat(int purpleChatId, const char *name)
{
    const td::td_api::chat *chat = m_data.getChatByPurpleId(purpleChatId);
    if (!chat) {
        purple_debug_warning(config::pluginId, "Unknown libpurple chat id %d\n", purpleChatId);
        return;
    }

    std::vector<const td::td_api::user *> users = getUsersByPurpleName(name, m_data, "kick user");
    if (users.size() != 1) {
        // TRANSLATOR: In-chat error message, appears after a colon (':')
        const char *reason = users.empty() ? _("User not found") :
                                             // Unlikely error message not worth translating
                                             "More than one user found with this name";
        // TRANSLATOR: In-chat error message, argument is a reason (text)
        std::string message = formatMessage(_("Cannot add user to group: {}"), std::string(reason));
        showChatNotification(m_data, *chat, message.c_str(), PURPLE_MESSAGE_NO_LOG);
        return;
    }

    if (getBasicGroupId(*chat).valid() || getSupergroupId(*chat).valid()) {
        auto request = td::td_api::make_object<td::td_api::addChatMember>();
        request->chat_id_ = chat->id_;
        request->user_id_ = users[0]->id_;
        uint64_t requestId = m_transceiver.sendQuery(std::move(request), &PurpleTdClient::chatActionResponse);
        m_data.addPendingRequest<ChatActionRequest>(requestId, ChatActionRequest::Type::Invite, getId(*chat));
    }
}

void PurpleTdClient::showInviteLink(const std::string& purpleChatName)
{
    ChatId                  chatId = getTdlibChatId(purpleChatName.c_str());
    const td::td_api::chat *chat   = chatId.valid() ? m_data.getChat(chatId) : nullptr;
    if (!chat) {
        purple_debug_warning(config::pluginId, "chat %s not found\n", purpleChatName.c_str());
        return;
    }
    BasicGroupId basicGroupId = getBasicGroupId(*chat);
    SupergroupId supergroupId = getSupergroupId(*chat);
    const td::td_api::basicGroupFullInfo *basicGroupInfo = basicGroupId.valid() ? m_data.getBasicGroupInfo(basicGroupId) : nullptr;
    const td::td_api::supergroupFullInfo *supergroupInfo = supergroupId.valid() ? m_data.getSupergroupInfo(supergroupId) : nullptr;
    bool fullInfoKnown = false;
    std::string inviteLink;

    if (basicGroupId.valid()) {
        fullInfoKnown = (basicGroupInfo != nullptr);
        if (basicGroupInfo && basicGroupInfo->invite_link_ && isInviteLinkActive(*basicGroupInfo->invite_link_))
            inviteLink = basicGroupInfo->invite_link_->invite_link_;
    }
    if (supergroupId.valid()) {
        fullInfoKnown = (supergroupInfo != nullptr);
        if (supergroupInfo && supergroupInfo->invite_link_ && isInviteLinkActive(*supergroupInfo->invite_link_))
            inviteLink = supergroupInfo->invite_link_->invite_link_;
    }

    if (!inviteLink.empty())
        showChatNotification(m_data, *chat, inviteLink.c_str());
    else if (fullInfoKnown) {
        auto linkRequest = td::td_api::make_object<td::td_api::createChatInviteLink>();
        linkRequest->chat_id_ = chat->id_;
        uint64_t requestId = m_transceiver.sendQuery(std::move(linkRequest), &PurpleTdClient::chatActionResponse);
        m_data.addPendingRequest<ChatActionRequest>(requestId, ChatActionRequest::Type::GenerateInviteLink, getId(*chat));
    } else
        // Unlikely error message not worth translating
        showChatNotification(m_data, *chat, "Failed to get invite link, full info not known");
}

void PurpleTdClient::getGroupChatList(PurpleRoomlist *roomlist)
{

    GList *fields = NULL;
    PurpleRoomlistField *f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, "",
                                                       getChatNameComponent(), TRUE);
    fields = g_list_append (fields, f);
    // "description" is hard-coded in bitlbee as possible field for chat topic
    // TRANSLATOR: Groupchat infobox key
    f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, _("Description"), "description", FALSE);
    fields = g_list_append (fields, f);
    purple_roomlist_set_fields (roomlist, fields);

    purple_roomlist_set_in_progress(roomlist, TRUE);
    if (m_chatListReady) {
        std::vector<const td::td_api::chat *> chats;
        m_data.getChats(chats);
        populateGroupChatList(roomlist, chats, m_data);
    } else {
        purple_roomlist_ref(roomlist);
        m_pendingRoomLists.push_back(roomlist);
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
    // TRANSLATOR: 2FA setup confirmation dialog, secondary content, argument is an e-mail description (address and code length)
    std::string secondary = formatMessage(_("Password will be changed after new e-mail is confirmed\n{}"), emailInfo);
    PurpleConnection *gc = purple_account_get_connection(m_account);
    // TRANSLATOR: 2FA setup confirmation dialog, title
    purple_request_input(gc, _("Two-factor authentication"),
                         // TRANSLATOR: 2FA setup confirmation dialog, primary content
                         _("Enter verification code received in the e-mail"), secondary.c_str(),
                         NULL,  // default value
                         FALSE, // multiline input
                         FALSE, // masked input
                         NULL,
                         // TRANSLATOR: 2FA setup confirmation dialog, alternative is "_Cancel". The underscore marks accelerator keys, they must be different!
                         _("_OK"), G_CALLBACK(PurpleTdClient::verifyRecoveryEmail),
                         // TRANSLATOR: 2FA setup confirmation dialog, alternative is "_OK". The underscore marks accelerator keys, they must be different!
                         _("_Cancel"), G_CALLBACK(inputCancelled),
                         purple_connection_get_account(gc),
                         NULL, // buddy
                         NULL, // conversation
                         this);
}

static void notifyPasswordChangeSuccess(PurpleAccount *account, const td::td_api::passwordState &passwordState)
{
    // TRANSLATOR: 2FA success notification, title
    purple_notify_info(account, _("Two-factor authentication"),
                        // TRANSLATOR: 2FA success notification, primary content
                        passwordState.has_password_ ? _("Password set") :
                                                      // TRANSLATOR: 2FA success notification, primary content
                                                      _("Password cleared"),
                        // TRANSLATOR: 2FA success notification, secondary content
                        passwordState.has_recovery_email_address_ ? _("Recovery e-mail is configured") :
                                                                    // TRANSLATOR: 2FA success notification, secondary content
                                                                    _("No recovery e-mail configured"));
}

void PurpleTdClient::setTwoFactorAuthResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    if (object && (object->get_id() == td::td_api::passwordState::ID)) {
        const td::td_api::passwordState &passwordState = static_cast<const td::td_api::passwordState &>(*object);
        if (passwordState.recovery_email_address_code_info_) {
            // TRANSLATOR: 2FA setup confirmation dialog, e-mail description
            std::string emailInfo = formatMessage(_("Code sent to {0} (length: {1})"),
                                                  {passwordState.recovery_email_address_code_info_->email_address_pattern_,
                                                   std::to_string(passwordState.recovery_email_address_code_info_->length_)});
            requestRecoveryEmailConfirmation(emailInfo);
        } else
            notifyPasswordChangeSuccess(m_account, passwordState);
    } else {
        std::string errorMessage = getDisplayedError(object);
        // TRANSLATOR: 2FA failure notification, title
        purple_notify_error(m_account, _("Two-factor authentication"),
                            // TRANSLATOR: 2FA failure notification, primary content
                            _("Failed to set password"), errorMessage.c_str());
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
                // Not expected to happen, so not worth translating
                std::string emailInfo = formatMessage("E-mail address: {}",
                                                      passwordState.recovery_email_address_code_info_->email_address_pattern_);
                purple_notify_info(m_account, _("Two-factor authentication"),
                                   "For some reason, new confirmation code was sent", emailInfo.c_str());
            } else
                // TRANSLATOR: 2FA failure notification, title
                purple_notify_error(m_account, _("Two-factor authentication"),
                                    // TRANSLATOR: 2FA failure notification, content
                                    _("Looks like the code was wrong"), NULL);
        } else
            notifyPasswordChangeSuccess(m_account, passwordState);
    } else {
        // Shouldn't really happen, so not worth translating. The only reasonable failure is wrong
        // code, which is handled elsewhere.
        std::string errorMessage = getDisplayedError(object);
        purple_notify_error(m_account, "Two-factor authentication",
                            "Failed to verify recovery e-mail", errorMessage.c_str());
    }
}

void PurpleTdClient::sendFileToChat(PurpleXfer *xfer, const char *purpleName,
                                    PurpleConversationType type, int purpleChatId)
{
    const char *filename = purple_xfer_get_local_filename(xfer);
    const td::td_api::user *privateUser = nullptr;
    const td::td_api::chat *chat        = nullptr;

    if (type == PURPLE_CONV_TYPE_IM) {
        SecretChatId secretChatId = purpleBuddyNameToSecretChatId(purpleName);
        if (secretChatId.valid())
            chat = m_data.getChatBySecretChat(secretChatId);
        else {
            std::vector<const td::td_api::user *> users = getUsersByPurpleName(purpleName, m_data, "send message");
            if (users.size() == 1) {
                privateUser = users[0];
                chat = m_data.getPrivateChatByUserId(getId(*privateUser));
            }
        }
    } else if (type == PURPLE_CONV_TYPE_CHAT)
        chat = m_data.getChatByPurpleId(purpleChatId);

    if (filename && chat)
        startDocumentUpload(getId(*chat), filename, xfer, m_transceiver, m_data, &PurpleTdClient::uploadResponse);
    else if (filename && privateUser) {
        purple_debug_misc(config::pluginId, "Requesting private chat for user id %d\n", (int)privateUser->id_);
        td::td_api::object_ptr<td::td_api::createPrivateChat> createChat =
            td::td_api::make_object<td::td_api::createPrivateChat>(privateUser->id_, false);
        uint64_t requestId = m_transceiver.sendQuery(std::move(createChat), &PurpleTdClient::sendMessageCreatePrivateChatResponse);
        purple_xfer_ref(xfer);
        m_data.addPendingRequest<NewPrivateChatForMessage>(requestId, purpleName, xfer);
    } else {
        if (!filename)
            purple_debug_warning(config::pluginId, "Failed to send file, no file name\n");
        else if (!chat)
            purple_debug_warning(config::pluginId, "Failed to send file %s, chat not found\n", filename);
        purple_xfer_cancel_local(xfer);
    }
}

void PurpleTdClient::sendMessageCreatePrivateChatResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    std::unique_ptr<NewPrivateChatForMessage> request = m_data.getPendingRequest<NewPrivateChatForMessage>(requestId);
    if (!request) return;
    auto chat = (object && (object->get_id() == td::td_api::chat::ID)) ?
                static_cast<const td::td_api::chat *>(object.get()) : nullptr;

    if (request->fileUpload) {
        if (purple_xfer_is_canceled(request->fileUpload)) {
            // User cancelled the upload really fast
        } else if (chat) {
            const char *filename = purple_xfer_get_local_filename(request->fileUpload);
            if (filename)
                startDocumentUpload(getId(*chat), filename, request->fileUpload, m_transceiver, m_data,
                                    &PurpleTdClient::uploadResponse);
            else
                purple_xfer_cancel_local(request->fileUpload);
        } else {
            std::string message = getDisplayedError(object);
            purple_xfer_cancel_local(request->fileUpload);
            purple_xfer_error(purple_xfer_get_type(request->fileUpload), m_account,
                              request->username.c_str(), message.c_str());
        }

        purple_xfer_unref(request->fileUpload);
    } else {
        std::string errorMessage;

        if (chat) {
            int ret = transmitMessage(getId(*chat), request->message.c_str(), m_transceiver, m_data,
                                      &PurpleTdClient::sendMessageResponse);
            // Messages copied from libpurple
            if (ret == -E2BIG) {
                // TRANSLATOR: In-chat error message
                errorMessage = _("Unable to send message: The message is too large.");
            } else if (ret < 0) {
                // TRANSLATOR: In-chat error message
                errorMessage = _("Unable to send message.");
            }
        } else {
            // TRANSLATOR: In-chat(?) error message, argument is an error description (text)
            errorMessage = formatMessage(_("Failed to open chat: {}"), getDisplayedError(object));
        }

        if (!errorMessage.empty())
            showMessageTextIm(m_data, request->username.c_str(), NULL, errorMessage.c_str(),
                              time(NULL), PURPLE_MESSAGE_ERROR);
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

bool PurpleTdClient::startVoiceCall(const char *buddyName)
{
    std::vector<const td::td_api::user *> users = getUsersByPurpleName(buddyName, m_data, "start voice call");
    if (users.size() != 1) {
        // Unlikely error messages not worth translating
        std::string errorMessage;
        if (users.empty())
            errorMessage = "User not found";
        else
            errorMessage = formatMessage("More than one user known with name '{}'", std::string(buddyName));
        showMessageTextIm(m_data, buddyName, NULL, errorMessage.c_str(), time(NULL), PURPLE_MESSAGE_ERROR);
        return false;
    }

    return initiateCall(users.front()->id_, m_data, m_transceiver);
}

bool PurpleTdClient::terminateCall(PurpleConversation *conv)
{
    if (!m_data.hasActiveCall())
        return false;

    discardCurrentCall(m_data, m_transceiver);
    return true;
}

void PurpleTdClient::createSecretChat(const char* buddyName)
{
    std::vector<const td::td_api::user *> users = getUsersByPurpleName(buddyName, m_data, "create secret chat");
    if (users.size() != 1) {
        // Unlikely error messages not worth translating
        const char *reason = users.empty() ? "User not found" :
                                             "More than one user found with this name";
        std::string message = formatMessage("Cannot create secret chat: {}", std::string(reason));
        purple_notify_error(purple_account_get_connection(m_account),
                            // TRANSLATOR: Failure notification, title
                            _("Failed to create secret chat"),
                            message.c_str(), NULL);

        return;
    }

    auto request = td::td_api::make_object<td::td_api::createNewSecretChat>(getId(*users[0]).value());
    m_transceiver.sendQuery(std::move(request), nullptr);
}
