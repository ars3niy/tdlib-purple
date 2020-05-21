#include "config.h"
#include "td-client.h"
#include "chat-info.h"
#include "format.h"
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
#include <ctype.h>

static char *_(const char *s) { return const_cast<char *>(s); }

static const char *tgprpl_list_icon (PurpleAccount *acct, PurpleBuddy *buddy)
{
    return "telegram";
}

static const char *getLastOnline(const td::td_api::UserStatus &status)
{
    switch (status.get_id()) {
        case td::td_api::userStatusOnline::ID:
            return _("now");
        case td::td_api::userStatusOffline::ID: {
            const td::td_api::userStatusOffline &offline = static_cast<const td::td_api::userStatusOffline &>(status);
            time_t timestamp = offline.was_online_;
            return ctime(&timestamp);
        }
        case td::td_api::userStatusRecently::ID:
            return _("recently");
        case td::td_api::userStatusLastWeek::ID:
            return _("last week");
        case td::td_api::userStatusLastMonth::ID:
            return _("last month");
    }

    return "";
}

static void tgprpl_tooltip_text (PurpleBuddy *buddy, PurpleNotifyUserInfo *info, gboolean full)
{
    PurpleConnection *connection = purple_account_get_connection(purple_buddy_get_account(buddy));
    PurpleTdClient   *tdClient   = static_cast<PurpleTdClient *>(purple_connection_get_protocol_data(connection));

    std::vector<const td::td_api::user *> users;
    tdClient->getUsers(purple_buddy_get_name(buddy), users);

    if ((users.size() == 1) && users[0]->status_) {
        const char *lastOnline = getLastOnline(*users[0]->status_);
        if (lastOnline && *lastOnline)
            purple_notify_user_info_add_pair(info, _("Last online"), lastOnline);
    }
}

static GList *tgprpl_status_types (PurpleAccount *acct)
{
    GList *types = NULL;
    PurpleStatusType *type;

    type = purple_status_type_new_full (PURPLE_STATUS_AVAILABLE, NULL, NULL, FALSE, TRUE, FALSE);
    types = g_list_prepend (types, type);

    type = purple_status_type_new_full (PURPLE_STATUS_OFFLINE, NULL, NULL, FALSE, TRUE, FALSE);
    types = g_list_prepend (types, type);

    return g_list_reverse (types);
}

static GList* tgprpl_blist_node_menu (PurpleBlistNode *node)
{
    /*
  if (PURPLE_BLIST_NODE_IS_CHAT(node)) {

    // Delete self from chat
    PurpleMenuAction* action = purple_menu_action_new (_("Delete and exit..."), PURPLE_CALLBACK(leave_and_delete_chat_gw),
        NULL, NULL);
    menu = g_list_append (menu, (gpointer)action);
  }
  */
    return NULL;
}

static GList *tgprpl_chat_join_info (PurpleConnection *gc)
{
    return getChatJoinInfo();
}

static GHashTable *tgprpl_chat_info_defaults (PurpleConnection *gc, const char *chat_name)
{
    return g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
}

static ITransceiverBackend *g_testBackend = nullptr;

void tgprpl_set_test_backend(ITransceiverBackend *backend)
{
    g_testBackend = backend;
}

static void tgprpl_login (PurpleAccount *acct)
{
    PurpleConnection *gc       = purple_account_get_connection (acct);
    PurpleTdClient   *tdClient = new PurpleTdClient(acct, g_testBackend);

    purple_connection_set_protocol_data (gc, tdClient);
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
    if (users.empty())
        purple_notify_user_info_add_pair(info, _("User not found"), NULL);

    for (const td::td_api::user *user: users) {
        if (purple_notify_user_info_get_entries(info))
            purple_notify_user_info_add_section_break(info);

        purple_notify_user_info_add_pair(info, _("First name"), user->first_name_.c_str());
        purple_notify_user_info_add_pair(info, _("Last name"), user->last_name_.c_str());
        if (!user->username_.empty())
            purple_notify_user_info_add_pair(info, _("Username"), user->username_.c_str());
        if (!user->phone_number_.empty())
            purple_notify_user_info_add_pair(info, _("Phone number"), user->phone_number_.c_str());
        if (user->status_) {
            const char *lastOnline = getLastOnline(*user->status_);
            if (lastOnline && *lastOnline)
                purple_notify_user_info_add_pair(info, _("Last online"), lastOnline);
        }
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

static void request_delete_contact_on_server_yes (void *data, PurpleRequestFields* fields)
{
}

static void request_delete_contact_on_server_no (void *data, PurpleRequestFields* fields)
{
}

static void tgprpl_request_delete_contact (PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group)
{
    g_return_if_fail(buddy);

    purple_request_action(gc, "Remove contact", "Remove contact", "Remove contact on the server?",
                          0, purple_connection_get_account(gc), NULL, NULL, NULL, 2,
                          _("_No"), request_delete_contact_on_server_yes,
                          _("_No"), request_delete_contact_on_server_no);
}

static std::array<const char *, 3> invitePrefixes {
    "https://t.me/joinchat/",
    "https://telegram.me/joinchat/",
    "https://telegram.dog/joinchat/"
};

static bool isValidInviteLink(const char *link)
{
    return !strncmp(link, invitePrefixes[0], strlen(invitePrefixes[0])) ||
           !strncmp(link, invitePrefixes[1], strlen(invitePrefixes[1])) ||
           !strncmp(link, invitePrefixes[2], strlen(invitePrefixes[2]));
}

static void tgprpl_chat_join (PurpleConnection *gc, GHashTable *data)
{
    PurpleTdClient *tdClient   = static_cast<PurpleTdClient *>(purple_connection_get_protocol_data(gc));
    const char     *name       = getChatName(data);
    const char     *inviteLink = getChatInviteLink(data);

    if (name && *name) {
        if (!tdClient->joinChat(name))
            purple_serv_got_join_chat_failed (gc, data);
    } else if (inviteLink && *inviteLink) {
        // Some user-friendliness
        if (!isValidInviteLink(inviteLink)) {
            std::string message = formatMessage(_("Invite link must begin with {}, {}, or {}"),
                                                {invitePrefixes[0], invitePrefixes[1], invitePrefixes[2]});
            purple_notify_error(gc, _("Failed to join chat"), message.c_str(), NULL);
        }
        else if (!tdClient->joinChatByLink(inviteLink))
            purple_serv_got_join_chat_failed (gc, data);
    } else
        purple_serv_got_join_chat_failed (gc, data);
}

static char *tgprpl_get_chat_name (GHashTable * data)
{
    return g_strdup(getChatName(data));
}

static void tgprpl_chat_invite (PurpleConnection *gc, int id, const char *message, const char *who)
{
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

static PurpleRoomlist *tgprpl_roomlist_get_list (PurpleConnection *gc)
{
    static PurpleRoomlist *roomlist = NULL; // put it on like protocol_data

    if (roomlist)
        purple_roomlist_unref (roomlist);
    roomlist = purple_roomlist_new (purple_connection_get_account (gc));

    purple_roomlist_set_in_progress (roomlist, TRUE);
    // blah blah blah
    purple_roomlist_set_in_progress (roomlist, FALSE);

    return roomlist;
}

static void tgprpl_roomlist_cancel (PurpleRoomlist *list)
{
}

static gboolean tgprpl_can_receive_file (PurpleConnection *gc, const char *who)
{
    return TRUE;
}

static void tgprpl_xfer_send_init (PurpleXfer *X)
{
}

static void tgprpl_xfer_canceled (PurpleXfer *X)
{
}

static PurpleXfer *tgprpl_new_xfer (PurpleConnection *gc, const char *who)
{
    PurpleXfer *X = purple_xfer_new (purple_connection_get_account (gc), PURPLE_XFER_SEND, who);
    if (X) {
        purple_xfer_set_init_fnc (X, tgprpl_xfer_send_init);
        purple_xfer_set_cancel_send_fnc (X, tgprpl_xfer_canceled);
    }

    return X;
}

static void tgprpl_send_file (PurpleConnection * gc, const char *who, const char *file)
{
    PurpleXfer *X = tgprpl_new_xfer (gc, who);
    if (file) {
        purple_xfer_request_accepted (X, file);
    } else {
        purple_xfer_request (X);
    }
}

static GHashTable *tgprpl_get_account_text_table (PurpleAccount *pa)
{
    GHashTable *HT;
    HT = g_hash_table_new (g_str_hash, g_str_equal);
    static char label[] = "login_label";
    g_hash_table_insert(HT, label, _("phone no. (+ country prefix)"));
    return HT;
}

static char png[] = "png";

static PurplePluginProtocolInfo prpl_info = {
    .options          = (PurpleProtocolOptions)(OPT_PROTO_NO_PASSWORD | OPT_PROTO_IM_IMAGE),
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
    .rename_group             = NULL,
    .buddy_free               = NULL,
    .convo_closed             = NULL,
    .normalize                = NULL,
    .set_buddy_icon           = NULL,
    .remove_group             = NULL,
    .get_cb_real_name         = NULL,
    .set_chat_topic           = NULL,
    .find_blist_chat          = NULL,
    .roomlist_get_list        = tgprpl_roomlist_get_list,
    .roomlist_cancel          = tgprpl_roomlist_cancel,
    .roomlist_expand_category = NULL,
    .can_receive_file         = tgprpl_can_receive_file,
    .send_file                = tgprpl_send_file,
    .new_xfer                 = tgprpl_new_xfer,
    .offline_message          = NULL,
    .whiteboard_prpl_ops      = NULL,
    .send_raw                 = NULL,
    .roomlist_room_serialize  = NULL,
    .unregister_user          = NULL,
    .send_attention           = NULL,
    .get_attention_types      = NULL,
    .struct_size              = sizeof(PurplePluginProtocolInfo),
    .get_account_text_table   = tgprpl_get_account_text_table,
    .initiate_media           = NULL,
    .get_media_caps           = NULL,
    .get_moods                = NULL,
    .set_public_alias         = NULL,
    .get_public_alias         = NULL,
    .add_buddy_with_invite    = NULL,
    .add_buddies_with_invite  = NULL,
};

static gboolean tgprpl_load (PurplePlugin *plugin)
{
    //purple_cmd_register("kick", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_CHAT |
    //                    PURPLE_CMD_FLAG_PRPL_ONLY | PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS,
    //                    PLUGIN_ID, tgprpl_cmd_kick,
    //                    _("kick <user>:  Kick a user from the room."), NULL);

    return TRUE;
}

static void tgprpl_init (PurplePlugin *plugin)
{
    if (purple_debug_is_verbose())
        // Log everything
        PurpleTdClient::setLogLevel(1024);
    else if (purple_debug_is_enabled())
        // Log up to info
        PurpleTdClient::setLogLevel(3);
    else
        // Log up to fatal errors and errors
        PurpleTdClient::setLogLevel(1);
}

static GList *tgprpl_actions (PurplePlugin *plugin, gpointer context)
{
    return (GList *)NULL;
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
