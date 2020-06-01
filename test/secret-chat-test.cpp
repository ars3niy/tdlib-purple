#include "fixture.h"

class SecretChatTest: public CommTest {
protected:
    const int32_t secretChatId = 700;
    const int64_t secretChatChatId = 70000;

    object_ptr<chat> makeChatForSecret(int32_t userId);
};

object_ptr<td::td_api::chat> SecretChatTest::makeChatForSecret(int32_t userId)
{
    return makeChat(secretChatChatId, make_object<chatTypeSecret>(secretChatId, userId),
                    "Secret Chat Title", nullptr, 0, 0, 0);
}

TEST_F(SecretChatTest, AutoAccept)
{
    purple_account_set_string(account, "accept-secret-chats", "always");
    loginWithOneContact();

    tgl.update(make_object<updateSecretChat>(make_object<secretChat>(
        secretChatId, userIds[0], make_object<secretChatStateReady>(), false, 60, "whatever", 0
    )));
    tgl.verifyRequest(closeSecretChat(secretChatId));

    tgl.update(make_object<updateNewChat>(makeChatForSecret(userIds[0])));
    tgl.update(make_object<updateNewMessage>(
        makeMessage(1, userIds[0], secretChatChatId, false, 123, makeTextMessage("text"))
    ));
    tgl.verifyRequest(viewMessages(secretChatChatId, {1}, true));
}

TEST_F(SecretChatTest, AutoDiscard)
{
    purple_account_set_string(account, "accept-secret-chats", "never");
    loginWithOneContact();

    tgl.update(make_object<updateSecretChat>(make_object<secretChat>(
        secretChatId, userIds[0], make_object<secretChatStateReady>(), false, 60, "whatever", 0
    )));
    tgl.verifyRequest(closeSecretChat(secretChatId));

    tgl.update(make_object<updateNewChat>(makeChatForSecret(userIds[0])));
    tgl.update(make_object<updateNewMessage>(
        makeMessage(1, userIds[0], secretChatChatId, false, 123, makeTextMessage("text"))
    ));
    tgl.verifyRequest(viewMessages(secretChatChatId, {1}, true));
}

TEST_F(SecretChatTest, AskAndAccept)
{
    loginWithOneContact();

    tgl.update(make_object<updateSecretChat>(make_object<secretChat>(
        secretChatId, userIds[0], make_object<secretChatStateReady>(), false, 60, "whatever", 0
    )));
    tgl.update(make_object<updateNewChat>(makeChatForSecret(userIds[0])));
    tgl.update(make_object<updateNewMessage>(
        makeMessage(1, userIds[0], secretChatChatId, false, 123, makeTextMessage("text"))
    ));
    tgl.verifyRequest(viewMessages(secretChatChatId, {1}, true));
    prpl.verifyEvents(RequestActionEvent(connection, account, NULL, NULL, 2));

    prpl.requestedAction("_Accept");
    tgl.verifyRequest(closeSecretChat(secretChatId));
}

TEST_F(SecretChatTest, AskAndDiscard)
{
    loginWithOneContact();

    tgl.update(make_object<updateSecretChat>(make_object<secretChat>(
        secretChatId, userIds[0], make_object<secretChatStateReady>(), false, 60, "whatever", 0
    )));
    tgl.update(make_object<updateNewChat>(makeChatForSecret(userIds[0])));
    tgl.update(make_object<updateNewMessage>(
        makeMessage(1, userIds[0], secretChatChatId, false, 123, makeTextMessage("text"))
    ));
    tgl.verifyRequest(viewMessages(secretChatChatId, {1}, true));
    prpl.verifyEvents(RequestActionEvent(connection, account, NULL, NULL, 2));

    prpl.requestedAction("_Cancel");
    tgl.verifyRequest(closeSecretChat(secretChatId));
}
