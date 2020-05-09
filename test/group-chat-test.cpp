#include "fixture.h"
#include <glib/gstrfuncs.h>

class GroupChatTest: public CommTest {
protected:
    const int32_t     groupId        = 700;
    const int64_t     groupChatId    = 7000;
    const std::string groupChatTitle = "Title";

    void loginWithBasicGroup();
};

void GroupChatTest::loginWithBasicGroup()
{
    login(
        {
            make_object<updateUser>(makeUser(
                // Chat member
                userIds[0],
                userFirstNames[0],
                userLastNames[0],
                "",
                make_object<userStatusOffline>()
            )),
            make_object<updateBasicGroup>(make_object<basicGroup>(
                groupId, 2, make_object<chatMemberStatusMember>(), true, 0
            )),
            make_object<updateNewChat>(makeChat(
                groupChatId, make_object<chatTypeBasicGroup>(groupId), groupChatTitle, nullptr, 0, 0, 0
            ))
        },
        make_object<users>(),
        make_object<chats>(std::vector<int64_t>(1, groupChatId)),
        {},
        {
            std::make_unique<ConnectionSetStateEvent>(connection, PURPLE_CONNECTED),
            std::make_unique<AddChatEvent>(
                "chat" + std::to_string(groupChatId), groupChatTitle, account, nullptr, nullptr
            ),
            std::make_unique<AccountSetAliasEvent>(account, selfFirstName + " " + selfLastName),
            std::make_unique<ShowAccountEvent>(account)
        }
    );
}

TEST_F(GroupChatTest, AddBasicGroupChatAtLogin)
{
    loginWithBasicGroup();
}

TEST_F(GroupChatTest, AddBasicGroupChat)
{
    login();
    tgl.update(make_object<updateBasicGroup>(make_object<basicGroup>(
        groupId, 2, make_object<chatMemberStatusMember>(), true, 0
    )));
    tgl.update(make_object<updateNewChat>(makeChat(
        groupChatId, make_object<chatTypeBasicGroup>(groupId), groupChatTitle, nullptr, 0, 0, 0
    )));

    prpl.verifyEvent(AddChatEvent(
        "chat" + std::to_string(groupChatId), groupChatTitle, account, NULL, NULL
    ));
}

TEST_F(GroupChatTest, ExistingBasicGroupChatAtLogin)
{
    constexpr int32_t groupId   = 700;
    constexpr int64_t chatId    = 7000;
    const std::string chatTitle = "Title";

    GHashTable *table = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
    g_hash_table_insert(table, (char *)"id", g_strdup(("chat" + std::to_string(chatId)).c_str()));
    purple_blist_add_chat(purple_chat_new(account, chatTitle.c_str(), table), NULL, NULL);
    prpl.discardEvents();

    login(
        {
            make_object<updateBasicGroup>(make_object<basicGroup>(
                groupId, 2, make_object<chatMemberStatusMember>(), true, 0
            )),
            make_object<updateNewChat>(makeChat(
                chatId, make_object<chatTypeBasicGroup>(groupId), chatTitle, nullptr, 0, 0, 0
            ))
        },
        make_object<users>(),
        make_object<chats>(std::vector<int64_t>(1, chatId))
    );
}

TEST_F(GroupChatTest, BasicGroupReceiveText)
{
    constexpr int32_t date[]      = {12345, 123456};
    constexpr int64_t messageId[] = {10000, 10001};
    loginWithBasicGroup();

    tgl.update(make_object<updateNewMessage>(
        makeMessage(messageId[0], userIds[0], groupChatId, false, date[0], makeTextMessage("Hello"))
    ));
    tgl.verifyRequest(viewMessages(
        groupChatId,
        {messageId[0]},
        true
    ));
    prpl.verifyEvents({
        std::make_unique<ServGotJoinedChatEvent>(connection, 1, "chat" + std::to_string(groupChatId)),
        std::make_unique<ServGotChatEvent>(connection, 1, userFirstNames[0] + " " + userLastNames[0],
                                           "Hello", PURPLE_MESSAGE_RECV, date[0])
    });

    tgl.update(make_object<updateNewMessage>(
        makeMessage(messageId[1], selfId, groupChatId, true, date[1], makeTextMessage("Reply"))
    ));
    tgl.verifyRequest(viewMessages(
        groupChatId,
        {messageId[1]},
        true
    ));
    prpl.verifyEvent(ConversationWriteEvent(
        "chat" + std::to_string(groupChatId), selfFirstName + " " + selfLastName,
        "Reply", PURPLE_MESSAGE_SEND, date[1]
    ));

}
