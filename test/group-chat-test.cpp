#include "fixture.h"
#include <glib/gstrfuncs.h>
#include <fmt/format.h>

class GroupChatTest: public CommTest {
protected:
    const int32_t     groupId             = 700;
    const int64_t     groupChatId         = 7000;
    const std::string groupChatTitle      = "Title";
    const std::string groupChatPurpleName = "chat" + std::to_string(groupChatId);

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
            )),
            makeUpdateChatListMain(groupChatId)
        },
        make_object<users>(),
        make_object<chats>(std::vector<int64_t>(1, groupChatId)),
        {}, {},
        {
            std::make_unique<ConnectionSetStateEvent>(connection, PURPLE_CONNECTED),
            std::make_unique<AddChatEvent>(
                groupChatPurpleName, groupChatTitle, account, nullptr, nullptr
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
    prpl.verifyNoEvents();
    tgl.verifyNoRequests();
    tgl.update(makeUpdateChatListMain(groupChatId));

    tgl.verifyRequest(getBasicGroupFullInfo(groupId));
    prpl.verifyEvents(AddChatEvent(
        groupChatPurpleName, groupChatTitle, account, NULL, NULL
    ));
}

TEST_F(GroupChatTest, ExistingBasicGroupChatAtLogin)
{
    GHashTable *components = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
    g_hash_table_insert(components, (char *)"id", g_strdup((groupChatPurpleName).c_str()));
    purple_blist_add_chat(purple_chat_new(account, groupChatTitle.c_str(), components), NULL, NULL);
    prpl.discardEvents();

    login(
        {
            make_object<updateBasicGroup>(make_object<basicGroup>(
                groupId, 2, make_object<chatMemberStatusMember>(), true, 0
            )),
            make_object<updateNewChat>(makeChat(
                groupChatId, make_object<chatTypeBasicGroup>(groupId), groupChatTitle, nullptr, 0, 0, 0
            )),
            makeUpdateChatListMain(groupChatId)
        },
        make_object<users>(),
        make_object<chats>(std::vector<int64_t>(1, groupChatId))
    );

    tgl.verifyRequest(getBasicGroupFullInfo(groupId));
}

TEST_F(GroupChatTest, BasicGroupReceiveTextAndReply)
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
        ServGotJoinedChatEvent(connection, 1, groupChatPurpleName, groupChatTitle),
        ServGotChatEvent(connection, 1, userFirstNames[0] + " " + userLastNames[0],
                         "Hello", PURPLE_MESSAGE_RECV, date[0])
    );

    object_ptr<message> reply = makeMessage(messageId[1], selfId, groupChatId, true, date[1],
                                            makeTextMessage("Reply"));
    reply->reply_to_message_id_ = messageId[0];
    tgl.update(make_object<updateNewMessage>(std::move(reply)));
    tgl.verifyRequest(viewMessages(
        groupChatId,
        {messageId[1]},
        true
    ));
    prpl.verifyEvents(ConversationWriteEvent(
        groupChatPurpleName, selfFirstName + " " + selfLastName,
        fmt::format(replyPattern, userFirstNames[0] + " " + userLastNames[0], "Hello", "Reply"),
        PURPLE_MESSAGE_SEND, date[1]
    ));
}

TEST_F(GroupChatTest, BasicGroupReceivePhoto)
{
    const int32_t date      = 12345;
    const int64_t messageId = 10001;
    const int32_t fileId    = 1234;
    loginWithBasicGroup();

    tgl.update(make_object<updateNewMessage>(makeMessage(
        messageId, userIds[0], groupChatId, false, date,
        make_object<messagePhoto>(
            makePhotoRemote(fileId, 10000, 640, 480),
            make_object<formattedText>("photo", std::vector<object_ptr<textEntity>>()),
            false
        )
    )));
    tgl.verifyRequests({
        make_object<viewMessages>(groupChatId, std::vector<int64_t>(1, messageId), true),
        make_object<downloadFile>(fileId, 1, 0, 0, true)
    });
    prpl.verifyEvents(
        ServGotJoinedChatEvent(connection, 1, groupChatPurpleName, groupChatTitle),
        ServGotChatEvent(connection, 1, userFirstNames[0] + " " + userLastNames[0], "photo",
                         PURPLE_MESSAGE_RECV, date),
        ConversationWriteEvent(groupChatPurpleName, "",
                               userFirstNames[0] + " " + userLastNames[0] + ": Downloading image",
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
    g_hash_table_insert(table, (char *)"id", g_strdup((groupChatPurpleName).c_str()));
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
            makeUpdateChatListMain(groupChatId),
            make_object<updateNewMessage>(
                makeMessage(messageId, userIds[0], groupChatId, false, date, makeTextMessage("Hello"))
            )
        },
        make_object<users>(1, std::vector<int32_t>(1, userIds[0])),
        make_object<chats>(std::vector<int64_t>(1, groupChatId)),
        {
            // chat title is wrong at this point because libpurple doesn't find the chat in contact
            // list while the contact is not online, and thus has no way of knowing the chat alias.
            // Real libpurple works like that and our mock version mirrors the behaviour.
            std::make_unique<ServGotJoinedChatEvent>(connection, 2, groupChatPurpleName,
                                                     groupChatPurpleName),
            std::make_unique<ConvSetTitleEvent>(groupChatPurpleName, groupChatTitle),
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
        ChatClearUsersEvent(groupChatPurpleName),
        ChatAddUserEvent(
            groupChatPurpleName,
            // This user is in our contact list so his libpurple user name is used
            purpleUserName(0),
            "", PURPLE_CBFLAGS_NONE, false
        ),
        ChatAddUserEvent(
            groupChatPurpleName,
            // This user is not in our contact list so first/last name is used
            userFirstNames[1] + " " + userLastNames[1],
            "", PURPLE_CBFLAGS_FOUNDER, false
        ),
        ChatAddUserEvent(
            groupChatPurpleName,
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
    g_hash_table_insert(components, (char *)"id", g_strdup((groupChatPurpleName).c_str()));
    pluginInfo().join_chat(connection, components);
    g_hash_table_destroy(components);

    // Another code path: adding chat users upon opening chat, with basicGroupFullInfo before that
    prpl.verifyEvents(
        ServGotJoinedChatEvent(connection, 1, groupChatPurpleName, groupChatTitle),
        ChatClearUsersEvent(groupChatPurpleName),
        ChatAddUserEvent(
            groupChatPurpleName,
            userFirstNames[0] + " " + userLastNames[0],
            "", PURPLE_CBFLAGS_NONE, false
        ),
        ChatAddUserEvent(
            groupChatPurpleName,
            userFirstNames[1] + " " + userLastNames[1],
            "", PURPLE_CBFLAGS_FOUNDER, false
        ),
        ChatAddUserEvent(
            groupChatPurpleName,
            // This is us (with + to match account name)
            "+" + selfPhoneNumber,
            "", PURPLE_CBFLAGS_NONE, false
        ),
        PresentConversationEvent(groupChatPurpleName)
    );
    tgl.verifyNoRequests();

    ASSERT_EQ(0, pluginInfo().chat_send(connection, 1, "message", PURPLE_MESSAGE_SEND));
    tgl.verifyRequest(sendMessage(
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
        groupChatPurpleName, selfFirstName + " " + selfLastName,
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

    auto chatUpdate = make_object<updateNewChat>(makeChat(
        groupChatId, make_object<chatTypeBasicGroup>(groupId), groupChatTitle, nullptr, 0, 0, 0
    ));
    chatUpdate->chat_->chat_list_ = make_object<chatListMain>();
    tgl.update(std::move(chatUpdate));
    // Chat is added, list of members requested
    prpl.verifyEvents(AddChatEvent(
        groupChatPurpleName, groupChatTitle, account, NULL, NULL
    ));
    uint64_t groupInfoRequestId = tgl.verifyRequest(getBasicGroupFullInfo(groupId));

    // There will always be this "message" about joining the group
    tgl.update(make_object<updateNewMessage>(
        makeMessage(1, selfId, groupChatId, true, 12345, make_object<messageChatJoinByLink>())
    ));
    uint64_t viewMessagesRequestId = tgl.verifyRequest(viewMessages(
        groupChatId,
        {1},
        true
    ));

    // The message is shown in chat conversation
    prpl.verifyEvents(
        ServGotJoinedChatEvent(connection, 1, groupChatPurpleName, groupChatTitle),
        ConversationWriteEvent(groupChatPurpleName, "",
                               selfFirstName + " " + selfLastName + ": " +
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
        ChatClearUsersEvent(groupChatPurpleName),
        ChatAddUserEvent(
            groupChatPurpleName,
            // This user is not in our contact list so first/last name is used
            userFirstNames[0] + " " + userLastNames[0],
            "", PURPLE_CBFLAGS_NONE, false
        ),
        ChatAddUserEvent(
            groupChatPurpleName,
            // This user is not in our contact list so first/last name is used
            userFirstNames[1] + " " + userLastNames[1],
            "", PURPLE_CBFLAGS_FOUNDER, false
        ),
        ChatAddUserEvent(
            groupChatPurpleName,
            // This is us (with + to match account name)
            "+" + selfPhoneNumber,
            "", PURPLE_CBFLAGS_NONE, false
        )
    );

    tgl.reply(viewMessagesRequestId, make_object<ok>());
}

TEST_F(GroupChatTest, GroupRenamed)
{
    const int64_t messageId = 1;
    const int32_t date      = 1234;
    loginWithBasicGroup();
    tgl.update(make_object<updateChatTitle>(groupChatId, "New Title"));
    prpl.verifyEvents(AliasChatEvent(groupChatPurpleName, "New Title"));

    tgl.update(make_object<updateNewMessage>(
        makeMessage(
            messageId, userIds[0], groupChatId, false, date,
            make_object<messageChatChangeTitle>("New Title")
        )
    ));
    tgl.verifyRequest(viewMessages(
        groupChatId,
        {messageId},
        true
    ));
    prpl.verifyEvents(
        ServGotJoinedChatEvent(connection, 1, groupChatPurpleName, "New Title"),
        ConversationWriteEvent(
            groupChatPurpleName, "",
            userFirstNames[0] + " " + userLastNames[0] + " changed group name to New Title",
            PURPLE_MESSAGE_SYSTEM, date
        )
    );
}

TEST_F(GroupChatTest, AddContactByGroupChatName)
{
    loginWithBasicGroup();

    // We get to know about a non-contact because it's in group members
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

    // Adding him to contact list from group chat members
    PurpleBuddy *buddy = purple_buddy_new(account, (userFirstNames[1] + " " + userLastNames[1]).c_str(), "");
    purple_blist_add_buddy(buddy, NULL, &standardPurpleGroup, NULL);
    prpl.discardEvents();
    pluginInfo().add_buddy(connection, buddy, &standardPurpleGroup);

    // The buddy is deleted right away, to be replaced later
    prpl.verifyEvents(RemoveBuddyEvent(account, userFirstNames[1] + " " + userLastNames[1]));
    tgl.verifyRequest(addContact(make_object<contact>(
        "", userFirstNames[1], userLastNames[1], "", userIds[1]
    ), true));

    tgl.reply(make_object<ok>());
    tgl.verifyRequest(createPrivateChat(userIds[1], false));
    // The rest is tested elsewhere
}
