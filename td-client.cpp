#include "td-client.h"
#include "chat-info.h"
#include "config.h"

enum {
    // Typing notifications seems to be resent every 5-6 seconds, so 10s timeout hould be appropriate
    REMOTE_TYPING_NOTICE_TIMEOUT = 10,
    FILE_DOWNLOAD_PRIORITY       = 1
};

class UpdateHandler {
public:
    UpdateHandler(PurpleTdClient *owner) : m_owner(owner) {}

    void operator()(td::td_api::updateAuthorizationState &update_authorization_state) const {
        purple_debug_misc(config::pluginId, "Incoming update: authorization state\n");
        if (update_authorization_state.authorization_state_) {
            m_owner->m_lastAuthState = update_authorization_state.authorization_state_->get_id();
            td::td_api::downcast_call(*update_authorization_state.authorization_state_, *m_owner->m_authUpdateHandler);
        }
    }

    void operator()(td::td_api::updateConnectionState &connectionUpdate) const {
        purple_debug_misc(config::pluginId, "Incoming update: connection state\n");
        if (connectionUpdate.state_) {
            if (connectionUpdate.state_->get_id() == td::td_api::connectionStateReady::ID)
                m_owner->connectionReady();
            else if (connectionUpdate.state_->get_id() == td::td_api::connectionStateConnecting::ID)
                m_owner->setPurpleConnectionInProgress();
            else if (connectionUpdate.state_->get_id() == td::td_api::connectionStateUpdating::ID)
                m_owner->setPurpleConnectionUpdating();
        }
    }

    void operator()(td::td_api::updateUser &userUpdate) const {
        m_owner->updateUser(std::move(userUpdate.user_));
    }

    void operator()(td::td_api::updateNewChat &newChat) const {
        purple_debug_misc(config::pluginId, "Incoming update: new chat\n");
        m_owner->addChat(std::move(newChat.chat_));
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
            updateChatAction.action_ ? updateChatAction.action_->get_id() : 0);
        m_owner->handleUserChatAction(updateChatAction);
    }

    void operator()(td::td_api::updateBasicGroup &groupUpdate) const {
        m_owner->updateGroup(std::move(groupUpdate.basic_group_));
    }

    void operator()(td::td_api::updateSupergroup &groupUpdate) const {
        m_owner->updateSupergroup(std::move(groupUpdate.supergroup_));
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
        m_owner->m_transceiver.sendQuery(td::td_api::make_object<td::td_api::checkDatabaseEncryptionKey>(""),
                                         &PurpleTdClient::authResponse);
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
        m_owner->requestAuthCode();
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

PurpleTdClient::PurpleTdClient(PurpleAccount *acct, ITransceiverBackend *testBackend)
:   m_transceiver(this, &PurpleTdClient::processUpdate, testBackend)
{
    m_account           = acct;
    m_updateHandler     = std::make_unique<UpdateHandler>(this);
    m_authUpdateHandler = std::make_unique<AuthUpdateHandler>(this);
}

PurpleTdClient::~PurpleTdClient()
{
}

void PurpleTdClient::setLogLevel(int level)
{
    // Why not just call setLogVerbosityLevel? No idea!
    td::Client::execute({0, td::td_api::make_object<td::td_api::setLogVerbosityLevel>(level)});
}

void PurpleTdClient::processUpdate(TdObjectPtr object)
{
    purple_debug_misc(config::pluginId, "Incoming update\n");
    td::td_api::downcast_call(*object, *m_updateHandler);
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

void PurpleTdClient::requestAuthCode()
{
    std::string message = "Enter authentication code\n";

    if (m_authCodeInfo) {
        if (m_authCodeInfo->type_)
            message += "Code sent via: " + getAuthCodeDesc(*m_authCodeInfo->type_) + "\n";
        if (m_authCodeInfo->next_type_)
            message += "Next code will be: " + getAuthCodeDesc(*m_authCodeInfo->next_type_) + "\n";
    }

    if (!purple_request_input (purple_account_get_connection(m_account),
                               (char *)"Login code",
                               message.c_str(),
                               NULL, // secondary message
                               NULL, // default value
                               FALSE, // multiline input
                               FALSE, // masked input
                               (char *)"the code",
                               (char *)"OK", G_CALLBACK(requestCodeEntered),
                               (char *)"Cancel", G_CALLBACK(requestCodeCancelled),
                               m_account,
                               NULL, // buddy
                               NULL, // conversation
                               this))
    {
        purple_connection_set_state (purple_account_get_connection(m_account), PURPLE_CONNECTED);
        PurpleConversation *conv = purple_conversation_new (PURPLE_CONV_TYPE_IM, m_account, "Telegram");
        purple_conversation_write (conv, "Telegram",
            "Authentication code needs to be entered but this libpurple won't cooperate",
            (PurpleMessageFlags)(PURPLE_MESSAGE_RECV | PURPLE_MESSAGE_SYSTEM), 0);
    }
}

void PurpleTdClient::requestCodeEntered(PurpleTdClient *self, const gchar *code)
{
    purple_debug_misc(config::pluginId, "Authentication code entered: '%s'\n", code);
    self->m_transceiver.sendQuery(td::td_api::make_object<td::td_api::checkAuthenticationCode>(code),
                                  &PurpleTdClient::authResponse);
}

void PurpleTdClient::requestCodeCancelled(PurpleTdClient *self)
{
    purple_connection_error(purple_account_get_connection(self->m_account),
                            "Authentication code required");
}

void PurpleTdClient::authResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    if (object->get_id() == td::td_api::error::ID) {
        td::td_api::object_ptr<td::td_api::error> error = td::move_tl_object_as<td::td_api::error>(object);
        purple_debug_misc(config::pluginId, "Authentication error on query %lu (auth step %d): code %d (%s)\n",
                          (unsigned long)requestId, (int)m_lastAuthState, (int)error->code_,
                          error->message_.c_str());
        notifyAuthError(std::move(error));
    } else
        purple_debug_misc(config::pluginId, "Authentication success on query %lu\n", (unsigned long)requestId);
}

static std::string getDisplayedError(const td::td_api::error &error)
{
    return "code " + std::to_string(error.code_) + " (" + error.message_ + ")";
}

void PurpleTdClient::notifyAuthError(td::td_api::object_ptr<td::td_api::error> error)
{
    std::string message;
    switch (m_lastAuthState) {
    case td::td_api::authorizationStateWaitEncryptionKey::ID:
        message = "Error applying database encryption key";
        break;
    case td::td_api::authorizationStateWaitPhoneNumber::ID:
        message = "Authentication error after sending phone number";
        break;
    default:
        message = "Authentication error";
    }

    if (error)
        message += ": " + getDisplayedError(*error);

    purple_connection_error(purple_account_get_connection(m_account), message.c_str());
}

void PurpleTdClient::connectionReady()
{
    purple_debug_misc(config::pluginId, "Connection ready\n");
    // This query ensures an updateUser for every contact
    m_transceiver.sendQuery(td::td_api::make_object<td::td_api::getContacts>(),
                            &PurpleTdClient::getContactsResponse);
}

void PurpleTdClient::setPurpleConnectionInProgress()
{
    purple_debug_misc(config::pluginId, "Connection in progress\n");
    PurpleConnection *gc = purple_account_get_connection(m_account);

    if (PURPLE_CONNECTION_IS_CONNECTED(gc))
        purple_blist_remove_account(m_account);
    purple_connection_set_state (gc, PURPLE_CONNECTING);
    purple_connection_update_progress(gc, "Connecting", 1, 3);
}

void PurpleTdClient::setPurpleConnectionUpdating()
{
    purple_debug_misc(config::pluginId, "Updating account status\n");
    PurpleConnection *gc = purple_account_get_connection(m_account);

    purple_connection_update_progress(gc, "Updating status", 2, 3);
}

void PurpleTdClient::getContactsResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    purple_debug_misc(config::pluginId, "getContacts response to request %llu\n", (unsigned long long)requestId);
    if (object->get_id() == td::td_api::users::ID) {
        td::td_api::object_ptr<td::td_api::users> users = td::move_tl_object_as<td::td_api::users>(object);
        m_data.setContacts(users->user_ids_);
        // td::td_api::chats response will be preceded by a string of updateNewChat for all chats
        // apparently even if td::td_api::getChats has limit_ of like 1
        m_transceiver.sendQuery(td::td_api::make_object<td::td_api::getChats>(
                                    nullptr, std::numeric_limits<std::int64_t>::max(), 0, 200),
                                &PurpleTdClient::getChatsResponse);
    } else
        notifyAuthError(td::td_api::make_object<td::td_api::error>(0, "Strange response to getContacts"));
}

void PurpleTdClient::getChatsResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    purple_debug_misc(config::pluginId, "getChats response to request %llu\n", (unsigned long long)requestId);
    if (object->get_id() == td::td_api::chats::ID) {
        td::td_api::object_ptr<td::td_api::chats> chats = td::move_tl_object_as<td::td_api::chats>(object);
        m_data.setActiveChats(std::move(chats->chat_ids_));
        m_data.getContactsWithNoChat(m_usersForNewPrivateChats);
        requestMissingPrivateChats();
    } else
        notifyAuthError(td::td_api::make_object<td::td_api::error>(0, "Strange response to getChats"));
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
    if (object->get_id() == td::td_api::chat::ID) {
        td::td_api::object_ptr<td::td_api::chat> chat = td::move_tl_object_as<td::td_api::chat>(object);
        purple_debug_misc(config::pluginId, "Requested private chat received: id %lld\n",
                          (long long)chat->id_);
        // Here the "new" chat already exists in AccountData because there has just been
        // updateNewChat about this same chat. But do addChat anyway, just in case.
        m_data.addChat(std::move(chat));
    } else
        purple_debug_misc(config::pluginId, "Failed to get requested private chat\n");
    requestMissingPrivateChats();
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
    return getCanonicalPhoneNumber(user.phone_number_.c_str());
}

void PurpleTdClient::updatePrivateChat(const td::td_api::chat &chat, const td::td_api::user &user)
{
    const char *purpleUserName = getPurpleUserName(user);
    if (*purpleUserName == '\0')
        return;

    PurpleBuddy *buddy = purple_find_buddy(m_account, purpleUserName);
    if (buddy == NULL) {
        purple_debug_misc(config::pluginId, "Adding new buddy %s for chat id %lld\n",
                            chat.title_.c_str(), (long long)chat.id_);
        buddy = purple_buddy_new(m_account, purpleUserName, chat.title_.c_str());
        purple_blist_add_buddy(buddy, NULL, NULL, NULL);
        // If a new buddy has been added here, it means that there was updateUser with phone number.
        // This has only been observed to happen if they are messaging us for the first time, or
        // we've just added them to contacts (perhaps in another client while this one was offline).
        // In either case, there is no need to for any extra notification about new contact.
    } else {
        const char *oldName = purple_buddy_get_alias_only(buddy);
        if (chat.title_ != oldName) {
            purple_debug_misc(config::pluginId, "Renaming buddy %s '%s' to '%s'\n",
                                purpleUserName, oldName, chat.title_.c_str());
            PurpleGroup *group = purple_buddy_get_group(buddy);
            purple_blist_remove_buddy(buddy);
            buddy = purple_buddy_new(m_account, purpleUserName, chat.title_.c_str());
            purple_blist_add_buddy(buddy, NULL, group, NULL);
        }
    }
}

void PurpleTdClient::updateBasicGroupChat(int32_t groupId)
{
    const td::td_api::basicGroup *group = m_data.getBasicGroup(groupId);
    const td::td_api::chat       *chat  = m_data.getBasicGroupChatByGroup(groupId);

    if (!group)
        purple_debug_misc(config::pluginId, "Basic group %d does not exist yet\n", groupId);
    else if (!chat)
        purple_debug_misc(config::pluginId, "Chat for basic group %d does not exist yet\n", groupId);
    else if (!isGroupMember(group->status_))
        purple_debug_misc(config::pluginId, "Skipping basic group %d because we are not a member\n",
                          group->id_);
    else {
        std::string  chatName   = getChatName(*chat);
        PurpleChat  *purpleChat = purple_blist_find_chat(m_account, chatName.c_str());
        if (!purpleChat) {
            purple_debug_misc(config::pluginId, "Adding new chat for basic group %d (%s)\n",
                              group->id_, chat->title_.c_str());
            purpleChat = purple_chat_new(m_account, chat->title_.c_str(), getChatComponents(*chat));
            purple_blist_add_chat(purpleChat, NULL, NULL);
        }
    }
}

void PurpleTdClient::updateSupergroupChat(int32_t groupId)
{
    const td::td_api::supergroup *group = m_data.getSupergroup(groupId);
    const td::td_api::chat       *chat  = m_data.getSupergroupChatByGroup(groupId);

    if (!group)
        purple_debug_misc(config::pluginId, "Supergroup %d does not exist yet\n", groupId);
    else if (!chat)
        purple_debug_misc(config::pluginId, "Chat for supergroup %d does not exist yet\n", groupId);
    else if (!isGroupMember(group->status_))
        purple_debug_misc(config::pluginId, "Skipping supergroup %d because we are not a member\n",
                          group->id_);
    else {
        std::string  chatName   = getChatName(*chat);
        PurpleChat  *purpleChat = purple_blist_find_chat(m_account, chatName.c_str());
        if (!purpleChat) {
            purple_debug_misc(config::pluginId, "Adding new chat for supergroup %d (%s)\n",
                              group->id_, chat->title_.c_str());
            purpleChat = purple_chat_new(m_account, chat->title_.c_str(), getChatComponents(*chat));
            purple_blist_add_chat(purpleChat, NULL, NULL);
        }
    }
}

void PurpleTdClient::updatePurpleChatListAndReportConnected()
{
    purple_connection_set_state (purple_account_get_connection(m_account), PURPLE_CONNECTED);

    std::vector<const td::td_api::chat *> chats;
    m_data.getActiveChats(chats);

    for (const td::td_api::chat *chat: chats) {
        const td::td_api::user *user = m_data.getUserByPrivateChat(*chat);
        if (user) {
            updatePrivateChat(*chat, *user);
            purple_prpl_got_user_status(m_account, getPurpleUserName(*user),
                                        getPurpleStatusId(*user->status_), NULL);
        }

        int32_t groupId = getBasicGroupId(*chat);
        if (groupId)
            updateBasicGroupChat(groupId);
        groupId = getSupergroupId(*chat);
        if (groupId)
            updateSupergroupChat(groupId);
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

static PurpleConversation *getImConversation(PurpleAccount *account, const char *username)
{
    PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, username, account);
    if (conv == NULL)
        conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, account, username);

    return conv;
}

static PurpleConvChat *getChatConversation(PurpleAccount *account, const td::td_api::chat &chat,
                                           int chatPurpleId)
{
    std::string chatName = getChatName(chat);
    PurpleConversation *conv = purple_find_chat(purple_account_get_connection(account), chatPurpleId);
    if (conv == NULL) {
        if (chatPurpleId != 0) {
            purple_debug_misc(config::pluginId, "Creating conversation for chat %s (purple id %d)\n",
                              chat.title_.c_str(), chatPurpleId);
            // TODO: when a message arrives during login, libpurple won't find the chat in contact
            // list because even it has been in the contact list since before, the chat lookup
            // doesn't work when account is not connected. Therefore, it won't know chat title and
            // will show chatXXXXXXXXXXX name in the conversation window instead. How to fix this?
            serv_got_joined_chat(purple_account_get_connection(account), chatPurpleId, chatName.c_str());
            conv = purple_find_chat(purple_account_get_connection(account), chatPurpleId);
            if (conv == NULL)
                purple_debug_warning(config::pluginId, "Did not create conversation for chat %s\n", chat.title_.c_str());
        } else
            purple_debug_warning(config::pluginId, "No internal ID for chat %s\n", chat.title_.c_str());
    }

    if (conv)
        return purple_conversation_get_chat_data(conv);
    return NULL;
}

static void showMessageTextIm(PurpleAccount *account, const char *purpleUserName, const char *text,
                              const char *notification, time_t timestamp, bool outgoing)
{
    PurpleConversation *conv = NULL;

    if (text) {
        if (outgoing) {
            // serv_got_im seems to work for messages sent from another client, but not for
            // echoed messages from this client. Therefore, this (code snippet from facebook plugin).
            conv = getImConversation(account, purpleUserName);
            purple_conversation_write(conv, purple_account_get_alias(account), text,
                                    PURPLE_MESSAGE_SEND, // TODO: maybe set PURPLE_MESSAGE_REMOTE_SEND when appropriate
                                    timestamp);
        } else {
            serv_got_im(purple_account_get_connection(account), purpleUserName, text,
                        PURPLE_MESSAGE_RECV, timestamp);
        }
    }

    if (notification) {
        if (conv == NULL)
            conv = getImConversation(account, purpleUserName);
        purple_conversation_write(conv, nullptr, notification, PURPLE_MESSAGE_SYSTEM, timestamp);
    }
}

void PurpleTdClient::showMessageTextChat(const td::td_api::chat &chat, const std::string &sender, const char *text,
                                         const char *notification, time_t timestamp, bool outgoing)
{
    // Again, doing what facebook plugin does
    int purpleId = m_data.getPurpleChatId(chat.id_);
    PurpleConvChat *conv = getChatConversation(m_account, chat, purpleId);

    if (text) {
        if (outgoing) {
            if (conv)
                purple_conv_chat_write(conv, purple_account_get_alias(m_account), text,
                                    PURPLE_MESSAGE_SEND, timestamp);
        } else {
            if (purpleId != 0)
                serv_got_chat_in(purple_account_get_connection(m_account), purpleId,
                                 sender.empty() ? "someone" : sender.c_str(),
                                 PURPLE_MESSAGE_RECV, text, timestamp);
        }
    }

    if (notification) {
        if (conv)
            purple_conversation_write(purple_conv_chat_get_conversation(conv), nullptr,
                                      notification, PURPLE_MESSAGE_SYSTEM, timestamp);
    }
}

void PurpleTdClient::showMessageText(const td::td_api::chat &chat, const std::string &sender, const char *text,
                                     const char *notification, time_t timestamp, bool outgoing)
{
    const td::td_api::user *privateUser = m_data.getUserByPrivateChat(chat);
    if (privateUser)
        showMessageTextIm(m_account, getPurpleUserName(*privateUser), text, notification, timestamp,
                          outgoing);

    if (getBasicGroupId(chat) || getSupergroupId(chat))
        showMessageTextChat(chat, sender, text, notification, timestamp, outgoing);
}

static std::string getSenderPurpleName(const td::td_api::user *user)
{
    if (user) {
        std::string result = user->first_name_;
        if (!result.empty() && !user->last_name_.empty())
            result += ' ';
        result += user->last_name_;
        return result;
    }

    return "";
}

std::string PurpleTdClient::getSenderPurpleName(const td::td_api::chat &chat, const td::td_api::message &message)
{
    if (!message.is_outgoing_ && (getBasicGroupId(chat) || getSupergroupId(chat))) {
        if (message.sender_user_id_)
            return ::getSenderPurpleName(m_data.getUser(message.sender_user_id_));
        else if (!message.author_signature_.empty())
            return message.author_signature_;
        else if (message.forward_info_ && message.forward_info_->origin_)
            switch (message.forward_info_->origin_->get_id()) {
            case td::td_api::messageForwardOriginUser::ID:
                return ::getSenderPurpleName(m_data.getUser(static_cast<const td::td_api::messageForwardOriginUser &>(*message.forward_info_->origin_).sender_user_id_));
            case td::td_api::messageForwardOriginHiddenUser::ID:
                return static_cast<const td::td_api::messageForwardOriginHiddenUser &>(*message.forward_info_->origin_).sender_name_;
            case td::td_api::messageForwardOriginChannel::ID:
                return static_cast<const td::td_api::messageForwardOriginChannel&>(*message.forward_info_->origin_).author_signature_;
            }
    }

    // For outgoing messages, our name will be used instead
    // For private chats, sender name will be determined from the chat instead

    return "";
}

void PurpleTdClient::showTextMessage(const td::td_api::chat &chat, const td::td_api::message &message,
                                     const td::td_api::messageText &text)
{
    if (text.text_)
        showMessageText(chat, getSenderPurpleName(chat, message), text.text_->text_.c_str(), NULL,
                        message.date_, message.is_outgoing_);
}

static const td::td_api::file *selectPhotoSize(const td::td_api::messagePhoto &photo)
{
    const td::td_api::photoSize *selectedSize = nullptr;
    if (photo.photo_)
        for (const auto &newSize: photo.photo_->sizes_)
            if (newSize && newSize->photo_ && (!selectedSize || (newSize->width_ > selectedSize->width_)))
                selectedSize = newSize.get();

    if (selectedSize)
        purple_debug_misc(config::pluginId, "Selected size %dx%d for photo\n",
                          (int)selectedSize->width_, (int)selectedSize->height_);
    else
        purple_debug_warning(config::pluginId, "No file found for a photo\n");

    return selectedSize ? selectedSize->photo_.get() : nullptr;
}

void PurpleTdClient::showPhotoMessage(const td::td_api::chat &chat, const td::td_api::message &message,
                                      const td::td_api::messagePhoto &photo)
{
    const td::td_api::file *file = selectPhotoSize(photo);

    showMessageText(chat, getSenderPurpleName(chat, message),
                    photo.caption_ ? photo.caption_->text_.c_str() : NULL,
                    file ? "Downloading image" : "Faulty image", message.date_, message.is_outgoing_);

    if (file) {
        purple_debug_misc(config::pluginId, "Downloading photo (file id %d)\n", (int)file->id_);
        td::td_api::object_ptr<td::td_api::downloadFile> downloadReq =
            td::td_api::make_object<td::td_api::downloadFile>();
        downloadReq->file_id_     = file->id_;
        downloadReq->priority_    = FILE_DOWNLOAD_PRIORITY;
        downloadReq->offset_      = 0;
        downloadReq->limit_       = 0;
        downloadReq->synchronous_ = true;

        uint64_t requestId = m_transceiver.sendQuery(std::move(downloadReq),
                                                     &PurpleTdClient::messagePhotoDownloadResponse);
        m_data.addDownloadRequest(requestId, message.chat_id_, getSenderPurpleName(chat, message),
                                  message.date_, message.is_outgoing_);
    }
}

void PurpleTdClient::messagePhotoDownloadResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    int64_t     chatId;
    std::string sender;
    int32_t     timestamp;
    bool        outgoing;
    if (!m_data.extractDownloadRequest(requestId, chatId, sender, timestamp, outgoing))
        return;

    std::string path;

    if (object->get_id() == td::td_api::file::ID) {
        const td::td_api::file &file = static_cast<const td::td_api::file &>(*object);
        if (!file.local_)
            purple_debug_misc(config::pluginId, "No local file info after downloading photo\n");
        else if (!file.local_->is_downloading_completed_)
            purple_debug_misc(config::pluginId, "Photo not completely downloaded\n");
        else
            path = file.local_->path_;
    } else
        purple_debug_misc(config::pluginId, "Unexpected response to downloading photo: id %d\n",
                          (int)object->get_id());

    if (!path.empty()) {
        purple_debug_misc(config::pluginId, "Photo downloaded, path: %s\n", path.c_str());
        showPhoto(chatId, sender, timestamp, outgoing, path);
    }
}

void PurpleTdClient::showPhoto(int64_t chatId, const std::string &sender, int32_t timestamp, 
                               bool outgoing, const std::string &filePath)
{
    const td::td_api::chat *chat = m_data.getChat(chatId);
    if (chat) {
        if (filePath.find('"') != std::string::npos)
            purple_debug_misc(config::pluginId, "Cannot show photo: file path contains quotes\n");
        else {
            std::string text = "<img src=\"file://" + filePath + "\">";
            showMessageText(*chat, sender, text.c_str(), NULL, timestamp, outgoing);
        }
    }
}

void PurpleTdClient::showDocument(const td::td_api::chat &chat, const td::td_api::message &message,
                                  const td::td_api::messageDocument &document)
{
    std::string description = "Sent a file";
    if (document.document_)
        description = description + ": " + document.document_->file_name_ + " [" +
        document.document_->mime_type_ + "]";

    showMessageText(chat, getSenderPurpleName(chat, message),
                    document.caption_ ? document.caption_->text_.c_str() : NULL,
                    description.c_str(), message.date_, message.is_outgoing_);
}

void PurpleTdClient::showVideo(const td::td_api::chat &chat, const td::td_api::message &message,
                               const td::td_api::messageVideo &video)
{
    std::string description = "Sent a video";
    if (video.video_)
        description = description + ": " + video.video_->file_name_ + " [" +
        std::to_string(video.video_->width_) + "x" + std::to_string(video.video_->height_) + ", " +
        std::to_string(video.video_->duration_) + "s]";

    showMessageText(chat, getSenderPurpleName(chat, message),
                    video.caption_ ? video.caption_->text_.c_str() : NULL,
                    description.c_str(), message.date_, message.is_outgoing_);
}

static std::string messageTypeToString(const td::td_api::MessageContent &content)
{
#define C(type) case td::td_api::type::ID: return #type;
    switch (content.get_id()) {
    C(messageText)
    C(messageAnimation)
    C(messageAudio)
    C(messageDocument)
    C(messagePhoto)
    C(messageExpiredPhoto)
    C(messageSticker)
    C(messageVideo)
    C(messageExpiredVideo)
    C(messageVideoNote)
    C(messageVoiceNote)
    C(messageLocation)
    C(messageVenue)
    C(messageContact)
    C(messageGame)
    C(messagePoll)
    C(messageInvoice)
    C(messageCall)
    C(messageBasicGroupChatCreate)
    C(messageSupergroupChatCreate)
    C(messageChatChangeTitle)
    C(messageChatChangePhoto)
    C(messageChatDeletePhoto)
    C(messageChatAddMembers)
    C(messageChatJoinByLink)
    C(messageChatDeleteMember)
    C(messageChatUpgradeTo)
    C(messageChatUpgradeFrom)
    C(messagePinMessage)
    C(messageScreenshotTaken)
    C(messageChatSetTtl)
    C(messageCustomServiceAction)
    C(messageGameScore)
    C(messagePaymentSuccessful)
    C(messagePaymentSuccessfulBot)
    C(messageContactRegistered)
    C(messageWebsiteConnected)
    C(messagePassportDataSent)
    C(messagePassportDataReceived)
    C(messageUnsupported)
    }
#undef C
    return "id " + std::to_string(content.get_id());
}

void PurpleTdClient::showMessage(const td::td_api::chat &chat, const td::td_api::message &message)
{
    td::td_api::object_ptr<td::td_api::viewMessages> viewMessagesReq = td::td_api::make_object<td::td_api::viewMessages>();
    viewMessagesReq->chat_id_ = message.chat_id_;
    viewMessagesReq->force_read_ = true; // no idea what "closed chats" are at this point
    viewMessagesReq->message_ids_.push_back(message.id_);
    m_transceiver.sendQuery(std::move(viewMessagesReq), nullptr);

    if (!message.content_)
        return;

    switch (message.content_->get_id()) {
        case td::td_api::messageText::ID: {
            const td::td_api::messageText &text = static_cast<const td::td_api::messageText &>(*message.content_);
            showTextMessage(chat, message, text);
            break;
        }
        case td::td_api::messagePhoto::ID: {
            const td::td_api::messagePhoto &photo = static_cast<const td::td_api::messagePhoto &>(*message.content_);
            showPhotoMessage(chat, message, photo);
            break;
        }
        case td::td_api::messageDocument::ID: {
            const td::td_api::messageDocument &document = static_cast<const td::td_api::messageDocument &>(*message.content_);
            showDocument(chat, message, document);
            break;
        }
        case td::td_api::messageVideo::ID: {
            const td::td_api::messageVideo &video = static_cast<const td::td_api::messageVideo &>(*message.content_);
            showVideo(chat, message, video);
            break;
        }
        default: {
            std::string notice = "Received unsupported message type " +
                                 messageTypeToString(*message.content_);
            showMessageText(chat, getSenderPurpleName(chat, message), NULL, notice.c_str(),
                            message.date_, message.is_outgoing_);
        }
    }
}

void PurpleTdClient::onIncomingMessage(td::td_api::object_ptr<td::td_api::message> message)
{
    if (!message)
        return;

    const td::td_api::chat *chat = m_data.getChat(message->chat_id_);
    if (!chat) {
        purple_debug_warning(config::pluginId, "Received message with unknown chat id %lld\n",
                            (long long)message->chat_id_);
        return;
    }

    const td::td_api::user *privateUser = m_data.getUserByPrivateChat(*chat);

    if (privateUser) {
        const char *who = getPurpleUserName(*privateUser);
        if (*who == '\0') {
            // A message from someone not yet in the contact list can be like this.
            // If somehow updateUser with a phone number never follows, this messages will linger
            // as delayed and never get shown. However, it shouldn't really happen - unless maybe
            // we go offline just as we receive this message, but frankly that's a problem even
            // for normal messages. Or who knows, maybe not sending viewMessages will save us then?
            m_data.addDelayedMessage(privateUser->id_, std::move(message));
            return;
        }
    }

    showMessage(*chat, *message);
}

int PurpleTdClient::sendMessage(const char *buddyName, const char *message)
{
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

    td::td_api::object_ptr<td::td_api::sendMessage> sendMessageRequest = td::td_api::make_object<td::td_api::sendMessage>();
    sendMessageRequest->chat_id_ = tdChat->id_;
    td::td_api::object_ptr<td::td_api::inputMessageText> content = td::td_api::make_object<td::td_api::inputMessageText>();
    content->text_ = td::td_api::make_object<td::td_api::formattedText>();
    content->text_->text_ = message;
    sendMessageRequest->input_message_content_ = std::move(content);

    m_transceiver.sendQuery(std::move(sendMessageRequest), nullptr);

    // Message shall not be echoed: tdlib will shortly present it as a new message and it will be displayed then
    return 0;
}

void PurpleTdClient::updateUserStatus(uint32_t userId, td::td_api::object_ptr<td::td_api::UserStatus> status)
{
    const td::td_api::user *user     = m_data.getUser(userId);
    const char             *userName = getPurpleUserName(*user);

    // Empty phone number is possible after someone new adds us to their contact list. If that's the
    // case, don't call purple_prpl_got_user_status on empty string.
    if (user && *userName)
        purple_prpl_got_user_status(m_account, userName, getPurpleStatusId(*status), NULL);
}

void PurpleTdClient::updateUser(td::td_api::object_ptr<td::td_api::user> user)
{
    if (!user) {
        purple_debug_warning(config::pluginId, "updateUser with null user info\n");
        return;
    }

    purple_debug_misc(config::pluginId, "Update user: %s '%s' '%s'\n", user->phone_number_.c_str(),
                      user->first_name_.c_str(), user->last_name_.c_str());

    bool        hasPhoneNumber = !user->phone_number_.empty();
    int32_t     userId         = user->id_;
    m_data.updateUser(std::move(user));

    if (hasPhoneNumber) {
        const td::td_api::user *user = m_data.getUser(userId);
        const td::td_api::chat *chat = m_data.getPrivateChatByUserId(userId);

        // In case updateNewChat came before an updateUser with non-empty phone number
        // For chats, find_chat doesn't work if account is not yet connected, so just in case, don't
        // user find_buddy either
        if (user && chat && purple_account_is_connected(m_account))
            updatePrivateChat(*chat, *user);

        std::vector<td::td_api::object_ptr<td::td_api::message>> messages;
        m_data.extractDelayedMessagesByUser(userId, messages);

        if (!messages.empty() && user && chat) {
            for (auto &pMessage: messages)
                showMessage(*chat, *pMessage);
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
        updateBasicGroupChat(id);
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
        updateSupergroupChat(id);
}

void PurpleTdClient::addChat(td::td_api::object_ptr<td::td_api::chat> chat)
{
    if (!chat) {
        purple_debug_warning(config::pluginId, "updateNewChat with null chat info\n");
        return;
    }

    const td::td_api::user *privateChatUser = m_data.getUserByPrivateChat(*chat);
    int32_t                 basicGroupId    = getBasicGroupId(*chat);
    int32_t                 supergroupId    = getSupergroupId(*chat);

    purple_debug_misc(config::pluginId, "Add chat: '%s' private=%d basic group=%d supergroup=%d\n",
                      chat->title_.c_str(), privateChatUser ? privateChatUser->id_ : 0,
                      basicGroupId, supergroupId);

    // If updateNewChat came after an updateUser with non-empty phone number (happens during login)
    // For chats, find_chat doesn't work if account is not yet connected, so just in case, don't
    // user find_buddy either
    if (privateChatUser && !privateChatUser->phone_number_.empty() &&
        purple_account_is_connected(m_account))
    {
        updatePrivateChat(*chat, *privateChatUser);
    }

    m_data.addChat(std::move(chat));

    // purple_blist_find_chat doesn't work if account is not connected
    if (basicGroupId && purple_account_is_connected(m_account))
        updateBasicGroupChat(basicGroupId);
    if (supergroupId && purple_account_is_connected(m_account))
        updateSupergroupChat(supergroupId);
}

void PurpleTdClient::handleUserChatAction(const td::td_api::updateUserChatAction &updateChatAction)
{
    const td::td_api::chat *chat;
    chat = m_data.getChat(updateChatAction.chat_id_);

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
    } else
        purple_debug_misc(config::pluginId, "Ignoring user chat action for non-private chat %lld\n",
                          (long long)updateChatAction.chat_id_);
}

void PurpleTdClient::showUserChatAction(int32_t userId, bool isTyping)
{
    const td::td_api::user *user = m_data.getUser(userId);
    if (user) {
        if (isTyping)
            serv_got_typing(purple_account_get_connection(m_account),
                            getPurpleUserName(*user), REMOTE_TYPING_NOTICE_TIMEOUT,
                            PURPLE_TYPING);
        else
            serv_got_typing_stopped(purple_account_get_connection(m_account),
                                    getPurpleUserName(*user));
    }
}

void PurpleTdClient::addContact(const char *phoneNumber, const char *alias)
{
    if (m_data.getUserByPhone(phoneNumber)) {
        purple_debug_info(config::pluginId, "User with phone number %s already exists\n", phoneNumber);
        return;
    }

    td::td_api::object_ptr<td::td_api::contact> contact =
        td::td_api::make_object<td::td_api::contact>(phoneNumber, "", "", "", 0);
    td::td_api::object_ptr<td::td_api::importContacts> importReq =
        td::td_api::make_object<td::td_api::importContacts>();
    importReq->contacts_.push_back(std::move(contact));
    uint64_t requestId = m_transceiver.sendQuery(std::move(importReq),
                                                 &PurpleTdClient::importContactResponse);

    m_data.addNewContactRequest(requestId, phoneNumber, alias);
}

void PurpleTdClient::importContactResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    std::string         phoneNumber;
    std::string         alias;
    int32_t             dummy;
    if (! m_data.extractContactRequest(requestId, phoneNumber, alias, dummy))
        return;

    int32_t userId = 0;
    if (object->get_id() == td::td_api::importedContacts::ID) {
        td::td_api::object_ptr<td::td_api::importedContacts> reply =
            td::move_tl_object_as<td::td_api::importedContacts>(object);
        if (!reply->user_ids_.empty())
            userId = reply->user_ids_[0];
    }

    // For whatever reason, complaining at an earlier stage leads to error message not being shown in pidgin
    if (!isPhoneNumber(phoneNumber.c_str()))
        notifyFailedContact(phoneNumber, "Not a valid phone number");
    else if (userId) {
        td::td_api::object_ptr<td::td_api::contact> contact =
            td::td_api::make_object<td::td_api::contact>(phoneNumber, alias, "", "", userId);
        td::td_api::object_ptr<td::td_api::addContact> addContact =
            td::td_api::make_object<td::td_api::addContact>(std::move(contact), true);
        uint64_t newRequestId = m_transceiver.sendQuery(std::move(addContact),
                                                        &PurpleTdClient::addContactResponse);
        m_data.addNewContactRequest(newRequestId, phoneNumber.c_str(), alias.c_str(), userId);
    } else
        notifyFailedContact(phoneNumber, "User not found");
}

void PurpleTdClient::addContactResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    std::string         phoneNumber;
    std::string         alias;
    int32_t             userId;
    if (! m_data.extractContactRequest(requestId, phoneNumber, alias, userId))
        return;

    if (object->get_id() == td::td_api::ok::ID) {
        td::td_api::object_ptr<td::td_api::createPrivateChat> createChat =
            td::td_api::make_object<td::td_api::createPrivateChat>(userId, false);
        uint64_t newRequestId = m_transceiver.sendQuery(std::move(createChat),
                                                        &PurpleTdClient::addContactCreatePrivateChatResponse);
        m_data.addNewContactRequest(newRequestId, phoneNumber.c_str(), alias.c_str(), userId);
    } else if (object->get_id() == td::td_api::error::ID) {
        td::td_api::object_ptr<td::td_api::error> error = td::move_tl_object_as<td::td_api::error>(object);
        purple_debug_misc(config::pluginId, "Failed to add contact (%s): code %d (%s)\n",
                        phoneNumber.c_str(), (int)error->code_, error->message_.c_str());
        notifyFailedContact(phoneNumber, getDisplayedError(*error));
    } else
        notifyFailedContact(phoneNumber, "Strange reply to adding contact");
}

void PurpleTdClient::addContactCreatePrivateChatResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    std::string         phoneNumber;
    std::string         alias;
    int32_t             userId;
    if (! m_data.extractContactRequest(requestId, phoneNumber, alias, userId))
        return;

    if (object->get_id() != td::td_api::chat::ID) {
        if (object->get_id() == td::td_api::error::ID) {
            td::td_api::object_ptr<td::td_api::error> error = td::move_tl_object_as<td::td_api::error>(object);
            purple_debug_misc(config::pluginId, "Failed to create private chat (to %s): code %d (%s)\n",
                              phoneNumber.c_str(), (int)error->code_, error->message_.c_str());
            notifyFailedContact(phoneNumber, getDisplayedError(*error));
        } else
            notifyFailedContact(phoneNumber, "Strange reply to creating private chat");
    }
}

void PurpleTdClient::notifyFailedContact(const std::string &phoneNumber, const std::string &errorMessage)
{
    std::string message;
    message += "Failed to add contact (";
    message += phoneNumber;
    message += "): ";
    message += errorMessage;

    PurpleBuddy *buddy = purple_find_buddy(m_account, phoneNumber.c_str());
    if (buddy)
        purple_blist_remove_buddy(buddy);

    purple_notify_error(purple_account_get_connection(m_account),
                        "Failed to add contact", message.c_str(), NULL);
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
        conv = getChatConversation(m_account, *chat, purpleId);
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
        purple_debug_warning(config::pluginId, "purple id %d (chat %s) is not a group we a member of\n",
                             purpleChatId, chat->title_.c_str());
    else {
        td::td_api::object_ptr<td::td_api::sendMessage> sendMessageRequest = td::td_api::make_object<td::td_api::sendMessage>();
        sendMessageRequest->chat_id_ = chat->id_;
        td::td_api::object_ptr<td::td_api::inputMessageText> content = td::td_api::make_object<td::td_api::inputMessageText>();
        content->text_ = td::td_api::make_object<td::td_api::formattedText>();
        content->text_->text_ = message;
        sendMessageRequest->input_message_content_ = std::move(content);

        m_transceiver.sendQuery(std::move(sendMessageRequest), nullptr);
        // Message shall not be echoed: tdlib will shortly present it as a new message and it will be displayed then
        return 0;
    }

    return -1;
}
