#include "tdlib-purple.h"
#include "config.h"
#include "td-client.h"
#include "purple-info.h"
#include "format.h"
#include "buildopt.h"
#include <purple.h>

#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <array>
#include <ctype.h>
#include <unistd.h>

#ifndef NoLottie
#include <rlottie.h>
#endif

static const char *tgprpl_list_icon (PurpleAccount *acct, PurpleBuddy *buddy)
{
    return "telegram";
}

static const char *getLastOnline(const td::td_api::UserStatus &status)
{
    switch (status.get_id()) {
        case td::td_api::userStatusOnline::ID:
            // TRANSLATOR: Buddy infobox, value for "last online"
            return _("now");
        case td::td_api::userStatusOffline::ID: {
            const td::td_api::userStatusOffline &offline = static_cast<const td::td_api::userStatusOffline &>(status);
            time_t timestamp = offline.was_online_;
            return ctime(&timestamp);
        }
        case td::td_api::userStatusRecently::ID:
            // TRANSLATOR: Buddy infobox, value for "last online"
            return _("recently");
        case td::td_api::userStatusLastWeek::ID:
            // TRANSLATOR: Buddy infobox, value for "last online"
            return _("last week");
        case td::td_api::userStatusLastMonth::ID:
            // TRANSLATOR: Buddy infobox, value for "last online"
            return _("last month");
    }

    return "";
}

static void tgprpl_tooltip_text (PurpleBuddy *buddy, PurpleNotifyUserInfo *info, gboolean full)
{
    PurpleTdClient *tdClient = getTdClient(purple_buddy_get_account(buddy));
    if (!tdClient) return;

    std::vector<const td::td_api::user *> users;
    tdClient->getUsers(purple_buddy_get_name(buddy), users);

    if ((users.size() == 1) && users[0]->status_) {
        const char *lastOnline = getLastOnline(*users[0]->status_);
        if (lastOnline && *lastOnline) {
            // TRANSLATOR: Buddy infobox, key
            purple_notify_user_info_add_pair(info, _("Last online"), lastOnline);
        }
    }
}

static GList *tgprpl_status_types (PurpleAccount *acct)
{
    GList *types = NULL;
    PurpleStatusType *type;

    type = purple_status_type_new_full (PURPLE_STATUS_AVAILABLE, NULL, NULL, FALSE, TRUE, FALSE);
    types = g_list_prepend (types, type);

    type = purple_status_type_new_full (PURPLE_STATUS_AWAY, NULL, NULL, FALSE, FALSE, FALSE);
    types = g_list_prepend (types, type);

    type = purple_status_type_new_full (PURPLE_STATUS_OFFLINE, NULL, NULL, FALSE, TRUE, FALSE);
    types = g_list_prepend (types, type);

    return g_list_reverse (types);
}

struct RequestData {
    PurpleAccount *account;
    std::string stringData;

    RequestData(PurpleAccount *account) : account(account) {}
};

static void cancelRequest(RequestData *data, int action)
{
    delete data;
}

static void deleteGroupConfirm(RequestData *data, int action)
{
    std::unique_ptr<RequestData> request(data);
    PurpleTdClient *tdClient = getTdClient(request->account);

    if (tdClient)
        tdClient->leaveGroup(request->stringData, true);
}

static void leaveGroupConfirm(RequestData *data, int action)
{
    std::unique_ptr<RequestData> request(data);
    PurpleTdClient *tdClient = getTdClient(request->account);

    if (tdClient)
        tdClient->leaveGroup(request->stringData, false);
}

static void leaveGroup(PurpleBlistNode *node, gpointer data)
{
    if (! PURPLE_BLIST_NODE_IS_CHAT(node))
        return;

    PurpleChat     *chat     = PURPLE_CHAT(node);
    PurpleAccount  *account  = purple_chat_get_account(chat);
    PurpleTdClient *tdClient = getTdClient(account);
    if (tdClient) {
        const char  *chatName  = getChatName(purple_chat_get_components(chat));
        RequestData *request   = new RequestData(account);
        request->stringData = chatName ? chatName : "";

        if (tdClient->getBasicGroupMembership(chatName) == BasicGroupMembership::Creator) {
            // TRANSLATOR: Owning group deletion dialog, title
            purple_request_action(purple_account_get_connection(account), _("Leaving group"),
                                  // TRANSLATOR: Owning group deletion dialog, primary content
                                  _("Confirm deleting group"),
                                  // TRANSLATOR: Owning group deletion dialog, secondary content
                                  _("Leaving basic group you created will delete the group. Continue?"),
                                  0, account, NULL, NULL, request, 2,
                                  // TRANSLATOR: Owning group deletion dialog, alternative is "_No"
                                  _("_Yes"), leaveGroupConfirm,
                                  // TRANSLATOR: Owning group deletion dialog, alternative is "_Yes"
                                  _("_No"), cancelRequest);
        } else {
            // TRANSLATOR: Group leave dialog, title
            purple_request_action(purple_account_get_connection(account), _("Leaving group"),
                                  // TRANSLATOR: Group leave dialog, content
                                  _("Leave the group?"), NULL,
                                  0, account, NULL, NULL, request, 2,
                                  // TRANSLATOR: Group leave dialog, alternative is "_No"
                                  _("_Yes"), leaveGroupConfirm,
                                  // TRANSLATOR: Group leave dialog, alternative is "_Yes"
                                  _("_No"), cancelRequest);
        }
    }
}

static void deleteGroup(PurpleBlistNode *node, gpointer data)
{
    if (! PURPLE_BLIST_NODE_IS_CHAT(node))
        return;

    PurpleChat     *chat     = PURPLE_CHAT(node);
    PurpleAccount  *account  = purple_chat_get_account(chat);
    PurpleTdClient *tdClient = getTdClient(account);
    if (tdClient) {
        const char  *chatName  = getChatName(purple_chat_get_components(chat));

        if (tdClient->getBasicGroupMembership(chatName) == BasicGroupMembership::NonCreator) {
            // TRANSLATOR: Group deletion error dialog, title
            purple_notify_error(account, _("Cannot delete group"),
                                // TRANSLATOR: Group deletion error dialog, content
                                _("Cannot delete basic group created by someone else"), NULL);
        } else {
            RequestData *request = new RequestData(account);
            request->stringData = chatName ? chatName : "";
            // TRANSLATOR: Group deletion confirmation dialog, title
            purple_request_action(purple_account_get_connection(account), _("Deleting group"),
                                  // TRANSLATOR: Group deletion confirmation dialog, content
                                  _("Delete the group?"), NULL,
                                  0, account, NULL, NULL, request, 2,
                                  // TRANSLATOR: Group deletion dialog, alternative is "_No"
                                  _("_Yes"), deleteGroupConfirm,
                                  // TRANSLATOR: Group deletion dialog, alternative is "_Yes"
                                  _("_No"), cancelRequest);
        }
    }
}

static void showInviteLink(PurpleBlistNode *node, gpointer data)
{
    if (! PURPLE_BLIST_NODE_IS_CHAT(node))
        return;

    PurpleChat     *chat     = PURPLE_CHAT(node);
    PurpleAccount  *account  = purple_chat_get_account(chat);
    PurpleTdClient *tdClient = getTdClient(account);
    if (tdClient)
        tdClient->showInviteLink(getChatName(purple_chat_get_components(chat)));
}

static void createSecretChat(PurpleBlistNode *node, gpointer data)
{
    if (! PURPLE_BLIST_NODE_IS_BUDDY(node))
        return;

    PurpleBuddy    *buddy    = PURPLE_BUDDY(node);
    PurpleAccount  *account  = purple_buddy_get_account(buddy);
    PurpleTdClient *tdClient = getTdClient(account);
    if (tdClient)
        tdClient->createSecretChat(purple_buddy_get_name(buddy));
}

static GList* tgprpl_blist_node_menu (PurpleBlistNode *node)
{
    GList *menu = NULL;

    if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
        PurpleChat *chat = PURPLE_CHAT(node);
        if (!getTdlibChatId(getChatName(purple_chat_get_components(chat))).valid())
            return menu;

        PurpleMenuAction* action;
        // TRANSLATOR: Group menu action item
        action = purple_menu_action_new(_("Leave group"),
                                        PURPLE_CALLBACK(leaveGroup),
                                        NULL, NULL);
        menu = g_list_append(menu, action);

        // TRANSLATOR: Group menu action item
        action = purple_menu_action_new(_("Delete group"),
                                        PURPLE_CALLBACK(deleteGroup),
                                        NULL, NULL);
        menu = g_list_append(menu, action);

        // TRANSLATOR: Group menu action item
        action = purple_menu_action_new(_("Show invite link"),
                                        PURPLE_CALLBACK(showInviteLink),
                                        NULL, NULL);
        menu = g_list_append(menu, action);
    } else if ( PURPLE_BLIST_NODE_IS_BUDDY(node) &&
                purpleBuddyNameToUserId(purple_buddy_get_name(PURPLE_BUDDY(node))).valid() )
    {
        PurpleMenuAction *action;
        // TRANSLATOR: Buddy menu action item
        action = purple_menu_action_new(_("Start secret chat"),
                                        PURPLE_CALLBACK(createSecretChat),
                                        NULL, NULL);
        menu = g_list_append(menu, action);
    }

    return menu;
}

static GList *tgprpl_chat_join_info (PurpleConnection *gc)
{
    return getChatJoinInfo();
}

static GHashTable *tgprpl_chat_info_defaults (PurpleConnection *gc, const char *chat_name)
{
    GHashTable *components = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
    g_hash_table_insert(components, (gpointer)getChatNameComponent(), g_strdup(chat_name));
    return components;
}

static ITransceiverBackend *g_testBackend = nullptr;

void tgprpl_set_test_backend(ITransceiverBackend *backend)
{
    g_testBackend = backend;
}

void tgprpl_set_single_thread()
{
    AccountThread::setSingleThread();
}

struct PurpleConversationInfo {
    std::string accountName;
    std::string convName;
    PurpleConversationType type;
};

static gboolean sendConversationReadReceipts(void *arg)
{
    std::unique_ptr<PurpleConversationInfo> info(static_cast<PurpleConversationInfo *>(arg));
    PurpleAccount *account = purple_accounts_find(info->accountName.c_str(), config::pluginId);
    PurpleConversation *conv = NULL;
    PurpleTdClient *tdClient = NULL;
    if (account != NULL) {
        tdClient = getTdClient(account);
        conv = purple_find_conversation_with_account(info->type, info->convName.c_str(), account);
    }

    if (conv && tdClient)
        tdClient->sendReadReceipts(conv);

    return G_SOURCE_REMOVE;
}

static void
conversation_updated_cb(PurpleConversation *conv, PurpleConvUpdateType type)
{
    PurpleAccount *account = purple_conversation_get_account(conv);
    if (!strcmp(purple_account_get_protocol_id(account), config::pluginId) &&
        (type == PURPLE_CONV_UPDATE_UNSEEN))
    {
        // With pidgin, when this callback is triggered by conversation window coming into window
        // manager focus, purple_conversation_has_focus will still return false if called right now,
        // so postpone the rest by half a second to get a better value when
        // purple_conversation_has_focus is called down the line to determine if the read receipts
        // should actually be sent.
        PurpleConversationInfo *arg = new PurpleConversationInfo;
        arg->accountName = purple_account_get_username(account);
        arg->convName = purple_conversation_get_name(conv);
        arg->type = purple_conversation_get_type(conv);
        g_timeout_add(500, sendConversationReadReceipts, arg);
    }
}

static void tgprpl_login (PurpleAccount *acct)
{
    purple_debug_misc(config::pluginId, "version %s\n", config::versionString);
    PurpleConnection *gc       = purple_account_get_connection (acct);
    PurpleTdClient   *tdClient = new PurpleTdClient(acct, g_testBackend);

    purple_connection_set_protocol_data (gc, tdClient);
    // this would enable formatting buttons in pidgin
    // gc->flags = static_cast<PurpleConnectionFlags>(gc->flags | PURPLE_CONNECTION_HTML);

    purple_signal_connect(purple_conversations_get_handle(), "conversation-updated",
                          acct, PURPLE_CALLBACK(conversation_updated_cb), NULL);
}

static void tgprpl_close (PurpleConnection *gc)
{
    delete static_cast<PurpleTdClient *>(purple_connection_get_protocol_data(gc));
    purple_connection_set_protocol_data(gc, NULL);
}

static int tgprpl_send_im (PurpleConnection *gc, const char *who, const char *message, PurpleMessageFlags flags)
{
    PurpleTdClient *tdClient = static_cast<PurpleTdClient *>(purple_connection_get_protocol_data(gc));
    purple_debug_misc(config::pluginId, "tgprpl_send_im to '%s' flags=0x%x\n", who, (unsigned)flags);
    return tdClient->sendMessage(who, message);
}

static unsigned int tgprpl_send_typing (PurpleConnection *gc, const char *who, PurpleTypingState typing)
{
    PurpleTdClient *tdClient = static_cast<PurpleTdClient *>(purple_connection_get_protocol_data(gc));
    tdClient->sendTyping(who, typing == PURPLE_TYPING);
    return 0;
}

static void tgprpl_info_show (PurpleConnection *gc, const char *who)
{
    PurpleTdClient *tdClient = static_cast<PurpleTdClient *>(purple_connection_get_protocol_data(gc));
    std::vector<const td::td_api::user *> users;
    tdClient->getUsers(who, users);

    PurpleNotifyUserInfo *info = purple_notify_user_info_new();
    if (users.empty()) {
        // TRANSLATOR: Buddy infobox, error
        purple_notify_user_info_add_pair(info, _("User not found"), NULL);
    }

    for (const td::td_api::user *user: users) {
        if (purple_notify_user_info_get_entries(info))
            purple_notify_user_info_add_section_break(info);

        // TRANSLATOR: Buddy infobox, key
        purple_notify_user_info_add_pair(info, _("First name"), user->first_name_.c_str());
        // TRANSLATOR: Buddy infobox, key
        purple_notify_user_info_add_pair(info, _("Last name"), user->last_name_.c_str());
        if (!user->username_.empty()) {
            // TRANSLATOR: Buddy infobox, key
            purple_notify_user_info_add_pair(info, _("Username"), user->username_.c_str());
        }
        if (!user->phone_number_.empty()) {
            // TRANSLATOR: Buddy infobox, key
            purple_notify_user_info_add_pair(info, _("Phone number"), user->phone_number_.c_str());
        }
        if (user->status_) {
            const char *lastOnline = getLastOnline(*user->status_);
            if (lastOnline && *lastOnline) {
                // TRANSLATOR: Buddy infobox, key
                purple_notify_user_info_add_pair(info, _("Last online"), lastOnline);
            }
        }
        std::string username = getPurpleBuddyName(*user);
        // TRANSLATOR: Buddy infobox, key
        purple_notify_user_info_add_pair(info, _("Internal id"), username.c_str());
    }

    purple_notify_userinfo(gc, who, info, NULL, NULL);
}

static void tgprpl_set_status (PurpleAccount *acct, PurpleStatus *status)
{
}

static void tgprpl_add_buddy (PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group)
{
    const char  *phoneNumber = purple_buddy_get_name(buddy);
    const char  *alias       = purple_buddy_get_alias(buddy);
    const char  *groupName   = group ? purple_group_get_name(group) : NULL;

    std::string  phoneNumberStr = phoneNumber;
    std::string  aliasStr       = alias ? alias : "";

    // Buddy will be re-added with different username if it succeeds
    purple_blist_remove_buddy(buddy);
    // Now original C strings are gone

    PurpleTdClient *tdClient = static_cast<PurpleTdClient *>(purple_connection_get_protocol_data(gc));
    tdClient->addContact(phoneNumberStr, aliasStr, groupName ? groupName : "");
}

static void request_delete_contact_on_server_yes (RequestData *data, int action)
{
    std::unique_ptr<RequestData> request(data);
    PurpleTdClient *tdClient = getTdClient(request->account);

    if (tdClient)
        tdClient->removeContactAndPrivateChat(request->stringData);
}

static void tgprpl_request_delete_contact (PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group)
{
    g_return_if_fail(buddy);

    RequestData *data = new RequestData(purple_connection_get_account(gc));
    data->stringData = purple_buddy_get_name(buddy);

    // TRANSLATOR: Buddy deletion confirmation, title
//     purple_request_yes_no(gc, _("Remove contact"),
//                           // TRANSLATOR: Buddy deletion confirmation, content
//                           _("Remove from global contact list and delete chat history from the server?\n"),
//                           NULL,
//                           0, purple_connection_get_account(gc), purple_buddy_get_name(buddy),
//                           NULL, data, request_delete_contact_on_server_yes,
//                           cancelRequest);
    request_delete_contact_on_server_yes(data, 0);
}

static std::array<const char *, 3> invitePrefixes {
    "https://t.me/joinchat/",
    "https://telegram.me/joinchat/",
    "https://telegram.dog/joinchat/"
};

static std::array<const char *, 2> groupLinkPrefixes {
    "https://t.me/s/",
    "https://t.me/"
};

static bool isValidInviteLink(const char *link)
{
    return !strncmp(link, invitePrefixes[0], strlen(invitePrefixes[0])) ||
           !strncmp(link, invitePrefixes[1], strlen(invitePrefixes[1])) ||
           !strncmp(link, invitePrefixes[2], strlen(invitePrefixes[2]));
}

static std::string getGroupUsernameFromLink(const char *link)
{
    for (const char *prefix: groupLinkPrefixes) {
        size_t prefixLen = strlen(prefix);
        if (!strncmp(link, prefix, prefixLen) && (strchr(link + prefixLen, '/') == NULL))
            return link + prefixLen;
    }

    return std::string();
}

static void create_group_chat_cb (RequestData *data, PurpleRequestFields* fields)
{
    std::unique_ptr<RequestData> request(data);

    std::vector<std::string> members;
    for (const char *label: {"user1", "user2", "user3"}) {
        const char *userName = purple_request_fields_get_string(fields, label);
        if (userName && *userName)
            members.emplace_back(userName);
    }

    PurpleTdClient *tdClient = getTdClient(request->account);
    if (tdClient)
        tdClient->createGroup(request->stringData.c_str(), GROUP_TYPE_BASIC, members);
}

static void requestCreateBasicGroup(PurpleConnection *gc, const char *name)
{
    // Telegram doesn't allow to create chats with only one user, so we need to force
    // the user to specify at least one other one.
    PurpleRequestFields* fields = purple_request_fields_new ();
    PurpleRequestFieldGroup* group = purple_request_field_group_new (
        // TRANSLATOR: Group creation dialog, secondary content
        _("Invite at least one additional user by specifying their full name (autocompletion available)."));

    // TRANSLATOR: Group creation dialog, label
    PurpleRequestField *field = purple_request_field_string_new ("user1", _("Username"), NULL, FALSE);
    purple_request_field_set_type_hint (field, "screenname");
    purple_request_field_group_add_field (group, field);

    field = purple_request_field_string_new ("user2", _("Username"), NULL, FALSE);
    purple_request_field_set_type_hint (field, "screenname");
    purple_request_field_group_add_field (group, field);

    field = purple_request_field_string_new ("user3", _("Username"), NULL, FALSE);
    purple_request_field_set_type_hint (field, "screenname");
    purple_request_field_group_add_field (group, field);

    purple_request_fields_add_group (fields, group);

    RequestData *data = new RequestData(purple_connection_get_account(gc));
    data->stringData = name;
    // TRANSLATOR: Group creation dialog, title
    purple_request_fields (gc, _("Create group chat"),
                           // TRANSLATOR: Group creation dialog, primary content
                           _("Invite users"), NULL, fields,
                           // TRANSLATOR: Group creation dialog, alternative is "_Cancel". The underscore marks accelerator keys, they must be different!
                           _("_OK"), G_CALLBACK(create_group_chat_cb),
                           // TRANSLATOR: Group creation dialog, alternative is "_OK". The underscore marks accelerator keys, they must be different!
                           _("_Cancel"), G_CALLBACK(cancelRequest),
                           purple_connection_get_account(gc), NULL, NULL, data);
}

static void tgprpl_chat_join (PurpleConnection *gc, GHashTable *data)
{
    PurpleTdClient *tdClient   = static_cast<PurpleTdClient *>(purple_connection_get_protocol_data(gc));
    const char     *name       = getChatName(data);
    const char     *joinString = getChatJoinString(data);

    if (name && *name) {
        if (!tdClient->joinChat(name))
            purple_serv_got_join_chat_failed (gc, data);
    } else if (joinString && *joinString) {
        if (isValidInviteLink(joinString))
            tdClient->joinChatByInviteLink(joinString);
        else {
            std::string name = getGroupUsernameFromLink(joinString);
            if (name.empty() && (strchr(joinString, '/') == NULL))
                name = joinString;

            if (!name.empty())
                tdClient->joinChatByGroupName(joinString, name.c_str());
            else {
                // TRANSLATOR: Join error dialog, secondary content. all five arguments are URLs. "name" should be part of the URL, and indicate that it can be a name in your language.
                std::string extraMessage = formatMessage(_("Invite link must start with {0}, {1} or {2}. Public group link must be {3}name or {4}name."),
                                                         {invitePrefixes[0], invitePrefixes[1], invitePrefixes[2],
                                                         groupLinkPrefixes[0], groupLinkPrefixes[1]});
                // TRANSLATOR: Join error dialog, title
                purple_notify_error(gc, _("Failed to join chat"),
                                    // TRANSLATOR: Join error dialog, primary content
                                    _("Must be invite link, public group link or group name"),
                                    extraMessage.c_str());
                purple_serv_got_join_chat_failed (gc, data);
            }
        }
    } else {
        const char *groupName  = getChatGroupName(data);
        int         groupType  = getChatGroupType(data);
        if (groupName && *groupName && ((groupType == GROUP_TYPE_BASIC) ||
                                        (groupType == GROUP_TYPE_SUPER) ||
                                        (groupType == GROUP_TYPE_CHANNEL))) {
            if (groupType == GROUP_TYPE_BASIC)
                requestCreateBasicGroup(gc, groupName);
            else
                tdClient->createGroup(groupName, groupType, {});
        } else {
            // TRANSLATOR: Join error dialog, title
            purple_notify_error(gc, _("Failed to join chat"),
                                // TRANSLATOR: Join error dialog, primary content
                                _("Please enter group name and valid type"), NULL);
            purple_serv_got_join_chat_failed (gc, data);
        }
    }
}

static char *tgprpl_get_chat_name (GHashTable * data)
{
    return g_strdup(getChatName(data));
}

static void tgprpl_chat_invite (PurpleConnection *gc, int id, const char *message, const char *who)
{
    PurpleTdClient *tdClient = static_cast<PurpleTdClient *>(purple_connection_get_protocol_data(gc));
    if (tdClient)
        tdClient->addUserToChat(id, who);
}

static int tgprpl_send_chat (PurpleConnection *gc, int id, const char *message, PurpleMessageFlags flags)
{
    purple_debug_misc(config::pluginId, "Sending group chat message: purple chat id %d, flags=0x%x\n",
                      id, (unsigned)flags);
    PurpleTdClient *tdClient = static_cast<PurpleTdClient *>(purple_connection_get_protocol_data(gc));
    return tdClient->sendGroupMessage(id, message);
}

static void tgprpl_rename_buddy(PurpleConnection *gc, const char *who, const char *alias)
{
    PurpleTdClient *tdClient = static_cast<PurpleTdClient *>(purple_connection_get_protocol_data(gc));
    tdClient->renameContact(who, alias);
}

static void renameGroup(PurpleConnection *, const char *old_name,
						PurpleGroup *group, GList *moved_buddies)
{
    // If PRPL does not have this function, libpurple will mass-remove all buddies that were in the
    // group (really remove, using remove_buddy) and then mass-create them again with add_buddy.
    // With some effort, it might even work, but it's completely unnecessary, so avoid it by
    // implementing a rename_group that does nothing.
}

static PurpleRoomlist *tgprpl_roomlist_get_list (PurpleConnection *gc)
{
    PurpleTdClient *tdClient = static_cast<PurpleTdClient *>(purple_connection_get_protocol_data(gc));
    PurpleRoomlist *roomlist = purple_roomlist_new(purple_connection_get_account(gc));

    if (tdClient)
        tdClient->getGroupChatList(roomlist);

    return roomlist;
}

static void tgprpl_roomlist_cancel (PurpleRoomlist *list)
{
}

static char *getRoomlistChatName(PurpleRoomlistRoom *room)
{
    return g_strdup((char *)purple_roomlist_room_get_fields(room)->data);
}

static gboolean tgprpl_can_receive_file (PurpleConnection *gc, const char *who)
{
    return TRUE;
}

struct UploadData {
    enum class Type {
        User,
        Chat
    };
    PurpleAccount *account;
    Type           type;
    int            chatId;
};

static void startUpload(PurpleXfer *xfer)
{
    const UploadData *data     = static_cast<UploadData *>(xfer->data);
    PurpleTdClient   *tdClient = getTdClient(data->account);
    if (!tdClient)
        return;

    switch (data->type) {
        case UploadData::Type::User: {
            const char *username = purple_xfer_get_remote_user(xfer);
            if (username)
                tdClient->sendFileToChat(xfer, username, PURPLE_CONV_TYPE_IM, 0);
            break;
        }
        case UploadData::Type::Chat: {
            tdClient->sendFileToChat(xfer, NULL, PURPLE_CONV_TYPE_CHAT, data->chatId);
            break;
        }
    }
}

static void cancelUpload(PurpleXfer *xfer)
{
    std::unique_ptr<UploadData> data(static_cast<UploadData *>(xfer->data));
    PurpleTdClient *tdClient = getTdClient(data->account);

    if (tdClient)
        tdClient->cancelUpload(xfer);
}

static void endUpload(PurpleXfer *xfer)
{
    delete static_cast<UploadData *>(xfer->data);
}

static PurpleXfer *newUploadTransfer(PurpleConnection *gc, const char *who)
{
    PurpleAccount *account = purple_connection_get_account (gc);
    PurpleXfer    *xfer    = purple_xfer_new(account, PURPLE_XFER_SEND, who);
    if (xfer) {
        purple_xfer_set_init_fnc(xfer, startUpload);
        purple_xfer_set_cancel_send_fnc(xfer, cancelUpload);
        purple_xfer_set_end_fnc(xfer, endUpload);
    }

    return xfer;
}

static void tgprpl_send_file (PurpleConnection * gc, const char *who, const char *file)
{
    PurpleXfer *X = newUploadTransfer (gc, who);
    X->data = new UploadData{purple_connection_get_account(gc), UploadData::Type::User, 0};
    if (file) {
        purple_xfer_request_accepted (X, file);
    } else {
        purple_xfer_request (X);
    }
}

static void sendFileToChat(PurpleConnection *gc, int id, const char *filename)
{
    PurpleConversation *conv = purple_find_chat(gc, id);
    if (!conv) {
        purple_debug_warning(config::pluginId, "No chat conversation with id %d\n", id);
        return;
    }

    PurpleXfer *X = newUploadTransfer(gc, purple_conversation_get_title(conv));
    X->data = new UploadData{purple_connection_get_account(gc), UploadData::Type::Chat, id};

    if (filename) {
        purple_xfer_request_accepted (X, filename);
    } else {
        purple_xfer_request (X);
    }
}

static GHashTable *tgprpl_get_account_text_table (PurpleAccount *pa)
{
    GHashTable *HT;
    HT = g_hash_table_new (g_str_hash, g_str_equal);
    static char label[] = "login_label";
    // TRANSLATOR: Account creation, telephone hint. Keep it short!
    g_hash_table_insert(HT, label, _("phone no. (+ country prefix)"));
    return HT;
}

static void tgprpl_set_chat_topic(PurpleConnection *gc, int id, const char *topic)
{
    PurpleTdClient *tdClient = static_cast<PurpleTdClient *>(purple_connection_get_protocol_data(gc));
    tdClient->setGroupDescription(id, topic);
}

static PurpleCmdRet tgprpl_cmd_kick(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data)
{
    PurpleTdClient *tdClient = getTdClient(purple_conversation_get_account(conv));

    if (!*args || !tdClient)
        return PURPLE_CMD_RET_FAILED;

    tdClient->kickUserFromChat(conv, *args);
    return PURPLE_CMD_RET_OK;
}

static PurpleMediaCaps getMediaCaps(PurpleAccount *account, const char *who)
{
#ifndef NoVoip
    return PURPLE_MEDIA_CAPS_AUDIO;
#else
    return PURPLE_MEDIA_CAPS_NONE;
#endif
}

gboolean initiateMedia(PurpleAccount *account, const char *who, PurpleMediaSessionType type)
{
#ifndef NoVoip
    if (!(type & PURPLE_MEDIA_AUDIO))
        return FALSE;

    PurpleTdClient *tdClient = getTdClient(account);
    if (tdClient)
        return tdClient->startVoiceCall(who) ? TRUE : FALSE;
    else
        return FALSE;
#else
    return FALSE;
#endif
}

static PurpleCmdRet hangupCommand(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data)
{
    PurpleTdClient *tdClient = getTdClient(purple_conversation_get_account(conv));

    if (!tdClient)
        return PURPLE_CMD_RET_FAILED;

    if (tdClient->terminateCall(conv))
        return PURPLE_CMD_RET_OK;
    else
        return PURPLE_CMD_RET_FAILED;
}

static char png[] = "png";

static PurplePluginProtocolInfo prpl_info = {
    .options          = (PurpleProtocolOptions)(OPT_PROTO_NO_PASSWORD | OPT_PROTO_IM_IMAGE | OPT_PROTO_CHAT_TOPIC),
    .user_splits      = NULL, // was initialized in tgprpl_init()
    .protocol_options = NULL, // was initialized in tgprpl_init()

    .icon_spec = {
        .format       = png,
        .min_width    = 1,
        .min_height   = 1,
        .max_width    = 512,
        .max_height   = 512,
        .max_filesize = 64000,
        .scale_rules  = PURPLE_ICON_SCALE_SEND,
    },

    .list_icon                = tgprpl_list_icon,
    .list_emblem              = NULL,
    .status_text              = NULL,
    .tooltip_text             = tgprpl_tooltip_text,
    .status_types             = tgprpl_status_types,
    .blist_node_menu          = tgprpl_blist_node_menu,
    .chat_info                = tgprpl_chat_join_info,
    .chat_info_defaults       = tgprpl_chat_info_defaults,
    .login                    = tgprpl_login,
    .close                    = tgprpl_close,
    .send_im                  = tgprpl_send_im,
    .set_info                 = NULL,
    .send_typing              = tgprpl_send_typing,
    .get_info                 = tgprpl_info_show,
    .set_status               = tgprpl_set_status,
    .set_idle                 = NULL,
    .change_passwd            = NULL,
    .add_buddy                = tgprpl_add_buddy,
    .add_buddies              = NULL,
    .remove_buddy             = tgprpl_request_delete_contact,
    .remove_buddies           = NULL,
    .add_permit               = NULL,
    .add_deny                 = NULL,
    .rem_permit               = NULL,
    .rem_deny                 = NULL,
    .set_permit_deny          = NULL,
    .join_chat                = tgprpl_chat_join,
    .reject_chat              = NULL,
    .get_chat_name            = tgprpl_get_chat_name,
    .chat_invite              = tgprpl_chat_invite,
    .chat_leave               = NULL,
    .chat_whisper             = NULL,
    .chat_send                = tgprpl_send_chat,
    .keepalive                = NULL,
    .register_user            = NULL,
    .get_cb_info              = NULL,
    .get_cb_away              = NULL,
    .alias_buddy              = tgprpl_rename_buddy,
    .group_buddy              = NULL,
    .rename_group             = renameGroup,
    .buddy_free               = NULL,
    .convo_closed             = NULL,
    .normalize                = NULL,
    .set_buddy_icon           = NULL,
    .remove_group             = NULL,
    .get_cb_real_name         = NULL,
    .set_chat_topic           = tgprpl_set_chat_topic,
    .find_blist_chat          = NULL,
    .roomlist_get_list        = tgprpl_roomlist_get_list,
    .roomlist_cancel          = tgprpl_roomlist_cancel,
    .roomlist_expand_category = NULL,
    .can_receive_file         = tgprpl_can_receive_file,
    .send_file                = tgprpl_send_file,
    .new_xfer                 = newUploadTransfer,
    .offline_message          = NULL,
    .whiteboard_prpl_ops      = NULL,
    .send_raw                 = NULL,
    .roomlist_room_serialize  = getRoomlistChatName,
    .unregister_user          = NULL,
    .send_attention           = NULL,
    .get_attention_types      = NULL,
    .struct_size              = sizeof(PurplePluginProtocolInfo),
    .get_account_text_table   = tgprpl_get_account_text_table,
    .initiate_media           = initiateMedia,
    .get_media_caps           = getMediaCaps,
    .get_moods                = NULL,
    .set_public_alias         = NULL,
    .get_public_alias         = NULL,
    .add_buddy_with_invite    = NULL,
    .add_buddies_with_invite  = NULL,
#if PURPLE_VERSION_CHECK(2,14,0)
    .get_cb_alias             = NULL,
    .chat_can_receive_file    = NULL,
    .chat_send_file           = sendFileToChat,
#endif
};

static gboolean tgprpl_load (PurplePlugin *plugin)
{
    purple_cmd_register("kick", "s", PURPLE_CMD_P_PLUGIN,
                        (PurpleCmdFlag)(PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_PRPL_ONLY),
                        config::pluginId, tgprpl_cmd_kick,
                        // TRANSLATOR: Command description, the initial "kick <user>" must remain verbatim!
                        _("kick <user>: Kick a user from the room using name or internal id"), NULL);

    purple_cmd_register("hangup", "", PURPLE_CMD_P_PLUGIN,
                        (PurpleCmdFlag)(PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_PRPL_ONLY),
                        config::pluginId, hangupCommand,
                        // TRANSLATOR: Command description, the initial "hangup" must remain verbatim!
                        _("hangup: Terminate any active call (with any user)"), NULL);

    return TRUE;
}

static void addChoice(GList *&choices, const char *description, const char *value)
{
    PurpleKeyValuePair *kvp = g_new0(PurpleKeyValuePair, 1);
    kvp->key = g_strdup(description);
    kvp->value = g_strdup(value);
    choices = g_list_append(choices, kvp);
}

static PurplePluginInfo *getPluginInfo();

static gboolean tdlibFatalErrorHandler(void *data)
{
    char *message = static_cast<char *>(data);
    const char *dbMessage =
        // TRANSLATOR: Tdlib crash dialog, secondary content. Argument is a filesystem path. Please keep the space after it!
        _("The error may be caused by corrupt account data. "
          "If this is the case, it could be fixed by removing account data under {} . "
          "You will be required to log in into the account again.");

    // tdlib messages are untranslated, so can as well leave "tdlib error" untranslated as well
    std::string details = formatMessage("tdlib error: {}", std::string(message));
    details += '\n';
    details += formatMessage(dbMessage, PurpleTdClient::getBaseDatabasePath());

    // TRANSLATOR: Tdlib crash dialog, title
    purple_notify_error(getPluginInfo(), _("Fatal error encountered in telegram plugin"),
                        details.c_str(), NULL);

    free(message);
    return FALSE; // this idle handler will not be called again
}

static void tdlibFatalErrorCallback(const char *message)
{
    g_idle_add(tdlibFatalErrorHandler, strdup(message));
    // The error must have come either from the poll thread or from one of the threads created by tdlib.
    // So, hang the thread to avoid crash. All other accounts will be unaffected until an attempt to
    // disconnect this account is made, because then TdTransceiver destructor will wait forever for
    // poll thread to terminate, and everything will hang.
    // However, it's still possible to disable auto-login on the problematic account using
    // purple_account_set_enabled (Account -> Disable in pidgin, etc.), because it will first disable
    // auto-login, and only then disconnect the account and hang.
    while (1) sleep(1000);
}

static void tgprpl_init (PurplePlugin *plugin)
{
#if !PURPLE_VERSION_CHECK(2,14,0)
    (void)sendFileToChat;
#endif
    if (purple_debug_is_verbose())
        // Log everything
        PurpleTdClient::setLogLevel(1024);
    else if (purple_debug_is_enabled())
        // Log up to info
        PurpleTdClient::setLogLevel(3);
    else
        // Log up to fatal errors and errors
        PurpleTdClient::setLogLevel(1);
    PurpleTdClient::setTdlibFatalErrorCallback(tdlibFatalErrorCallback);

#ifndef NoLottie
    rlottie::configureModelCacheSize(0);
#endif

    GList *choices = NULL;
    if (!strcmp(AccountOptions::DownloadBehaviourDefault(), AccountOptions::DownloadBehaviourHyperlink)) {
        // TRANSLATOR: Account settings, value for file downloads (hyperlink link file:///tmp/asdf)
        addChoice(choices, _("Inline (hyperlinks in chat)"), AccountOptions::DownloadBehaviourHyperlink);
        // TRANSLATOR: Account settings, value for file downloads (file transfer dialog)
        addChoice(choices, _("Standard file transfers"), AccountOptions::DownloadBehaviourStandard);
    } else {
        addChoice(choices, _("Standard file transfers"), AccountOptions::DownloadBehaviourStandard);
        addChoice(choices, _("Inline (hyperlinks in chat)"), AccountOptions::DownloadBehaviourHyperlink);
    }
    
    // TRANSLATOR: Account settings, key (choice)
    PurpleAccountOption *opt = purple_account_option_list_new (_("File downloads"),
                                                               AccountOptions::DownloadBehaviour, choices);
    prpl_info.protocol_options = g_list_append (prpl_info.protocol_options, opt);

    // TRANSLATOR: Account settings, key (choice)
    opt = purple_account_option_string_new (_("Inline auto-download size limit, MB (0 for unlimited)"),
                                            AccountOptions::AutoDownloadLimit,
                                            AccountOptions::AutoDownloadLimitDefault);
    prpl_info.protocol_options = g_list_append (prpl_info.protocol_options, opt);

    static_assert(AccountOptions::BigDownloadHandlingDefault == AccountOptions::BigDownloadHandlingAsk,
                  "default choice must be first");
    choices = NULL;
    // TRANSLATOR: Account settings, value for large file downloads
    addChoice(choices, _("Ask"), AccountOptions::BigDownloadHandlingAsk);
    // TRANSLATOR: Account settings, value for large file downloads
    addChoice(choices, _("Discard"), AccountOptions::BigDownloadHandlingDiscard);

    // TRANSLATOR: Account settings, check box label
    opt = purple_account_option_bool_new(_("Do not clean up inline downloads"),
                                         AccountOptions::KeepInlineDownloads,
                                         AccountOptions::KeepInlineDownloadsDefault);
    prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, opt);

    // TRANSLATOR: Account settings, key (choice)
    opt = purple_account_option_list_new (_("Bigger inline file downloads"), AccountOptions::BigDownloadHandling, choices);
    prpl_info.protocol_options = g_list_append (prpl_info.protocol_options, opt);

    // TRANSLATOR: Account settings, key (boolean)
    opt = purple_account_option_bool_new (_("Enable secret chats (takes effect at reconnect)"),
                                          AccountOptions::EnableSecretChats,
                                          AccountOptions::EnableSecretChatsDefault);
    prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, opt);

#ifndef NoLottie
    // TRANSLATOR: Account settings, key (boolean)
    opt = purple_account_option_bool_new(_("Show animated stickers"), AccountOptions::AnimatedStickers,
                                         AccountOptions::AnimatedStickersDefault);
    prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, opt);
#endif

    // TRANSLATOR: Account settings, key (boolean)
    opt = purple_account_option_bool_new(_("Show self-destructing messages anyway"), AccountOptions::ShowSelfDestruct,
                                         AccountOptions::ShowSelfDestructDefault);
    prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, opt);

    if (canDisableReadReceipts()) {
        opt = purple_account_option_bool_new ("Send read receipts",
                                              AccountOptions::ReadReceipts,
                                              AccountOptions::ReadReceiptsDefault);
        prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, opt);
    }
}

static void setTwoFactorAuth(RequestData *data, PurpleRequestFields* fields);

static void requestTwoFactorAuth(PurpleConnection *gc, const char *primaryText, const char *email)
{
    PurpleRequestFields     *fields  = purple_request_fields_new();
    PurpleRequestFieldGroup *group   = purple_request_field_group_new(NULL);

    // TRANSLATOR: 2FA settings, key
    PurpleRequestField *field = purple_request_field_string_new ("oldpw", _("Current password"), NULL, FALSE);
    purple_request_field_string_set_masked(field, TRUE);
    purple_request_field_group_add_field (group, field);

    // TRANSLATOR: 2FA settings, key
    field = purple_request_field_string_new ("pw1", _("New password"), NULL, FALSE);
    purple_request_field_string_set_masked(field, TRUE);
    purple_request_field_group_add_field (group, field);

    // TRANSLATOR: 2FA settings, key
    field = purple_request_field_string_new ("pw2", _("Repeat password"), NULL, FALSE);
    purple_request_field_string_set_masked(field, TRUE);
    purple_request_field_group_add_field (group, field);

    // TRANSLATOR: 2FA settings, key
    field = purple_request_field_string_new ("hint", _("Password hint"), NULL, FALSE);
    purple_request_field_group_add_field (group, field);

    // TRANSLATOR: 2FA settings, key
    field = purple_request_field_string_new ("email", _("Recovery e-mail"), email, FALSE);
    purple_request_field_group_add_field (group, field);

    purple_request_fields_add_group (fields, group);

    RequestData *data = new RequestData(purple_connection_get_account(gc));
    data->account = purple_connection_get_account(gc);
    // TRANSLATOR: 2FA settings, title
    purple_request_fields (gc, _("Two-factor authentication"), primaryText, NULL, fields,
                           // TRANSLATOR: 2FA settings, alternative is "_Cancel". The underscore marks accelerator keys, they must be different!
                           _("_OK"), G_CALLBACK(setTwoFactorAuth),
                           // TRANSLATOR: 2FA settings, alternative is "_OK". The underscore marks accelerator keys, they must be different!
                           _("_Cancel"), G_CALLBACK(cancelRequest),
                           purple_connection_get_account(gc), NULL, NULL, data);
}

static int reRequestTwoFactorAuth(gpointer user_data)
{
    std::unique_ptr<RequestData> request(static_cast<RequestData *>(user_data));
    requestTwoFactorAuth(purple_account_get_connection(request->account),
                        // TRANSLATOR: 2FA settings, primary content (after mistype)
                        _("Please enter same password twice"), request->stringData.c_str());
    return FALSE; // this idle handler will not be called again
}

static void setTwoFactorAuth(RequestData *data, PurpleRequestFields* fields)
{
    std::unique_ptr<RequestData> request(data);
    PurpleTdClient *tdClient = getTdClient(request->account);

    if (tdClient) {
        const char *oldPass   = purple_request_fields_get_string(fields, "oldpw");
        const char *password1 = purple_request_fields_get_string(fields, "pw1");
        const char *password2 = purple_request_fields_get_string(fields, "pw2");
        const char *hint      = purple_request_fields_get_string(fields, "hint");
        const char *email     = purple_request_fields_get_string(fields, "email");

        if ((password1 != password2) && (!password1 || !password2 || strcmp(password1, password2))) {
            // Calling purple_request_fields synchronously causes glitch in pidgin
            RequestData *newRequest = new RequestData(request->account);
            if (email)
                newRequest->stringData = email;
            g_idle_add(reRequestTwoFactorAuth, newRequest);
        } else if (tdClient)
            tdClient->setTwoFactorAuth(oldPass, password1, hint, email);
    }
}

static void configureTwoFactorAuth(PurplePluginAction *action)
{
    PurpleConnection *gc = static_cast<PurpleConnection *>(action->context);
    // TRANSLATOR: 2FA settings, primary content
    requestTwoFactorAuth(gc, _("Enter new password and recovery e-mail address"), NULL);
}

static GList *tgprpl_actions (PurplePlugin *plugin, gpointer context)
{
    GList *actionsList = NULL;
    PurplePluginAction *action;

    // TRANSLATOR: 2FA settings, title
    action = purple_plugin_action_new(_("Configure two-factor authentication..."),
                                      configureTwoFactorAuth);
    actionsList = g_list_append(actionsList, action);

    return actionsList;
}

static PurplePluginInfo plugin_info = {
    .magic             = PURPLE_PLUGIN_MAGIC,
    .major_version     = PURPLE_MAJOR_VERSION,
    .minor_version     = PURPLE_MINOR_VERSION,
    .type              = PURPLE_PLUGIN_PROTOCOL,
    .ui_requirement    = NULL,
    .flags             = 0,
    .dependencies      = NULL,
    .priority          = PURPLE_PRIORITY_DEFAULT,
    .id                = config::pluginId,
    .name              = config::pluginName,
    .version           = config::versionString,
    .summary           = config::pluginSummary,
    .description       = _(config::pluginDesc),
    .author            = config::pluginAuthor,
    .homepage          = config::projectUrl,
    .load              = tgprpl_load,
    .unload            = NULL,
    .destroy           = NULL,
    .ui_info           = NULL,
    .extra_info        = &prpl_info,
    .prefs_info        = NULL,
    .actions           = tgprpl_actions,
    ._purple_reserved1 = NULL,
    ._purple_reserved2 = NULL,
    ._purple_reserved3 = NULL,
    ._purple_reserved4 = NULL,
};

extern "C" {
    PURPLE_INIT_PLUGIN (telegram_tdlib, tgprpl_init, plugin_info)
}

static PurplePluginInfo *getPluginInfo()
{
    return &plugin_info;
}
