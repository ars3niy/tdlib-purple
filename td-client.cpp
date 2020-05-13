#include "td-client.h"
#include "chat-info.h"
#include "config.h"
#include "purple-utils.h"
#include "format.h"

static char *_(const char *s) { return const_cast<char *>(s); }

enum {
    // Typing notifications seems to be resent every 5-6 seconds, so 10s timeout hould be appropriate
    REMOTE_TYPING_NOTICE_TIMEOUT = 10,
    FILE_DOWNLOAD_PRIORITY       = 1
};

PurpleTdClient::PurpleTdClient(PurpleAccount *acct, ITransceiverBackend *testBackend)
:   m_transceiver(this, &PurpleTdClient::processUpdate, testBackend)
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
        sendTdlibParameters();
        break;

    case td::td_api::authorizationStateWaitPhoneNumber::ID:
        purple_debug_misc(config::pluginId, "Authorization state update: phone number requested\n");
        sendPhoneNumber();
        break;

    case td::td_api::authorizationStateWaitCode::ID: {
        auto &codeState = static_cast<td::td_api::authorizationStateWaitCode &>(authState);
        purple_debug_misc(config::pluginId, "Authorization state update: authentication code requested\n");
        m_authCodeInfo = std::move(codeState.code_info_);
        requestAuthCode();
        break;
    }

    case td::td_api::authorizationStateReady::ID:
        purple_debug_misc(config::pluginId, "Authorization state update: ready\n");
        break;
    }
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

void PurpleTdClient::requestAuthCode()
{
    std::string message = _("Enter authentication code") + std::string("\n");

    if (m_authCodeInfo) {
        if (m_authCodeInfo->type_)
            message += formatMessage(_("Code sent via: {}"), getAuthCodeDesc(*m_authCodeInfo->type_)) + "\n";
        if (m_authCodeInfo->next_type_)
            message += formatMessage(_("Next code will be: {}"), getAuthCodeDesc(*m_authCodeInfo->next_type_)) + "\n";
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
    if (object && (object->get_id() == td::td_api::ok::ID))
        purple_debug_misc(config::pluginId, "Authentication success on query %lu\n", (unsigned long)requestId);
    else
        notifyAuthError(object);
}

static std::string getDisplayedError(const td::td_api::object_ptr<td::td_api::Object> &object)
{
    if (!object)
        return _("No response received");
    else if (object->get_id() == td::td_api::error::ID) {
        const td::td_api::error &error = static_cast<const td::td_api::error &>(*object);
        return formatMessage("code {} ({})", {std::to_string(error.code_), error.message_});
    } else
        return _("Unexpected response");
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
    purple_debug_misc(config::pluginId, "getContacts response to request %" G_GUINT64_FORMAT "\n", requestId);
    if (object && (object->get_id() == td::td_api::users::ID)) {
        td::td_api::object_ptr<td::td_api::users> users = td::move_tl_object_as<td::td_api::users>(object);
        m_data.setContacts(users->user_ids_);
        // td::td_api::chats response will be preceded by a string of updateNewChat for all chats
        // apparently even if td::td_api::getChats has limit_ of like 1
        m_transceiver.sendQuery(td::td_api::make_object<td::td_api::getChats>(
                                    nullptr, std::numeric_limits<std::int64_t>::max(), 0, 200),
                                &PurpleTdClient::getChatsResponse);
    } else
        notifyAuthError(object);
}

void PurpleTdClient::getChatsResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    purple_debug_misc(config::pluginId, "getChats response to request %" G_GUINT64_FORMAT "\n", requestId);
    if (object && (object->get_id() == td::td_api::chats::ID)) {
        td::td_api::object_ptr<td::td_api::chats> chats = td::move_tl_object_as<td::td_api::chats>(object);
        m_data.setActiveChats(std::move(chats->chat_ids_));
        m_data.getContactsWithNoChat(m_usersForNewPrivateChats);
        requestMissingPrivateChats();
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
    if (object->get_id() == td::td_api::chat::ID) {
        td::td_api::object_ptr<td::td_api::chat> chat = td::move_tl_object_as<td::td_api::chat>(object);
        purple_debug_misc(config::pluginId, "Requested private chat received: id %" G_GUINT64_FORMAT "\n",
                          chat->id_);
        // Here the "new" chat already exists in AccountData because there has just been
        // updateNewChat about this same chat. But do addChat anyway, just in case.
        m_data.addChat(std::move(chat));
    } else
        purple_debug_misc(config::pluginId, "Failed to get requested private chat\n");
    requestMissingPrivateChats();
}

void PurpleTdClient::updatePrivateChat(const td::td_api::chat &chat, const td::td_api::user &user)
{
    std::string purpleUserName = getPurpleUserName(user);

    PurpleBuddy *buddy = purple_find_buddy(m_account, purpleUserName.c_str());
    if (buddy == NULL) {
        purple_debug_misc(config::pluginId, "Adding new buddy %s for user %s, chat id %" G_GUINT64_FORMAT "\n",
                          chat.title_.c_str(), purpleUserName.c_str(), chat.id_);

        const ContactRequest *contactReq = m_data.findContactRequest(user.id_);
        PurpleGroup          *group      = (contactReq && !contactReq->groupName.empty()) ?
                                           purple_find_group(contactReq->groupName.c_str()) : NULL;
        if (group)
            purple_debug_misc(config::pluginId, "Adding into group %s\n", purple_group_get_name(group));

        buddy = purple_buddy_new(m_account, purpleUserName.c_str(), chat.title_.c_str());
        purple_blist_add_buddy(buddy, NULL, group, NULL);
        // If a new buddy has been added here, it means that there was updateNewChat with the private
        // chat. This means either we added them to contacts or started messaging them, or they
        // messaged us. Either way, there is no need to for any extra notification about new contact
        // because the user will be aware anyway.
    } else {
        const char *oldName = purple_buddy_get_alias_only(buddy);
        if (chat.title_ != oldName) {
            purple_debug_misc(config::pluginId, "Renaming buddy %s '%s' to '%s'\n",
                                purpleUserName.c_str(), oldName, chat.title_.c_str());
            PurpleGroup *group = purple_buddy_get_group(buddy);
            purple_blist_remove_buddy(buddy);
            buddy = purple_buddy_new(m_account, purpleUserName.c_str(), chat.title_.c_str());
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

void PurpleTdClient::requestBasicGroupMembers(int32_t groupId)
{
    uint64_t requestId = m_transceiver.sendQuery(td::td_api::make_object<td::td_api::getBasicGroupFullInfo>(groupId),
                                                 &PurpleTdClient::groupInfoResponse);
    m_data.addPendingRequest<GroupInfoRequest>(requestId, groupId);
}

// TODO process messageChatAddMembers and messageChatDeleteMember
// TODO process messageChatUpgradeTo and messageChatUpgradeFrom
void PurpleTdClient::groupInfoResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    std::unique_ptr<GroupInfoRequest> request = m_data.getPendingRequest<GroupInfoRequest>(requestId);

    if (request && object && (object->get_id() == td::td_api::basicGroupFullInfo::ID)) {
        td::td_api::object_ptr<td::td_api::basicGroupFullInfo> groupInfo =
            td::move_tl_object_as<td::td_api::basicGroupFullInfo>(object);
        const td::td_api::chat *chat = m_data.getBasicGroupChatByGroup(request->groupId);

        if (chat) {
            std::string         name = getChatName(*chat);
            PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT,
                                                                             name.c_str(), m_account);
            PurpleConvChat     *purpleChat = conv ? purple_conversation_get_chat_data(conv) : NULL;

            if (purpleChat)
                setChatMembers(purpleChat, *groupInfo, m_data);
        }

        m_data.updateBasicGroupInfo(request->groupId, std::move(groupInfo));
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
            std::string userName = getPurpleUserName(*user);
            purple_prpl_got_user_status(m_account, userName.c_str(),
                                        getPurpleStatusId(*user->status_), NULL);
        }

        int32_t groupId = getBasicGroupId(*chat);
        if (groupId) {
            requestBasicGroupMembers(groupId);
            updateBasicGroupChat(groupId);
        }
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

void PurpleTdClient::showTextMessage(const td::td_api::chat &chat, const td::td_api::message &message,
                                     const td::td_api::messageText &text)
{
    if (text.text_)
        showMessageText(m_account, chat, getSenderPurpleName(chat, message, m_data), text.text_->text_.c_str(), NULL,
                        message.date_, message.is_outgoing_, m_data);
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
    const char             *notice;

    if (!file)
        notice = "Faulty image";
    else if (file->local_ && file->local_->is_downloading_completed_)
        notice = NULL;
    else
        notice = "Downloading image";

    if ((!photo.caption_ && photo.caption_->text_.empty()) || notice)
        showMessageText(m_account, chat, getSenderPurpleName(chat, message, m_data),
                        photo.caption_ ? photo.caption_->text_.c_str() : NULL,
                        notice, message.date_, message.is_outgoing_, m_data);

    if (file)
        showImage(chat, message, *file);
}

void PurpleTdClient::requestDownload(int32_t fileId, int64_t chatId, const std::string &sender,
                                     int32_t timestamp, bool outgoing,
                                     td::td_api::object_ptr<td::td_api::file> thumbnail,
                                     TdTransceiver::ResponseCb responseCb)
{
    td::td_api::object_ptr<td::td_api::downloadFile> downloadReq =
        td::td_api::make_object<td::td_api::downloadFile>();
    downloadReq->file_id_     = fileId;
    downloadReq->priority_    = FILE_DOWNLOAD_PRIORITY;
    downloadReq->offset_      = 0;
    downloadReq->limit_       = 0;
    downloadReq->synchronous_ = true;

    uint64_t requestId = m_transceiver.sendQuery(std::move(downloadReq), responseCb);
    m_data.addPendingRequest<DownloadRequest>(requestId, chatId, sender, timestamp, outgoing, thumbnail.release());
}

void PurpleTdClient::showImage(const td::td_api::chat &chat, const td::td_api::message &message,
                               const td::td_api::file &file)
{
    std::string sender = getSenderPurpleName(chat, message, m_data);

    if (file.local_ && file.local_->is_downloading_completed_)
        showDownloadedImage(chat.id_, sender, message.date_, message.is_outgoing_, file.local_->path_);
    else {
        purple_debug_misc(config::pluginId, "Downloading image (file id %d)\n", (int)file.id_);
        requestDownload(file.id_, chat.id_, sender, message.date_, message.is_outgoing_,
                        nullptr, &PurpleTdClient::imageDownloadResponse);
    }
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

void PurpleTdClient::imageDownloadResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    std::string                      path    = getDownloadPath(object.get());
    std::unique_ptr<DownloadRequest> request = m_data.getPendingRequest<DownloadRequest>(requestId);

    if (request && !path.empty()) {
        purple_debug_misc(config::pluginId, "Image downloaded, path: %s\n", path.c_str());
        showDownloadedImage(request->chatId, request->sender, request->timestamp, request->outgoing, path);
    }
}

void PurpleTdClient::showDownloadedImage(int64_t chatId, const std::string &sender, int32_t timestamp, 
                                         bool outgoing, const std::string &filePath)
{
    const td::td_api::chat *chat = m_data.getChat(chatId);
    if (chat) {
        if (filePath.find('"') != std::string::npos)
            showMessageText(m_account, *chat, sender, NULL,
                            "Cannot show photo: file path contains quotes", timestamp, outgoing, m_data);
        else {
            std::string  text;
            gchar       *data = NULL;
            size_t       len  = 0;

            if (g_file_get_contents (filePath.c_str(), &data, &len, NULL)) {
                int id = purple_imgstore_add_with_id (data, len, NULL);
                text = "\n<img id=\"" + std::to_string(id) + "\">";
            } else
                text = "<img src=\"file://" + filePath + "\">";
            showMessageText(m_account, *chat, sender, text.c_str(), NULL, timestamp, outgoing,
                            m_data, PURPLE_MESSAGE_IMAGES);
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

    showMessageText(m_account, chat, getSenderPurpleName(chat, message, m_data),
                    document.caption_ ? document.caption_->text_.c_str() : NULL,
                    description.c_str(), message.date_, message.is_outgoing_, m_data);
}

void PurpleTdClient::showVideo(const td::td_api::chat &chat, const td::td_api::message &message,
                               const td::td_api::messageVideo &video)
{
    std::string description = "Sent a video";
    if (video.video_)
        description = description + ": " + video.video_->file_name_ + " [" +
        std::to_string(video.video_->width_) + "x" + std::to_string(video.video_->height_) + ", " +
        std::to_string(video.video_->duration_) + "s]";

    showMessageText(m_account, chat, getSenderPurpleName(chat, message, m_data),
                    video.caption_ ? video.caption_->text_.c_str() : NULL,
                    description.c_str(), message.date_, message.is_outgoing_, m_data);
}

void PurpleTdClient::showSticker(const td::td_api::chat &chat, const td::td_api::message &message,
                                 td::td_api::messageSticker &stickerContent)
{
    if (!stickerContent.sticker_) return;
    td::td_api::sticker &sticker = *stickerContent.sticker_;

    if (sticker.sticker_) {
        std::string sender    = getSenderPurpleName(chat, message, m_data);
        auto        thumbnail = sticker.thumbnail_ ? std::move(sticker.thumbnail_->photo_) : nullptr;

        if (sticker.sticker_->local_ && sticker.sticker_->local_->is_downloading_completed_)
            showDownloadedSticker(chat.id_, sender, message.date_, message.is_outgoing_,
                                     sticker.sticker_->local_->path_, std::move(thumbnail));
        else {
            purple_debug_misc(config::pluginId, "Downloading sticker (file id %d)\n", (int)sticker.sticker_->id_);
            requestDownload(sticker.sticker_->id_, chat.id_, getSenderPurpleName(chat, message, m_data),
                            message.date_, message.is_outgoing_, std::move(thumbnail),
                            &PurpleTdClient::stickerDownloadResponse);
        }
    }
}

static bool isTgs(const std::string &path)
{
    size_t dot = path.rfind('.');
    if (dot != std::string::npos)
        return !strcmp(path.c_str() + dot + 1, "tgs");

    return false;
}


void PurpleTdClient::stickerDownloadResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    std::string                      path    = getDownloadPath(object.get());
    std::unique_ptr<DownloadRequest> request = m_data.getPendingRequest<DownloadRequest>(requestId);

    if (request && !path.empty())
        showDownloadedSticker(request->chatId, request->sender, request->timestamp, request->outgoing,
                              path, std::move(request->thumbnail));
}

void PurpleTdClient::showDownloadedSticker(int64_t chatId, const std::string &sender, int32_t timestamp,
                                           bool outgoing, const std::string &filePath,
                                           td::td_api::object_ptr<td::td_api::file> thumbnail)
{
    if (isTgs(filePath) && thumbnail) {
        if (thumbnail->local_ && thumbnail->local_->is_downloading_completed_)
            showDownloadedInlineFile(chatId, sender, timestamp, outgoing,
                                        thumbnail->local_->path_, "Sticker");
        else
            requestDownload(thumbnail->id_, chatId, sender,
                            timestamp, outgoing, nullptr,
                            &PurpleTdClient::stickerDownloadResponse);
    } else
        showDownloadedInlineFile(chatId, sender, timestamp, outgoing, filePath, "Sticker");
}


void PurpleTdClient::showInlineFile(const td::td_api::chat &chat, const td::td_api::message &message,
                                    const td::td_api::file &file)
{
    std::string sender = getSenderPurpleName(chat, message, m_data);

    if (file.local_ && file.local_->is_downloading_completed_)
        showDownloadedInlineFile(chat.id_, sender, message.date_, message.is_outgoing_,
                                 file.local_->path_, "Sent file");
    else {
        purple_debug_misc(config::pluginId, "Downloading file (id %d)\n", (int)file.id_);
        requestDownload(file.id_, chat.id_, sender, message.date_, message.is_outgoing_, nullptr,
                        &PurpleTdClient::fileDownloadResponse);
    }
}

void PurpleTdClient::fileDownloadResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    std::string                      path    = getDownloadPath(object.get());
    std::unique_ptr<DownloadRequest> request = m_data.getPendingRequest<DownloadRequest>(requestId);

    if (request && !path.empty()) {
        purple_debug_misc(config::pluginId, "File downloaded, path: %s\n", path.c_str());
        showDownloadedInlineFile(request->chatId, request->sender, request->timestamp,
                                 request->outgoing, path, "Sent file");
    }
}

void PurpleTdClient::showDownloadedInlineFile(int64_t chatId, const std::string &sender, int32_t timestamp, 
                                              bool outgoing, const std::string &filePath, const char *label)
{
    const td::td_api::chat *chat = m_data.getChat(chatId);
    if (chat) {
        if (filePath.find('"') != std::string::npos)
            showMessageText(m_account, *chat, sender, NULL,
                            "Cannot show file: path contains quotes", timestamp, outgoing, m_data);
        else {
            std::string text = "<a href=\"file://" + filePath + "\">" + label + "</a>";
            showMessageText(m_account, *chat, sender, text.c_str(), NULL, timestamp, outgoing, m_data);
        }
    }
}

void PurpleTdClient::showMessage(const td::td_api::chat &chat, td::td_api::message &message)
{
    td::td_api::object_ptr<td::td_api::viewMessages> viewMessagesReq = td::td_api::make_object<td::td_api::viewMessages>();
    viewMessagesReq->chat_id_ = message.chat_id_;
    viewMessagesReq->force_read_ = true; // no idea what "closed chats" are at this point
    viewMessagesReq->message_ids_.push_back(message.id_);
    m_transceiver.sendQuery(std::move(viewMessagesReq), nullptr);

    if (!message.content_)
        return;

    switch (message.content_->get_id()) {
        case td::td_api::messageText::ID:
            showTextMessage(chat, message, static_cast<const td::td_api::messageText &>(*message.content_));
            break;
        case td::td_api::messagePhoto::ID:
            showPhotoMessage(chat, message, static_cast<const td::td_api::messagePhoto &>(*message.content_));
            break;
        case td::td_api::messageDocument::ID:
            showDocument(chat, message, static_cast<const td::td_api::messageDocument &>(*message.content_));
            break;
        case td::td_api::messageVideo::ID:
            showVideo(chat, message, static_cast<const td::td_api::messageVideo &>(*message.content_));
            break;
        case td::td_api::messageSticker::ID:
            showSticker(chat, message, static_cast<td::td_api::messageSticker &>(*message.content_));
            break;
        default: {
            std::string notice = "Received unsupported message type " +
                                 messageTypeToString(*message.content_);
            showMessageText(m_account, chat, getSenderPurpleName(chat, message, m_data), NULL, notice.c_str(),
                            message.date_, message.is_outgoing_, m_data);
        }
    }
}

void PurpleTdClient::onIncomingMessage(td::td_api::object_ptr<td::td_api::message> message)
{
    if (!message)
        return;

    const td::td_api::chat *chat = m_data.getChat(message->chat_id_);
    if (!chat) {
        purple_debug_warning(config::pluginId, "Received message with unknown chat id %" G_GUINT64_FORMAT "\n",
                            message->chat_id_);
        return;
    }

    showMessage(*chat, *message);
}

int PurpleTdClient::sendMessage(const char *buddyName, const char *message)
{
    int32_t userId = stringToUserId(buddyName);
    if (userId == 0) {
        purple_debug_warning(config::pluginId, "'%s' is not a valid user id\n", buddyName);
        return -1;
    }
    const td::td_api::user *tdUser = m_data.getUser(userId);
    if (tdUser == nullptr) {
        purple_debug_warning(config::pluginId, "No user with id %s\n", buddyName);
        return -1;
    }
    const td::td_api::chat *tdChat = m_data.getPrivateChatByUserId(tdUser->id_);
    if (tdChat == nullptr) {
        purple_debug_warning(config::pluginId, "No chat with user %s\n", buddyName);
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
    const td::td_api::user *user = m_data.getUser(userId);
    if (user) {
        std::string userName = getPurpleUserName(*user);
        purple_prpl_got_user_status(m_account, userName.c_str(), getPurpleStatusId(*status), NULL);
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

    const td::td_api::user *user = m_data.getUser(userId);
    const td::td_api::chat *chat = m_data.getPrivateChatByUserId(userId);

    // In case user gets renamed in another client maybe?
    // For chats, find_chat doesn't work if account is not yet connected, so just in case, don't
    // user find_buddy either
    if (user && chat && purple_account_is_connected(m_account))
        updatePrivateChat(*chat, *user);
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

    // For chats, find_chat doesn't work if account is not yet connected, so just in case, don't
    // user find_buddy either
    if (privateChatUser && purple_account_is_connected(m_account))
    {
        updatePrivateChat(*chat, *privateChatUser);
    }

    m_data.addChat(std::move(chat));

    // purple_blist_find_chat doesn't work if account is not connected
    if (basicGroupId && purple_account_is_connected(m_account)) {
        requestBasicGroupMembers(basicGroupId);
        updateBasicGroupChat(basicGroupId);
    }
    if (supergroupId && purple_account_is_connected(m_account))
        updateSupergroupChat(supergroupId);
}

void PurpleTdClient::handleUserChatAction(const td::td_api::updateUserChatAction &updateChatAction)
{
    const td::td_api::chat *chat;
    chat = m_data.getChat(updateChatAction.chat_id_);

    if (!chat)
        purple_debug_warning(config::pluginId, "Got user chat action for unknown chat %" G_GUINT64_FORMAT "\n",
                             updateChatAction.chat_id_);
    else if (chat->type_->get_id() == td::td_api::chatTypePrivate::ID) {
        const td::td_api::chatTypePrivate &privType = static_cast<const td::td_api::chatTypePrivate &>(*chat->type_);
        if (privType.user_id_ != updateChatAction.user_id_)
            purple_debug_warning(config::pluginId, "Got user action for private chat %" G_GUINT64_FORMAT " (with user %d) for another user %d\n",
                                 updateChatAction.chat_id_, privType.user_id_,
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
        purple_debug_misc(config::pluginId, "Ignoring user chat action for non-private chat %" G_GUINT64_FORMAT "\n",
                          updateChatAction.chat_id_);
}

void PurpleTdClient::showUserChatAction(int32_t userId, bool isTyping)
{
    const td::td_api::user *user = m_data.getUser(userId);
    if (user) {
        std::string userName = getPurpleUserName(*user);
        if (isTyping)
            serv_got_typing(purple_account_get_connection(m_account),
                            userName.c_str(), REMOTE_TYPING_NOTICE_TIMEOUT,
                            PURPLE_TYPING);
        else
            serv_got_typing_stopped(purple_account_get_connection(m_account),
                                    userName.c_str());
    }
}

void PurpleTdClient::addContact(const std::string &phoneNumber, const std::string &alias,
                                const std::string &groupName)
{
    if (m_data.getUserByPhone(phoneNumber.c_str())) {
        purple_debug_info(config::pluginId, "User with phone number %s already exists\n", phoneNumber.c_str());
        return;
    }

    td::td_api::object_ptr<td::td_api::contact> contact =
        td::td_api::make_object<td::td_api::contact>(phoneNumber, "", "", "", 0);
    td::td_api::object_ptr<td::td_api::importContacts> importReq =
        td::td_api::make_object<td::td_api::importContacts>();
    importReq->contacts_.push_back(std::move(contact));
    uint64_t requestId = m_transceiver.sendQuery(std::move(importReq),
                                                 &PurpleTdClient::importContactResponse);

    m_data.addPendingRequest<ContactRequest>(requestId, phoneNumber, alias, groupName, 0);
}

void PurpleTdClient::importContactResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    std::unique_ptr<ContactRequest> request = m_data.getPendingRequest<ContactRequest>(requestId);
    if (!request)
        return;

    int32_t userId = 0;
    if (object->get_id() == td::td_api::importedContacts::ID) {
        td::td_api::object_ptr<td::td_api::importedContacts> reply =
            td::move_tl_object_as<td::td_api::importedContacts>(object);
        if (!reply->user_ids_.empty())
            userId = reply->user_ids_[0];
    }

    // For whatever reason, complaining at an earlier stage leads to error message not being shown in pidgin
    if (!isPhoneNumber(request->phoneNumber.c_str()))
        notifyFailedContact(request->phoneNumber, _("Not a valid phone number"));
    else if (userId) {
        td::td_api::object_ptr<td::td_api::contact> contact =
            td::td_api::make_object<td::td_api::contact>(request->phoneNumber, request->alias, "", "", userId);
        td::td_api::object_ptr<td::td_api::addContact> addContact =
            td::td_api::make_object<td::td_api::addContact>(std::move(contact), true);
        uint64_t newRequestId = m_transceiver.sendQuery(std::move(addContact),
                                                        &PurpleTdClient::addContactResponse);
        m_data.addPendingRequest<ContactRequest>(newRequestId, request->phoneNumber, request->alias,
                                                 request->groupName, userId);
    } else
        notifyFailedContact(request->phoneNumber, _("User not found"));
}

void PurpleTdClient::addContactResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    std::unique_ptr<ContactRequest> request = m_data.getPendingRequest<ContactRequest>(requestId);
    if (!request)
        return;

    if (object->get_id() == td::td_api::ok::ID) {
        td::td_api::object_ptr<td::td_api::createPrivateChat> createChat =
            td::td_api::make_object<td::td_api::createPrivateChat>(request->userId, false);
        uint64_t newRequestId = m_transceiver.sendQuery(std::move(createChat),
                                                        &PurpleTdClient::addContactCreatePrivateChatResponse);
        m_data.addPendingRequest(newRequestId, std::move(request));
    } else
        notifyFailedContact(request->phoneNumber, getDisplayedError(object));
}

void PurpleTdClient::addContactCreatePrivateChatResponse(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    std::unique_ptr<ContactRequest> request = m_data.getPendingRequest<ContactRequest>(requestId);
    if (!request)
        return;

    if (!object || (object->get_id() != td::td_api::chat::ID)) {
        purple_debug_misc(config::pluginId, "Failed to create private chat to %s\n",
                          request->phoneNumber.c_str());
        notifyFailedContact(request->phoneNumber, getDisplayedError(object));
    }
}

void PurpleTdClient::notifyFailedContact(const std::string &phoneNumber, const std::string &errorMessage)
{
    std::string message = formatMessage(_("Failed to add contact (phone number {}): {}"),
                                        {phoneNumber, errorMessage});

    purple_notify_error(purple_account_get_connection(m_account),
                        _("Failed to add contact"), message.c_str(), NULL);
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
        conv = getChatConversation(m_account, *chat, purpleId, m_data);
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

bool PurpleTdClient::joinChatByLink(const char *inviteLink)
{
    auto     request   = td::td_api::make_object<td::td_api::joinChatByInviteLink>(inviteLink);
    uint64_t requestId = m_transceiver.sendQuery(std::move(request), &PurpleTdClient::joinChatByLinkResponse);
    m_data.addPendingRequest<GroupJoinRequest>(requestId, inviteLink);

    return true;
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
