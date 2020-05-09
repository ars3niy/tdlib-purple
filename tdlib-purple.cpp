#include "config.h"
#include "td-client.h"
#include "chat-info.h"
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

static const char *tgprpl_list_icon (PurpleAccount *acct, PurpleBuddy *buddy)
{
    return "telegram";
}

static void tgprpl_tooltip_text (PurpleBuddy *buddy, PurpleNotifyUserInfo *info, gboolean full)
{
    purple_notify_user_info_add_pair (info, "test: ", "TEST");
}

static GList *tgprpl_status_types (PurpleAccount *acct)
{
    GList *types = NULL;
    PurpleStatusType *type;

    type = purple_status_type_new_full (PURPLE_STATUS_AVAILABLE, NULL, NULL, FALSE, TRUE, FALSE);
    types = g_list_prepend (types, type);

    type = purple_status_type_new_full (PURPLE_STATUS_MOBILE, NULL, NULL, FALSE, TRUE, FALSE);
    types = g_list_prepend (types, type);

    type = purple_status_type_new_full (PURPLE_STATUS_OFFLINE, NULL, NULL, FALSE, TRUE, FALSE);
    types = g_list_prepend (types, type);

    /*
    The states below are only registered internally so that we get notified about
    state changes to away and unavailable. This is useful for deciding when to send
    No other peer should ever have those states.
    */
    type = purple_status_type_new_full (PURPLE_STATUS_AWAY, NULL, NULL, FALSE, TRUE, FALSE);
    types = g_list_prepend (types, type);

    type = purple_status_type_new_full (PURPLE_STATUS_UNAVAILABLE, NULL, NULL, FALSE, TRUE, FALSE);
    types = g_list_prepend (types, type);

    return g_list_reverse (types);
}

static GList* tgprpl_blist_node_menu (PurpleBlistNode *node)
{
    return NULL;
}

static const char *_(const char *s) { return s; }
static char *_(char *s) { return s; }

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
    return 0;
}

static void tgprpl_info_show (PurpleConnection *gc, const char *who)
{
    //tgl_do_get_channel_info (gc_get_tls (gc), P->id, FALSE, tgp_info_load_channel_done, P);
    //tgl_do_get_user_info (gc_get_tls (gc), P->id, 0, tgp_info_load_user_done, P);
}

static void tgprpl_set_status (PurpleAccount *acct, PurpleStatus *status)
{
}

static void tgprpl_add_buddy (PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group)
{
    const char *phoneNumber = purple_buddy_get_name(buddy);
    if (isPhoneNumber(phoneNumber) && !isCanonicalPhoneNumber(phoneNumber)) {
        std::string newNumber = getCanonicalPhoneNumber(phoneNumber);
        std::string alias     = purple_buddy_get_alias(buddy);
        purple_blist_remove_buddy(buddy);
        buddy = purple_buddy_new(purple_connection_get_account(gc), newNumber.c_str(), alias.c_str());
        purple_blist_add_buddy(buddy, NULL, group, NULL);
    }

    PurpleTdClient *tdClient = static_cast<PurpleTdClient *>(purple_connection_get_protocol_data(gc));
    tdClient->addContact(purple_buddy_get_name(buddy), purple_buddy_get_alias(buddy));
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

static void tgprpl_chat_join (PurpleConnection *gc, GHashTable *data)
{
    purple_serv_got_join_chat_failed (gc, data);
}

static char *tgprpl_get_chat_name (GHashTable * data)
{
    return g_strdup("test");
}

static void tgprpl_chat_invite (PurpleConnection *gc, int id, const char *message, const char *who)
{
}

static int tgprpl_send_chat (PurpleConnection *gc, int id, const char *message, PurpleMessageFlags flags)
{
    return -1;
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
    static char phoneno[] = "phone no. (+ country prefix)";
    g_hash_table_insert(HT, label, _(phoneno));
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
    .alias_buddy              = NULL,
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
