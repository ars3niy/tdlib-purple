#include "config.h"
#include "td-client.h"
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
    struct proto_chat_entry *pce;
    pce = g_new0 (struct proto_chat_entry, 1);
    pce->label = _("Subject:");
    pce->identifier = "subject";
    pce->required = FALSE;
    GList *info = g_list_append (NULL, pce);

    pce = g_new0 (struct proto_chat_entry, 1);
    pce->label = _("Invite link:");
    pce->identifier = "link";
    pce->required = FALSE;
    info = g_list_append (info, pce);

    pce = g_new0 (struct proto_chat_entry, 1);
    pce->label = _("Chat ID:");
    pce->identifier = "id";
    pce->required = FALSE;

    return g_list_append (info, pce);
}

static GHashTable *tgprpl_chat_info_defaults (PurpleConnection *gc, const char *chat_name)
{
    return g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
}

static void tgprpl_login (PurpleAccount *acct)
{
    PurpleConnection *gc       = purple_account_get_connection (acct);
    PurpleTdClient   *tdClient = new PurpleTdClient(acct);

    purple_connection_set_protocol_data (gc, tdClient);
    purple_connection_set_state (gc, PURPLE_CONNECTING);
    purple_connection_update_progress(gc, "Connecting", 1, 3);

    //purple_connection_set_state (gc, PURPLE_CONNECTED);
    //purple_blist_add_account (acct);
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
    // TODO fix and enable
    // PurpleTdClient *tdClient = static_cast<PurpleTdClient *>(purple_connection_get_protocol_data(gc));
    // tdClient->addContact(buddy->name, buddy->alias);
}

static void request_delete_contact_ok (void *data, PurpleRequestFields* fields)
{
}

static void request_delete_contact_cancel (void *data, PurpleRequestFields* fields)
{
}

static void tgprpl_request_delete_contact (PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group)
{
    const char *title1 = NULL, *title2 = NULL, *msg = NULL;

    g_return_if_fail(buddy);

    title1 = _("Test");
    title2 = title1;
    msg = _("Do you want to?");

    if (msg) {
        purple_request_ok_cancel(gc, title1, title2, msg,
            0, purple_connection_get_account(gc), "test", NULL,
            NULL, // user_data
            request_delete_contact_ok, request_delete_contact_cancel);
    }
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
    (PurpleProtocolOptions)(OPT_PROTO_NO_PASSWORD | OPT_PROTO_IM_IMAGE),
    NULL,                    // user_¡splits, initialized in tgprpl_init()
    NULL,                    // protocol_options, initialized in tgprpl_init()
    {
        png,
        1,                     // min_width
        1,                     // min_height
        512,                   // max_width
        512,                   // max_height
        64000,                 // max_filesize
        PURPLE_ICON_SCALE_SEND,
    },
    tgprpl_list_icon,
    NULL,
    NULL,
    tgprpl_tooltip_text,
    tgprpl_status_types,
    tgprpl_blist_node_menu,
    tgprpl_chat_join_info,
    tgprpl_chat_info_defaults,
    tgprpl_login,
    tgprpl_close,
    tgprpl_send_im,
    NULL,                    // set_info
    tgprpl_send_typing,
    tgprpl_info_show,
    tgprpl_set_status,
    NULL,                    // set_idle
    NULL,                    // change_passwd
    tgprpl_add_buddy,
    NULL,                    // add_buddies
    tgprpl_request_delete_contact,
    NULL,                    // remove_buddies
    NULL,                    // add_permit
    NULL,                    // add_deny
    NULL,                    // rem_permit
    NULL,                    // rem_deny
    NULL,                    // set_permit_deny
    tgprpl_chat_join,
    NULL,                    // reject_chat
    tgprpl_get_chat_name,
    tgprpl_chat_invite,
    NULL,                    // chat_leave
    NULL,                    // chat_whisper
    tgprpl_send_chat,
    NULL,                    // keepalive
    NULL,                    // register_user
    NULL,                    // get_cb_info
    NULL,                    // get_cb_away
    NULL,                    // alias_buddy
    NULL,                    // group_buddy
    NULL,                    // rename_group
    NULL,                    // buddy_free
    NULL,                    // convo_closed
    NULL,                    // normalize
    NULL,                    // tgprpl_set_buddy_icon
    NULL,                    // remove_group
    NULL,
    NULL,                    // set_chat_topic
    NULL,                    // find_blist_chat
    tgprpl_roomlist_get_list,
    tgprpl_roomlist_cancel,
    NULL,                    // roomlist_expand_category
    tgprpl_can_receive_file,
    tgprpl_send_file,
    tgprpl_new_xfer,
    NULL,                    // offline_message
    NULL,                    // whiteboard_prpl_ops
    NULL,                    // send_raw
    NULL,                    // roomlist_room_serialize
    NULL,                    // unregister_user
    NULL,                    // send_attention
    NULL,                    // get_attention_types
    sizeof(PurplePluginProtocolInfo),
    tgprpl_get_account_text_table, // get_account_text_table
    NULL,                    // initiate_media
    NULL,                    // get_media_caps
    NULL,                    // get_moods
    NULL,                    // set_public_alias
    NULL,                    // get_public_alias
    NULL,                    // add_buddy_with_invite
    NULL                     // add_buddies_with_invite
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
    PURPLE_PLUGIN_MAGIC,
    PURPLE_MAJOR_VERSION,
    PURPLE_MINOR_VERSION,
    PURPLE_PLUGIN_PROTOCOL,
    NULL,
    0,
    NULL,
    PURPLE_PRIORITY_DEFAULT,
    config::pluginId,
    config::pluginName,
    config::versionString,
    config::pluginSummary,
    _(config::pluginDesc),
    config::pluginAuthor,
    config::projectUrl,
    tgprpl_load,    // on load
    NULL,           // on unload
    NULL,           // on destroy
    NULL,           // ui specific struct
    &prpl_info,
    NULL,           // prefs info
    tgprpl_actions,
    NULL,           // reserved
    NULL,           // reserved
    NULL,           // reserved
    NULL            // reserved
};

extern "C" {
    PURPLE_INIT_PLUGIN (telegram_tdlib, tgprpl_init, plugin_info)
}
