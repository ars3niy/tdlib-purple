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

TEST_F(SecretChatTest, AutoAccept)
{
    const int32_t date = 123;

    purple_account_set_string(account, "accept-secret-chats", "always");
    loginWithOneContact();

    tgl.update(make_object<updateSecretChat>(make_object<secretChat>(
        secretChatId, userIds[0], make_object<secretChatStatePending>(), false, 60, "whatever", 0
    )));
    tgl.update(make_object<updateNewChat>(makeChatForSecret(userIds[0])));

    tgl.update(make_object<updateSecretChat>(make_object<secretChat>(
        secretChatId, userIds[0], make_object<secretChatStateReady>(), false, 60, "whatever", 0
    )));
    prpl.verifyEvents(AddBuddyEvent(
        secretChatBuddyName, secretChatBuddyAlias, account, NULL, NULL, NULL
    ));

    tgl.update(make_object<updateNewMessage>(
        makeMessage(1, userIds[0], secretChatChatId, false, 123, makeTextMessage("text"))
    ));
    tgl.verifyRequest(viewMessages(secretChatChatId, {1}, true));
    prpl.verifyEvents(ServGotImEvent(
        connection, secretChatBuddyName, "text", PURPLE_MESSAGE_RECV, date
    ));
}

TEST_F(SecretChatTest, DISABLED_AutoDiscard)
{
    purple_account_set_string(account, "accept-secret-chats", "never");
    loginWithOneContact();

    tgl.update(make_object<updateSecretChat>(make_object<secretChat>(
        secretChatId, userIds[0], make_object<secretChatStatePending>(), false, 60, "whatever", 0
    )));
    tgl.verifyRequest(closeSecretChat(secretChatId));

    tgl.update(make_object<updateNewChat>(makeChatForSecret(userIds[0])));
    tgl.update(make_object<updateSecretChat>(make_object<secretChat>(
        secretChatId, userIds[0], make_object<secretChatStateClosed>(), false, 60, "whatever", 0
    )));
    tgl.verifyRequest(deleteChatHistory(secretChatChatId, true, false));
}

TEST_F(SecretChatTest, AskAndAccept)
{
    const int32_t date = 123;

    loginWithOneContact();

    tgl.update(make_object<updateSecretChat>(make_object<secretChat>(
        secretChatId, userIds[0], make_object<secretChatStatePending>(), false, 60, "whatever", 0
    )));
    tgl.update(make_object<updateNewChat>(makeChatForSecret(userIds[0])));

    prpl.verifyEvents(RequestActionEvent(connection, account, NULL, NULL, 2));
    prpl.requestedAction("_Accept");

    tgl.update(make_object<updateSecretChat>(make_object<secretChat>(
        secretChatId, userIds[0], make_object<secretChatStateReady>(), false, 60, "whatever", 0
    )));
    prpl.verifyEvents(AddBuddyEvent(
        secretChatBuddyName, secretChatBuddyAlias, account, NULL, NULL, NULL
    ));

    tgl.update(make_object<updateNewMessage>(
        makeMessage(1, userIds[0], secretChatChatId, false, date, makeTextMessage("text"))
    ));
    tgl.verifyRequest(viewMessages(secretChatChatId, {1}, true));
    prpl.verifyEvents(ServGotImEvent(
        connection, secretChatBuddyName, "text", PURPLE_MESSAGE_RECV, date
    ));
}

TEST_F(SecretChatTest, DISABLED_AskAndDiscard)
{
    loginWithOneContact();

    tgl.update(make_object<updateSecretChat>(make_object<secretChat>(
        secretChatId, userIds[0], make_object<secretChatStatePending>(), false, 60, "whatever", 0
    )));
    tgl.update(make_object<updateNewChat>(makeChatForSecret(userIds[0])));

    prpl.verifyEvents(RequestActionEvent(connection, account, NULL, NULL, 2));
    prpl.requestedAction("_Cancel");
    tgl.verifyRequest(closeSecretChat(secretChatId));

    tgl.update(make_object<updateSecretChat>(make_object<secretChat>(
        secretChatId, userIds[0], make_object<secretChatStateClosed>(), false, 60, "whatever", 0
    )));
    tgl.verifyRequest(deleteChatHistory(secretChatChatId, true, false));
}
