#include "td-client.h"
#include "config.h"

enum {
    // Typing notifications seems to be resent every 5-6 seconds, so 10s timeout hould be appropriate
    REMOTE_TYPING_NOTICE_TIMEOUT = 10,
};

class UpdateHandler {
public:
    UpdateHandler(PurpleTdClient *owner) : m_owner(owner) {}

    void operator()(td::td_api::updateAuthorizationState &update_authorization_state) const {
        purple_debug_misc(config::pluginId, "Incoming update: authorization state\n");
        m_owner->m_lastAuthState = update_authorization_state.authorization_state_->get_id();
        td::td_api::downcast_call(*update_authorization_state.authorization_state_, *m_owner->m_authUpdateHandler);
    }

    void operator()(td::td_api::updateConnectionState &connectionUpdate) const {
        purple_debug_misc(config::pluginId, "Incoming update: connection state\n");
        if (connectionUpdate.state_) {
            if (connectionUpdate.state_->get_id() == td::td_api::connectionStateReady::ID)
                m_owner->connectionReady();
            else if (connectionUpdate.state_->get_id() == td::td_api::connectionStateConnecting::ID)
                g_idle_add(PurpleTdClient::setPurpleConnectionInProgress, m_owner);
            else if (connectionUpdate.state_->get_id() == td::td_api::connectionStateUpdating::ID)
                g_idle_add(PurpleTdClient::setPurpleConnectionUpdating, m_owner);
        }
    }

    void operator()(td::td_api::updateUser &userUpdate) const {
        purple_debug_misc(config::pluginId, "Incoming update: update user\n");
        m_owner->updateUser(std::move(userUpdate.user_));
    }

    void operator()(td::td_api::updateNewChat &newChat) const {
        purple_debug_misc(config::pluginId, "Incoming update: new chat\n");
        TdAccountData::Lock lock(m_owner->m_data);
        m_owner->m_data.addNewChat(std::move(newChat.chat_));
    }

    void operator()(td::td_api::updateNewMessage &newMessageUpdate) const {
        purple_debug_misc(config::pluginId, "Incoming update: new message\n");
        if (newMessageUpdate.message_)
            m_owner->onIncomingMessage(std::move(newMessageUpdate.message_));
        else
            purple_debug_warning(config::pluginId, "Received null new message\n");
    }

    void operator()(td::td_api::updateUserStatus &updateStatus) const {
        purple_debug_misc(config::pluginId, "Incoming update: user status\n");
        if (updateStatus.status_)
            m_owner->updateUserStatus(updateStatus.user_id_, std::move(updateStatus.status_));
    }

    void operator()(td::td_api::updateUserChatAction &updateChatAction) const {
        purple_debug_misc(config::pluginId, "Incoming update: chat action %d\n",
            updateChatAction.action_->get_id());
        m_owner->handleUserChatAction(updateChatAction);
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

    void operator()(td::td_api::authorizationStateWaitCode &codeState) const {
        purple_debug_misc(config::pluginId, "Authorization state update: authentication code requested\n");
        m_owner->m_authCodeInfo = std::move(codeState.code_info_);
        g_idle_add(PurpleTdClient::requestAuthCode, m_owner);
    }

    void operator()(td::td_api::authorizationStateReady &) const {
        purple_debug_misc(config::pluginId, "Authorization state update: ready\n");
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
            ResponseCb callback = nullptr;
            {
                std::unique_lock<std::mutex> dataLock(m_queryMutex);
                auto it = m_responseHandlers.find(response.id);
                if (it != m_responseHandlers.end()) {
                    callback = it->second;
                    m_responseHandlers.erase(it);
                } else
                    purple_debug_misc(config::pluginId, "Ignoring response to request %llu\n",
                                      (unsigned long long)response.id);
            }
            if (callback)
                (this->*callback)(response.id, std::move(response.object));
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

static std::string getAuthCodeDesc(const td::td_api::AuthenticationCodeType &codeType)
{
    switch (codeType.get_id()) {
    case td::td_api::authenticationCodeTypeTelegramMessage::ID:
        return "Telegram message (length: " +
               std::to_string(static_cast<const td::td_api::authenticationCodeTypeTelegramMessage &>(codeType).length_) +
               ")";
    case td::td_api::authenticationCodeTypeSms::ID:
        return "SMS (length: " +
               std::to_string(static_cast<const td::td_api::authenticationCodeTypeSms &>(codeType).length_) +
               ")";
    case td::td_api::authenticationCodeTypeCall::ID:
        return "Phone call (length: " +
               std::to_string(static_cast<const td::td_api::authenticationCodeTypeCall &>(codeType).length_) +
               ")";
    case td::td_api::authenticationCodeTypeFlashCall::ID:
        return "Poor man's phone call (pattern: " +
               static_cast<const td::td_api::authenticationCodeTypeFlashCall &>(codeType).pattern_ +
               ")";
    default:
        return "Pigeon post";
    }
}

int PurpleTdClient::requestAuthCode(gpointer user_data)
{
    PurpleTdClient *self = static_cast<PurpleTdClient *>(user_data);
    std::string message = "Enter authentication code\n";

    if (self->m_authCodeInfo) {
        if (self->m_authCodeInfo->type_)
            message += "Code sent via: " + getAuthCodeDesc(*self->m_authCodeInfo->type_) + "\n";
        if (self->m_authCodeInfo->next_type_)
            message += "Next code will be: " + getAuthCodeDesc(*self->m_authCodeInfo->next_type_) + "\n";
    }

    if (!purple_request_input (purple_account_get_connection(self->m_account),
                               (char *)"Login code",
                               message.c_str(),
                               NULL, // secondary message
                               NULL, // default value
                               FALSE, // multiline input
                               FALSE, // masked input
                               (char *)"the code",
                               (char *)"OK", G_CALLBACK(requestCodeEntered),
                               (char *)"Cancel", G_CALLBACK(requestCodeCancelled),
                               self->m_account,
                               NULL, // buddy
                               NULL, // conversation
                               self))
    {
        purple_connection_set_state (purple_account_get_connection(self->m_account), PURPLE_CONNECTED);
        PurpleConversation *conv = purple_conversation_new (PURPLE_CONV_TYPE_IM, self->m_account, "Telegram");
        purple_conversation_write (conv, "Telegram",
            "Authentication code needs to be entered but this libpurple won't cooperate",
            (PurpleMessageFlags)(PURPLE_MESSAGE_RECV | PURPLE_MESSAGE_SYSTEM), 0);
    }

    return FALSE; // This idle handler will not be called again
}

void PurpleTdClient::requestCodeEntered(PurpleTdClient *self, const gchar *code)
{
    purple_debug_misc(config::pluginId, "Authentication code entered: '%s'\n", code);
    self->sendQuery(td::td_api::make_object<td::td_api::checkAuthenticationCode>(code),
                    &PurpleTdClient::authResponse);
}

void PurpleTdClient::requestCodeCancelled(PurpleTdClient *self)
{
    purple_connection_error(purple_account_get_connection(self->m_account),
                            "Authentication code required");
}

uint64_t PurpleTdClient::sendQuery(td::td_api::object_ptr<td::td_api::Function> f, ResponseCb handler)
{
    uint64_t queryId = ++m_lastQueryId;
    purple_debug_misc(config::pluginId, "Sending query id %lu\n", (unsigned long)queryId);
    if (handler) {
        std::unique_lock<std::mutex> dataLock(m_queryMutex);
        m_responseHandlers.emplace(queryId, std::move(handler));
    }
    m_client->send({queryId, std::move(f)});
    return queryId;
}

void PurpleTdClient::authResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    if (object->get_id() == td::td_api::error::ID) {
        td::td_api::object_ptr<td::td_api::error> error = td::move_tl_object_as<td::td_api::error>(object);
        purple_debug_misc(config::pluginId, "Authentication error on query %lu (auth step %d): code %d (%s)\n",
                          (unsigned long)requestId, (int)m_lastAuthState, (int)error->code_,
                          error->message_.c_str());
        m_authError     = std::move(error);
        g_idle_add(notifyAuthError, this);
    } else
        purple_debug_misc(config::pluginId, "Authentication success on query %lu\n", (unsigned long)requestId);
}

static std::string getDisplayedError(const td::td_api::error &error)
{
    return "code " + std::to_string(error.code_) + " (" + error.message_ + ")";
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

    if (self->m_authError) {
        message += ": " + getDisplayedError(*self->m_authError);
        self->m_authError.reset();
    }

    purple_connection_error(purple_account_get_connection(self->m_account), message.c_str());
    return FALSE; // This idle handler will not be called again
}

void PurpleTdClient::connectionReady()
{
    purple_debug_misc(config::pluginId, "Connection ready\n");
    // This query ensures an updateUser for every contact
    sendQuery(td::td_api::make_object<td::td_api::getContacts>(), &PurpleTdClient::getContactsResponse);
}

int PurpleTdClient::setPurpleConnectionInProgress(gpointer user_data)
{
    purple_debug_misc(config::pluginId, "Connection in progress\n");
    PurpleTdClient   *self = static_cast<PurpleTdClient *>(user_data);
    PurpleConnection *gc   = purple_account_get_connection(self->m_account);

    purple_connection_set_state (gc, PURPLE_CONNECTING);
    purple_blist_remove_account(self->m_account);
    purple_connection_update_progress(gc, "Connecting", 1, 3);

    return FALSE; // This idle handler will not be called again
}

int PurpleTdClient::setPurpleConnectionUpdating(gpointer user_data)
{
    purple_debug_misc(config::pluginId, "Updating account status\n");
    PurpleTdClient   *self = static_cast<PurpleTdClient *>(user_data);
    PurpleConnection *gc   = purple_account_get_connection(self->m_account);

    purple_connection_update_progress(gc, "Updating status", 2, 3);

    return FALSE; // This idle handler will not be called again
}

void PurpleTdClient::getContactsResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    purple_debug_misc(config::pluginId, "getChats response to request %llu\n", (unsigned long long)requestId);
    if (object->get_id() == td::td_api::users::ID) {
        // td::td_api::chats response will be preceded by a string of updateNewChat for all chats
        // apparently even if td::td_api::getChats has limit_ of like 1
        sendQuery(td::td_api::make_object<td::td_api::getChats>(
                      nullptr, std::numeric_limits<std::int64_t>::max(), 0, 200),
                  &PurpleTdClient::getChatsResponse);
    } else {
        m_authError = td::td_api::make_object<td::td_api::error>(0, "Strange response to getContacts");
        g_idle_add(notifyAuthError, this);
    }
}

void PurpleTdClient::getChatsResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    purple_debug_misc(config::pluginId, "getChats response to request %llu\n", (unsigned long long)requestId);
    if (object->get_id() == td::td_api::chats::ID) {
        td::td_api::object_ptr<td::td_api::chats> chats = td::move_tl_object_as<td::td_api::chats>(object);
        {
            TdAccountData::Lock lock(m_data);
            m_data.setActiveChats(std::move(chats->chat_ids_));
        }
        g_idle_add(updatePurpleChatListAndReportConnected, this);
    } else {
        m_authError = td::td_api::make_object<td::td_api::error>(0, "Strange response to getChats");
        g_idle_add(notifyAuthError, this);
    }
}

static const char *getPurpleStatusId(const td::td_api::UserStatus &tdStatus)
{
    if (tdStatus.get_id() == td::td_api::userStatusOnline::ID)
        return purple_primitive_get_id_from_type(PURPLE_STATUS_AVAILABLE);
    else
        return purple_primitive_get_id_from_type(PURPLE_STATUS_OFFLINE);
}

static const char *getPurpleUserName(const td::td_api::user &user)
{
    return user.phone_number_.c_str();
}

int PurpleTdClient::updatePurpleChatListAndReportConnected(gpointer user_data)
{
    PurpleTdClient *self = static_cast<PurpleTdClient *>(user_data);

    purple_connection_set_state (purple_account_get_connection(self->m_account), PURPLE_CONNECTED);
    purple_blist_add_account(self->m_account);

    // Only populate the list from scratch
    TdAccountData::Lock lock(self->m_data);

    std::vector<PrivateChat> privateChats;
    self->m_data.getPrivateChats(privateChats);
    // lock must be held for as long as references from privateChats elements are used

    for (const PrivateChat &c: privateChats) {
        const td::td_api::chat &chat           = c.chat;
        const td::td_api::user &user           = c.user;
        const char             *purpleUserName = getPurpleUserName(user);

        PurpleBuddy *buddy = purple_find_buddy(self->m_account, purpleUserName);
        if (buddy == NULL) {
            purple_debug_misc(config::pluginId, "Adding new buddy %s for chat id %lld\n",
                                chat.title_.c_str(), (long long)chat.id_);
            buddy = purple_buddy_new(self->m_account, purpleUserName, chat.title_.c_str());
            purple_blist_add_buddy(buddy, NULL, NULL, NULL);
        } else {
            const char *oldName = purple_buddy_get_alias_only(buddy);
            if (chat.title_ != oldName) {
                purple_debug_misc(config::pluginId, "Renaming buddy %s '%s' to '%s'\n",
                                  purpleUserName, oldName, chat.title_.c_str());
                purple_blist_remove_buddy(buddy);
                buddy = purple_buddy_new(self->m_account, purpleUserName, chat.title_.c_str());
                purple_blist_add_buddy(buddy, NULL, NULL, NULL);
            }
        }

        purple_prpl_got_user_status(self->m_account, purpleUserName, getPurpleStatusId(*user.status_), NULL);
    }

    const td::td_api::user *selfInfo = self->m_data.getUserByPhone(purple_account_get_username(self->m_account));
    if (selfInfo != nullptr) {
        std::string alias = selfInfo->first_name_ + " " + selfInfo->last_name_;
        purple_debug_misc(config::pluginId, "Setting own alias to '%s'\n", alias.c_str());
        purple_account_set_alias(self->m_account, alias.c_str());
    } else
        purple_debug_warning(config::pluginId, "Did not receive user information for self (%s) at login\n",
            purple_account_get_username(self->m_account));

    // TODO: try to have same code handle pre-login and post-login updates
    // For now, discard accumulated user updates because they have just been handled above
    std::vector<UserUpdate> updates;
    self->m_data.getUpdatedUsers(updates);

    return FALSE; // This idle handler will not be called again
}

int PurpleTdClient::showUnreadMessages(gpointer user_data)
{
    PurpleTdClient *self = static_cast<PurpleTdClient *>(user_data);
    std::vector<UnreadChat> chats;
    TdAccountData::Lock lock(self->m_data);

    self->m_data.getUnreadChatMessages(chats);

    for (const UnreadChat &unreadChat: chats)
        if (!unreadChat.messages.empty()) {
            td::td_api::object_ptr<td::td_api::viewMessages> viewMessagesReq = td::td_api::make_object<td::td_api::viewMessages>();
            viewMessagesReq->chat_id_ = unreadChat.chatId;
            viewMessagesReq->force_read_ = true; // no idea what "closed chats" are at this point
            for (const auto &pMessage: unreadChat.messages)
                viewMessagesReq->message_ids_.push_back(pMessage->id_);
            self->sendQuery(std::move(viewMessagesReq), nullptr);

            for (const auto &pMessage: unreadChat.messages)
                self->showMessage(*pMessage);
        }

    return FALSE; // This idle handler will not be called again
}

static const char *getText(const td::td_api::message &message)
{
    if (message.content_) {
        if ((message.content_->get_id() == td::td_api::messageText::ID)) {
            const td::td_api::messageText &text = static_cast<const td::td_api::messageText &>(*message.content_);
            if (text.text_)
                return text.text_->text_.c_str();
        } else if ((message.content_->get_id() == td::td_api::messagePhoto::ID)) {
            const td::td_api::messagePhoto &photo = static_cast<const td::td_api::messagePhoto &>(*message.content_);
            if (photo.caption_)
                return photo.caption_->text_.c_str();
        }
    }
    return nullptr;
}

void PurpleTdClient::showMessage(const td::td_api::message &message)
{
    // Skip unsupported content
    const char *text = getText(message);
    if (text == nullptr) {
        purple_debug_misc(config::pluginId, "Skipping message: no supported content\n");
        return;
    }

    // m_dataMutex already locked
    const td::td_api::chat *chat = m_data.getChat(message.chat_id_);
    if (!chat) {
        purple_debug_warning(config::pluginId, "Received message with unknown chat id %lld\n",
                            (long long)message.chat_id_);
        return;
    }

    if (chat->type_->get_id() == td::td_api::chatTypePrivate::ID) {
        int32_t userId = static_cast<const td::td_api::chatTypePrivate &>(*chat->type_).user_id_;
        const td::td_api::user *user = m_data.getUser(userId);
        if (user) {
            const char *who = getPurpleUserName(*user);
            if (message.is_outgoing_) {
                // serv_got_im seems to work for messages sent from another client, but not for
                // echoed messages from this client. Therefore, this (code snippet from facebook plugin).
                PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, who, m_account);
                if (conv == NULL)
                    conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, m_account, who);
                purple_conversation_write(conv, purple_account_get_alias(m_account), text,
                                          PURPLE_MESSAGE_SEND, // TODO: maybe set PURPLE_MESSAGE_REMOTE_SEND when appropriate
                                          message.date_);
            } else
                serv_got_im(purple_account_get_connection(m_account), who, text,
                            PURPLE_MESSAGE_RECV, message.date_);
        }
    }
}

void PurpleTdClient::onIncomingMessage(td::td_api::object_ptr<td::td_api::message> message)
{
    // Pass it to the main thread
    TdAccountData::Lock lock(m_data);
    m_data.addNewMessage(std::move(message));
    g_idle_add(showUnreadMessages, this);
}

int PurpleTdClient::sendMessage(const char *buddyName, const char *message)
{
    TdAccountData::Lock lock(m_data);
    const td::td_api::user *tdUser = m_data.getUserByPhone(buddyName);
    if (tdUser == nullptr) {
        purple_debug_warning(config::pluginId, "No user with phone '%s'\n", buddyName);
        return -1;
    }
    const td::td_api::chat *tdChat = m_data.getPrivateChatByUserId(tdUser->id_);
    if (tdChat == nullptr) {
        purple_debug_warning(config::pluginId, "No chat with user %s\n", tdUser->phone_number_.c_str());
        return -1;
    }

    td::td_api::object_ptr<td::td_api::sendMessage> send_message = td::td_api::make_object<td::td_api::sendMessage>();
    send_message->chat_id_ = tdChat->id_;
    td::td_api::object_ptr<td::td_api::inputMessageText> message_content = td::td_api::make_object<td::td_api::inputMessageText>();
    message_content->text_ = td::td_api::make_object<td::td_api::formattedText>();
    message_content->text_->text_ = message;
    send_message->input_message_content_ = std::move(message_content);

    sendQuery(std::move(send_message), nullptr);

    // Message shall not be echoed: tdlib will shortly present it as a new message and it will be displayed then
    return 0;
}

void PurpleTdClient::updateUserStatus(uint32_t userId, td::td_api::object_ptr<td::td_api::UserStatus> status)
{
    {
        TdAccountData::Lock lock(m_data);
        m_data.updateUserStatus(userId, std::move(status));
    }
    g_idle_add(showUserUpdates, this);
}

void PurpleTdClient::updateUser(td::td_api::object_ptr<td::td_api::user> user)
{
    {
        TdAccountData::Lock lock(m_data);
        m_data.updateUser(std::move(user));
    }
    if (purple_connection_get_state (purple_account_get_connection(m_account)) == PURPLE_CONNECTED)
        g_idle_add(showUserUpdates, this);
}

int PurpleTdClient::showUserUpdates(gpointer user_data)
{
    PurpleTdClient *self = static_cast<PurpleTdClient *>(user_data);
    std::vector<UserUpdate> userUpdates;

    {
        TdAccountData::Lock lock(self->m_data);
        self->m_data.getUpdatedUsers(userUpdates);
    }
    for (const UserUpdate &updateInfo: userUpdates) {
        const td::td_api::user *user;
        {
            TdAccountData::Lock lock(self->m_data);
            user = self->m_data.getUser(updateInfo.userId);
        }
        if (user == nullptr)
            continue;

        if (updateInfo.updates.status)
            purple_prpl_got_user_status(self->m_account, getPurpleUserName(*user),
                                        getPurpleStatusId(*user->status_), NULL);
        // TODO: handle other updates
    }

    return FALSE; // This idle handler will not be called again
}

void PurpleTdClient::handleUserChatAction(const td::td_api::updateUserChatAction &updateChatAction)
{
    const td::td_api::chat *chat;
    {
        TdAccountData::Lock lock(m_data);
        chat = m_data.getChat(updateChatAction.chat_id_);
    }

    if (!chat)
        purple_debug_warning(config::pluginId, "Got user chat action for unknown chat %lld\n",
                             (long long)updateChatAction.chat_id_);
    else if (chat->type_->get_id() == td::td_api::chatTypePrivate::ID) {
        const td::td_api::chatTypePrivate &privType = static_cast<const td::td_api::chatTypePrivate &>(*chat->type_);
        if (privType.user_id_ != updateChatAction.user_id_)
            purple_debug_warning(config::pluginId, "Got user action for private chat %lld (with user %d) for another user %d\n",
                                 (long long)updateChatAction.chat_id_, privType.user_id_,
                                 updateChatAction.user_id_);
        else if (updateChatAction.action_) {
            TdAccountData::Lock lock(m_data);
            if (updateChatAction.action_->get_id() == td::td_api::chatActionCancel::ID) {
                purple_debug_misc(config::pluginId, "User (id %d) stopped chat action\n",
                                  updateChatAction.user_id_);
                m_data.addUserAction(updateChatAction.user_id_, false);
            } else if (updateChatAction.action_->get_id() == td::td_api::chatActionStartPlayingGame::ID) {
                purple_debug_misc(config::pluginId, "User (id %d): treating chatActionStartPlayingGame as cancel\n",
                                  updateChatAction.user_id_);
                m_data.addUserAction(updateChatAction.user_id_, false);
            } else {
                purple_debug_misc(config::pluginId, "User (id %d) started chat action (id %d)\n",
                                  updateChatAction.user_id_, updateChatAction.action_->get_id());
                m_data.addUserAction(updateChatAction.user_id_, true);
            }
            g_idle_add(PurpleTdClient::showUserChatActions, this);
        }
    } else
        purple_debug_misc(config::pluginId, "Ignoring user chat action for non-private chat %lld\n",
                          (long long)updateChatAction.chat_id_);
}

int PurpleTdClient::showUserChatActions(gpointer user_data)
{
    PurpleTdClient          *self = static_cast<PurpleTdClient *>(user_data);
    std::vector<UserAction>  actions;
    TdAccountData::Lock      lock(self->m_data);

    self->m_data.getNewUserActions(actions);

    for (const UserAction &action: actions) {
        const td::td_api::user *user = self->m_data.getUser(action.userId);
        if (user) {
            if (action.isTyping)
                serv_got_typing(purple_account_get_connection(self->m_account),
                                getPurpleUserName(*user), REMOTE_TYPING_NOTICE_TIMEOUT,
                                PURPLE_TYPING);
            else
                serv_got_typing_stopped(purple_account_get_connection(self->m_account),
                                getPurpleUserName(*user));
        }
    }

    return FALSE; // This idle handler will not be called again
}

void PurpleTdClient::addContact(const char *phoneNumber, const char *alias)
{
    {
        TdAccountData::Lock lock(m_data);
        if (m_data.getUserByPhone(phoneNumber)) {
            purple_debug_info(config::pluginId, "User with phone number %s already exists\n", phoneNumber);
            return;
        }
    }

    td::td_api::object_ptr<td::td_api::contact> contact =
        td::td_api::make_object<td::td_api::contact>(phoneNumber, "", "", "", 0);
    td::td_api::object_ptr<td::td_api::importContacts> importReq =
        td::td_api::make_object<td::td_api::importContacts>();
    importReq->contacts_.push_back(std::move(contact));
    uint64_t requestId = sendQuery(std::move(importReq), &PurpleTdClient::importContactResponse);

    {
        TdAccountData::Lock lock(m_data);
        m_data.addNewContactRequest(requestId, phoneNumber);
    }
}

void PurpleTdClient::importContactResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    std::string         phoneNumber;
    int32_t             dummy;
    TdAccountData::Lock lock(m_data);
    if (! m_data.extractContactRequest(requestId, phoneNumber, dummy))
        return;

    int32_t userId = 0;
    if (object->get_id() == td::td_api::importedContacts::ID) {
        td::td_api::object_ptr<td::td_api::importedContacts> reply =
            td::move_tl_object_as<td::td_api::importedContacts>(object);
        if (!reply->user_ids_.empty())
            userId = reply->user_ids_[0];
    }

    if (userId) {
        td::td_api::object_ptr<td::td_api::contact> contact =
            td::td_api::make_object<td::td_api::contact>(phoneNumber, "Beh", "Meh", "", userId);
        td::td_api::object_ptr<td::td_api::addContact> addContact =
            td::td_api::make_object<td::td_api::addContact>(std::move(contact), true);
        uint64_t newRequestId = sendQuery(std::move(addContact), &PurpleTdClient::addContactResponse);
        m_data.addNewContactRequest(newRequestId, phoneNumber.c_str(), userId);
    } else {
        m_data.addFailedContact(std::move(phoneNumber), nullptr);
        g_idle_add(&PurpleTdClient::notifyFailedContacts, this);
    }
}

void PurpleTdClient::addContactResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    std::string         phoneNumber;
    int32_t             userId;
    TdAccountData::Lock lock(m_data);
    if (! m_data.extractContactRequest(requestId, phoneNumber, userId))
        return;

    if (object->get_id() == td::td_api::ok::ID) {
        td::td_api::object_ptr<td::td_api::createPrivateChat> createChat =
            td::td_api::make_object<td::td_api::createPrivateChat>(userId, false);
        uint64_t newRequestId = sendQuery(std::move(createChat), &PurpleTdClient::createPrivateChatResponse);
        m_data.addNewContactRequest(newRequestId, phoneNumber.c_str(), userId);
    } else {
        td::td_api::object_ptr<td::td_api::error> error;
        if (object->get_id() == td::td_api::error::ID) {
            error = td::move_tl_object_as<td::td_api::error>(object);
            purple_debug_misc(config::pluginId, "Failed to add contact (%s): code %d (%s)\n",
                            phoneNumber.c_str(), (int)error->code_, error->message_.c_str());
        } else
            error = td::td_api::make_object<td::td_api::error>(0, "Strange reply to adding contact");
        m_data.addFailedContact(std::move(phoneNumber), std::move(error));
        g_idle_add(&PurpleTdClient::notifyFailedContacts, this);
    }
}

void PurpleTdClient::createPrivateChatResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    std::string         phoneNumber;
    int32_t             userId;
    TdAccountData::Lock lock(m_data);
    if (! m_data.extractContactRequest(requestId, phoneNumber, userId))
        return;

    if (object->get_id() != td::td_api::chat::ID) {
        td::td_api::object_ptr<td::td_api::error> error;
        if (object->get_id() == td::td_api::error::ID) {
            error = td::move_tl_object_as<td::td_api::error>(object);
            purple_debug_misc(config::pluginId, "Failed to create private chat (to %s): code %d (%s)\n",
                            phoneNumber.c_str(), (int)error->code_, error->message_.c_str());
        } else
            error = td::td_api::make_object<td::td_api::error>(0, "Strange reply to creating private chat");
        m_data.addFailedContact(std::move(phoneNumber), std::move(error));
        g_idle_add(&PurpleTdClient::notifyFailedContacts, this);
    }
}

int PurpleTdClient::notifyFailedContacts(gpointer user_data)
{
    PurpleTdClient             *self = static_cast<PurpleTdClient *>(user_data);
    std::vector<FailedContact>  failedContacts;
    {
        TdAccountData::Lock lock(self->m_data);
        self->m_data.getFailedContacts(failedContacts);
    }

    if (!failedContacts.empty()) {
        std::string message;
        for (const FailedContact &contact: failedContacts) {
            if (!message.empty())
                message += " ";
            message += "Failed to add contact (";
            message += contact.phoneNumber;
            message += "): ";
            if (contact.error)
                message += getDisplayedError(*contact.error);
            else
                message += "User not found";

            PurpleBuddy *buddy = purple_find_buddy(self->m_account, contact.phoneNumber.c_str());
            if (buddy)
                purple_blist_remove_buddy(buddy);
        }
        purple_notify_error(purple_account_get_connection(self->m_account),
                            "Failed to add contact", message.c_str(), NULL);
    }


    return FALSE; // This idle handler will not be called again
}
