#include "secret-chat.h"
#include "format.h"
#include "purple-info.h"
#include "client-utils.h"
#include "config.h"

void updateKnownSecretChat(SecretChatId secretChatId, TdTransceiver &transceiver,
                           TdAccountData &account)
{
    const td::td_api::secretChat *secretChat = account.getSecretChat(secretChatId);

    const td::td_api::chat *chat = account.getChatBySecretChat(secretChatId);
    if (! chat) return;

    int state = (secretChat && secretChat->state_) ? secretChat->state_->get_id() :
                                                     td::td_api::secretChatStateClosed::ID;
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

        // This should be a newly created secret chat, so if we requested it, open the conversation
        if (secretChat && secretChat->is_outbound_) {
            if (state == td::td_api::secretChatStatePending::ID)
                showChatNotification(account, *chat, _("The secret chat will be available when activated by the peer"),
                                     PURPLE_MESSAGE_NO_LOG);
            else
                // Shouldn't really be possible, but just in case
                getImConversation(account.purpleAccount, purpleBuddyName.c_str());
        }
    } else
        purple_blist_alias_buddy(buddy, alias.c_str());

    if (state == td::td_api::secretChatStateReady::ID)
        purple_prpl_got_user_status(account.purpleAccount, purpleBuddyName.c_str(),
                                    purple_primitive_get_id_from_type(PURPLE_STATUS_AVAILABLE), NULL);
    else
        purple_prpl_got_user_status(account.purpleAccount, purpleBuddyName.c_str(),
                                    purple_primitive_get_id_from_type(PURPLE_STATUS_OFFLINE), NULL);
}

void updateSecretChat(td::td_api::object_ptr<td::td_api::secretChat> secretChat,
                      TdTransceiver &transceiver, TdAccountData &account)
{
    if (!secretChat) return;

    SecretChatId secretChatId = getId(*secretChat);
    if (secretChat->state_ && (secretChat->state_->get_id() == td::td_api::secretChatStateClosed::ID))
        // Just good manners
        account.deleteSecretChat(secretChatId);
    else
        account.addSecretChat(std::move(secretChat));
    updateKnownSecretChat(secretChatId, transceiver, account);
}
