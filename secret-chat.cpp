#include "secret-chat.h"
#include "format.h"
#include "purple-info.h"

static void closeSecretChat(SecretChatId secretChatId, TdTransceiver &transceiver)
{
    transceiver.sendQuery(td::td_api::make_object<td::td_api::closeSecretChat>(secretChatId.value()), nullptr);
}

static void secretChatNotSupported(SecretChatId secretChatId, const std::string &userDescription,
                                   TdTransceiver &transceiver, PurpleAccount *purpleAccount)
{
    closeSecretChat(secretChatId, transceiver);
    std::string message = formatMessage("Rejected secret chat with {}", userDescription);
    purple_notify_info(purple_account_get_connection(purpleAccount),
                        "Secret chat", message.c_str(),
                        "Secret chats not supported");
}

struct SecretChatInfo {
    SecretChatId   secretChatId;
    std::string    userDescription;
    TdTransceiver *transceiver;
    PurpleAccount *purpleAccount;
};

static void acceptSecretChatCb(SecretChatInfo *data)
{
    std::unique_ptr<SecretChatInfo> info(data);
    secretChatNotSupported(info->secretChatId, info->userDescription, *info->transceiver, info->purpleAccount);
}

static void discardSecretChatCb(SecretChatInfo *data)
{
    std::unique_ptr<SecretChatInfo> info(data);
    closeSecretChat(info->secretChatId, *info->transceiver);
}

void deleteSecretChat(SecretChatId secretChatId, TdTransceiver &transceiver, TdAccountData &account)
{
    const td::td_api::chat *chat = account.getChatBySecretChat(secretChatId);
    if (chat) {
        auto deleteChat = td::td_api::make_object<td::td_api::deleteChatHistory>();
        deleteChat->chat_id_ = getId(*chat).value();
        deleteChat->remove_from_chat_list_ = true;
        deleteChat->revoke_ = false;
        transceiver.sendQuery(std::move(deleteChat), nullptr);
    }
}

void updateSecretChat(td::td_api::object_ptr<td::td_api::secretChat> secretChat,
                      TdTransceiver &transceiver, TdAccountData &account)
{
    if (!secretChat) return;

    SecretChatId secretChatId = getId(*secretChat);
    bool         isOutbound   = secretChat->is_outbound_;
    bool         isExisting   = (account.getSecretChat(secretChatId) != nullptr);
    auto         state        = secretChat->state_ ? secretChat->state_->get_id() :
                                td::td_api::secretChatStateClosed::ID;

    const td::td_api::user *user = account.getUser(getUserId(*secretChat));
    account.addSecretChat(std::move(secretChat));

    std::string userDescription;
    if (user)
        userDescription = '\'' + account.getDisplayName(*user) + '\'';
    else {
        // Not supposed to be possible, because every user id should be preceded by user info
        userDescription = "(unknown user)";
    }

    if (state == td::td_api::secretChatStateClosed::ID)
        deleteSecretChat(secretChatId, transceiver, account);

    if (!isExisting && !isOutbound && (state == td::td_api::secretChatStatePending::ID)) {
        const char *secretChatHandling = purple_account_get_string(account.purpleAccount,
                                                                   AccountOptions::AcceptSecretChats,
                                                                   AccountOptions::AcceptSecretChatsDefault);
        if (!strcmp(secretChatHandling, AccountOptions::AcceptSecretChatsNever)) {
            closeSecretChat(secretChatId, transceiver);
            // TRANSLATOR: Dialog content, argument will be a username
            std::string message = formatMessage(_("Rejected secret chat with {}"), userDescription);
            purple_notify_info(purple_account_get_connection(account.purpleAccount),
                               // TRANSLATOR: Dialog title
                               _("Secret chat"), message.c_str(), NULL);
        } else if (!strcmp(secretChatHandling, AccountOptions::AcceptSecretChatsAlways))
            secretChatNotSupported(secretChatId, userDescription, transceiver, account.purpleAccount);
        else {
            // TRANSLATOR: Dialog content, argument will be a username, options will be "_Accept" and "_Cancel".
            std::string message = formatMessage(_("Accept secret chat with {} on this device?"), userDescription);
            SecretChatInfo *data = new SecretChatInfo{secretChatId, userDescription, &transceiver, account.purpleAccount};
            purple_request_action(purple_account_get_connection(account.purpleAccount),
                // TRANSLATOR: Dialog title
                _("Secret chat"), message.c_str(),
                // TRANSLATOR: Dialog secondary content. Options will be "_Accept" and "_Cancel".
                _("Secret chats can only have one "
                "end point. If you accept a secret chat on this device, its messages will not be available anywhere "
                "else. If you decline, you can still accept the chat on other devices."),
                0, account.purpleAccount, NULL, NULL, data, 2,
                // TRANSLATOR: Dialog option, regarding a secret chat; the alternative is "_Cancel"
                _("_Accept"), acceptSecretChatCb,
                // TRANSLATOR: Dialog option, regarding a secret chat; the alternative is "_Accept"
                _("_Cancel"), discardSecretChatCb);
        }
    }
}
