#include "client-utils.h"
#include "purple-info.h"
#include "config.h"
#include "format.h"
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "buildopt.h"
#ifndef NoWebp
#include <png.h>
#include <webp/decode.h>
#endif

enum {
    FILE_UPLOAD_PRIORITY = 1,
};

static const char *_(const char *s) { return s; }

std::string messageTypeToString(const td::td_api::MessageContent &content)
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
        return purple_primitive_get_id_from_type(PURPLE_STATUS_OFFLINE);
}

std::string getPurpleBuddyName(const td::td_api::user &user)
{
    // Prepend "id" so it's not accidentally equal to our phone number which is account name
    return "id" + std::to_string(user.id_);
}

void getUsersByPurpleName(const char *username, std::vector<const td::td_api::user*> &users,
                         TdAccountData &account)
{
    int32_t userId = stringToUserId(username);

    if (userId != 0) {
        users.clear();
        const td::td_api::user *user = account.getUser(userId);
        if (user)
            users.push_back(user);
    } else
        account.getUsersByDisplayName(username, users);
}

int64_t getPrivateChatIdByPurpleName(const char *buddyName, TdAccountData &account,
                                     const char *action)
{
    int32_t userId = stringToUserId(buddyName);
    if (userId == 0) {
        purple_debug_warning(config::pluginId, "Cannot %s: '%s' is not a valid user id\n", action, buddyName);
        return 0;
    }
    const td::td_api::user *tdUser = account.getUser(userId);
    if (tdUser == nullptr) {
        purple_debug_warning(config::pluginId, "Cannot %s: no user with id %s\n", action, buddyName);
        return 0;
    }
    const td::td_api::chat *tdChat = account.getPrivateChatByUserId(tdUser->id_);
    if (tdChat == nullptr) {
        purple_debug_warning(config::pluginId, "Cannot %s: no chat with user %s\n", action, buddyName);
        return 0;
    }
    return tdChat->id_;
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
    std::string chatName       = getChatName(chat);
    bool        newChatCreated = false;
    PurpleConversation *conv = purple_find_chat(purple_account_get_connection(account.purpleAccount), chatPurpleId);
    if (conv == NULL) {
        if (chatPurpleId != 0) {
            purple_debug_misc(config::pluginId, "Creating conversation for chat %s (purple id %d)\n",
                              chat.title_.c_str(), chatPurpleId);
            // when a message arrives during login, libpurple won't find the chat in contact
            // list because even it has been in the contact list since before, the chat lookup
            // doesn't work when account is not connected. Therefore, it won't know chat title and
            // would show chatXXXXXXXXXXX name in the conversation window instead.
            serv_got_joined_chat(purple_account_get_connection(account.purpleAccount), chatPurpleId, chatName.c_str());
            conv = purple_find_chat(purple_account_get_connection(account.purpleAccount), chatPurpleId);
            if (conv == NULL)
                purple_debug_warning(config::pluginId, "Did not create conversation for chat %s\n", chat.title_.c_str());
            else {
                //... so, fix conversation title if we can't find chat in contact list
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
        PurpleConvChat *purpleChat   = purple_conversation_get_chat_data(conv);

        if (purpleChat && newChatCreated) {
            int32_t                               basicGroupId = getBasicGroupId(chat);
            const td::td_api::basicGroupFullInfo *groupInfo    = basicGroupId ? account.getBasicGroupInfo(basicGroupId) : nullptr;
            if (groupInfo)
                setChatMembers(purpleChat, *groupInfo, account);
        }

        return purpleChat;
    }

    return NULL;
}

PurpleConvChat *findChatConversation(PurpleAccount *account, const td::td_api::chat &chat)
{
    std::string         name = getChatName(chat);
    PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT,
                                                                     name.c_str(), account);
    if (conv)
        return purple_conversation_get_chat_data(conv);
    return NULL;
}

void updatePrivateChat(TdAccountData &account, const td::td_api::chat &chat, const td::td_api::user &user)
{
    std::string purpleUserName = getPurpleBuddyName(user);

    PurpleBuddy *buddy = purple_find_buddy(account.purpleAccount, purpleUserName.c_str());
    if (buddy == NULL) {
        purple_debug_misc(config::pluginId, "Adding new buddy %s for user %s, chat id %" G_GINT64_FORMAT "\n",
                          chat.title_.c_str(), purpleUserName.c_str(), chat.id_);

        const ContactRequest *contactReq = account.findContactRequest(user.id_);
        PurpleGroup          *group      = (contactReq && !contactReq->groupName.empty()) ?
                                           purple_find_group(contactReq->groupName.c_str()) : NULL;
        if (group)
            purple_debug_misc(config::pluginId, "Adding into group %s\n", purple_group_get_name(group));

        buddy = purple_buddy_new(account.purpleAccount, purpleUserName.c_str(), chat.title_.c_str());
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
            /*PurpleGroup *group = purple_buddy_get_group(buddy);
            purple_blist_remove_buddy(buddy);
            buddy = purple_buddy_new(m_account, purpleUserName.c_str(), chat.title_.c_str());
            purple_blist_add_buddy(buddy, NULL, group, NULL);*/
            purple_blist_alias_buddy(buddy, chat.title_.c_str());
        }
    }
}

static void updateGroupChat(PurpleAccount *purpleAccount, const td::td_api::chat &chat,
                            const td::td_api::object_ptr<td::td_api::ChatMemberStatus> &groupStatus,
                            const char *groupType, int32_t groupId)
{
    if (!isGroupMember(groupStatus)) {
        purple_debug_misc(config::pluginId, "Skipping %s %d because we are not a member\n",
                          groupType, groupId);
        return;
    }

    std::string  chatName   = getChatName(chat);
    PurpleChat  *purpleChat = purple_blist_find_chat(purpleAccount, chatName.c_str());
    if (!purpleChat) {
        purple_debug_misc(config::pluginId, "Adding new chat for %s %d (%s)\n",
                          groupType, groupId, chat.title_.c_str());
        purpleChat = purple_chat_new(purpleAccount, chat.title_.c_str(), getChatComponents(chat));
        purple_blist_add_chat(purpleChat, NULL, NULL);
    } else {
        const char *oldName = purple_chat_get_name(purpleChat);
        if (chat.title_ != oldName) {
            purple_debug_misc(config::pluginId, "Renaming chat '%s' to '%s'\n", oldName, chat.title_.c_str());
            purple_blist_alias_chat(purpleChat, chat.title_.c_str());
        }
    }
}

void updateBasicGroupChat(TdAccountData &account, int32_t groupId)
{
    const td::td_api::basicGroup *group = account.getBasicGroup(groupId);
    const td::td_api::chat       *chat  = account.getBasicGroupChatByGroup(groupId);

    if (!group)
        purple_debug_misc(config::pluginId, "Basic group %d does not exist yet\n", groupId);
    else if (!chat)
        purple_debug_misc(config::pluginId, "Chat for basic group %d does not exist yet\n", groupId);
    else
        updateGroupChat(account.purpleAccount, *chat, group->status_, "basic group", groupId);
}

void updateSupergroupChat(TdAccountData &account, int32_t groupId)
{
    const td::td_api::supergroup *group = account.getSupergroup(groupId);
    const td::td_api::chat       *chat  = account.getSupergroupChatByGroup(groupId);

    if (!group)
        purple_debug_misc(config::pluginId, "Supergroup %d does not exist yet\n", groupId);
    else if (!chat)
        purple_debug_misc(config::pluginId, "Chat for supergroup %d does not exist yet\n", groupId);
    else
        updateGroupChat(account.purpleAccount, *chat, group->status_, "supergroup", groupId);
}

void removeGroupChat(PurpleAccount *purpleAccount, const td::td_api::chat &chat)
{
    std::string  chatName   = getChatName(chat);
    PurpleChat  *purpleChat = purple_blist_find_chat(purpleAccount, chatName.c_str());

    if (purpleChat)
        purple_blist_remove_chat(purpleChat);
}

static void showMessageTextIm(TdAccountData &account, const char *purpleUserName, const char *text,
                              const char *notification, time_t timestamp, PurpleMessageFlags flags)
{
    PurpleConversation *conv = NULL;

    if (text) {
        if (flags & PURPLE_MESSAGE_SEND) {
            // serv_got_im seems to work for messages sent from another client, but not for
            // echoed messages from this client. Therefore, this (code snippet from facebook plugin).
            conv = getImConversation(account.purpleAccount, purpleUserName);
            purple_conversation_write(conv, purple_account_get_alias(account.purpleAccount), text,
                                      flags, timestamp);
        } else {
            serv_got_im(purple_account_get_connection(account.purpleAccount), purpleUserName, text,
                        flags, timestamp);
        }
    }

    if (notification) {
        if (conv == NULL)
            conv = getImConversation(account.purpleAccount, purpleUserName);
        purple_conversation_write(conv, nullptr, notification, PURPLE_MESSAGE_SYSTEM, timestamp);
    }
}

static void showMessageTextChat(TdAccountData &account, const td::td_api::chat &chat,
                                const TgMessageInfo &message, const char *text,
                                const char *notification, PurpleMessageFlags flags)
{
    // Again, doing what facebook plugin does
    int purpleId = account.getPurpleChatId(chat.id_);
    PurpleConvChat *conv = getChatConversation(account, chat, purpleId);

    if (text) {
        if (flags & PURPLE_MESSAGE_SEND) {
            if (conv)
                purple_conv_chat_write(conv, purple_account_get_alias(account.purpleAccount), text,
                                       flags, message.timestamp);
        } else {
            if (purpleId != 0)
                serv_got_chat_in(purple_account_get_connection(account.purpleAccount), purpleId,
                                 message.sender.empty() ? "someone" : message.sender.c_str(),
                                 flags, text, message.timestamp);
        }
    }

    if (notification) {
        if (conv)
            purple_conversation_write(purple_conv_chat_get_conversation(conv), nullptr,
                                      notification, PURPLE_MESSAGE_SYSTEM, message.timestamp);
    }
}

static std::string quoteMessage(const td::td_api::message *message, TdAccountData &account)
{
    const td::td_api::user *originalAuthor = nullptr;
    if (message)
        originalAuthor = account.getUser(message->sender_user_id_);

    std::string originalName;
    if (originalAuthor)
        originalName = account.getDisplayName(*originalAuthor);
    else
        originalName = _("unknown user");

    std::string text;
    if (!message || !message->content_)
        text = _("[message unavailable]");
    else switch (message->content_->get_id()) {
        case td::td_api::messageText::ID: {
            const td::td_api::messageText &messageText = static_cast<const td::td_api::messageText &>(*message->content_);
            if (messageText.text_)
                text = messageText.text_->text_;
            else
                text = "";
            break;
        }
        case td::td_api::messagePhoto::ID: {
            const td::td_api::messagePhoto &photo = static_cast<const td::td_api::messagePhoto &>(*message->content_);
            text = _("[photo]");
            if (photo.caption_)
                text += " " + photo.caption_->text_;
            break;
        }
        case td::td_api::messageDocument::ID: {
            const td::td_api::messageDocument &document = static_cast<const td::td_api::messageDocument &>(*message->content_);
            if (document.document_)
                text = formatMessage(_("[file: {} ({})"), {document.document_->file_name_,
                                                           document.document_->mime_type_});
            else
                text = _("[file]");
            if (document.caption_)
                text += " " + document.caption_->text_;
            break;
        }
        case td::td_api::messageVideo::ID: {
            const td::td_api::messageVideo &video = static_cast<const td::td_api::messageVideo &>(*message->content_);
            if (video.video_)
                text = formatMessage(_("[video: {}]"), video.video_->file_name_);
            else
                text = _("[video]");
            if (video.caption_)
                text += " " + video.caption_->text_;
            break;
        }
        case td::td_api::messageSticker::ID:
            text = _("[sticker]");
            break;
        default:
            text = formatMessage(_("[message type {}]"), messageTypeToString(*message->content_));
    }

    char *newText = purple_markup_escape_text(text.c_str(), text.size());
    text = newText;
    g_free(newText);

    for (unsigned i = 0; i < text.size(); i++)
        if (text[i] == '\n') text[i] = ' ';

    return formatMessage(_("<b>&gt; {} wrote:</b>\n&gt; {}"), {originalName, text});
}

void showMessageText(TdAccountData &account, const td::td_api::chat &chat, const TgMessageInfo &message,
                     const char *text, const char *notification, uint32_t extraFlags)
{
    // TODO: maybe set PURPLE_MESSAGE_REMOTE_SEND when appropriate
    PurpleMessageFlags directionFlag = message.outgoing ? PURPLE_MESSAGE_SEND : PURPLE_MESSAGE_RECV;
    PurpleMessageFlags flags = (PurpleMessageFlags) (extraFlags | directionFlag);

    std::string newText;
    if (message.repliedMessageId != 0)
        newText = quoteMessage(message.repliedMessage.get(), account);
    if (!message.forwardedFrom.empty()) {
        if (!newText.empty())
            newText += "\n";
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
        std::string userName = getPurpleBuddyName(*privateUser);
        showMessageTextIm(account, userName.c_str(), text, notification, message.timestamp, flags);
    }

    if (getBasicGroupId(chat) || getSupergroupId(chat))
        showMessageTextChat(account, chat, message, text, notification, flags);
}

std::string getSenderPurpleName(const td::td_api::chat &chat, const td::td_api::message &message,
                                TdAccountData &account)
{
    if (!message.is_outgoing_ && (getBasicGroupId(chat) || getSupergroupId(chat))) {
        if (message.sender_user_id_)
            return account.getDisplayName(message.sender_user_id_);
        else if (!message.author_signature_.empty())
            return message.author_signature_;
        else if (message.forward_info_ && message.forward_info_->origin_)
            switch (message.forward_info_->origin_->get_id()) {
            case td::td_api::messageForwardOriginUser::ID:
                return account.getDisplayName(static_cast<const td::td_api::messageForwardOriginUser &>(*message.forward_info_->origin_).sender_user_id_);
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

std::string getForwardSource(const td::td_api::messageForwardInfo &forwardInfo,
                             TdAccountData &account)
{
    if (!forwardInfo.origin_)
        return "";

    switch (forwardInfo.origin_->get_id()) {
        case td::td_api::messageForwardOriginUser::ID:
            return account.getDisplayName(static_cast<const td::td_api::messageForwardOriginUser &>(*forwardInfo.origin_).sender_user_id_);
        case td::td_api::messageForwardOriginHiddenUser::ID:
            return static_cast<const td::td_api::messageForwardOriginHiddenUser &>(*forwardInfo.origin_).sender_name_;
        case td::td_api::messageForwardOriginChannel::ID: {
            const td::td_api::chat *chat = account.getChat(static_cast<const td::td_api::messageForwardOriginChannel&>(*forwardInfo.origin_).chat_id_);
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
                                  const char *inviteLink, const char *groupName, int groupType,
                                  std::vector<PurpleChat *> &result)
{
    PurpleBlistNodeType nodeType = purple_blist_node_get_type(node);

    if (nodeType == PURPLE_BLIST_CHAT_NODE) {
        PurpleChat *chat          = PURPLE_CHAT(node);
        GHashTable *components    = purple_chat_get_components(chat);
        const char *nodeName      = getChatName(components);
        const char *nodeLink      = getChatInviteLink(components);
        const char *nodeGroupName = getChatGroupName(components);
        int         nodeGroupType = getChatGroupType(components);

        if (!nodeName) nodeName = "";
        if (!nodeLink) nodeLink = "";
        if (!nodeGroupName) nodeGroupName = "";

        if (!strcmp(nodeName, "") && !strcmp(nodeLink, inviteLink)) {
            if ((*inviteLink != '\0') ||
                (!strcmp(nodeGroupName, groupName) && (nodeGroupType == groupType)))
            {
                result.push_back(chat);
            }
        }
    }

    for (PurpleBlistNode *child = purple_blist_node_get_first_child(node); child;
         child = purple_blist_node_get_sibling_next(child))
    {
        findChatsByComponents(child, inviteLink, groupName, groupType, result);
    }
}

std::vector<PurpleChat *>findChatsByInviteLink(const std::string &inviteLink)
{
    std::vector<PurpleChat *> result;

    for (PurpleBlistNode *root = purple_blist_get_root(); root;
         root = purple_blist_node_get_sibling_next(root)) // LOL
    {
        findChatsByComponents(root, inviteLink.c_str(), "", 0, result);
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

void setChatMembers(PurpleConvChat *purpleChat, const td::td_api::basicGroupFullInfo &groupInfo,
                    const TdAccountData &account)
{
    GList *flags = NULL;
    std::vector<std::string> nameData;

    for (const auto &member: groupInfo.members_) {
        if (!member || !isGroupMember(member->status_))
            continue;

        const td::td_api::user *user = account.getUser(member->user_id_);
        if (!user)
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

struct MessagePart {
    bool        isImage;
    int         imageId;
    std::string text;
};

static void appendText(std::vector<MessagePart> &parts, const char *s, size_t len)
{
    if (parts.empty())
        parts.emplace_back();
    parts.back().text = std::string(s, len);
}

static void parseMessage(const char *message, std::vector<MessagePart> &parts)
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
            }
        }

        if (isImage) {
            if (s != textStart)
                appendText(parts, textStart, s-textStart);
            parts.emplace_back();
            parts.back().isImage = true;
            parts.back().imageId = imageId;
            s = pastImage;
            textStart = pastImage;
        } else
            s++;
    }

    if (s != textStart)
        appendText(parts, textStart, s-textStart);
}

static bool saveImage(int id, char **fileName)
{
    *fileName = NULL;
    char *tempFileName;

    PurpleStoredImage *psi = purple_imgstore_find_by_id (id);
    if (!psi) {
        purple_debug_misc(config::pluginId, "Failed to send image: id %d not found\n", id);
        return false;
    }
    int fd = g_file_open_tmp("tdlib_upload_XXXXXXXX", &tempFileName, NULL);
    if (fd < 0) {
        purple_debug_misc(config::pluginId, "Failed to send image: could not create temporary file\n");
        return false;
    }
    ssize_t len = write(fd, purple_imgstore_get_data (psi), purple_imgstore_get_size (psi));
    close(fd);
    if (len != (ssize_t)purple_imgstore_get_size(psi)) {
        purple_debug_misc(config::pluginId, "Failed to send image: could not write temporary file\n");
        remove(tempFileName);
        g_free(tempFileName);
        return false;
    }

    *fileName = tempFileName;
    return true;
}

void transmitMessage(int64_t chatId, const char *message, TdTransceiver &transceiver,
                     TdAccountData &account, TdTransceiver::ResponseCb response)
{
    std::vector<MessagePart> parts;
    parseMessage(message, parts);
    for (const MessagePart &input: parts) {
        td::td_api::object_ptr<td::td_api::sendMessage> sendMessageRequest = td::td_api::make_object<td::td_api::sendMessage>();
        sendMessageRequest->chat_id_ = chatId;
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

        // successive updateFile follow, can show transfer dialog with progress
        // when one with is_uploading_active comes, can delete temproray file
        // and if not is_uploading_completed, show error message
        // Or updateMessageContent, or updateMessageSendSucceeded

        uint64_t requestId = transceiver.sendQuery(std::move(sendMessageRequest), response);
        if (tempFileName) {
            account.addPendingRequest<SendMessageRequest>(requestId, tempFileName);
            g_free(tempFileName);
        }
    }
}

void startDocumentUpload(int64_t chatId, const std::string &filename, PurpleXfer *xfer,
                         TdTransceiver &transceiver, TdAccountData &account,
                         TdTransceiver::ResponseCb response)
{
    auto uploadRequest = td::td_api::make_object<td::td_api::uploadFile>();
    uploadRequest->file_ = td::td_api::make_object<td::td_api::inputFileLocal>(filename);
    uploadRequest->file_type_ = td::td_api::make_object<td::td_api::fileTypeDocument>();
    uploadRequest->priority_ = FILE_UPLOAD_PRIORITY;
    purple_xfer_ref(xfer);
    uint64_t requestId = transceiver.sendQuery(std::move(uploadRequest), response);
    account.addPendingRequest<UploadRequest>(requestId, xfer, chatId);
}

void startDocumentUploadProgress(int64_t chatId, PurpleXfer *xfer, const td::td_api::file &file,
                                 TdTransceiver &transceiver, TdAccountData &account)
{
    if (purple_xfer_is_canceled(xfer)) {
        // Someone managed to cancel the upload REAL fast
        auto cancelRequest = td::td_api::make_object<td::td_api::cancelUploadFile>(file.id_);
        transceiver.sendQuery(std::move(cancelRequest), nullptr);
        purple_xfer_unref(xfer);
    } else {
        purple_debug_misc(config::pluginId, "Got file id %d for uploading %s\n", (int)file.id_,
                            purple_xfer_get_local_filename(xfer));
        account.addUpload(file.id_, xfer, chatId);
        updateDocumentUploadProgress(file, transceiver, account);
    }
}

void uploadResponseError(PurpleXfer *xfer, const std::string &message, TdAccountData &account)
{
    purple_xfer_cancel_remote(xfer);
    purple_xfer_error(purple_xfer_get_type(xfer), account.purpleAccount,
                      purple_xfer_get_remote_user(xfer), message.c_str());
    purple_xfer_unref(xfer);
}

void updateDocumentUploadProgress(const td::td_api::file &file, TdTransceiver &transceiver,
                                  TdAccountData &account)
{
    PurpleXfer *upload;
    int64_t     chatId;
    if (!account.getUpload(file.id_, upload, chatId))
        return;
    size_t fileSize = purple_xfer_get_size(upload);

    if (file.remote_) {
        if (file.remote_->is_uploading_active_) {
            if (purple_xfer_get_status(upload) != PURPLE_XFER_STATUS_STARTED) {
                purple_debug_misc(config::pluginId, "Started uploading %s\n", purple_xfer_get_local_filename(upload));
                purple_xfer_start(upload, -1, NULL, 0);
            }
            size_t bytesSent = std::max(0, file.remote_->uploaded_size_);
            purple_xfer_set_bytes_sent(upload, std::min(fileSize, bytesSent));
            purple_xfer_update_progress(upload);
        } else if (file.local_ && (file.remote_->uploaded_size_ == file.local_->downloaded_size_)) {
            purple_debug_misc(config::pluginId, "Finishing uploading %s\n", purple_xfer_get_local_filename(upload));
            purple_xfer_set_bytes_sent(upload, fileSize);
            purple_xfer_set_completed(upload, TRUE);
            purple_xfer_end(upload);
            purple_xfer_unref(upload);
            account.removeUpload(file.id_);
            auto sendMessageRequest = td::td_api::make_object<td::td_api::sendMessage>();
            auto content = td::td_api::make_object<td::td_api::inputMessageDocument>();
            content->caption_ = td::td_api::make_object<td::td_api::formattedText>();
            content->document_ = td::td_api::make_object<td::td_api::inputFileId>(file.id_);
            sendMessageRequest->input_message_content_ = std::move(content);
            sendMessageRequest->chat_id_ = chatId;
            transceiver.sendQuery(std::move(sendMessageRequest), nullptr);
        }
    } else {
        purple_xfer_cancel_remote(upload);
        purple_xfer_unref(upload);
        account.removeUpload(file.id_);
    }
}

unsigned getFileSize(const td::td_api::file &file)
{
    int32_t size = file.size_;
    if (size == 0)
        size = file.expected_size_;

    if (size <= 0)
        return 0;
    else
        return size;
}

unsigned getFileSizeKb(const td::td_api::file &file)
{
    return getFileSize(file)/1024;
}

std::string makeDocumentDescription(const td::td_api::voiceNote *document)
{
    if (!document)
        // Unlikely error message not worth translating
        return "faulty voice note";
    return formatMessage(_("voice note [{}]"), document->mime_type_);
}

std::string makeDocumentDescription(const td::td_api::videoNote *document)
{
    if (!document)
        // Unlikely error message not worth translating
        return "faulty voice note";
    return formatMessage(_("video note [{} s]"), document->duration_);
}

std::string getSenderDisplayName(const td::td_api::chat &chat, const TgMessageInfo &message,
                                 PurpleAccount *account)
{
    if (message.outgoing)
        return purple_account_get_alias(account);
    else if (isPrivateChat(chat))
        return chat.title_;
    else
        return message.sender;
}

std::string makeNoticeWithSender(const td::td_api::chat &chat, const TgMessageInfo &message,
                                 const char *noticeText, PurpleAccount *account)
{
    std::string prefix = getSenderDisplayName(chat, message, account);
    if (!prefix.empty())
        prefix += ": ";
    return prefix + noticeText;
}

void showGenericFile(const td::td_api::chat &chat, const TgMessageInfo &message,
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

#ifndef NoWebp

static void p2tgl_png_mem_write (png_structp png_ptr, png_bytep data, png_size_t length)
{
    GByteArray *png_mem = (GByteArray *) png_get_io_ptr(png_ptr);
    g_byte_array_append (png_mem, data, length);
}

int p2tgl_imgstore_add_with_id_png (const unsigned char *raw_bitmap, unsigned width, unsigned height)
{
    GByteArray *png_mem = NULL;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    png_bytepp rows = NULL;

    // init png write struct
    png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL) {
        purple_debug_misc(config::pluginId, "error encoding png (create_write_struct failed)\n");
        return 0;
    }

    // init png info struct
    info_ptr = png_create_info_struct (png_ptr);
    if (info_ptr == NULL) {
        png_destroy_write_struct(&png_ptr, NULL);
        purple_debug_misc(config::pluginId, "error encoding png (create_info_struct failed)\n");
        return 0;
    }

    // Set up error handling.
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        purple_debug_misc(config::pluginId, "error while writing png\n");
        return 0;
    }

    // set img attributes
    png_set_IHDR (png_ptr, info_ptr, width, height,
                    8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                    PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    // alloc row pointers
    rows = g_new0 (png_bytep, height);
    if (rows == NULL) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        purple_debug_misc(config::pluginId, "error converting to png: malloc failed\n");
        return 0;
    }

    unsigned i;
    for (i = 0; i < height; i++)
        rows[i] = (png_bytep)(raw_bitmap + i * width * 4);

    // create array and set own png write function
    png_mem = g_byte_array_new();
    png_set_write_fn (png_ptr, png_mem, p2tgl_png_mem_write, NULL);

    // write png
    png_set_rows (png_ptr, info_ptr, rows);
    png_write_png (png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

    // cleanup
    g_free(rows);
    png_destroy_write_struct (&png_ptr, &info_ptr);
    unsigned png_size = png_mem->len;
    gpointer png_data = g_byte_array_free (png_mem, FALSE);

    return purple_imgstore_add_with_id (png_data, png_size, NULL);
}

int p2tgl_imgstore_add_with_id_webp (const char *filename)
{
    constexpr int MAX_W = 256;
    constexpr int MAX_H = 256;

    const uint8_t *data = NULL;
    size_t len;
    GError *err = NULL;
    g_file_get_contents (filename, (gchar **) &data, &len, &err);
    if (err) {
        purple_debug_misc(config::pluginId, "cannot open file %s: %s\n", filename, err->message);
        g_error_free(err);
        return 0;
    }

    // downscale oversized sticker images displayed in chat, otherwise it would harm readabillity
    WebPDecoderConfig config;
    WebPInitDecoderConfig (&config);
    if (WebPGetFeatures(data, len, &config.input) != VP8_STATUS_OK) {
        purple_debug_misc(config::pluginId, "error reading webp bitstream: %s\n", filename);
        g_free ((gchar *)data);
        return 0;
    }

    config.options.use_scaling = 0;
    config.options.scaled_width = config.input.width;
    config.options.scaled_height = config.input.height;
    if (config.options.scaled_width > MAX_W || config.options.scaled_height > MAX_H) {
        const float max_scale_width = MAX_W * 1.0f / config.options.scaled_width;
        const float max_scale_height = MAX_H * 1.0f / config.options.scaled_height;
        if (max_scale_width < max_scale_height) {
        // => the width is most limiting
        config.options.scaled_width = MAX_W;
        // Can't use ' *= ', because we need to do the multiplication in float
        // (or double), and only THEN cast back to int.
        config.options.scaled_height = (int) (config.options.scaled_height * max_scale_width);
        } else {
        // => the height is most limiting
        config.options.scaled_height = MAX_H;
        // Can't use ' *= ', because we need to do the multiplication in float
        // (or double), and only THEN cast back to int.
        config.options.scaled_width = (int) (config.options.scaled_width * max_scale_height);
        }
        config.options.use_scaling = 1;
    }
    config.output.colorspace = MODE_RGBA;
    if (WebPDecode(data, len, &config) != VP8_STATUS_OK) {
        purple_debug_misc(config::pluginId, "error decoding webp: %s\n", filename);
        g_free ((gchar *)data);
        return 0;
    }
    g_free ((gchar *)data);
    const uint8_t *decoded = config.output.u.RGBA.rgba;

    // convert and add
    int imgStoreId = p2tgl_imgstore_add_with_id_png(decoded, config.options.scaled_width, config.options.scaled_height);
    WebPFreeDecBuffer (&config.output);
    return imgStoreId;
}

#else

int p2tgl_imgstore_add_with_id_webp (const char *filename)
{
    return 0;
}

#endif

void showWebpSticker(const td::td_api::chat &chat, const TgMessageInfo &message,
                     const std::string &filePath, const std::string &fileDescription,
                     TdAccountData &account)
{
    int id = p2tgl_imgstore_add_with_id_webp(filePath.c_str());
    if (id != 0) {
        std::string text = "\n<img id=\"" + std::to_string(id) + "\">";
        showMessageText(account, chat, message, text.c_str(), NULL);
    } else
        showGenericFile(chat, message, filePath, fileDescription, account);
}
