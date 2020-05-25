#include "purple-info.h"
#include "config.h"
#include <algorithm>

static const char *_(const char *s) { return s; }

static char idKey[]     = "id";
static char inviteKey[] = "link";
static char nameKey[]   = "name";
static char typeKey[]   = "type";

GList *getChatJoinInfo()
{
    // First entry is the internal chat name used to look up conversations
    struct proto_chat_entry *pce;
    pce = g_new0 (struct proto_chat_entry, 1);
    pce->label = _("Chat ID (don't change):");
    pce->identifier = idKey;
    pce->required = FALSE;
    GList *info = g_list_append (NULL, pce);

    pce = g_new0 (struct proto_chat_entry, 1);
    pce->label = _("Invite link (empty if creating new):");
    pce->identifier = inviteKey;
    pce->required = FALSE;
    info = g_list_append (info, pce);

    pce = g_new0 (struct proto_chat_entry, 1);
    pce->label = _("Group name (if creating a group):");
    pce->identifier = nameKey;
    pce->required = FALSE;
    info = g_list_append (info, pce);

    pce = g_new0 (struct proto_chat_entry, 1);
    pce->label = _("Group to create: 1=small 2=big 3=channel");
    pce->identifier = typeKey;
    pce->required = FALSE;
    pce->is_int = TRUE;
    static_assert((GROUP_TYPE_BASIC > 0) && (GROUP_TYPE_SUPER > 0) && (GROUP_TYPE_CHANNEL > 0), "positive please");
    pce->min = std::min({GROUP_TYPE_BASIC, GROUP_TYPE_SUPER, GROUP_TYPE_CHANNEL});
    pce->max = std::max({GROUP_TYPE_BASIC, GROUP_TYPE_SUPER, GROUP_TYPE_CHANNEL});
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

const char *getChatGroupName(GHashTable *components)
{
    return (const char *)g_hash_table_lookup(components, nameKey);
}

int getChatGroupType(GHashTable *components)
{
    const char *s = static_cast<const char *>(g_hash_table_lookup(components, typeKey));
    return s ? atoi(s) : 0;
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
