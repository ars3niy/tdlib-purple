#include "purple-events.h"
#include <purple.h>
#include <stdarg.h>
#include <vector>
#include <gtest/gtest.h>

struct AccountInfo {
    PurpleAccount                     *account;
    std::vector<PurpleBuddy *>         buddies;
    std::vector<PurpleConversation *>  conversations;
};

std::vector<AccountInfo> g_accounts;

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

    g_accounts.emplace_back();
    g_accounts.back().account = account;

    return account;
}

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
    g_accounts.erase(it);

    delete account;
}

void purple_blist_add_account(PurpleAccount *account)
{
    EVENT(ShowAccountEvent, account);
}

void purple_blist_add_buddy(PurpleBuddy *buddy, PurpleContact *contact, PurpleGroup *group, PurpleBlistNode *node)
{
    auto pAccount = std::find_if(g_accounts.begin(), g_accounts.end(),
                                 [buddy](const AccountInfo &info) { return (info.account == buddy->account); });
    ASSERT_FALSE(pAccount == g_accounts.end()) << "Adding buddy with unknown account";

    ASSERT_TRUE(std::find_if(pAccount->buddies.begin(), pAccount->buddies.end(), 
                             [buddy](const PurpleBuddy *existing) {
                                 return !strcmp(existing->name, buddy->name);
                             }) == pAccount->buddies.end())
        << "Buddy already exists in this account";

    buddy->node.parent = group ? &group->node : NULL;
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

    free(buddy->name);
    free(buddy->alias);
    delete buddy;
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

PurpleBuddy *purple_buddy_new(PurpleAccount *account, const char *name, const char *alias)
{
    PurpleBuddy *buddy = new PurpleBuddy;

    buddy->account = account;
    buddy->name = strdup(name);
    buddy->alias = strdup(alias);
    buddy->node.parent = NULL;

    return buddy;
}

void purple_connection_error(PurpleConnection *gc, const char *reason)
{
    // TODO event
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
    delete conv;
}

void purple_conversation_write(PurpleConversation *conv, const char *who,
		const char *message, PurpleMessageFlags flags,
		time_t mtime)
{
    EVENT(ConversationWriteEvent, conv->name, who ? who : "", message, flags, mtime);
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

void purple_notify_user_info_add_pair(PurpleNotifyUserInfo *user_info, const char *label, const char *value)
{
}

gboolean purple_plugin_register(PurplePlugin *plugin)
{
    // TODO maybe event
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
    // TODO call callbacks somehow
    return NULL;
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
    return xfer;
}

void purple_xfer_unref(PurpleXfer *xfer)
{
    if (--xfer->ref == 0) {
        free(xfer->who);
        delete xfer;
    }
}

void purple_xfer_request(PurpleXfer *xfer)
{
}

void purple_xfer_request_accepted(PurpleXfer *xfer, const char *filename)
{
}

void purple_xfer_set_init_fnc(PurpleXfer *xfer, void (*fnc)(PurpleXfer *))
{
    xfer->ops.init = fnc;
}

void purple_xfer_set_cancel_send_fnc(PurpleXfer *xfer, void (*fnc)(PurpleXfer *))
{
    xfer->ops.cancel_send = fnc;
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

void serv_got_typing(PurpleConnection *gc, const char *name, int timeout,
					 PurpleTypingState state)
{
    // TODO event
}

void serv_got_typing_stopped(PurpleConnection *gc, const char *name)
{
    // TODO event
}

};
