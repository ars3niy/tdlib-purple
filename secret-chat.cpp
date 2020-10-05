#include "secret-chat.h"
#include "format.h"
#include "purple-info.h"
#include "client-utils.h"
#include "config.h"

static void updateReadySecretChat(SecretChatId secretChatId, TdTransceiver &transceiver, TdAccountData &account)
{
    const td::td_api::chat *chat = account.getChatBySecretChat(secretChatId);
    if (! chat) return;

    std::string purpleBuddyName = getSecretChatBuddyName(secretChatId);
    std::string alias = formatMessage(_("Secret chat: {}"), chat->title_);

    PurpleBuddy *buddy = purple_find_buddy(account.purpleAccount, purpleBuddyName.c_str());
    if (buddy == NULL) {
        purple_debug_misc(config::pluginId, "Adding buddy '%s' for secret chat %d with %s\n",
                          alias.c_str(), secretChatId.value(), chat->title_.c_str());
        buddy = purple_buddy_new(account.purpleAccount, purpleBuddyName.c_str(), alias.c_str());
        purple_blist_add_buddy(buddy, NULL, NULL, NULL);

        // Don't bother updating the photo - only set it when creating secret chat
        const td::td_api::file *photo = chat->photo_ ? chat->photo_->small_.get() : nullptr;
        if (photo && photo->local_ && photo->local_->is_downloading_completed_) {
            gchar  *img = NULL;
            size_t  len;
            GError *err = NULL;
            g_file_get_contents(photo->local_->path_.c_str(), &img, &len, &err);
            if (err) {
                purple_debug_warning(config::pluginId, "Failed to load photo %s for %s: %s\n",
                                     photo->local_->path_.c_str(), purpleBuddyName.c_str(),  err->message);
                g_error_free(err);
            } else {
                purple_debug_info(config::pluginId, "Using downloaded photo for %s\n", purpleBuddyName.c_str());
                purple_buddy_icons_set_for_user(account.purpleAccount, purpleBuddyName.c_str(),
                                                img, len, NULL);
            }
        }
    } else
        purple_blist_alias_buddy(buddy, alias.c_str());
}

void updateSecretChat(td::td_api::object_ptr<td::td_api::secretChat> secretChat,
                      TdTransceiver &transceiver, TdAccountData &account)
{
    if (!secretChat) return;

    SecretChatId secretChatId = getId(*secretChat);
    bool         isExisting   = (account.getSecretChat(secretChatId) != nullptr);
    account.addSecretChat(std::move(secretChat));
    updateKnownSecretChat(secretChatId, !isExisting, transceiver, account);
}

void updateKnownSecretChat(SecretChatId secretChatId, bool isNew, TdTransceiver &transceiver,
                           TdAccountData &account)
{
    const td::td_api::secretChat *secretChat = account.getSecretChat(secretChatId);
    if (! secretChat) return;

    auto state = secretChat->state_ ? secretChat->state_->get_id() :
                                      td::td_api::secretChatStateClosed::ID;

    const td::td_api::user *user = account.getUser(getUserId(*secretChat));
    std::string userDescription;
    if (user)
        userDescription = '\'' + account.getDisplayName(*user) + '\'';
    else {
        // Not supposed to be possible, because every user id should be preceded by user info
        userDescription = "(unknown user)";
    }

    if (state == td::td_api::secretChatStateReady::ID)
        updateReadySecretChat(secretChatId, transceiver, account);
}
