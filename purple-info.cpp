#include "purple-info.h"
#include "config.h"
#include "format.h"
#include <algorithm>
#include <math.h>

static char chatNameComponent[] = "id";
static char joinStringKey[]     = "link";
static char nameKey[]           = "name";
static char typeKey[]           = "type";

const char *getChatNameComponent()
{
    return chatNameComponent;
}

GList *getChatJoinInfo()
{
    // First entry is the internal chat name used to look up conversations
    struct proto_chat_entry *pce;
    pce = g_new0 (struct proto_chat_entry, 1);
    // TRANSLATOR: Info item *and* dialog item.
    pce->label = _("Chat ID (don't change):");
    pce->identifier = chatNameComponent;
    pce->required = FALSE;
    GList *info = g_list_append (NULL, pce);

    pce = g_new0 (struct proto_chat_entry, 1);
    // TRANSLATOR: Info item *and* dialog item.
    pce->label = _("Join URL or name (empty if creating new):");
    pce->identifier = joinStringKey;
    pce->required = FALSE;
    info = g_list_append (info, pce);

    pce = g_new0 (struct proto_chat_entry, 1);
    // TRANSLATOR: Info item *and* dialog item.
    pce->label = _("Group name (if creating a group):");
    pce->identifier = nameKey;
    pce->required = FALSE;
    info = g_list_append (info, pce);

    pce = g_new0 (struct proto_chat_entry, 1);
    // TRANSLATOR: Info item *and* dialog item.
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

std::string getPurpleChatName(const td::td_api::chat &chat)
{
    return "chat" + std::to_string(chat.id_);
}

GHashTable *getChatComponents(const td::td_api::chat &chat)
{
    char name[32];
    snprintf(name, sizeof(name)-1, "chat%" G_GINT64_FORMAT "", chat.id_);
    name[sizeof(name)-1] = '\0';

    GHashTable *table = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
    g_hash_table_insert(table, chatNameComponent, g_strdup(name));
    return table;
}

const char *getChatName(GHashTable *components)
{
    return (const char *)g_hash_table_lookup(components, chatNameComponent);
}

const char *getChatJoinString(GHashTable *components)
{
    return (const char *)g_hash_table_lookup(components, joinStringKey);
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

unsigned getAutoDownloadLimitKb(PurpleAccount *account)
{
    std::string dlLimitStr = purple_account_get_string(account, AccountOptions::AutoDownloadLimit,
                                                       AccountOptions::AutoDownloadLimitDefault);
    for (size_t i = 0; i < dlLimitStr.size(); i++)
        if (dlLimitStr[i] == ',')
            dlLimitStr[i] = '.';
    char *endptr;
    float dlLimit = strtof(dlLimitStr.c_str(), &endptr);

    if (*endptr != '\0') {
        // TRANSLATOR: Buddy-window error message, argument will be a "number".
        std::string message = formatMessage(_("Invalid auto-download limit '{}', resetting to default"), dlLimitStr);
        // TRANSLATOR: Title of a buddy-window error message
        purple_notify_warning(account, _("Download limit"), message.c_str(), NULL);
        purple_account_set_string(account, AccountOptions::AutoDownloadLimit,
                                  AccountOptions::AutoDownloadLimitDefault);
        dlLimit = atof(AccountOptions::AutoDownloadLimitDefault);
    } else if (!isfinite(dlLimit) || (dlLimit >= UINT_MAX/1024-1)) {
        purple_account_set_string(account, AccountOptions::AutoDownloadLimit, "0");
        dlLimit = 0;
    }

    return floorf(dlLimit*1024);
}

bool isSizeWithinLimit(unsigned size, unsigned limit)
{
    return (limit == 0) || (size <= limit);
}

bool ignoreBigDownloads(PurpleAccount *account)
{
    return !strcmp(purple_account_get_string(account, AccountOptions::BigDownloadHandling,
                                             AccountOptions::BigDownloadHandlingDefault),
                   AccountOptions::BigDownloadHandlingDiscard);
}

PurpleTdClient *getTdClient(PurpleAccount *account)
{
    PurpleConnection *connection = purple_account_get_connection(account);
    if (connection)
        return static_cast<PurpleTdClient *>(purple_connection_get_protocol_data(connection));
    else
        return NULL;
}

const char *getUiName()
{
    GHashTable *ui_info = purple_core_get_ui_info();
    const char *name = static_cast<char *>(g_hash_table_lookup(ui_info, "name"));
    return name ? name : "";
}

const char *AccountOptions::DownloadBehaviourDefault()
{
    if (!strcasecmp(getUiName(), "pidgin"))
        return AccountOptions::DownloadBehaviourHyperlink;
    else
        return AccountOptions::DownloadBehaviourStandard;
}
