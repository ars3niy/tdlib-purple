#include "fixture.h"

class SecretChatTest: public CommTest {
protected:
    const int32_t secretChatId = 700;
    const int64_t secretChatChatId = 70000;
    const std::string secretChatTitle = "Secret Chat Title";

    const std::string secretChatBuddyName = "secret" + std::to_string(secretChatId);
    const std::string secretChatBuddyAlias = "Secret chat: " + secretChatTitle;

    object_ptr<chat> makeChatForSecret(int32_t userId);
};

object_ptr<td::td_api::chat> SecretChatTest::makeChatForSecret(int32_t userId)
{
    return makeChat(secretChatChatId, make_object<chatTypeSecret>(secretChatId, userId),
                    secretChatTitle, nullptr, 0, 0, 0);
}

TEST_F(SecretChatTest, ReceiveMessage)
{
    const int32_t date = 123;

    loginWithOneContact();

    tgl.update(make_object<updateSecretChat>(make_object<secretChat>(
        secretChatId, userIds[0], make_object<secretChatStatePending>(), false, 60, "whatever", 0
    )));
    tgl.update(make_object<updateNewChat>(makeChatForSecret(userIds[0])));
    prpl.verifyEvents(
        AddBuddyEvent(secretChatBuddyName, secretChatBuddyAlias, account, NULL, NULL, NULL),
        UserStatusEvent(account, secretChatBuddyName, PURPLE_STATUS_OFFLINE)
    );

    tgl.update(make_object<updateSecretChat>(make_object<secretChat>(
        secretChatId, userIds[0], make_object<secretChatStateReady>(), false, 60, "whatever", 0
    )));
    prpl.verifyEvents(
        UserStatusEvent(account, secretChatBuddyName, PURPLE_STATUS_AVAILABLE)
    );

    tgl.update(make_object<updateNewMessage>(
        makeMessage(1, userIds[0], secretChatChatId, false, 123, makeTextMessage("text"))
    ));
    tgl.verifyRequest(viewMessages(secretChatChatId, {1}, true));
    prpl.verifyEvents(ServGotImEvent(
        connection, secretChatBuddyName, "text", PURPLE_MESSAGE_RECV, date
    ));
}

TEST_F(SecretChatTest, CreateSecretChat_Lifecycle)
{
    loginWithOneContact();

    PurpleBuddy *buddy = purple_find_buddy(account, purpleUserName(0).c_str());
    ASSERT_NE(nullptr, buddy);
    GList *actions = pluginInfo().blist_node_menu(&buddy->node);
    nodeMenuAction(&buddy->node, actions, "Start secret chat");

    tgl.verifyRequest(createNewSecretChat(userIds[0]));
    tgl.update(make_object<updateSecretChat>(make_object<secretChat>(
        secretChatId, userIds[0], make_object<secretChatStatePending>(), true, 60, "whatever", 0
    )));
    tgl.update(make_object<updateNewChat>(makeChatForSecret(userIds[0])));
    prpl.verifyEvents(
        AddBuddyEvent(secretChatBuddyName, secretChatBuddyAlias, account, NULL, NULL, NULL),
        NewConversationEvent(PURPLE_CONV_TYPE_IM, account, secretChatBuddyName),
        ConversationWriteEvent(
            secretChatBuddyName, secretChatBuddyName,
            "The secret chat will be available when activated by the peer",
            PURPLE_MESSAGE_NO_LOG, 0
        ),
        UserStatusEvent(account, secretChatBuddyName, PURPLE_STATUS_OFFLINE)
    );

    tgl.update(make_object<updateSecretChat>(make_object<secretChat>(
        secretChatId, userIds[0], make_object<secretChatStateReady>(), false, 60, "whatever", 0
    )));
    prpl.verifyEvents(
        UserStatusEvent(account, secretChatBuddyName, PURPLE_STATUS_AVAILABLE)
    );

    purple_blist_remove_buddy(purple_find_buddy(account, secretChatBuddyName.c_str()));
    prpl.discardEvents();
    PurpleBuddy *dup = purple_buddy_new(account, secretChatBuddyName.c_str(),
                                        secretChatBuddyAlias.c_str());
    pluginInfo().remove_buddy(connection, dup, NULL);
    purple_buddy_destroy(dup);
    tgl.verifyRequests({
        make_object<deleteChatHistory>(secretChatChatId, true, false),
        make_object<closeSecretChat>(secretChatId)
    });
    tgl.update(make_object<updateSecretChat>(make_object<secretChat>(
        secretChatId, userIds[0], make_object<secretChatStateClosed>(), true, 60, "whatever", 0
    )));

    g_list_free_full(actions, (GDestroyNotify)purple_menu_action_free);
}

TEST_F(SecretChatTest, SecretChatsDisabled)
{
    purple_account_set_bool(account, "enable-secret-chats", FALSE);
    pluginInfo().login(account);
    prpl.verifyEvents(
        ConnectionSetStateEvent(connection, PURPLE_CONNECTING),
        ConnectionUpdateProgressEvent(connection, 1, 2)
    );

    tgl.update(make_object<updateAuthorizationState>(make_object<authorizationStateWaitTdlibParameters>()));
    tgl.verifyRequests({
        make_object<disableProxy>(),
        make_object<getProxies>(),
        make_object<setTdlibParameters>(make_object<tdlibParameters>(
            false,
            std::string(purple_user_dir()) + G_DIR_SEPARATOR_S +
            "tdlib" + G_DIR_SEPARATOR_S + "+" + selfPhoneNumber,
            "",
            false,
            false,
            false,
            false, // use secret chats
            0,
            "",
            "",
            "",
            "",
            "",
            false,
            false
        ))
    });
}
