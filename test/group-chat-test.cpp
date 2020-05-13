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
        {}, {},
        {
            std::make_unique<ConnectionSetStateEvent>(connection, PURPLE_CONNECTED),
            std::make_unique<AddChatEvent>(
                "chat" + std::to_string(groupChatId), groupChatTitle, account, nullptr, nullptr
            ),
            std::make_unique<AccountSetAliasEvent>(account, selfFirstName + " " + selfLastName),
            std::make_unique<ShowAccountEvent>(account)
        }
    );

    tgl.verifyRequest(getBasicGroupFullInfo(groupId));
}

TEST_F(GroupChatTest, AddBasicGroupChatAtLogin)
{
    loginWithBasicGroup();
}

TEST_F(GroupChatTest, BasicGroupChatAppearsAfterLogin)
{
    login();

    tgl.update(make_object<updateBasicGroup>(make_object<basicGroup>(
        groupId, 2, make_object<chatMemberStatusMember>(), true, 0
    )));
    tgl.verifyNoRequests();
    tgl.update(make_object<updateNewChat>(makeChat(
        groupChatId, make_object<chatTypeBasicGroup>(groupId), groupChatTitle, nullptr, 0, 0, 0
    )));
    tgl.verifyRequest(getBasicGroupFullInfo(groupId));

    prpl.verifyEvents(AddChatEvent(
        "chat" + std::to_string(groupChatId), groupChatTitle, account, NULL, NULL
    ));
}

TEST_F(GroupChatTest, ExistingBasicGroupChatAtLogin)
{
    GHashTable *components = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
    g_hash_table_insert(components, (char *)"id", g_strdup(("chat" + std::to_string(groupChatId)).c_str()));
    purple_blist_add_chat(purple_chat_new(account, groupChatTitle.c_str(), components), NULL, NULL);
    prpl.discardEvents();

    login(
        {
            make_object<updateBasicGroup>(make_object<basicGroup>(
                groupId, 2, make_object<chatMemberStatusMember>(), true, 0
            )),
            make_object<updateNewChat>(makeChat(
                groupChatId, make_object<chatTypeBasicGroup>(groupId), groupChatTitle, nullptr, 0, 0, 0
            ))
        },
        make_object<users>(),
        make_object<chats>(std::vector<int64_t>(1, groupChatId))
    );

    tgl.verifyRequest(getBasicGroupFullInfo(groupId));
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
    prpl.verifyEvents(
        ServGotJoinedChatEvent(connection, 1, "chat" + std::to_string(groupChatId), groupChatTitle),
        ServGotChatEvent(connection, 1, userFirstNames[0] + " " + userLastNames[0],
                         "Hello", PURPLE_MESSAGE_RECV, date[0])
    );

    tgl.update(make_object<updateNewMessage>(
        makeMessage(messageId[1], selfId, groupChatId, true, date[1], makeTextMessage("Reply"))
    ));
    tgl.verifyRequest(viewMessages(
        groupChatId,
        {messageId[1]},
        true
    ));
    prpl.verifyEvents(ConversationWriteEvent(
        "chat" + std::to_string(groupChatId), selfFirstName + " " + selfLastName,
        "Reply", PURPLE_MESSAGE_SEND, date[1]
    ));

}

TEST_F(GroupChatTest, BasicGroupReceivePhoto)
{
    const int32_t date      = 12345;
    const int64_t messageId = 10001;
    const int32_t fileId    = 1234;
    loginWithBasicGroup();

    std::vector<object_ptr<photoSize>> sizes;
    sizes.push_back(make_object<photoSize>(
        "whatever",
        make_object<file>(
            fileId, 10000, 10000,
            make_object<localFile>("", true, true, false, false, 0, 0, 0),
            make_object<remoteFile>("beh", "bleh", false, true, 10000)
        ),
        640, 480
    ));
    tgl.update(make_object<updateNewMessage>(makeMessage(
        messageId, userIds[0], groupChatId, false, date,
        make_object<messagePhoto>(
            make_object<photo>(false, nullptr, std::move(sizes)),
            make_object<formattedText>("photo", std::vector<object_ptr<textEntity>>()),
            false
        )
    )));
    tgl.verifyRequests({
        make_object<viewMessages>(groupChatId, std::vector<int64_t>(1, messageId), true),
        make_object<downloadFile>(fileId, 1, 0, 0, true)
    });
    prpl.verifyEvents(
        ServGotJoinedChatEvent(connection, 1, "chat" + std::to_string(groupChatId), groupChatTitle),
        ServGotChatEvent(connection, 1, userFirstNames[0] + " " + userLastNames[0], "photo",
                         PURPLE_MESSAGE_RECV, date),
        ConversationWriteEvent("chat" + std::to_string(groupChatId), "", "Downloading image",
                               PURPLE_MESSAGE_SYSTEM, date)
    );

    tgl.reply(make_object<ok>());
    tgl.reply(make_object<file>(
        fileId, 10000, 10000,
        make_object<localFile>("/path", true, true, false, true, 0, 10000, 10000),
        make_object<remoteFile>("beh", "bleh", false, true, 10000)
    ));
    prpl.verifyEvents(ServGotChatEvent(
        connection, 1, userFirstNames[0] + " " + userLastNames[0], "<img src=\"file:///path\">",
        (PurpleMessageFlags)(PURPLE_MESSAGE_RECV | PURPLE_MESSAGE_IMAGES), date
    ));
}

TEST_F(GroupChatTest, ExistingBasicGroupReceiveMessageAtLogin_WithMemberList)
{
    constexpr int64_t messageId = 10001;
    constexpr int32_t date      = 12345;

    GHashTable *table = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
    g_hash_table_insert(table, (char *)"id", g_strdup(("chat" + std::to_string(groupChatId)).c_str()));
    purple_blist_add_chat(purple_chat_new(account, groupChatTitle.c_str(), table), NULL, NULL);

    // Pre-add one of two group members as a contact
    purple_blist_add_buddy(purple_buddy_new(account, purpleUserName(0).c_str(),
                                            (userFirstNames[0] + " " + userLastNames[0]).c_str()),
                           NULL, NULL, NULL);
    prpl.discardEvents();

    login(
        {
            // Private chat with the contact
            standardUpdateUser(0),
            standardPrivateChat(0),

            make_object<updateBasicGroup>(make_object<basicGroup>(
                groupId, 2, make_object<chatMemberStatusMember>(), true, 0
            )),
            make_object<updateNewChat>(makeChat(
                groupChatId, make_object<chatTypeBasicGroup>(groupId), groupChatTitle, nullptr, 0, 0, 0
            )),
            make_object<updateNewMessage>(
                makeMessage(messageId, userIds[0], groupChatId, false, date, makeTextMessage("Hello"))
            )
        },
        make_object<users>(1, std::vector<int32_t>(1, userIds[0])),
        make_object<chats>(std::vector<int64_t>(1, groupChatId)),
        {
            // TODO: chat title is wrong here because libpurple doesn't find the chat in contact
            // list while the contact is not online, and thus has no way of knowing the chat alias.
            // Real libpurple works like that and our mock version mirrors the behaviour.
            std::make_unique<ServGotJoinedChatEvent>(connection, 2, "chat" + std::to_string(groupChatId),
                                                     "chat" + std::to_string(groupChatId)),
            std::make_unique<ServGotChatEvent>(connection, 2, userFirstNames[0] + " " + userLastNames[0],
                                               "Hello", PURPLE_MESSAGE_RECV, date)
        },
        {make_object<viewMessages>(groupChatId, std::vector<int64_t>(1, messageId), true)}
    );

    tgl.verifyRequest(getBasicGroupFullInfo(groupId));
    tgl.update(standardUpdateUser(1));
    std::vector<object_ptr<chatMember>> members;
    members.push_back(make_object<chatMember>(
        userIds[0],
        userIds[1],
        0,
        make_object<chatMemberStatusMember>(),
        nullptr
    ));
    members.push_back(make_object<chatMember>(
        userIds[1],
        userIds[1],
        0,
        make_object<chatMemberStatusCreator>(),
        nullptr
    ));
    members.push_back(make_object<chatMember>(
        selfId,
        userIds[1],
        0,
        make_object<chatMemberStatusMember>(),
        nullptr
    ));
    tgl.reply(make_object<basicGroupFullInfo>(
        "basic group",
        userIds[1],
        std::move(members),
        ""
    ));

    // One code path: adding chat users upon receiving getBasicGroupFullInfo reply, because the chat
    // window is already open due to the received message
    prpl.verifyEvents(
        ChatAddUserEvent(
            "chat" + std::to_string(groupChatId),
            // This user is in our contact list so his libpurple user name is used
            purpleUserName(0),
            "", PURPLE_CBFLAGS_NONE, false
        ),
        ChatAddUserEvent(
            "chat" + std::to_string(groupChatId),
            // This user is not in our contact list so first/last name is used
            userFirstNames[1] + " " + userLastNames[1],
            "", PURPLE_CBFLAGS_FOUNDER, false
        ),
        ChatAddUserEvent(
            "chat" + std::to_string(groupChatId),
            // This is us (with + to match account name)
            "+" + selfPhoneNumber,
            "", PURPLE_CBFLAGS_NONE, false
        )
    );
}

TEST_F(GroupChatTest, SendMessageWithMemberList)
{
    constexpr int64_t messageId = 10001;
    constexpr int32_t date      = 12345;

    loginWithBasicGroup();

    tgl.update(standardUpdateUser(1));
    std::vector<object_ptr<chatMember>> members;
    members.push_back(make_object<chatMember>(
        userIds[0],
        userIds[1],
        0,
        make_object<chatMemberStatusMember>(),
        nullptr
    ));
    members.push_back(make_object<chatMember>(
        userIds[1],
        userIds[1],
        0,
        make_object<chatMemberStatusCreator>(),
        nullptr
    ));
    members.push_back(make_object<chatMember>(
        selfId,
        userIds[1],
        0,
        make_object<chatMemberStatusMember>(),
        nullptr
    ));
    tgl.reply(make_object<basicGroupFullInfo>(
        "basic group",
        userIds[1],
        std::move(members),
        ""
    ));

    GHashTable *components = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
    g_hash_table_insert(components, (char *)"id", g_strdup(("chat" + std::to_string(groupChatId)).c_str()));
    pluginInfo().join_chat(connection, components);
    g_hash_table_destroy(components);

    // Another code path: adding chat users upon opening chat, with basicGroupFullInfo before that
    prpl.verifyEvents(
        ServGotJoinedChatEvent(connection, 1, "chat" + std::to_string(groupChatId), groupChatTitle),
        ChatAddUserEvent(
            "chat" + std::to_string(groupChatId),
            userFirstNames[0] + " " + userLastNames[0],
            "", PURPLE_CBFLAGS_NONE, false
        ),
        ChatAddUserEvent(
            "chat" + std::to_string(groupChatId),
            userFirstNames[1] + " " + userLastNames[1],
            "", PURPLE_CBFLAGS_FOUNDER, false
        ),
        ChatAddUserEvent(
            "chat" + std::to_string(groupChatId),
            // This is us (with + to match account name)
            "+" + selfPhoneNumber,
            "", PURPLE_CBFLAGS_NONE, false
        ),
        PresentConversationEvent("chat" + std::to_string(groupChatId))
    );
    tgl.verifyNoRequests();

    ASSERT_EQ(0, pluginInfo().chat_send(connection, 1, "message", PURPLE_MESSAGE_SEND));
    tgl.verifyRequest(td::td_api::sendMessage(
        groupChatId,
        0,
        nullptr,
        nullptr,
        make_object<inputMessageText>(
            make_object<formattedText>("message", std::vector<object_ptr<textEntity>>()),
            false,
            false
        )
    ));
    prpl.verifyNoEvents();

    tgl.update(make_object<updateNewMessage>(
        makeMessage(messageId, selfId, groupChatId, true, date, makeTextMessage("message"))
    ));
    tgl.verifyRequest(viewMessages(
        groupChatId,
        {messageId},
        true
    ));
    prpl.verifyEvents(ConversationWriteEvent(
        "chat" + std::to_string(groupChatId), selfFirstName + " " + selfLastName,
        "message", PURPLE_MESSAGE_SEND, date
    ));
}

TEST_F(GroupChatTest, JoinBasicGroupByInviteLink)
{
    const char *const LINK = "https://t.me/joinchat/";
    login();

    // As if "Add chat" function in pidgin was used
    GHashTable *components = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
    g_hash_table_insert(components, (char *)"id", g_strdup(""));
    g_hash_table_insert(components, (char *)"link", g_strdup(LINK));

    PurpleChat *chat = purple_chat_new(account, "old chat", components);
    purple_blist_add_chat(chat, NULL, NULL);
    prpl.discardEvents();

    // And now that chat is being joined
    pluginInfo().join_chat(connection, components);
    prpl.verifyNoEvents();

    uint64_t joinRequestId = tgl.verifyRequest(joinChatByInviteLink(LINK));

    // Success
    tgl.update(make_object<updateBasicGroup>(make_object<basicGroup>(
        groupId, 2, make_object<chatMemberStatusMember>(), true, 0
    )));
    prpl.verifyNoEvents();
    tgl.verifyNoRequests();

    tgl.update(make_object<updateNewChat>(makeChat(
        groupChatId, make_object<chatTypeBasicGroup>(groupId), groupChatTitle, nullptr, 0, 0, 0
    )));
    // Chat is added, list of members requested
    prpl.verifyEvents(AddChatEvent(
        "chat" + std::to_string(groupChatId), groupChatTitle, account, NULL, NULL
    ));
    uint64_t groupInfoRequestId = tgl.verifyRequest(getBasicGroupFullInfo(groupId));

    // There will always be this "message" about joining the group
    tgl.update(make_object<updateNewMessage>(
        makeMessage(1, selfId, groupChatId, false, 12345, make_object<messageChatJoinByLink>())
    ));
    uint64_t viewMessagesRequestId = tgl.verifyRequest(viewMessages(
        groupChatId,
        {1},
        true
    ));

    // The message is shown in chat conversation
    prpl.verifyEvents(
        ServGotJoinedChatEvent(connection, 1, "chat" + std::to_string(groupChatId), groupChatTitle),
        ConversationWriteEvent("chat" + std::to_string(groupChatId), "",
                               "Received unsupported message type messageChatJoinByLink",
                               PURPLE_MESSAGE_SYSTEM, 12345)
    );

    // Now reply to join group - original chat is removed
    tgl.reply(joinRequestId, makeChat(
        groupChatId, make_object<chatTypeBasicGroup>(groupId), groupChatTitle, nullptr, 0, 0, 0
    ));
    prpl.verifyEvents(RemoveChatEvent("", LINK));

    // Replying to group full info request with list of members
    tgl.update(standardUpdateUser(0));
    tgl.update(standardUpdateUser(1));
    std::vector<object_ptr<chatMember>> members;
    members.push_back(make_object<chatMember>(
        userIds[0],
        userIds[1],
        0,
        make_object<chatMemberStatusMember>(),
        nullptr
    ));
    members.push_back(make_object<chatMember>(
        userIds[1],
        userIds[1],
        0,
        make_object<chatMemberStatusCreator>(),
        nullptr
    ));
    members.push_back(make_object<chatMember>(
        selfId,
        userIds[1],
        0,
        make_object<chatMemberStatusMember>(),
        nullptr
    ));
    tgl.reply(groupInfoRequestId, make_object<basicGroupFullInfo>(
        "basic group",
        userIds[1],
        std::move(members),
        ""
    ));

    prpl.verifyEvents(
        ChatAddUserEvent(
            "chat" + std::to_string(groupChatId),
            // This user is not in our contact list so first/last name is used
            userFirstNames[0] + " " + userLastNames[0],
            "", PURPLE_CBFLAGS_NONE, false
        ),
        ChatAddUserEvent(
            "chat" + std::to_string(groupChatId),
            // This user is not in our contact list so first/last name is used
            userFirstNames[1] + " " + userLastNames[1],
            "", PURPLE_CBFLAGS_FOUNDER, false
        ),
        ChatAddUserEvent(
            "chat" + std::to_string(groupChatId),
            // This is us (with + to match account name)
            "+" + selfPhoneNumber,
            "", PURPLE_CBFLAGS_NONE, false
        )
    );

    tgl.reply(viewMessagesRequestId, make_object<ok>());
}
