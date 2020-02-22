#include "td-client.h"
#include "config.h"
#include <algorithm>

enum {
    CHAT_HISTORY_REQUEST_LIMIT  = 50,
    CHAT_HISTORY_RETREIVE_LIMIT = 100
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
        if (connectionUpdate.state_ && (connectionUpdate.state_->get_id() == td::td_api::connectionStateReady::ID))
            m_owner->connectionReady();
    }

    void operator()(td::td_api::updateUser &userUpdate) const {
        purple_debug_misc(config::pluginId, "Incoming update: update user\n");
        m_owner->updateUser(std::move(userUpdate.user_));
    }

    void operator()(td::td_api::updateNewChat &newChat) const {
        purple_debug_misc(config::pluginId, "Incoming update: new chat\n");
        m_owner->addNewChat(std::move(newChat.chat_));
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

void PurpleTdClient::retreiveUnreadHistory(int64_t chatId, int64_t lastReadInId, int64_t lastReadOutId)
{
    // This is only called once for a given chatId after login
    // m_dataMutex is already locked at the time of the call
    m_chatHistoryRequests.emplace_back();
    RequestHistoryState &state = m_chatHistoryRequests.back();
    state.chatId          = chatId;
    state.lastReadInId    = lastReadInId;
    state.lastReadOutId   = lastReadOutId;
    state.oldestSeenInId  = 0;
    state.oldestSeenOutId = 0;
    state.inboxFinished   = false;
    state.outboxFinished  = false;

    auto query = td::td_api::make_object<td::td_api::getChatHistory>(chatId, 0, 0, CHAT_HISTORY_REQUEST_LIMIT, false);
    uint64_t queryId = sendQuery(std::move(query), &PurpleTdClient::chatHistoryResponse);
    state.queryId = queryId;
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
        message += ": code " + std::to_string(self->m_authError->code_) + " (" +
                self->m_authError->message_ + ")";
        self->m_authError.reset();
    }

    purple_connection_error(purple_account_get_connection(self->m_account), message.c_str());
    return FALSE; // This idle handler will not be called again
}

void PurpleTdClient::connectionReady()
{
    g_idle_add(PurpleTdClient::setPurpleConnectionReady, this);

    // td::td_api::chats response will be preceded by a string of updateNewChat and updateUser for
    // all chats and contacts, apparently even if td::td_api::getChats has limit_ of like 1
    sendQuery(td::td_api::make_object<td::td_api::getChats>(
                  nullptr, std::numeric_limits<std::int64_t>::max(), 0, 200),
              &PurpleTdClient::getChatsResponse);
}

int PurpleTdClient::setPurpleConnectionReady(gpointer user_data)
{
    purple_debug_misc(config::pluginId, "Connection ready\n");
    PurpleTdClient *self = static_cast<PurpleTdClient *>(user_data);

    purple_connection_set_state (purple_account_get_connection(self->m_account), PURPLE_CONNECTED);
    purple_blist_add_account(self->m_account);

    return FALSE; // This idle handler will not be called again
}

void PurpleTdClient::updateUser(TdUserPtr user)
{
    if (!user) {
        purple_debug_warning(config::pluginId, "updateUser with null user info\n");
        return;
    }
    purple_debug_misc(config::pluginId, "Update user: %s '%s' '%s'\n", user->phone_number_.c_str(),
                      user->first_name_.c_str(), user->last_name_.c_str());

    std::unique_lock<std::mutex> dataLock(m_dataMutex);
    m_userInfo[user->id_] = std::move(user);
}

void PurpleTdClient::addNewChat(TdChatPtr chat)
{
    if (!chat) {
        purple_debug_warning(config::pluginId, "addNewChat with null chat info\n");
        return;
    }
    purple_debug_misc(config::pluginId, "Add new chat: %s\n", chat->title_.c_str());

    std::unique_lock<std::mutex> dataLock(m_dataMutex);
    m_chatInfo[chat->id_] = std::move(chat);
}

void PurpleTdClient::getChatsResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    purple_debug_misc(config::pluginId, "getChats response to request %llu\n", (unsigned long long)requestId);
    if (object->get_id() == td::td_api::chats::ID) {
        td::td_api::object_ptr<td::td_api::chats> chats = td::move_tl_object_as<td::td_api::chats>(object);
        {
            std::unique_lock<std::mutex> dataLock(m_dataMutex);
            m_activeChats = std::move(chats->chat_ids_);
        }
        g_idle_add(updatePurpleChatList, this);
    }
}

static const char *getPurpleStatusId(const td::td_api::UserStatus &tdStatus)
{
    if (tdStatus.get_id() == td::td_api::userStatusOnline::ID)
        return purple_primitive_get_id_from_type(PURPLE_STATUS_AVAILABLE);
    else
        return purple_primitive_get_id_from_type(PURPLE_STATUS_OFFLINE);
}

int PurpleTdClient::updatePurpleChatList(gpointer user_data)
{
    PurpleTdClient *self = static_cast<PurpleTdClient *>(user_data);

    // Only populate the list from scratch
    std::unique_lock<std::mutex> dataLock(self->m_dataMutex);
    for (int64_t chatId: self->m_activeChats) {
        auto pChatInfo = self->m_chatInfo.find(chatId);
        if (pChatInfo == self->m_chatInfo.end()) {
            purple_debug_warning(config::pluginId, "Received unknown chat id %lld\n", (long long)chatId);
            continue;
        }
        const td::td_api::chat &chat = *pChatInfo->second;

        if (chat.type_->get_id() == td::td_api::chatTypePrivate::ID) {
            const td::td_api::chatTypePrivate &privType = static_cast<const td::td_api::chatTypePrivate &>(*chat.type_);
            auto pUser = self->m_userInfo.find(privType.user_id_);
            if (pUser == self->m_userInfo.end()) {
                purple_debug_warning(config::pluginId, "Received private chat with unknown user id %d\n", (int)privType.user_id_);
                continue;
            }
            const td::td_api::user &user   = *pUser->second;
            const char             *userId = user.phone_number_.c_str();

            PurpleBuddy *buddy = purple_find_buddy(self->m_account, userId);
            if (buddy == NULL) {
                purple_debug_misc(config::pluginId, "Adding new buddy %s for chat id %lld\n",
                                  chat.title_.c_str(), (long long)chat.id_);
                buddy = purple_buddy_new(self->m_account, userId, chat.title_.c_str());
                purple_blist_add_buddy(buddy, NULL, NULL, NULL);
            }

            purple_prpl_got_user_status(self->m_account, userId, getPurpleStatusId(*user.status_), NULL);

            // TODO unread_count means not read on any client, that's not the right comparison
            // Instead, need to compare last_read_*box_message_id_ (these are for this client only)
            // to last message on the chat that server always sends
            //if (chat.unread_count_ != 0) {
                purple_debug_misc(config::pluginId, "chat %lld (%s) has %d unread messages, retreiving history\n",
                                  (long long)chat.id_, chat.title_.c_str(), (int)chat.unread_count_);
                self->retreiveUnreadHistory(chat.id_, chat.last_read_inbox_message_id_, chat.last_read_outbox_message_id_);
            //}
        }
    }

    return FALSE; // This idle handler will not be called again
}

void PurpleTdClient::chatHistoryResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    bool finished = false;
    std::unique_lock<std::mutex> lock(m_dataMutex);
    auto it = std::find_if(m_chatHistoryRequests.begin(), m_chatHistoryRequests.end(),
                           [requestId](const RequestHistoryState &state) {
                               return (state.queryId == requestId);
                           });
    if (it == m_chatHistoryRequests.end()) {
        purple_debug_warning(config::pluginId, "Received history response id %llu doesn't match any RequestHistoryState\n",
            (unsigned long long)requestId);
    } else {
        RequestHistoryState &state = *it;
        int64_t lastMessageId = 0;
        if (object->get_id() != td::td_api::messages::ID) {
            purple_debug_misc(config::pluginId, "Error retreiving unread messages for chat %lld (object id %d) - aborting\n",
                            (long long)state.chatId, (int)object->get_id());
            state.inboxFinished = true;
            state.outboxFinished = true;
        } else {
            td::td_api::messages &messages = static_cast<td::td_api::messages &>(*object);
            purple_debug_misc(config::pluginId, "Received %zu messages for chat %lld\n",
                              messages.messages_.size(), (long long)state.chatId);
            for (auto it = messages.messages_.begin(); it != messages.messages_.end(); ++it)
                if (*it) {
                    td::td_api::object_ptr<td::td_api::message> message = std::move(*it);
                    lastMessageId = message->id_;
                    if (message->is_outgoing_) {
                        if (!state.outboxFinished) {
                            purple_debug_misc(config::pluginId, "Retreived outgoing message %lld for chat %lld\n",
                                              (long long)message->id_, (long long)state.chatId);
                            state.oldestSeenOutId = message->id_;
                            state.outboxFinished = (message->id_ == state.lastReadOutId);
                            if (state.outboxFinished)
                                purple_debug_misc(config::pluginId, "All unread outgoing messages retreived for chat %lld\n",
                                                  (long long)state.chatId);
                            else
                                state.messages.push_back(std::move(message));
                        }
                    } else {
                        if (!state.inboxFinished) {
                            purple_debug_misc(config::pluginId, "Retreived incoming message %lld for chat %lld\n",
                                              (long long)message->id_, (long long)state.chatId);
                            state.oldestSeenInId = message->id_;
                            state.inboxFinished = (message->id_ == state.lastReadInId);
                            if (state.inboxFinished)
                                purple_debug_misc(config::pluginId, "All unread incoming messages retreived for chat %lld\n",
                                                  (long long)state.chatId);
                            else
                                state.messages.push_back(std::move(message));
                        }
                    }

                    if (state.messages.size() >= CHAT_HISTORY_RETREIVE_LIMIT) {
                        purple_debug_misc(config::pluginId, "Reached unread message limit for chat id %lld\n",
                                          (long long)state.chatId);
                        state.outboxFinished = true;
                        state.inboxFinished = true;
                    }
                }
        }
        if (lastMessageId == 0) {
            purple_debug_misc(config::pluginId, "No messages in the batch - aborting\n");
            state.outboxFinished = true;
            state.inboxFinished = true;
        }
        if (state.inboxFinished && state.outboxFinished) {
            purple_debug_misc(config::pluginId, "Retreived %zu unread messages for chat %lld\n",
                state.messages.size(), (long long)state.chatId);
            finished = true;
        } else {
            auto query = td::td_api::make_object<td::td_api::getChatHistory>(
                state.chatId, lastMessageId, 0, CHAT_HISTORY_REQUEST_LIMIT, false
            );
            uint64_t queryId = sendQuery(std::move(query), &PurpleTdClient::chatHistoryResponse);
            state.queryId = queryId;
        }
    }

    lock.unlock();
    if (finished)
        g_idle_add(showUnreadMessages, this);
}

int PurpleTdClient::showUnreadMessages(gpointer user_data)
{
    PurpleTdClient *self = static_cast<PurpleTdClient *>(user_data);

    std::unique_lock<std::mutex> lock(self->m_dataMutex);
    for (size_t i = 0; i < self->m_chatHistoryRequests.size(); ) {
        if (self->m_chatHistoryRequests[i].inboxFinished && self->m_chatHistoryRequests[i].outboxFinished) {
            purple_debug_misc(config::pluginId, "Need to show %zu unread messages for chat %lld\n",
                              self->m_chatHistoryRequests[i].messages.size(),
                              (long long)self->m_chatHistoryRequests[i].chatId);
            for (auto it = self->m_chatHistoryRequests[i].messages.begin();
                 it != self->m_chatHistoryRequests[i].messages.end(); ++it)
            {
                self->showMessage(*(*it));
            }
            self->m_chatHistoryRequests.erase(self->m_chatHistoryRequests.begin()+i);
        } else
            i++;
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
    auto pChat = m_chatInfo.find(message.chat_id_);
    if (pChat == m_chatInfo.end()) {
        purple_debug_warning(config::pluginId, "Received message with unknown chat id %lld\n",
                            (long long)message.chat_id_);
        return;
    }
    const td::td_api::chat &chat = *pChat->second;

    if (chat.type_->get_id() == td::td_api::chatTypePrivate::ID) {
        int32_t userId = static_cast<const td::td_api::chatTypePrivate &>(*chat.type_).user_id_;
        auto pUser = m_userInfo.find(userId);
        if (pUser != m_userInfo.end()) {
            const td::td_api::user &user = *pUser->second;
            int flags;
            if (message.is_outgoing_)
                flags = PURPLE_MESSAGE_SEND | PURPLE_MESSAGE_NO_LOG | PURPLE_MESSAGE_REMOTE_SEND;
            else
                flags = PURPLE_MESSAGE_RECV | PURPLE_MESSAGE_NO_LOG;
            serv_got_im(purple_account_get_connection(m_account), user.phone_number_.c_str(),
                        text, (PurpleMessageFlags)flags, message.date_);
        }
    }
}
