#include "client-utils.h"
#include "purple-info.h"
#include "config.h"
#include "format.h"
#include "file-transfer.h"
#include <string.h>
#include <stdlib.h>
#include <algorithm>
#include <functional>

enum {
    MAX_MESSAGE_PARTS = 10,
};

const char *errorCodeMessage()
{
    // TRANSLATOR: In-line error message, appears after a colon (':'), arguments will be a number and some error text from Telegram
    return _("code {0} ({1})");
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

std::string getUnsupportedMessageDescription(const td::td_api::MessageContent &content)
{
    // TRANSLSATOR: In-line placeholder when an unsupported message is being replied to.
    return formatMessage(_("Unsupported message type {}"), messageTypeToString(content));
}

std::string proxyTypeToString(PurpleProxyType proxyType)
{
    switch (proxyType) {
    case PURPLE_PROXY_NONE:
    case PURPLE_PROXY_USE_GLOBAL:
    case PURPLE_PROXY_USE_ENVVAR:
        return "unknown";
    case PURPLE_PROXY_HTTP:
        return "HTTP";
    case PURPLE_PROXY_SOCKS4:
        return "SOCKS4";
    case PURPLE_PROXY_SOCKS5:
        return "SOCKS5";
    case PURPLE_PROXY_TOR:
        return "TOR";
    }

    return "unknown";
}

const char *getPurpleStatusId(const td::td_api::UserStatus &tdStatus)
{
    if (tdStatus.get_id() == td::td_api::userStatusOnline::ID)
        return purple_primitive_get_id_from_type(PURPLE_STATUS_AVAILABLE);
    else
        return purple_primitive_get_id_from_type(PURPLE_STATUS_AWAY);
}

std::string getPurpleBuddyName(const td::td_api::user &user)
{
    // Prepend "id" so it's not accidentally equal to our phone number which is account name
    return "id" + std::to_string(user.id_);
}

std::string getSecretChatBuddyName(SecretChatId secretChatId)
{
    return "secret" + std::to_string(secretChatId.value());
}

std::vector<const td::td_api::user *> getUsersByPurpleName(const char *buddyName, TdAccountData &account,
                                                           const char *action)
{
    std::vector<const td::td_api::user *> result;

    UserId userId = purpleBuddyNameToUserId(buddyName);
    if (userId.valid()) {
        const td::td_api::user *tdUser = account.getUser(userId);
        if (tdUser != nullptr)
            result.push_back(tdUser);
        else if (action)
            purple_debug_warning(config::pluginId, "Cannot %s: no user with id %s\n", action, buddyName);
    } else {
        account.getUsersByDisplayName(buddyName, result);
        if (action) {
            if (result.empty())
                purple_debug_warning(config::pluginId, "Cannot %s: no user with display name '%s'\n",
                                    action, buddyName);
            else if (result.size() != 1)
                purple_debug_warning(config::pluginId, "Cannot %s: more than one user with display name '%s'\n",
                                    action, buddyName);
        }
    }

    return result;
}

PurpleConversation *getImConversation(PurpleAccount *account, const char *username)
{
    PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, username, account);
    if (conv == NULL)
        conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, account, username);

    return conv;
}

PurpleConvChat *getChatConversation(TdAccountData &account, const td::td_api::chat &chat,
                                    int chatPurpleId)
{
    std::string chatName       = getPurpleChatName(chat);
    bool        newChatCreated = false;

    // If account logged off with chats open, these chats will be purple_conv_chat_left()'d but not
    // purple_conversation_destroy()'d by purple_connection_destroy. So when logging back in,
    // conversation will exist but not necessarily with correct libpurple id. Therefore, lookup by
    // libpurple id using purple_find_chat cannot be used.
    PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT, chatName.c_str(),
                                                                     account.purpleAccount);

    // Such pre-open chats will (unless some other logic intervenes) be inactive (as in,
    // purple_conv_chat_has_left returns true) and if that's the case, serv_got_joined_chat must
    // still be called to make them active and thus able to send or receive messages, because that's
    // the kind of thing we were called for here.
    if ((conv == NULL) || purple_conv_chat_has_left(purple_conversation_get_chat_data(conv))) {
        if (chatPurpleId != 0) {
            purple_debug_misc(config::pluginId, "Creating conversation for chat %s (purple id %d)\n",
                              chat.title_.c_str(), chatPurpleId);
            serv_got_joined_chat(purple_account_get_connection(account.purpleAccount), chatPurpleId, chatName.c_str());
            conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT, chatName.c_str(),
                                                         account.purpleAccount);
            if (conv == NULL)
                purple_debug_warning(config::pluginId, "Did not create conversation for chat %s\n", chat.title_.c_str());
            else {
                // Sometimes when the group has just been created, or we left it and then got
                // messageChatDeleteMember, the chat will not be in buddy list. In that case,
                // libpurpleis going to use chatXXXXXXXXXXX as chat title. Set chat title explicitly
                // to prevent that.
                PurpleChat *purpleChat = purple_blist_find_chat(account.purpleAccount, chatName.c_str());
                if (!purpleChat) {
                    purple_debug_misc(config::pluginId, "Setting conversation title to '%s'\n", chat.title_.c_str());
                    purple_conversation_set_title(conv, chat.title_.c_str());
                }
                newChatCreated = true;
            }

        } else
            purple_debug_warning(config::pluginId, "No internal ID for chat %s\n", chat.title_.c_str());
    }

    if (conv) {
        PurpleConvChat *purpleChat = purple_conversation_get_chat_data(conv);

        if (purpleChat && newChatCreated) {
            BasicGroupId                          basicGroupId = getBasicGroupId(chat);
            const td::td_api::basicGroupFullInfo *groupInfo    = basicGroupId.valid() ? account.getBasicGroupInfo(basicGroupId) : nullptr;
            if (groupInfo)
                updateChatConversation(purpleChat, *groupInfo, account);

            SupergroupId supergroupId = getSupergroupId(chat);
            if (supergroupId.valid()) {
                const td::td_api::supergroupFullInfo *supergroupInfo = account.getSupergroupInfo(supergroupId);
                const td::td_api::chatMembers        *members        = account.getSupergroupMembers(supergroupId);
                if (supergroupInfo)
                    updateChatConversation(purpleChat, *supergroupInfo, account);
                if (members)
                    updateSupergroupChatMembers(purpleChat, *members, account);
            }
        }

        return purpleChat;
    }

    return NULL;
}

PurpleConvChat *findChatConversation(PurpleAccount *account, const td::td_api::chat &chat)
{
    std::string         name = getPurpleChatName(chat);
    PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT,
                                                                     name.c_str(), account);
    if (conv)
        return purple_conversation_get_chat_data(conv);
    return NULL;
}

void updatePrivateChat(TdAccountData &account, const td::td_api::chat *chat, const td::td_api::user &user)
{
    std::string purpleUserName = getPurpleBuddyName(user);
    std::string alias          = chat ? chat->title_ : makeBasicDisplayName(user);

    PurpleBuddy *buddy = purple_find_buddy(account.purpleAccount, purpleUserName.c_str());
    if (buddy == NULL) {
        purple_debug_misc(config::pluginId, "Adding new buddy %s for user %s\n",
                          alias.c_str(), purpleUserName.c_str());

        const ContactRequest *contactReq = account.findContactRequest(getId(user));
        PurpleGroup          *group      = (contactReq && !contactReq->groupName.empty()) ?
                                           purple_find_group(contactReq->groupName.c_str()) : NULL;
        if (group)
            purple_debug_misc(config::pluginId, "Adding into group %s\n", purple_group_get_name(group));

        buddy = purple_buddy_new(account.purpleAccount, purpleUserName.c_str(), alias.c_str());
        purple_blist_add_buddy(buddy, NULL, group, NULL);
        // If a new buddy has been added here, it means that there was updateNewChat with the private
        // chat. This means either we added them to contacts or started messaging them, or they
        // messaged us. Either way, there is no need to for any extra notification about new contact
        // because the user will be aware anyway.

        // Now, in case this buddy resulted from sending a message to group chat member or from
        // someone new messaging us
        std::string displayName = account.getDisplayName(user);
        PurpleConversation *oldConv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, displayName.c_str(),
                                                                            account.purpleAccount);
        if (oldConv) {
            purple_conv_im_write(purple_conversation_get_im_data(oldConv), "",
                                 // TRANSLATOR: In-chat status update
                                 _("Future messages in this conversation will be shown in a different tab"),
                                 PURPLE_MESSAGE_SYSTEM, time(NULL));
        }
    } else {
        purple_blist_alias_buddy(buddy, alias.c_str());

        const char *oldPhotoIdStr = purple_blist_node_get_string(PURPLE_BLIST_NODE(buddy), BuddyOptions::ProfilePhotoId);
        int64_t     oldPhotoId    = 0;
        if (oldPhotoIdStr)
            sscanf(oldPhotoIdStr, "%" G_GINT64_FORMAT, &oldPhotoId);
        if (user.profile_photo_ && user.profile_photo_->small_)
        {
            const td::td_api::file &photo = *user.profile_photo_->small_;
            if (photo.local_ && photo.local_->is_downloading_completed_ &&
                (user.profile_photo_->id_ != oldPhotoId))
            {
                gchar  *img = NULL;
                size_t  len;
                GError *err = NULL;
                g_file_get_contents(photo.local_->path_.c_str(), &img, &len, &err);
                if (err) {
                    purple_debug_warning(config::pluginId, "Failed to load profile photo %s for %s: %s\n",
                                         photo.local_->path_.c_str(), purpleUserName.c_str(),  err->message);
                    g_error_free(err);
                } else {
                    std::string newPhotoIdStr = std::to_string(user.profile_photo_->id_);
                    purple_blist_node_set_string(PURPLE_BLIST_NODE(buddy), BuddyOptions::ProfilePhotoId,
                                                 newPhotoIdStr.c_str());
                    purple_debug_info(config::pluginId, "Loaded new profile photo for %s (id %s)\n",
                                      purpleUserName.c_str(), newPhotoIdStr.c_str());
                    purple_buddy_icons_set_for_user(account.purpleAccount, purpleUserName.c_str(),
                                                    img, len, NULL);
                }
            }
        } else if (oldPhotoId) {
            purple_debug_info(config::pluginId, "Removing profile photo from %s\n", purpleUserName.c_str());
            purple_blist_node_remove_setting(PURPLE_BLIST_NODE(buddy), BuddyOptions::ProfilePhotoId);
            purple_buddy_icons_set_for_user(account.purpleAccount, purpleUserName.c_str(), NULL, 0, NULL);
        }
    }
}

static void updateGroupChat(TdAccountData &account, const td::td_api::chat &chat,
                            const td::td_api::object_ptr<td::td_api::ChatMemberStatus> &groupStatus,
                            const char *groupType, int32_t groupId)
{
    if (!isGroupMember(groupStatus)) {
        purple_debug_misc(config::pluginId, "Skipping %s %d because we are not a member\n",
                          groupType, groupId);
        return;
    }

    std::string  chatName   = getPurpleChatName(chat);
    PurpleChat  *purpleChat = purple_blist_find_chat(account.purpleAccount, chatName.c_str());
    if (!purpleChat) {
        purple_debug_misc(config::pluginId, "Adding new chat for %s %d (%s)\n",
                          groupType, groupId, chat.title_.c_str());
        purpleChat = purple_chat_new(account.purpleAccount, chat.title_.c_str(), getChatComponents(chat));
        purple_blist_add_chat(purpleChat, NULL, NULL);
    } else {
        const char *oldName = purple_chat_get_name(purpleChat);
        if (chat.title_ != oldName) {
            purple_debug_misc(config::pluginId, "Renaming chat '%s' to '%s'\n", oldName, chat.title_.c_str());
            purple_blist_alias_chat(purpleChat, chat.title_.c_str());
        }
    }

    if (account.isExpectedChat(getId(chat))) {
        PurpleConversation *baseConv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT,
                                                                            chatName.c_str(), account.purpleAccount);
        if (baseConv && purple_conv_chat_has_left(purple_conversation_get_chat_data(baseConv))) {
            purple_debug_misc(config::pluginId, "Rejoining chat %s as previously requested\n", chatName.c_str());
            serv_got_joined_chat(purple_account_get_connection(account.purpleAccount),
                                 account.getPurpleChatId(getId(chat)), chatName.c_str());
        }
        account.removeExpectedChat(getId(chat));
    }

    const char *oldPhotoId = purple_blist_node_get_string(PURPLE_BLIST_NODE(purpleChat), BuddyOptions::ProfilePhotoId);
    if (chat.photo_ && chat.photo_->small_)
    {
        const td::td_api::file &photo = *chat.photo_->small_;
        if (photo.local_ && photo.local_->is_downloading_completed_ && photo.remote_ &&
            !photo.remote_->unique_id_.empty() && (!oldPhotoId || (photo.remote_->unique_id_ != oldPhotoId)))
        {
            gchar  *img = NULL;
            size_t  len;
            GError *err = NULL;
            g_file_get_contents(photo.local_->path_.c_str(), &img, &len, &err);
            if (err) {
                purple_debug_warning(config::pluginId, "Failed to load chat photo %s for %s: %s\n",
                                        photo.local_->path_.c_str(), chat.title_.c_str(),  err->message);
                g_error_free(err);
            } else {
                purple_blist_node_set_string(PURPLE_BLIST_NODE(purpleChat), BuddyOptions::ProfilePhotoId,
                                             photo.remote_->unique_id_.c_str());
                purple_debug_info(config::pluginId, "Loaded new chat photo for %s (id %s)\n",
                                  chat.title_.c_str(), photo.remote_->unique_id_.c_str());
                purple_buddy_icons_node_set_custom_icon(PURPLE_BLIST_NODE(purpleChat),
                                                        reinterpret_cast<guchar *>(img), len);
            }
        }
    } else if (oldPhotoId) {
        purple_debug_info(config::pluginId, "Removing chat photo from %s\n", chat.title_.c_str());
        purple_blist_node_remove_setting(PURPLE_BLIST_NODE(purpleChat), BuddyOptions::ProfilePhotoId);
        purple_buddy_icons_node_set_custom_icon(PURPLE_BLIST_NODE(purpleChat), NULL, 0);
    }
}

void updateBasicGroupChat(TdAccountData &account, BasicGroupId groupId)
{
    const td::td_api::basicGroup *group = account.getBasicGroup(groupId);
    const td::td_api::chat       *chat  = account.getBasicGroupChatByGroup(groupId);

    if (!group)
        purple_debug_misc(config::pluginId, "Basic group %d does not exist yet\n", groupId.value());
    else if (!chat)
        purple_debug_misc(config::pluginId, "Chat for basic group %d does not exist yet\n", groupId.value());
    else
        updateGroupChat(account, *chat, group->status_, "basic group", groupId.value());
}

void updateSupergroupChat(TdAccountData &account, SupergroupId groupId)
{
    const td::td_api::supergroup *group = account.getSupergroup(groupId);
    const td::td_api::chat       *chat  = account.getSupergroupChatByGroup(groupId);

    if (!group)
        purple_debug_misc(config::pluginId, "Supergroup %d does not exist yet\n", groupId.value());
    else if (!chat)
        purple_debug_misc(config::pluginId, "Chat for supergroup %d does not exist yet\n", groupId.value());
    else
        updateGroupChat(account, *chat, group->status_, "supergroup", groupId.value());
}

void removeGroupChat(PurpleAccount *purpleAccount, const td::td_api::chat &chat)
{
    std::string  chatName   = getPurpleChatName(chat);
    PurpleChat  *purpleChat = purple_blist_find_chat(purpleAccount, chatName.c_str());

    if (purpleChat)
        purple_blist_remove_chat(purpleChat);
}

static PurpleMessageFlags getNotificationFlags(PurpleMessageFlags extraFlags)
{
    unsigned flags = (extraFlags & PURPLE_MESSAGE_ERROR) | (extraFlags & PURPLE_MESSAGE_NO_LOG);
    if (flags == 0)
        flags = PURPLE_MESSAGE_SYSTEM;

    return (PurpleMessageFlags)flags;
}

void showMessageTextIm(TdAccountData &account, const char *purpleUserName, const char *text,
                       const char *notification, time_t timestamp, PurpleMessageFlags flags)
{
    PurpleConversation *conv = NULL;

    if (text) {
        if (flags & PURPLE_MESSAGE_SEND) {
            // serv_got_im seems to work for messages sent from another client, but not for
            // echoed messages from this client. Therefore, this (code snippet from facebook plugin).
            conv = getImConversation(account.purpleAccount, purpleUserName);
            purple_conv_im_write(purple_conversation_get_im_data(conv),
                                 purple_account_get_name_for_display(account.purpleAccount),
                                 text, flags, timestamp);
        } else {
            serv_got_im(purple_account_get_connection(account.purpleAccount), purpleUserName, text,
                        flags, timestamp);
        }
    }

    if (notification) {
        if (conv == NULL)
            conv = getImConversation(account.purpleAccount, purpleUserName);
        purple_conv_im_write(purple_conversation_get_im_data(conv), purpleUserName, notification,
                             getNotificationFlags(flags), timestamp);
    }
}

static void showMessageTextChat(TdAccountData &account, const td::td_api::chat &chat,
                                const TgMessageInfo &message, const char *text,
                                const char *notification, PurpleMessageFlags flags)
{
    // Again, doing what facebook plugin does
    int purpleId = account.getPurpleChatId(getId(chat));
    PurpleConvChat *conv = getChatConversation(account, chat, purpleId);

    if (text) {
        if (flags & PURPLE_MESSAGE_SEND) {
            if (conv)
                purple_conv_chat_write(conv, purple_account_get_name_for_display(account.purpleAccount),
                                       text, flags, message.timestamp);
        } else {
            if (purpleId != 0)
                serv_got_chat_in(purple_account_get_connection(account.purpleAccount), purpleId,
                                 message.incomingGroupchatSender.empty() ? "someone" : message.incomingGroupchatSender.c_str(),
                                 flags, text, message.timestamp);
        }
    }

    if (notification) {
        if (conv)
            // Protocol plugins mostly use who="" for such messages, but this currently causes problems
            // with Spectrum. Use some non-empty string. Pidgin will ignore the who parameter for
            // notification messages.
            purple_conv_chat_write(conv, " ", notification, getNotificationFlags(flags), message.timestamp);
    }
}

std::string getMessageText(const td::td_api::formattedText &text)
{
    char *newText = purple_markup_escape_text(text.text_.c_str(), text.text_.size());
    std::string result(newText);
    g_free(newText);
    return result;
}

std::string makeInlineImageText(int imgstoreId)
{
    return "\n<img id=\"" + std::to_string(imgstoreId) + "\">";
}

static std::string quoteMessage(const td::td_api::message *message, TdAccountData &account)
{
    const td::td_api::user *originalAuthor = nullptr;
    if (message)
        originalAuthor = account.getUser(getSenderUserId(*message));

    std::string originalName;
    if (originalAuthor)
        originalName = account.getDisplayName(*originalAuthor);
    else {
        // message == NULL means it could not be fetched, or took too long to fetch
        // TRANSLATOR: In-line placeholder if the original author of a quote is unknown. Is at the beginning of the line if and only if you make it so, see "<b>&bt {} wrote:"...
        originalName = _("Unknown user");
    }

    std::string text;
    if (!message || !message->content_) {
        // TRANSLATOR: In-chat placeholder when something unknown is being replied to.
        text = _("[message unavailable]");
    } else switch (message->content_->get_id()) {
        case td::td_api::messageText::ID: {
            const td::td_api::messageText &messageText = static_cast<const td::td_api::messageText &>(*message->content_);
            if (messageText.text_)
                text = getMessageText(*messageText.text_);
            else
                text = "";
            break;
        }
        case td::td_api::messagePhoto::ID: {
            const td::td_api::messagePhoto &photo = static_cast<const td::td_api::messagePhoto &>(*message->content_);
            // TRANSLATOR: In-line placeholder when a photo is being replied to.
            text = _("[photo]");
            if (photo.caption_)
                text += " " + photo.caption_->text_;
            break;
        }
        case td::td_api::messageDocument::ID: {
            const td::td_api::messageDocument &document = static_cast<const td::td_api::messageDocument &>(*message->content_);
            if (document.document_) {
                // TRANSLATOR: In-line placeholder when a file is being replied to. Arguments will be the file name and MIME type (e.g. "application/gzip")
                text = formatMessage(_("[file: {0} ({1})]"), {document.document_->file_name_,
                                                           document.document_->mime_type_});
            } else {
                // Not supposed to be possible, but just in case
                text = "[file]";
            }
            if (document.caption_)
                text += " " + document.caption_->text_;
            break;
        }
        case td::td_api::messageVideo::ID: {
            const td::td_api::messageVideo &video = static_cast<const td::td_api::messageVideo &>(*message->content_);
            if (video.video_) {
                // TRANSLATOR: In-line placeholder when a video is being replied to. Argument will be the file name.
                text = formatMessage(_("[video: {}]"), video.video_->file_name_);
            } else {
                // Not supposed to be possible, but just in case
                text = "[video]";
            }
            if (video.caption_)
                text += " " + video.caption_->text_;
            break;
        }
        case td::td_api::messageSticker::ID:
            // TRANSLATOR: In-line placeholder when a sticker is being replied to.
            text = _("[sticker]");
            break;
        default:
            text = '[' + getUnsupportedMessageDescription(*message->content_) + ']';
            break;
    }

    for (unsigned i = 0; i < text.size(); i++)
        if (text[i] == '\n') text[i] = ' ';

    // TRANSLATOR: In-chat notification of a reply. Arguments will be username and the original text or description thereof. Please preserve the HTML.
    return formatMessage(_("<b>&gt; {0} wrote:</b>\n&gt; {1}"), {originalName, text});
}

void showMessageText(TdAccountData &account, const td::td_api::chat &chat, const TgMessageInfo &message,
                     const char *text, const char *notification, uint32_t extraFlags)
{
    PurpleMessageFlags directionFlag = message.outgoing ? PURPLE_MESSAGE_SEND : PURPLE_MESSAGE_RECV;
    PurpleMessageFlags flags = (PurpleMessageFlags) (extraFlags | directionFlag);
    if (message.outgoing && !message.sentLocally)
        flags = (PurpleMessageFlags) (flags | PURPLE_MESSAGE_REMOTE_SEND);

    std::string newText;
    if (message.repliedMessageId != 0)
        newText = quoteMessage(message.repliedMessage.get(), account);
    if (!message.forwardedFrom.empty()) {
        if (!newText.empty())
            newText += "\n";
        // TRANSLATOR: In-chat notification of forward. Argument will be a username. Please preserve the HTML.
        newText += formatMessage(_("<b>Forwarded from {}:</b>"), message.forwardedFrom);
    }
    if (text) {
        if (!newText.empty())
            newText += "\n";
        newText += text;
    }
    if (!newText.empty())
        text = newText.c_str();

    const td::td_api::user *privateUser = account.getUserByPrivateChat(chat);
    if (privateUser) {
        std::string userName;

        // If the chat is in contact list telegram-wise, there should be a buddy for it.
        // If not, libpurple won't be able to translate buddy name to alias, so use display name
        // instead of idXXXXXXXXX
        if (isChatInContactList(chat, privateUser))
            userName = getPurpleBuddyName(*privateUser);
        else
            userName = account.getDisplayName(*privateUser);
        showMessageTextIm(account, userName.c_str(), text, notification, message.timestamp, flags);
    }

    SecretChatId secretChatId = getSecretChatId(chat);
    if (secretChatId.valid()) {
        std::string userName = getSecretChatBuddyName(secretChatId);
        showMessageTextIm(account, userName.c_str(), text, notification, message.timestamp, flags);
    }

    if (getBasicGroupId(chat).valid() || getSupergroupId(chat).valid())
        showMessageTextChat(account, chat, message, text, notification, flags);
}

void showChatNotification(TdAccountData &account, const td::td_api::chat &chat,
                          const char *notification, PurpleMessageFlags flags)
{
    TgMessageInfo messageInfo;
    messageInfo.type = TgMessageInfo::Type::Other;
    messageInfo.timestamp = (flags == PURPLE_MESSAGE_NO_LOG) ? 0 : time(NULL);
    messageInfo.outgoing = true;
    showMessageText(account, chat, messageInfo, NULL, notification, flags);
}

std::string makeBasicDisplayName(const td::td_api::user &user)
{
    std::string result = user.first_name_;
    if (!result.empty() && !user.last_name_.empty())
        result += ' ';
    result += user.last_name_;

    return result;
}

std::string getIncomingGroupchatSenderPurpleName(const td::td_api::chat &chat, const td::td_api::message &message,
                                                 TdAccountData &account)
{
    if (!message.is_outgoing_ && (getBasicGroupId(chat).valid() || getSupergroupId(chat).valid())) {
        UserId senderId = getSenderUserId(message);
        if (senderId.valid())
            return account.getDisplayName(senderId);
        else if (!message.author_signature_.empty())
            return message.author_signature_;
        else if (message.is_channel_post_) {
            // TRANSLATOR: The "sender" of a message that was posted to a channel. Will be used like a username.
            return _("Channel post");
        } else if (message.forward_info_ && message.forward_info_->origin_)
            switch (message.forward_info_->origin_->get_id()) {
            case td::td_api::messageForwardOriginUser::ID:
                return account.getDisplayName(getSenderUserId(static_cast<const td::td_api::messageForwardOriginUser &>(*message.forward_info_->origin_)));
            case td::td_api::messageForwardOriginHiddenUser::ID:
                return static_cast<const td::td_api::messageForwardOriginHiddenUser &>(*message.forward_info_->origin_).sender_name_;
            case td::td_api::messageForwardOriginChannel::ID:
                return static_cast<const td::td_api::messageForwardOriginChannel&>(*message.forward_info_->origin_).author_signature_;
            }
    }

    // For outgoing messages, our name will be used instead
    // For private and secret chats, sender name will be determined from the chat instead

    return "";
}

std::string getForwardSource(const td::td_api::messageForwardInfo &forwardInfo,
                             TdAccountData &account)
{
    if (!forwardInfo.origin_)
        return "";

    switch (forwardInfo.origin_->get_id()) {
        case td::td_api::messageForwardOriginUser::ID:
            return account.getDisplayName(getSenderUserId(static_cast<const td::td_api::messageForwardOriginUser &>(*forwardInfo.origin_)));
        case td::td_api::messageForwardOriginHiddenUser::ID:
            return static_cast<const td::td_api::messageForwardOriginHiddenUser &>(*forwardInfo.origin_).sender_name_;
        case td::td_api::messageForwardOriginChannel::ID: {
            const td::td_api::chat *chat = account.getChat(getChatId(static_cast<const td::td_api::messageForwardOriginChannel&>(*forwardInfo.origin_)));
            if (chat)
                return chat->title_;
        }
    }

    return "";
}

void getNamesFromAlias(const char *alias, std::string &firstName, std::string &lastName)
{
    if (!alias) alias = "";

    const char *name1end = alias;
    while (*name1end && isspace(*name1end)) name1end++;
    while (*name1end && !isspace(*name1end)) name1end++;
    firstName = std::string(alias, name1end-alias);

    const char *name2start = name1end;
    while (*name2start && isspace(*name2start)) name2start++;
    lastName = name2start;
}

static void findChatsByComponents(PurpleBlistNode *node,
                                  const char *joinString, const char *groupName, int groupType,
                                  std::vector<PurpleChat *> &result)
{
    PurpleBlistNodeType nodeType = purple_blist_node_get_type(node);

    if (nodeType == PURPLE_BLIST_CHAT_NODE) {
        PurpleChat *chat           = PURPLE_CHAT(node);
        GHashTable *components     = purple_chat_get_components(chat);
        const char *nodeName       = getChatName(components);
        const char *nodeJoinString = getChatJoinString(components);
        const char *nodeGroupName  = getChatGroupName(components);
        int         nodeGroupType  = getChatGroupType(components);

        if (!nodeName) nodeName = "";
        if (!nodeJoinString) nodeJoinString = "";
        if (!nodeGroupName) nodeGroupName = "";

        if (!strcmp(nodeName, "") && !strcmp(nodeJoinString, joinString)) {
            if ((*joinString != '\0') ||
                (!strcmp(nodeGroupName, groupName) && (nodeGroupType == groupType)))
            {
                result.push_back(chat);
            }
        }
    }

    for (PurpleBlistNode *child = purple_blist_node_get_first_child(node); child;
         child = purple_blist_node_get_sibling_next(child))
    {
        findChatsByComponents(child, joinString, groupName, groupType, result);
    }
}

std::vector<PurpleChat *>findChatsByJoinString(const std::string &joinString)
{
    std::vector<PurpleChat *> result;

    for (PurpleBlistNode *root = purple_blist_get_root(); root;
         root = purple_blist_node_get_sibling_next(root)) // LOL
    {
        findChatsByComponents(root, joinString.c_str(), "", 0, result);
    }

    return result;
}

std::vector<PurpleChat *> findChatsByNewGroup(const char *name, int type)
{
    std::vector<PurpleChat *> result;

    for (PurpleBlistNode *root = purple_blist_get_root(); root;
         root = purple_blist_node_get_sibling_next(root)) // LOL
    {
        findChatsByComponents(root, "", name, type, result);
    }

    return result;
}

static void setChatMembers(PurpleConvChat *purpleChat,
                           const std::vector<td::td_api::object_ptr<td::td_api::chatMember>> &members,
                           const TdAccountData &account)
{
    GList *flags = NULL;
    std::vector<std::string> nameData;

    for (const auto &member: members) {
        if (!member || !isGroupMember(member->status_))
            continue;

        const td::td_api::user *user = account.getUser(getUserId(*member));
        if (!user || (user->type_ && (user->type_->get_id() == td::td_api::userTypeDeleted::ID)))
            continue;

        std::string userName    = getPurpleBuddyName(*user);
        const char *phoneNumber = getCanonicalPhoneNumber(user->phone_number_.c_str());
        if (purple_find_buddy(account.purpleAccount, userName.c_str()))
            // libpurple will be able to map user name to alias because there is a buddy
            nameData.emplace_back(userName);
        else if (!strcmp(getCanonicalPhoneNumber(purple_account_get_username(account.purpleAccount)), phoneNumber))
            // This is us, so again libpurple will map phone number to alias
            nameData.emplace_back(purple_account_get_username(account.purpleAccount));
        else {
            // Use first and last name instead
            std::string displayName = account.getDisplayName(*user);
            nameData.emplace_back(displayName);
        }

        PurpleConvChatBuddyFlags flag;
        if (member->status_->get_id() == td::td_api::chatMemberStatusCreator::ID)
            flag = PURPLE_CBFLAGS_FOUNDER;
        else if (member->status_->get_id() == td::td_api::chatMemberStatusAdministrator::ID)
            flag = PURPLE_CBFLAGS_OP;
        else
            flag = PURPLE_CBFLAGS_NONE;
        flags = g_list_append(flags, GINT_TO_POINTER(flag));
    }

    GList *names = NULL;
    for (const std::string &name: nameData)
        names = g_list_append(names, const_cast<char *>(name.c_str()));

    purple_conv_chat_clear_users(purpleChat);
    purple_conv_chat_add_users(purpleChat, names, NULL, flags, false);
    g_list_free(names);
    g_list_free(flags);
}

void updateChatConversation(PurpleConvChat *purpleChat, const td::td_api::basicGroupFullInfo &groupInfo,
                    const TdAccountData &account)
{
    purple_conv_chat_set_topic(purpleChat, NULL, groupInfo.description_.c_str());
    setChatMembers(purpleChat, groupInfo.members_, account);
}

void updateChatConversation(PurpleConvChat *purpleChat, const td::td_api::supergroupFullInfo &groupInfo,
                    const TdAccountData &account)
{
    purple_conv_chat_set_topic(purpleChat, NULL, groupInfo.description_.c_str());
}

void updateSupergroupChatMembers(PurpleConvChat* purpleChat, const td::td_api::chatMembers& members,
                                 const TdAccountData& account)
{
    setChatMembers(purpleChat, members.members_, account);
}

struct MessagePart {
    bool        isImage = false;
    int         imageId = 0;
    std::string text;
};

static size_t splitTextChunk(MessagePart &part, const char *text, size_t length, TdAccountData &account)
{
    enum {MIN_LENGTH_LIMIT = 8};
    unsigned lengthLimit = part.isImage ? account.options.maxCaptionLength : account.options.maxMessageLength;
    if (lengthLimit == 0)
        purple_debug_warning(config::pluginId, "No %s length limit\n", part.isImage ? "caption" : "message");
    else if (lengthLimit <= MIN_LENGTH_LIMIT)
        purple_debug_warning(config::pluginId, "%u is a ridiculous %s length limit\n",
                             lengthLimit, part.isImage ? "caption" : "message");
    if ((lengthLimit <= MIN_LENGTH_LIMIT) || (length <= lengthLimit)) {
        part.text = std::string(text, length);
        return length;
    }

    // Try to truncate at a line break, with no lower length limit in case of image caption
    unsigned newlineSplitLowerLimit = part.isImage ? 1 : lengthLimit/2;
    for (unsigned chunkLength = lengthLimit; chunkLength >= newlineSplitLowerLimit; chunkLength--)
        if (text[chunkLength-1] == '\n') {
            part.text = std::string(text, chunkLength-1);
            return chunkLength;
        }

    // Try to truncate to a whole number of utf-8 characters
    size_t      chunkLen;
    const char *pos = g_utf8_find_prev_char(text, text+lengthLimit);
    if (pos != NULL) {
        const char *next = g_utf8_find_next_char(pos, NULL);
        if (next == text+lengthLimit)
            chunkLen = lengthLimit;
        else
            chunkLen = pos-text;
    } else
        chunkLen = lengthLimit;
    part.text = std::string(text, chunkLen);
    return chunkLen;
}

static void appendText(std::vector<MessagePart> &parts, const char *s, size_t len, TdAccountData &account)
{
    if (len != 0) {
        if (parts.empty())
            parts.emplace_back();

        std::string sourceText(s, len);
        char *halfNewText = purple_markup_strip_html(sourceText.c_str());
        char *newText = purple_unescape_html(halfNewText);
        g_free(halfNewText);

        const char *remaining    = newText;
        size_t      lenRemaining = strlen(newText);
        while (lenRemaining) {
            size_t chunkLength = splitTextChunk(parts.back(), remaining, lenRemaining, account);
            lenRemaining -= chunkLength;
            remaining += chunkLength;
            if (lenRemaining)
                parts.emplace_back();
        }

        g_free(newText);
    }
}

static void parseMessage(const char *message, std::vector<MessagePart> &parts, TdAccountData &account)
{
    parts.clear();
    if (!message)
        return;

    const char *s         = message;
    const char *textStart = message;

    while (*s) {
        bool  isImage = false;
        long  imageId;
        char *pastImage = NULL;

        if (!strncasecmp(s, "<img id=\"", 9)) {
            const char *idString = s+9;

            imageId = strtol(idString, &pastImage, 10);
            if ((pastImage != idString) && !strncmp(pastImage, "\">", 2) && (imageId <= INT32_MAX) &&
                (imageId >= INT32_MIN))
            {
                isImage = true;
                pastImage += 2;
                if (*pastImage == '\n')
                    pastImage++;
            }
        }

        if (isImage) {
            appendText(parts, textStart, s-textStart, account);
            parts.emplace_back();
            parts.back().isImage = true;
            parts.back().imageId = imageId;
            s = pastImage;
            textStart = pastImage;
        } else
            s++;
    }

    appendText(parts, textStart, s-textStart, account);
}

int transmitMessage(ChatId chatId, const char *message, TdTransceiver &transceiver,
                    TdAccountData &account, TdTransceiver::ResponseCb response)
{
    std::vector<MessagePart> parts;
    parseMessage(message, parts, account);
    if (parts.size() > MAX_MESSAGE_PARTS)
        return -E2BIG;

    for (const MessagePart &input: parts) {
        td::td_api::object_ptr<td::td_api::sendMessage> sendMessageRequest = td::td_api::make_object<td::td_api::sendMessage>();
        sendMessageRequest->chat_id_ = chatId.value();
        char *tempFileName = NULL;
        bool  hasImage     = false;

        if (input.isImage)
            hasImage = saveImage(input.imageId, &tempFileName);

        if (hasImage) {
            td::td_api::object_ptr<td::td_api::inputMessagePhoto> content = td::td_api::make_object<td::td_api::inputMessagePhoto>();
            content->photo_ = td::td_api::make_object<td::td_api::inputFileLocal>(tempFileName);
            content->caption_ = td::td_api::make_object<td::td_api::formattedText>();
            content->caption_->text_ = input.text;

            sendMessageRequest->input_message_content_ = std::move(content);
            purple_debug_misc(config::pluginId, "Sending photo %s\n", tempFileName);
        } else {
            td::td_api::object_ptr<td::td_api::inputMessageText> content = td::td_api::make_object<td::td_api::inputMessageText>();
            content->text_ = td::td_api::make_object<td::td_api::formattedText>();
            content->text_->text_ = input.text;
            sendMessageRequest->input_message_content_ = std::move(content);
        }

        uint64_t requestId = transceiver.sendQuery(std::move(sendMessageRequest), response);
        account.addPendingRequest<SendMessageRequest>(requestId, chatId, tempFileName);
        if (tempFileName)
            g_free(tempFileName);
    }

    return 0;
}

std::string getSenderDisplayName(const td::td_api::chat &chat, const TgMessageInfo &message,
                                 PurpleAccount *account)
{
    if (message.outgoing)
        return purple_account_get_name_for_display(account);
    else if (isPrivateChat(chat) || getSecretChatId(chat).valid())
        return chat.title_;
    else
        return message.incomingGroupchatSender;
}

std::string getDownloadXferPeerName(ChatId chatId,
                                    const TgMessageInfo &message,
                                    TdAccountData &account)
{
    const td::td_api::chat *chat = account.getChat(chatId);
    if (chat) {
        const td::td_api::user *privateUser = account.getUserByPrivateChat(*chat);
        if (privateUser)
            return getPurpleBuddyName(*privateUser);
        auto secretChatId = getSecretChatId(*chat);
        if (secretChatId.valid())
            return getSecretChatBuddyName(secretChatId);
    }
    return message.incomingGroupchatSender;
}

std::string makeNoticeWithSender(const td::td_api::chat &chat, const TgMessageInfo &message,
                                 const char *noticeText, PurpleAccount *account)
{
    std::string prefix = getSenderDisplayName(chat, message, account);
    if (!prefix.empty())
        prefix += ": ";
    return prefix + noticeText;
}

void showGenericFileInline(const td::td_api::chat &chat, const TgMessageInfo &message,
                           const std::string &filePath, const std::string &fileDescription,
                           TdAccountData &account)
{
    if (filePath.find('"') != std::string::npos) {
        std::string notice = makeNoticeWithSender(chat, message, "Cannot show file: path contains quotes",
                                                    account.purpleAccount);
        showMessageText(account, chat, message, NULL, notice.c_str());
    } else {
        std::string text = "<a href=\"file://" + filePath + "\">" + fileDescription + "</a>";
        showMessageText(account, chat, message, text.c_str(), NULL);
    }
}

void notifySendFailed(const td::td_api::updateMessageSendFailed &sendFailed, TdAccountData &account)
{
    if (sendFailed.message_) {
        const td::td_api::chat *chat = account.getChat(getChatId(*sendFailed.message_));
        if (chat) {
            TgMessageInfo messageInfo;
            messageInfo.type = TgMessageInfo::Type::Other;
            messageInfo.timestamp = sendFailed.message_->date_;
            messageInfo.outgoing = true;
            std::string errorMessage = formatMessage(errorCodeMessage(), {std::to_string(sendFailed.error_code_),
                                                     sendFailed.error_message_});
            // TRANSLATOR: In-chat error message, argument will be text.
            errorMessage = formatMessage(_("Failed to send message: {}"), errorMessage);
            showMessageText(account, *chat, messageInfo, NULL, errorMessage.c_str());
        }
    }
}

void updateOption(const td::td_api::updateOption &option, TdAccountData &account)
{
    if ((option.name_ == "version") && option.value_ &&
        (option.value_->get_id() == td::td_api::optionValueString::ID))
    {
        purple_debug_misc(config::pluginId, "tdlib version: %s\n",
                            static_cast<const td::td_api::optionValueString &>(*option.value_).value_.c_str());
    } else if ((option.name_ == "message_caption_length_max") && option.value_ &&
        (option.value_->get_id() == td::td_api::optionValueInteger::ID))
    {
        account.options.maxCaptionLength = std::max(0, static_cast<const td::td_api::optionValueInteger &>(*option.value_).value_);
    } else if ((option.name_ == "message_text_length_max") && option.value_ &&
        (option.value_->get_id() == td::td_api::optionValueInteger::ID))
    {
        account.options.maxMessageLength = std::max(0, static_cast<const td::td_api::optionValueInteger &>(*option.value_).value_);
    } else
        purple_debug_misc(config::pluginId, "Option update %s\n", option.name_.c_str());
}

void populateGroupChatList(PurpleRoomlist *roomlist, const std::vector<const td::td_api::chat *> &chats,
                           const TdAccountData &account)
{
    for (const td::td_api::chat *chat: chats)
        if (account.isGroupChatWithMembership(*chat)) {
            PurpleRoomlistRoom *room = purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_ROOM,
                                                                chat->title_.c_str(), NULL);
            purple_roomlist_room_add_field (roomlist, room, getPurpleChatName(*chat).c_str());
            BasicGroupId groupId = getBasicGroupId(*chat);
            if (groupId.valid()) {
                const td::td_api::basicGroupFullInfo *fullInfo = account.getBasicGroupInfo(groupId);
                if (fullInfo && !fullInfo->description_.empty())
                    purple_roomlist_room_add_field(roomlist, room, fullInfo->description_.c_str());
            }
            SupergroupId supergroupId = getSupergroupId(*chat);
            if (supergroupId.valid()) {
                const td::td_api::supergroupFullInfo *fullInfo = account.getSupergroupInfo(supergroupId);
                if (fullInfo && !fullInfo->description_.empty())
                    purple_roomlist_room_add_field(roomlist, room, fullInfo->description_.c_str());
            }
            purple_roomlist_room_add(roomlist, room);
        }
    purple_roomlist_set_in_progress(roomlist, FALSE);
}

AccountThread::AccountThread(PurpleAccount* purpleAccount, AccountThread::Callback callback)
{
    m_accountUserName   = purple_account_get_username(purpleAccount);
    m_accountProtocolId = purple_account_get_protocol_id(purpleAccount);
    m_callback          = callback;
}

void AccountThread::threadFunc()
{
    run();
    g_idle_add(&AccountThread::mainThreadCallback, this);
}

static bool g_singleThread = false;

void AccountThread::setSingleThread()
{
    g_singleThread = true;
}

bool AccountThread::isSingleThread()
{
    return g_singleThread;
}

void AccountThread::startThread()
{
    if (!g_singleThread) {
        if (!m_thread.joinable())
            m_thread = std::thread(std::bind(&AccountThread::threadFunc, this));
    } else {
        run();
        mainThreadCallback(this);
    }
}

gboolean AccountThread::mainThreadCallback(gpointer data)
{
    AccountThread  *self     = static_cast<AccountThread *>(data);
    PurpleAccount  *account  = purple_accounts_find(self->m_accountUserName.c_str(),
                                                    self->m_accountProtocolId.c_str());
    PurpleTdClient *tdClient = account ? getTdClient(account) : nullptr;
    if (self->m_thread.joinable())
        self->m_thread.join();

    if (tdClient)
        (tdClient->*(self->m_callback))(self);

    return FALSE; // this idle callback will not be called again
}
