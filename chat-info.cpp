#include "chat-info.h"
#include "config.h"

static const char *_(const char *s) { return s; }

static char idKey[]     = "id";
static char inviteKey[] = "link";

GList *getChatJoinInfo()
{
    // First entry is the internal chat name used to look up conversations
    struct proto_chat_entry *pce;
    pce = g_new0 (struct proto_chat_entry, 1);
    pce->label = _("Chat ID (leave empty):");
    pce->identifier = idKey;
    pce->required = FALSE;
    GList *info = g_list_append (NULL, pce);

    pce = g_new0 (struct proto_chat_entry, 1);
    pce->label = _("Invite link (empty if creating new):");
    pce->identifier = inviteKey;
    pce->required = FALSE;
    info = g_list_append (info, pce);

    pce = g_new0 (struct proto_chat_entry, 1);
    pce->label = _("Subject (if creating a group):");
    pce->identifier = "subject";
    pce->required = FALSE;
    info = g_list_append (info, pce);

    return info;
}

std::string getChatName(const td::td_api::chat &chat)
{
    return "chat" + std::to_string(chat.id_);
}

GHashTable *getChatComponents(const td::td_api::chat &chat)
{
    char name[32];
    snprintf(name, sizeof(name)-1, "chat%" G_GINT64_FORMAT "", chat.id_);
    name[sizeof(name)-1] = '\0';

    GHashTable *table = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
    g_hash_table_insert(table, idKey, g_strdup(name));
    return table;
}

const char *getChatName(GHashTable *components)
{
    return (const char *)g_hash_table_lookup(components, idKey);
}

const char *getChatInviteLink(GHashTable *components)
{
    return (const char *)g_hash_table_lookup(components, inviteKey);
}

int64_t getTdlibChatId(const char *chatName)
{
    if (chatName && !strncmp(chatName, "chat", 4)) {
        int64_t id;
        if (sscanf(chatName+4, "%" G_GINT64_FORMAT "", &id) == 1)
            return id;
    }

    return 0;
}
