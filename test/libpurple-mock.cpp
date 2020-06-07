#include "purple-events.h"
#include <purple.h>
#include <stdarg.h>
#include <vector>
#include <gtest/gtest.h>

struct AccountInfo {
    PurpleAccount                     *account;
    std::vector<PurpleBuddy *>         buddies;
    std::vector<PurpleChat *>          chats;
    std::vector<PurpleConversation *>  conversations;
    std::map<std::string, std::string> stringsOptions;
};

std::vector<AccountInfo>  g_accounts;
PurplePlugin             *g_plugin;

extern "C" {

#define EVENT(type, ...) g_purpleEvents.addEvent(std::make_unique<type>(__VA_ARGS__))

void purple_debug_misc(const char *category, const char *format, ...)
{
    va_list va;
    va_start(va, format);
    printf("%s: ", category);
    vprintf(format, va);
    va_end(va);
}

void purple_debug_info(const char *category, const char *format, ...)
{
    va_list va;
    va_start(va, format);
    printf("Info: %s: ", category);
    vprintf(format, va);
    va_end(va);
}

void purple_debug_warning(const char *category, const char *format, ...)
{
    va_list va;
    va_start(va, format);
    printf("Warning: %s: ", category);
    vprintf(format, va);
    va_end(va);
}

const char *purple_account_get_username(const PurpleAccount *account)
{
    return account->username;
}

const char *purple_account_get_alias(const PurpleAccount *account)
{
    return account->alias;
}

PurpleConnection *purple_account_get_connection(const PurpleAccount *account)
{
    return account->gc;
}

gboolean purple_account_is_connected(const PurpleAccount *account)
{
    return PURPLE_CONNECTION_IS_CONNECTED(account->gc);
}

void purple_account_set_alias(PurpleAccount *account, const char *alias)
{
    free(account->alias);
    account->alias = strdup(alias);
    EVENT(AccountSetAliasEvent, account, alias);
}

PurpleAccount *purple_account_new(const char *username, const char *protocol_id)
{
    PurpleAccount *account = new PurpleAccount;
    account->username = strdup(username);
    account->alias = nullptr;
    account->proxy_info = NULL;

    g_accounts.emplace_back();
    g_accounts.back().account = account;

    return account;
}

PurpleBlistNode root = {
    .type = PURPLE_BLIST_OTHER_NODE,
    .prev = NULL,
    .next = NULL,
    .parent = NULL,
    .child = NULL,
    .settings = NULL,
    .ui_data = NULL,
    .flags = (PurpleBlistNodeFlags)0
};

void purple_account_destroy(PurpleAccount *account)
{
    free(account->username);
    free(account->alias);

    auto it = std::find_if(g_accounts.begin(), g_accounts.end(),
                           [account](const AccountInfo &info) { return (info.account == account); });
    ASSERT_FALSE(it == g_accounts.end()) << "Destroying unknown account";
    while (!it->buddies.empty())
        purple_blist_remove_buddy(it->buddies.back());
    while (!it->conversations.empty())
        purple_conversation_destroy(it->conversations.back());
    while (!it->chats.empty())
        purple_blist_remove_chat(it->chats.back());
    g_accounts.erase(it);

    delete account;
    ASSERT_EQ(nullptr, root.child) << "Blist nodes remain";
}

const char *purple_account_get_protocol_id(const PurpleAccount *account)
{
    return "";
}

PurpleAccount *purple_accounts_find(const char *name, const char *protocol)
{
    auto it = std::find_if(g_accounts.begin(), g_accounts.end(),
                           [name](const AccountInfo &account) {
                               return !strcmp(account.account->username, name);
                           });
    if (it != g_accounts.end())
        return it->account;
    return NULL;
}

void purple_blist_add_account(PurpleAccount *account)
{
    EVENT(ShowAccountEvent, account);
}

static void addNode(PurpleBlistNode &node)
{
    node.next = root.child;
    node.prev = NULL;
    if (root.child)
        root.child->prev = &node;
    root.child = &node;
}

static void removeNode(PurpleBlistNode &node)
{
    PurpleBlistNode *found;
    for (found = root.child; found; found = found->next)
        if (found == &node)
            break;
    ASSERT_TRUE(found != NULL) << "Removing unknown blist node";
    if (node.prev)
        node.prev->next = node.next;
    if (node.next)
        node.next->prev = node.prev;
    if (&node == root.child)
        root.child = node.next;
}

void purple_blist_add_buddy(PurpleBuddy *buddy, PurpleContact *contact, PurpleGroup *group, PurpleBlistNode *node)
{
    ASSERT_EQ(NULL, node) << "Not supported";
    auto pAccount = std::find_if(g_accounts.begin(), g_accounts.end(),
                                 [buddy](const AccountInfo &info) { return (info.account == buddy->account); });
    ASSERT_FALSE(pAccount == g_accounts.end()) << "Adding buddy with unknown account";

    ASSERT_TRUE(std::find_if(pAccount->buddies.begin(), pAccount->buddies.end(), 
                             [buddy](const PurpleBuddy *existing) {
                                 return !strcmp(existing->name, buddy->name);
                             }) == pAccount->buddies.end())
        << "Buddy already exists in this account";

    buddy->node.parent = group ? &group->node : NULL;
    addNode(buddy->node);
    pAccount->buddies.push_back(buddy);

    EVENT(AddBuddyEvent, buddy->name, buddy->alias, buddy->account, contact, group, node);
}

void purple_blist_remove_account(PurpleAccount *account)
{
    // TODO add event
}

void purple_blist_remove_buddy(PurpleBuddy *buddy)
{
    auto pAccount = std::find_if(g_accounts.begin(), g_accounts.end(),
                                 [buddy](const AccountInfo &info) { return (info.account == buddy->account); });
    ASSERT_FALSE(pAccount == g_accounts.end()) << "Removing buddy with unknown account";

    auto it = std::find(pAccount->buddies.begin(), pAccount->buddies.end(), buddy);
    ASSERT_FALSE(it == pAccount->buddies.end()) << "Removing unkown buddy";
    pAccount->buddies.erase(it);

    EVENT(RemoveBuddyEvent, buddy->account, buddy->name);
    removeNode(buddy->node);
    purple_buddy_destroy(buddy);
}

void purple_blist_alias_buddy(PurpleBuddy *buddy, const char *alias)
{
    ASSERT_NE(nullptr, buddy);
    free(buddy->alias);
    buddy->alias = strdup(alias);
    EVENT(AliasBuddyEvent, buddy->name, alias);
}

static char *getChatName(const PurpleChat *chat)
{
    auto        pluginInfo  = (PurplePluginProtocolInfo *)g_plugin->info->extra_info;
    GList      *chatInfo    = (pluginInfo)->chat_info(chat->account->gc);
    const char *componentId = ((proto_chat_entry *)chatInfo->data)->identifier;
    char       *name        = (char *)g_hash_table_lookup(chat->components, componentId);

    g_list_free_full(chatInfo, g_free);
    return name;
}

void purple_blist_remove_chat(PurpleChat *chat)
{
    auto pAccount = std::find_if(g_accounts.begin(), g_accounts.end(),
                                 [chat](const AccountInfo &info) { return (info.account == chat->account); });
    ASSERT_FALSE(pAccount == g_accounts.end()) << "Removing buddy with unknown account";

    auto it = std::find(pAccount->chats.begin(), pAccount->chats.end(), chat);
    ASSERT_FALSE(it == pAccount->chats.end()) << "Removing unkown chat";
    pAccount->chats.erase(it);

    const char *inviteLink = (const char *)g_hash_table_lookup(chat->components, (char *)"link");
    EVENT(RemoveChatEvent, getChatName(chat), inviteLink ? inviteLink : "");

    free(chat->alias);
    g_hash_table_destroy(chat->components);
    removeNode(chat->node);
    g_hash_table_destroy(chat->node.settings);
    delete chat;
}

const char *purple_buddy_get_alias_only(PurpleBuddy *buddy)
{
    return buddy->alias;
}

const char *purple_buddy_get_alias(PurpleBuddy *buddy)
{
    return buddy->alias ? buddy->alias : buddy->name;
}

PurpleGroup *purple_buddy_get_group(PurpleBuddy *buddy)
{
    return reinterpret_cast<PurpleGroup *>(buddy->node.parent);
}

const char *purple_buddy_get_name(const PurpleBuddy *buddy)
{
    return buddy->name;
}

PurpleAccount *purple_buddy_get_account(const PurpleBuddy *buddy)
{
    return buddy->account;
}

static void newNode(PurpleBlistNode &node, PurpleBlistNodeType type)
{
    node.child = NULL;
    node.next = NULL;
    node.parent = NULL;
    node.prev = NULL;
    node.type = type;
    node.settings = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);
}

PurpleBuddy *purple_buddy_new(PurpleAccount *account, const char *name, const char *alias)
{
    PurpleBuddy *buddy = new PurpleBuddy;

    buddy->account = account;
    buddy->name = strdup(name);
    buddy->alias = strdup(alias);
    buddy->node.parent = NULL;
    newNode(buddy->node, PURPLE_BLIST_BUDDY_NODE);

    return buddy;
}

void purple_buddy_destroy(PurpleBuddy *buddy)
{
    free(buddy->name);
    free(buddy->alias);
    g_hash_table_destroy(buddy->node.settings);
    delete buddy;
}

void
purple_buddy_icons_set_for_user(PurpleAccount *account, const char *username,
                                void *icon_data, size_t icon_len,
                                const char *checksum)
{
    EVENT(SetUserPhotoEvent, account, username, icon_data, icon_len);
}

PurpleStoredImage *
purple_buddy_icons_node_set_custom_icon(PurpleBlistNode *node,
                                        guchar *icon_data, size_t icon_len)
{
    EXPECT_TRUE(PURPLE_BLIST_NODE_IS_CHAT(node));
    EVENT(SetUserPhotoEvent, PURPLE_CHAT(node)->account, getChatName(PURPLE_CHAT(node)), icon_data, icon_len);
    return NULL;
}

PurpleChat *purple_chat_new(PurpleAccount *account, const char *alias, GHashTable *components)
{
    PurpleChat *chat = new PurpleChat;
    chat->account = account;
    chat->alias = strdup(alias);
    chat->components = components;
    newNode(chat->node, PURPLE_BLIST_CHAT_NODE);
    return chat;
}

void purple_blist_add_chat(PurpleChat *chat, PurpleGroup *group, PurpleBlistNode *node)
{
    ASSERT_EQ(NULL, node) << "Not supported";
    char *name = getChatName(chat);

    auto pAccount = std::find_if(g_accounts.begin(), g_accounts.end(),
                                 [chat](const AccountInfo &info) { return (info.account == chat->account); });
    ASSERT_FALSE(pAccount == g_accounts.end()) << "Adding chat with unknown account";

    ASSERT_TRUE(std::find_if(pAccount->chats.begin(), pAccount->chats.end(), 
                             [name](const PurpleChat *existing) {
                                 return !strcmp(getChatName(existing), name);
                             }) == pAccount->chats.end())
        << "Chat already exists in this account";

    chat->node.parent = group ? &group->node : NULL;
    addNode(chat->node);
    pAccount->chats.push_back(chat);

    EVENT(AddChatEvent, name, chat->alias, chat->account, group, node);
}

PurpleChat *purple_blist_find_chat(PurpleAccount *account, const char *name)
{
    // real purple_blist_find_chat does this
    if (!purple_account_is_connected(account))
        return NULL;

    auto pAccount = std::find_if(g_accounts.begin(), g_accounts.end(),
                                 [account](const AccountInfo &info) { return (info.account == account); });
    EXPECT_FALSE(pAccount == g_accounts.end()) << "Searching chat with unknown account";

    if (pAccount != g_accounts.end()) {
        auto it = std::find_if(pAccount->chats.begin(), pAccount->chats.end(),
                               [name](const PurpleChat *existing) {
                                   return !strcmp(getChatName(existing), name);
                               });
        if (it != pAccount->chats.end())
            return *it;
    }

    return NULL;
}

const char *purple_chat_get_name(PurpleChat *chat)
{
    if (chat->alias)
        return chat->alias;
    return getChatName(chat);
}

PurpleAccount *purple_chat_get_account(PurpleChat *chat)
{
    return chat->account;
}

void purple_blist_alias_chat(PurpleChat *chat, const char *alias)
{
    free(chat->alias);
    chat->alias = strdup(alias);
    EVENT(AliasChatEvent, getChatName(chat), alias);
}

void purple_connection_error(PurpleConnection *gc, const char *reason)
{
    EVENT(ConnectionErrorEvent, gc, reason);
}

PurpleAccount *purple_connection_get_account(const PurpleConnection *gc)
{
    return gc->account;
}

void *purple_connection_get_protocol_data(const PurpleConnection *connection)
{
    return connection->proto_data;
}

PurpleConnectionState purple_connection_get_state(const PurpleConnection *gc)
{
    return gc->state;
}

void purple_connection_set_protocol_data(PurpleConnection *connection, void *proto_data)
{
    connection->proto_data = proto_data;
}

void purple_connection_set_state(PurpleConnection *gc, PurpleConnectionState state)
{
    gc->state = state;
    EVENT(ConnectionSetStateEvent, gc, state);
}

void purple_connection_update_progress(PurpleConnection *gc, const char *text,
									 size_t step, size_t count)
{
    EVENT(ConnectionUpdateProgressEvent, gc, step, count);
}

static PurpleConversation *purple_conversation_new_impl(PurpleConversationType type,
										PurpleAccount *account,
										const char *name)
{
    PurpleConversation *conv = new PurpleConversation;
    conv->type = type;
    conv->account = account;
    conv->name = strdup(name);
    conv->title = NULL;
    if (conv->type == PURPLE_CONV_TYPE_IM) {
        conv->u.im = new PurpleConvIm;
        conv->u.im->conv = conv;
    }
    if (conv->type == PURPLE_CONV_TYPE_CHAT) {
        conv->u.chat = new PurpleConvChat;
        conv->u.chat->conv = conv;
    }

    auto pAccount = std::find_if(g_accounts.begin(), g_accounts.end(),
                                 [account](const AccountInfo &info) { return (info.account == account); });
    EXPECT_FALSE(pAccount == g_accounts.end()) << "Adding conversation with unknown account";

    if (pAccount != g_accounts.end()) {
        EXPECT_TRUE(purple_find_conversation_with_account(type, name, account) == NULL)
            << "Conversation with this name already exists to this account";
        pAccount->conversations.push_back(conv);
    }

    return conv;
}

PurpleConversation *purple_conversation_new(PurpleConversationType type,
										PurpleAccount *account,
										const char *name)
{
    EVENT(NewConversationEvent, type, account, name);
    return purple_conversation_new_impl(type, account, name);
}

void purple_conversation_destroy(PurpleConversation *conv)
{
    auto pAccount = std::find_if(g_accounts.begin(), g_accounts.end(),
                                 [conv](const AccountInfo &info) { return (info.account == conv->account); });
    ASSERT_FALSE(pAccount == g_accounts.end()) << "Removing conversation with unknown account";

    auto it = std::find(pAccount->conversations.begin(), pAccount->conversations.end(), conv);
    ASSERT_FALSE(it == pAccount->conversations.end()) << "Removing unkown conversation";
    pAccount->conversations.erase(it);

    free(conv->name);
    free(conv->title);
    if (conv->type == PURPLE_CONV_TYPE_IM)
        delete conv->u.im;
    if (conv->type == PURPLE_CONV_TYPE_CHAT)
        delete conv->u.chat;
    delete conv;
}

PurpleConvIm *purple_conversation_get_im_data(const PurpleConversation *conv)
{
    if (conv->type == PURPLE_CONV_TYPE_IM)
        return conv->u.im;

    return NULL;
}

PurpleConvChat *purple_conversation_get_chat_data(const PurpleConversation *conv)
{
    if (conv->type == PURPLE_CONV_TYPE_CHAT)
        return conv->u.chat;

    return NULL;
}

PurpleAccount *purple_conversation_get_account(const PurpleConversation *conv)
{
    return conv->account;
}

void purple_conversation_set_title(PurpleConversation *conv, const char *title)
{
    conv->title = strdup(title);
    EVENT(ConvSetTitleEvent, conv->name, title);
}

void purple_conversation_write(PurpleConversation *conv, const char *who,
		const char *message, PurpleMessageFlags flags,
		time_t mtime)
{
    EVENT(ConversationWriteEvent, conv->name, who ? who : "", message, flags, mtime);
}

PurpleConversation *purple_conv_im_get_conversation(const PurpleConvIm *im)
{
    return im->conv;
}

PurpleConversation *purple_conv_chat_get_conversation(const PurpleConvChat *chat)
{
    return chat->conv;
}

int purple_conv_chat_get_id(const PurpleConvChat *chat)
{
    return chat->id;
}

void purple_conv_im_write(PurpleConvIm *im, const char *who,
						const char *message, PurpleMessageFlags flags,
						time_t mtime)
{
    purple_conversation_write(purple_conv_im_get_conversation(im), who, message, flags, mtime);
}

void purple_conv_chat_write(PurpleConvChat *chat, const char *who,
						  const char *message, PurpleMessageFlags flags,
						  time_t mtime)
{
    purple_conversation_write(purple_conv_chat_get_conversation(chat), who, message, flags, mtime);
}

void purple_conv_chat_set_topic(PurpleConvChat *chat, const char *who,
							  const char *topic)
{
    EVENT(ChatSetTopicEvent, chat->conv->name, topic ? topic : "", who ? who : "");
}

gboolean purple_debug_is_enabled(void)
{
    return true;
}

gboolean purple_debug_is_verbose(void)
{
    return true;
}

PurpleBuddy *purple_find_buddy(PurpleAccount *account, const char *name)
{
    // purple_blist_find_chat returns NULL if account is not connected, so just in case, assume
    // purple_find_buddy will do likewise
    if (!purple_account_is_connected(account))
        return NULL;

    auto pAccount = std::find_if(g_accounts.begin(), g_accounts.end(),
                                 [account](const AccountInfo &info) { return (info.account == account); });
    EXPECT_FALSE(pAccount == g_accounts.end()) << "Looking for buddy with unknown account";

    if (pAccount != g_accounts.end()) {
        auto it = std::find_if(pAccount->buddies.begin(), pAccount->buddies.end(),
                               [name](const PurpleBuddy *buddy) {
                                   return !strcmp(purple_buddy_get_name(buddy), name);
                               });
        if (it != pAccount->buddies.end())
            return *it;
    }

    return NULL;
}

PurpleConversation *purple_find_chat(const PurpleConnection *gc, int id)
{
    auto pAccount = std::find_if(g_accounts.begin(), g_accounts.end(),
                                 [gc](const AccountInfo &info) { return (info.account == gc->account); });
    EXPECT_FALSE(pAccount == g_accounts.end()) << "Looking for buddy with unknown account";

    if (pAccount != g_accounts.end()) {
        auto it = std::find_if(pAccount->conversations.begin(), pAccount->conversations.end(),
                               [id](const PurpleConversation *conv) {
                                   return (conv->type == PURPLE_CONV_TYPE_CHAT) &&
                                          (conv->u.chat->id == id);
                               });
        if (it != pAccount->conversations.end())
            return *it;
    }

    return NULL;
}

PurpleConversation *purple_find_conversation_with_account(
		PurpleConversationType type, const char *name,
		const PurpleAccount *account)
{
    auto pAccount = std::find_if(g_accounts.begin(), g_accounts.end(),
                                 [account](const AccountInfo &info) { return (info.account == account); });
    EXPECT_FALSE(pAccount == g_accounts.end()) << "Adding conversation with unknown account";

    if (pAccount != g_accounts.end()) {
        auto it = std::find_if(pAccount->conversations.begin(), pAccount->conversations.end(),
                               [type, name](PurpleConversation *existing) {
                                   return !strcmp(existing->name, name) && (existing->type == type);
                               });
        if (it != pAccount->conversations.end())
            return *it;
    }
    return NULL;
}

void *purple_notify_message(void *handle, PurpleNotifyMsgType type,
						  const char *title, const char *primary,
						  const char *secondary, PurpleNotifyCloseCallback cb,
						  gpointer user_data)
{
    // TODO event
    return NULL;
}

PurpleNotifyUserInfo *purple_notify_user_info_new(void)
{
    return NULL;
}

GList *purple_notify_user_info_get_entries(PurpleNotifyUserInfo *user_info)
{
    return NULL;
}

void purple_notify_user_info_add_section_break(PurpleNotifyUserInfo *user_info)
{
}

void purple_notify_user_info_add_pair(PurpleNotifyUserInfo *user_info, const char *label, const char *value)
{
}

void *purple_notify_userinfo(PurpleConnection *gc, const char *who,
						   PurpleNotifyUserInfo *user_info, PurpleNotifyCloseCallback cb,
						   gpointer user_data)
{
    return NULL;
}

gboolean purple_plugin_register(PurplePlugin *plugin)
{
    // TODO maybe event
    g_plugin = plugin;
    return TRUE;
}

const char *purple_primitive_get_id_from_type(PurpleStatusPrimitive type)
{
    return (const char *)type;
}

void purple_prpl_got_user_status(PurpleAccount *account, const char *name,
							   const char *status_id, ...)
{
    PurpleStatusPrimitive type = (PurpleStatusPrimitive)(unsigned long)status_id;
    EVENT(UserStatusEvent, account, name, type);
}

void *purple_request_action(void *handle, const char *title, const char *primary,
	const char *secondary, int default_action, PurpleAccount *account,
	const char *who, PurpleConversation *conv, void *user_data,
	size_t action_count, ...)
{
    std::vector<std::string> buttons;
    std::vector<PurpleRequestActionCb> callbacks;
    va_list ap;
    va_start(ap, action_count);
    for (size_t i = 0; i < action_count; i++) {
        buttons.emplace_back(va_arg(ap, char*));
        callbacks.push_back(va_arg(ap, PurpleRequestActionCb));
    }
    va_end(ap);

    EVENT(RequestActionEvent, handle, title, primary, secondary, account, who, conv, user_data, buttons, callbacks);
    return NULL;
}

void *purple_request_input(void *handle, const char *title, const char *primary,
	const char *secondary, const char *default_value, gboolean multiline,
	gboolean masked, gchar *hint,
	const char *ok_text, GCallback ok_cb,
	const char *cancel_text, GCallback cancel_cb,
	PurpleAccount *account, const char *who, PurpleConversation *conv,
	void *user_data)
{
    EVENT(RequestInputEvent, handle, title, primary, secondary, default_value, ok_text, ok_cb,
          cancel_text, cancel_cb, account, who, conv, user_data);

    // Just return some non-NULL pointer
    return &g_accounts;
}

PurpleRoomlist *purple_roomlist_new(PurpleAccount *account)
{
    return NULL;
}

void purple_roomlist_set_in_progress(PurpleRoomlist *list, gboolean in_progress)
{
}

void purple_roomlist_unref(PurpleRoomlist *list)
{
}

void purple_serv_got_join_chat_failed(PurpleConnection *gc, GHashTable *data)
{
    // TODO event
}

PurpleStatusType *purple_status_type_new_full(PurpleStatusPrimitive primitive,
										  const char *id, const char *name,
										  gboolean saveable,
										  gboolean user_settable,
										  gboolean independent)
{
    return NULL;
}

const char *purple_user_dir(void)
{
    return "purple_user_dir";
}

PurpleXfer *purple_xfer_new(PurpleAccount *account,
								PurpleXferType type, const char *who)
{
    PurpleXfer *xfer = new PurpleXfer;
    xfer->account = account;
    xfer->type = type;
    xfer->who = strdup(who);
    xfer->ref = 1;
    xfer->local_filename = NULL;
    xfer->status = PURPLE_XFER_STATUS_UNKNOWN;
    xfer->size = 0;
    memset(&xfer->ops.init, 0, sizeof(xfer->ops.init));
    return xfer;
}

void purple_xfer_ref(PurpleXfer *xfer)
{
    xfer->ref++;
}

void purple_xfer_unref(PurpleXfer *xfer)
{
    if (--xfer->ref == 0) {
        free(xfer->who);
        free(xfer->local_filename);
        delete xfer;
    }
}

void purple_xfer_request(PurpleXfer *xfer)
{
}

std::map<std::string, size_t> fakeFiles;

void setFakeFileSize(const char *path, size_t size)
{
    fakeFiles[path] = size;
}

void purple_xfer_request_accepted(PurpleXfer *xfer, const char *filename)
{
    EVENT(XferAcceptedEvent, xfer, filename);
    xfer->status = PURPLE_XFER_STATUS_ACCEPTED;
    xfer->local_filename = strdup(filename);
    if (xfer->type == PURPLE_XFER_SEND)
        xfer->size = fakeFiles.at(filename);
    if (xfer->ops.init)
        xfer->ops.init(xfer);
}

void purple_xfer_set_init_fnc(PurpleXfer *xfer, void (*fnc)(PurpleXfer *))
{
    xfer->ops.init = fnc;
}

void purple_xfer_set_cancel_send_fnc(PurpleXfer *xfer, void (*fnc)(PurpleXfer *))
{
    xfer->ops.cancel_send = fnc;
}

void purple_xfer_set_cancel_recv_fnc(PurpleXfer *xfer, void (*fnc)(PurpleXfer *))
{
    xfer->ops.cancel_recv = fnc;
}

const char *purple_xfer_get_remote_user(const PurpleXfer *xfer)
{
    return xfer->who;
}

const char *purple_xfer_get_local_filename(const PurpleXfer *xfer)
{
    return xfer->local_filename;
}

void purple_xfer_set_filename(PurpleXfer *xfer, const char *filename)
{
}

void purple_xfer_set_size(PurpleXfer *xfer, size_t size)
{
    xfer->size = size;
}

void purple_xfer_start(PurpleXfer *xfer, int fd, const char *ip,
					 unsigned int port)
{
    EVENT(XferStartEvent, xfer->local_filename);
    xfer->status = PURPLE_XFER_STATUS_STARTED;
}

void purple_xfer_cancel_local(PurpleXfer *xfer)
{
    EVENT(XferLocalCancelEvent, xfer->local_filename);
    xfer->status = PURPLE_XFER_STATUS_CANCEL_LOCAL;
    if ((xfer->type == PURPLE_XFER_SEND) && xfer->ops.cancel_send)
        xfer->ops.cancel_send(xfer);
    if ((xfer->type == PURPLE_XFER_RECEIVE) && xfer->ops.cancel_recv)
        xfer->ops.cancel_recv(xfer);

    purple_xfer_unref(xfer);
}

gboolean purple_xfer_is_canceled(const PurpleXfer *xfer)
{
    return (xfer->status == PURPLE_XFER_STATUS_CANCEL_LOCAL) ||
           (xfer->status == PURPLE_XFER_STATUS_CANCEL_REMOTE);
}

void purple_xfer_cancel_remote(PurpleXfer *xfer)
{
    EVENT(XferRemoteCancelEvent, xfer->local_filename);
    xfer->status = PURPLE_XFER_STATUS_CANCEL_REMOTE;
    if ((xfer->type == PURPLE_XFER_SEND) && xfer->ops.cancel_send)
        xfer->ops.cancel_send(xfer);
    if ((xfer->type == PURPLE_XFER_RECEIVE) && xfer->ops.cancel_recv)
        xfer->ops.cancel_recv(xfer);
    purple_xfer_unref(xfer);
}

void purple_xfer_error(PurpleXferType type, PurpleAccount *account, const char *who, const char *msg)
{
    purple_notify_error(account, "Xfer error", who, msg);
}

PurpleXferType purple_xfer_get_type(const PurpleXfer *xfer)
{
    return xfer->type;
}

void purple_xfer_set_bytes_sent(PurpleXfer *xfer, size_t bytes_sent)
{
    xfer->bytes_sent = bytes_sent;
}

void purple_xfer_set_completed(PurpleXfer *xfer, gboolean completed)
{
    EVENT(XferCompletedEvent, xfer->local_filename, completed, xfer->bytes_sent);
}

void purple_xfer_update_progress(PurpleXfer *xfer)
{
    EVENT(XferProgressEvent, xfer->local_filename, xfer->bytes_sent);
}

void purple_xfer_end(PurpleXfer *xfer)
{
    EVENT(XferEndEvent, xfer->local_filename);
    purple_xfer_unref(xfer);
}

PurpleXferStatusType purple_xfer_get_status(const PurpleXfer *xfer)
{
    return xfer->status;
}

size_t purple_xfer_get_size(const PurpleXfer *xfer)
{
    return xfer->size;
}

void serv_got_chat_in(PurpleConnection *g, int id, const char *who,
					  PurpleMessageFlags flags, const char *message, time_t mtime)
{
    EVENT(ServGotChatEvent, g, id, who, message, flags, mtime);
}

void serv_got_im(PurpleConnection *gc, const char *who, const char *msg,
				 PurpleMessageFlags flags, time_t mtime)
{
    if (purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, who, gc->account) == NULL) {
        std::cout << "Adding conversation\n";
        purple_conversation_new_impl(PURPLE_CONV_TYPE_IM, gc->account, who);
    }
    EVENT(ServGotImEvent, gc, who, msg, flags, mtime);
}

PurpleConversation *serv_got_joined_chat(PurpleConnection *gc,
									   int id, const char *name)
{
    PurpleConversation *conv = purple_conversation_new_impl(PURPLE_CONV_TYPE_CHAT, gc->account, name);
    purple_conversation_get_chat_data(conv)->id = id;

    PurpleChat *chat = purple_blist_find_chat(gc->account, name);
    if (chat && chat->alias)
        conv->title = strdup(chat->alias);

    EVENT(ServGotJoinedChatEvent, gc, id, name, conv->title ? conv->title : name);
    return conv;
}

void serv_got_typing(PurpleConnection *gc, const char *name, int timeout,
					 PurpleTypingState state)
{
    // TODO event
}

void serv_got_typing_stopped(PurpleConnection *gc, const char *name)
{
    // TODO event
}

void purple_conversation_present(PurpleConversation *conv)
{
    EVENT(PresentConversationEvent, conv->name);
}

void purple_conv_chat_add_user(PurpleConvChat *chat, const char *user,
							 const char *extra_msg, PurpleConvChatBuddyFlags flags,
							 gboolean new_arrival)
{
    EVENT(ChatAddUserEvent, chat->conv->name, user, extra_msg ? extra_msg : "", flags, new_arrival);
}

void purple_conv_chat_add_users(PurpleConvChat *chat, GList *users, GList *extra_msgs,
							  GList *flags, gboolean new_arrivals)
{
    GList *user, *flag;
    for (user = users, flag = flags; user; user = user->next, flag = flag->next)
        purple_conv_chat_add_user(chat, (const char *)user->data, NULL,
                                  (PurpleConvChatBuddyFlags)GPOINTER_TO_INT(flag->data), new_arrivals);
}

void purple_conv_chat_clear_users(PurpleConvChat *chat)
{
    EVENT(ChatClearUsersEvent, chat->conv->name);
}

PurpleBlistNode *purple_blist_get_root(void)
{
    return &root;
}

PurpleBlistNode *purple_blist_node_get_sibling_next(PurpleBlistNode *node)
{
    return node->next;
}

PurpleBlistNodeType purple_blist_node_get_type(PurpleBlistNode *node)
{
    return node->type;
}

GHashTable *purple_chat_get_components(PurpleChat *chat)
{
    return chat->components;
}

PurpleBlistNode *purple_blist_node_get_first_child(PurpleBlistNode *node)
{
    return node->child;
}

void purple_blist_node_set_string(PurpleBlistNode *node, const char *key,
		const char *value)
{
    g_hash_table_insert(node->settings, (void *)key, g_strdup(value));
}

const char *purple_blist_node_get_string(PurpleBlistNode *node, const char *key)
{
    return static_cast<const char *>(g_hash_table_lookup(node->settings, key));
}

void purple_blist_node_remove_setting(PurpleBlistNode *node, const char *key)
{
    g_hash_table_remove(node->settings, key);
}

static char groupName[] = "Group";

PurpleGroup standardPurpleGroup = {
    .node = PurpleBlistNode(),
	.name = groupName,
	.totalsize = 0,
	.currentsize = 0,
	.online = 0
};

PurpleGroup *purple_find_group(const char *name)
{
    if (!strcmp(name, standardPurpleGroup.name))
        return &standardPurpleGroup;
    return NULL;
}

const char *purple_group_get_name(PurpleGroup *group)
{
    return group->name;
}

struct _PurpleStoredImage {
    std::vector<uint8_t> data;
};

std::vector<std::unique_ptr<PurpleStoredImage>> imageStore;

guint8 *arrayDup(gpointer data, size_t size)
{
    guint8 *result = (guint8 *)g_malloc(size);
    memmove(result, data, size);
    return result;
}

int purple_imgstore_add_with_id(gpointer data, size_t size, const char *filename)
{
    imageStore.push_back(std::make_unique<PurpleStoredImage>());
    imageStore.back()->data = std::vector<uint8_t>(size);
    memmove(imageStore.back()->data.data(), data, size);
    g_free(data);
    return imageStore.size();
}

int getLastImgstoreId()
{
    return imageStore.size();
}

PurpleStoredImage *purple_imgstore_find_by_id(int id)
{
    if ((id >= 1) && ((unsigned)id <= imageStore.size()))
        return imageStore[id-1].get();
    else
        return NULL;
}

gconstpointer purple_imgstore_get_data(PurpleStoredImage *img)
{
    return img->data.data();
}

size_t purple_imgstore_get_size(PurpleStoredImage *img)
{
    return img->data.size();
}

gchar *purple_markup_escape_text(const gchar *text, gssize length)
{
    std::string s(text, length);
    size_t pos;
    while ((pos = s.find('<')) != std::string::npos)
        s.replace(pos, 1, "&lt;");
    while ((pos = s.find('>')) != std::string::npos)
        s.replace(pos, 1, "&gt;");
    return g_strdup(s.c_str());
}

char *purple_unescape_html(const char *html)
{
    std::string s(html);
    size_t pos;
    while ((pos = s.find("&lt;")) != std::string::npos)
        s.replace(pos, 4, "<");
    while ((pos = s.find("&gt;")) != std::string::npos)
        s.replace(pos, 4, ">");
    return g_strdup(s.c_str());
}

PurpleProxyInfo *purple_proxy_get_setup(PurpleAccount *account)
{
    return account->proxy_info;
}

PurpleProxyType purple_proxy_info_get_type(const PurpleProxyInfo *info)
{
    return info->type;
}

const char *purple_proxy_info_get_host(const PurpleProxyInfo *info)
{
    return info->host;
}

int purple_proxy_info_get_port(const PurpleProxyInfo *info)
{
    return info->port;
}

const char *purple_proxy_info_get_username(const PurpleProxyInfo *info)
{
    return info->username;
}

const char *purple_proxy_info_get_password(const PurpleProxyInfo *info)
{
    return info->password;
}

PurpleRequestFields *purple_request_fields_new(void)
{
    return NULL;
}

PurpleRequestFieldGroup *purple_request_field_group_new(const char *title)
{
    return NULL;
}

PurpleRequestField *purple_request_field_string_new(const char *id,
												const char *text,
												const char *default_value,
												gboolean multiline)
{
    return NULL;
}

void purple_request_field_set_type_hint(PurpleRequestField *field,
									  const char *type_hint)
{
}

void purple_request_field_string_set_masked(PurpleRequestField *field,
										  gboolean masked)
{
}

void purple_request_field_group_add_field(PurpleRequestFieldGroup *group,
										PurpleRequestField *field)
{
}

void purple_request_fields_add_group(PurpleRequestFields *fields,
								   PurpleRequestFieldGroup *group)
{
}

const char *purple_request_fields_get_string(const PurpleRequestFields *fields,
										   const char *id)
{
    return "";
}

void *purple_request_fields(void *handle, const char *title, const char *primary,
	const char *secondary, PurpleRequestFields *fields,
	const char *ok_text, GCallback ok_cb,
	const char *cancel_text, GCallback cancel_cb,
	PurpleAccount *account, const char *who, PurpleConversation *conv,
	void *user_data)
{
    return NULL;
}

PurpleMenuAction *purple_menu_action_new(const char *label, PurpleCallback callback,
                                     gpointer data, GList *children)
{
    PurpleMenuAction *action = new PurpleMenuAction;
    action->label = strdup(label);
    action->callback = callback;
    action->children = NULL;
    action->data = data;
    return action;
}

void purple_menu_action_free(PurpleMenuAction *act)
{
    free(act->label);
    delete act;
}

PurplePluginAction *purple_plugin_action_new(const char* label, void (*callback)(PurplePluginAction *))
{
    return NULL;
}

PurpleAccountOption *purple_account_option_string_new(const char *text,
	const char *pref_name, const char *default_value)
{
    return NULL;
}

PurpleAccountOption *purple_account_option_list_new(const char *text,
	const char *pref_name, GList *list)
{
    for (GList *choice = list; choice; choice = g_list_next(choice)) {
        PurpleKeyValuePair *kvp = static_cast<PurpleKeyValuePair *>(choice->data);
        g_free(kvp->key);
        g_free(kvp->value);
        g_free(kvp);
    }
    g_list_free(list);
    return NULL;
}

const char *purple_account_get_string(const PurpleAccount *account,
									const char *name,
									const char *default_value)
{
    auto it = std::find_if(g_accounts.begin(), g_accounts.end(),
                           [account](const AccountInfo &info) { return (info.account == account); });
    EXPECT_FALSE(it == g_accounts.end()) << "Unknown account";

    if (it != g_accounts.end()) {
        auto itOption = it->stringsOptions.find(name);
        if (itOption != it->stringsOptions.end())
            return itOption->second.c_str();
    }

    return default_value;
}

void purple_account_set_string(PurpleAccount *account, const char *name,
                                                         const char *value)
{
    auto it = std::find_if(g_accounts.begin(), g_accounts.end(),
                           [account](const AccountInfo &info) { return (info.account == account); });
    EXPECT_FALSE(it == g_accounts.end()) << "Unknown account";

    if (it != g_accounts.end()) {
        it->stringsOptions[name] = value;
    }
}

char *purple_str_size_to_units(size_t size)
{
    return g_strdup("purple_str_size_to_units");
}

PurpleCmdId purple_cmd_register(const gchar *cmd, const gchar *args, PurpleCmdPriority p, PurpleCmdFlag f,
                             const gchar *prpl_id, PurpleCmdFunc func, const gchar *helpstr, void *data)
{
    g_purpleEvents.addCommand(cmd, func, data);
    return 0;
}

};
