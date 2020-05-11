#include "purple-utils.h"
#include "chat-info.h"
#include "config.h"

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

const char *getPurpleStatusId(const td::td_api::UserStatus &tdStatus)
{
    if (tdStatus.get_id() == td::td_api::userStatusOnline::ID)
        return purple_primitive_get_id_from_type(PURPLE_STATUS_AVAILABLE);
    else
        return purple_primitive_get_id_from_type(PURPLE_STATUS_OFFLINE);
}

const char *getPurpleUserName(const td::td_api::user &user)
{
    return getCanonicalPhoneNumber(user.phone_number_.c_str());
}

PurpleConversation *getImConversation(PurpleAccount *account, const char *username)
{
    PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, username, account);
    if (conv == NULL)
        conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, account, username);

    return conv;
}

PurpleConvChat *getChatConversation(PurpleAccount *account, const td::td_api::chat &chat,
                                    int chatPurpleId, TdAccountData &accountData)
{
    std::string chatName       = getChatName(chat);
    bool        newChatCreated = false;
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
            else
                newChatCreated = true;

        } else
            purple_debug_warning(config::pluginId, "No internal ID for chat %s\n", chat.title_.c_str());
    }

    if (conv) {
        PurpleConvChat *purpleChat   = purple_conversation_get_chat_data(conv);

        if (purpleChat && newChatCreated) {
            int32_t                               basicGroupId = getBasicGroupId(chat);
            const td::td_api::basicGroupFullInfo *groupInfo    = basicGroupId ? accountData.getBasicGroupInfo(basicGroupId) : nullptr;
            if (groupInfo)
                setChatMembers(purpleChat, *groupInfo, accountData);
        }

        return purpleChat;
    }

    return NULL;
}

void showMessageTextIm(PurpleAccount *account, const char *purpleUserName, const char *text,
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

void showMessageTextChat(PurpleAccount *account, const td::td_api::chat &chat,
                         const std::string &sender, const char *text,
                         const char *notification, time_t timestamp, bool outgoing,
                         TdAccountData &accountData)
{
    // Again, doing what facebook plugin does
    int purpleId = accountData.getPurpleChatId(chat.id_);
    PurpleConvChat *conv = getChatConversation(account, chat, purpleId, accountData);

    if (text) {
        if (outgoing) {
            if (conv)
                purple_conv_chat_write(conv, purple_account_get_alias(account), text,
                                    PURPLE_MESSAGE_SEND, timestamp);
        } else {
            if (purpleId != 0)
                serv_got_chat_in(purple_account_get_connection(account), purpleId,
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

void setChatMembers(PurpleConvChat *purpleChat, const td::td_api::basicGroupFullInfo &groupInfo,
                    const TdAccountData &accountData)
{
    GList *names = NULL;
    GList *flags = NULL;
    std::vector<std::string> nameData;
    for (const auto &member: groupInfo.members_)
        if (member && isGroupMember(member->status_)) {
            const td::td_api::user *user = accountData.getUser(member->user_id_);
            if (user) {
                nameData.emplace_back(getDisplayName(user));
                names = g_list_append(names, const_cast<char *>(nameData.back().c_str()));
                PurpleConvChatBuddyFlags flag;
                if (member->status_->get_id() == td::td_api::chatMemberStatusCreator::ID)
                    flag = PURPLE_CBFLAGS_FOUNDER;
                else if (member->status_->get_id() == td::td_api::chatMemberStatusAdministrator::ID)
                    flag = PURPLE_CBFLAGS_OP;
                else
                    flag = PURPLE_CBFLAGS_NONE;
                flags = g_list_append(flags, GINT_TO_POINTER(flag));
            }
        }
    purple_conv_chat_add_users(purpleChat, names, NULL, flags, false);
    g_list_free(names);
    g_list_free(flags);
}
