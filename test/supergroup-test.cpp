#include "fixture.h"
#include <glib/gstrfuncs.h>

static const char *NotificationWho = " ";

class SupergroupTest: public CommTest {
protected:
    const int32_t     groupId             = 700;
    const int64_t     groupChatId         = -7000;
    const std::string groupChatTitle      = "Title";
    const std::string groupChatPurpleName = "chat" + std::to_string(groupChatId);

    void loginWithSupergroup(object_ptr<supergroupFullInfo> fullInfo = nullptr,
                             object_ptr<chatMembers> recentMembers = nullptr,
                             object_ptr<chatMembers> administrators = nullptr);
};

void SupergroupTest::loginWithSupergroup(object_ptr<supergroupFullInfo> fullInfo,
                                         object_ptr<chatMembers> recentMembers,
                                         object_ptr<chatMembers> administrators)
{
    if (!fullInfo)
        fullInfo = make_object<supergroupFullInfo>();
    if (!recentMembers)
        recentMembers = make_object<chatMembers>();
    if (!administrators)
        administrators = make_object<chatMembers>();
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
            make_object<updateUser>(makeUser(
                // Chat member
                userIds[1],
                userFirstNames[1],
                userLastNames[1],
                "",
                make_object<userStatusOffline>()
            )),
            make_object<updateSupergroup>(make_object<supergroup>(
                groupId, "", 0, make_object<chatMemberStatusMember>(), 2,
                false, false, false, false, false, false, "", false
            )),
            make_object<updateNewChat>(makeChat(
                groupChatId, make_object<chatTypeSupergroup>(groupId, false), groupChatTitle,
                nullptr, 0, 0, 0
            )),
            makeUpdateChatListMain(groupChatId)
        },
        make_object<users>(),
        make_object<chats>(std::vector<int64_t>(1, groupChatId)),
        {
            std::make_unique<AddChatEvent>(
                groupChatPurpleName, groupChatTitle, account, nullptr, nullptr
            ),
        },
        {
            make_object<getSupergroupFullInfo>(groupId),
            make_object<getSupergroupMembers>(
                groupId,
                make_object<supergroupMembersFilterRecent>(),
                0,
                200
            ),
            std::move(fullInfo),
            std::move(recentMembers),
            make_object<getSupergroupMembers>(
                groupId,
                make_object<supergroupMembersFilterAdministrators>(),
                0, 200
            ),
            std::move(administrators)
        }
    );
}

TEST_F(SupergroupTest, AddSupergroupChatAtLogin)
{
    loginWithSupergroup();
}

TEST_F(SupergroupTest, AddSupergroupChatAtLogin_WithMemberList_OpenChatAfterFullInfo)
{
    constexpr int     purpleChatId = 1;

    auto fullInfo = make_object<supergroupFullInfo>();
    fullInfo->description_ = "Description";

    auto members = make_object<chatMembers>();
    members->members_.push_back(make_object<chatMember>(
        userIds[1],
        userIds[1],
        0,
        make_object<chatMemberStatusCreator>("", true),
        nullptr
    ));
    members->members_.push_back(make_object<chatMember>(
        selfId,
        userIds[1],
        0,
        make_object<chatMemberStatusMember>(),
        nullptr
    ));
    members->members_.push_back(nullptr);

    auto admins = make_object<chatMembers>();
    admins->members_.push_back(make_object<chatMember>(
        userIds[0],
        userIds[1],
        0,
        make_object<chatMemberStatusAdministrator>(),
        nullptr
    ));
    admins->members_.push_back(make_object<chatMember>(
        userIds[1],
        userIds[1],
        0,
        make_object<chatMemberStatusCreator>("", true),
        nullptr
    ));
    admins->members_.push_back(nullptr);
    loginWithSupergroup(std::move(fullInfo), std::move(members), std::move(admins));

    GHashTable *components = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
    g_hash_table_insert(components, (char *)"id", g_strdup((groupChatPurpleName).c_str()));
    pluginInfo().join_chat(connection, components);
    g_hash_table_destroy(components);

    prpl.verifyEvents(
        ServGotJoinedChatEvent(connection, purpleChatId, groupChatPurpleName, groupChatTitle),
        ChatSetTopicEvent(groupChatPurpleName, "Description", ""),
        ChatClearUsersEvent(groupChatPurpleName),
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
        ),
        ChatAddUserEvent(
            groupChatPurpleName,
            // This user is not in our contact list so first/last name is used
            userFirstNames[0] + " " + userLastNames[0],
            "", PURPLE_CBFLAGS_OP, false
        ),
        PresentConversationEvent(groupChatPurpleName)
    );
    tgl.verifyNoRequests();
}

TEST_F(SupergroupTest, ExistingSupergroupChatAtLogin)
{
    GHashTable *components = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
    g_hash_table_insert(components, (char *)"id", g_strdup((groupChatPurpleName).c_str()));
    purple_blist_add_chat(purple_chat_new(account, groupChatTitle.c_str(), components), NULL, NULL);
    prpl.discardEvents();

    auto chat = makeChat(
        groupChatId, make_object<chatTypeSupergroup>(groupId, false), groupChatTitle,
        nullptr, 0, 0, 0
    );
    chat->chat_list_ = make_object<chatListMain>();
    login(
        {
            make_object<updateSupergroup>(make_object<supergroup>(
                groupId, "", 0, make_object<chatMemberStatusMember>(), 2,
                false, false, false, false, false, false, "", false
            )),
            make_object<updateNewChat>(std::move(chat)),
        },
        make_object<users>(),
        make_object<chats>(std::vector<int64_t>(1, groupChatId)),
        {},
        {
            make_object<getSupergroupFullInfo>(groupId),
            make_object<getSupergroupMembers>(
                groupId,
                make_object<supergroupMembersFilterRecent>(),
                0,
                200
            ),
        }
    );
}

TEST_F(SupergroupTest, ExistingSupergroupReceiveMessageAtLogin_WithMemberList_OpenChatBeforeFullInfo)
{
    constexpr int64_t messageId    = 10001;
    constexpr int32_t date         = 12345;
    constexpr int     purpleChatId = 1;

    GHashTable *components = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
    g_hash_table_insert(components, (char *)"id", g_strdup((groupChatPurpleName).c_str()));
    purple_blist_add_chat(purple_chat_new(account, groupChatTitle.c_str(), components), NULL, NULL);
    prpl.discardEvents();

    auto chat = makeChat(
        groupChatId, make_object<chatTypeSupergroup>(groupId, false), groupChatTitle,
        nullptr, 0, 0, 0
    );
    chat->chat_list_ = make_object<chatListMain>();

    auto fullInfo = make_object<supergroupFullInfo>();
    fullInfo->description_ = "Description";

    auto members = make_object<chatMembers>();
    members->members_.push_back(make_object<chatMember>(
        userIds[1],
        userIds[1],
        0,
        make_object<chatMemberStatusCreator>("", true),
        nullptr
    ));
    members->members_.push_back(make_object<chatMember>(
        selfId,
        userIds[1],
        0,
        make_object<chatMemberStatusMember>(),
        nullptr
    ));
    members->members_.push_back(nullptr);

    auto admins = make_object<chatMembers>();
    admins->members_.push_back(make_object<chatMember>(
        userIds[0],
        userIds[1],
        0,
        make_object<chatMemberStatusAdministrator>(),
        nullptr
    ));
    admins->members_.push_back(make_object<chatMember>(
        userIds[1],
        userIds[1],
        0,
        make_object<chatMemberStatusCreator>("", true),
        nullptr
    ));
    admins->members_.push_back(nullptr);

    login(
        {
            make_object<updateSupergroup>(make_object<supergroup>(
                groupId, "", 0, make_object<chatMemberStatusMember>(), 2,
                false, false, false, false, false, false, "", false
            )),
            make_object<updateNewChat>(std::move(chat)),
            standardUpdateUser(0), // Incoming message will be from this guy
            standardUpdateUser(1), // Another group member
            make_object<updateNewMessage>(
                makeMessage(messageId, userIds[0], groupChatId, false, date, makeTextMessage("Hello"))
            )
        },
        make_object<users>(),
        make_object<chats>(std::vector<int64_t>(1, groupChatId)),
        {
            std::make_unique<ServGotJoinedChatEvent>(connection, purpleChatId, groupChatPurpleName,
                                                     groupChatTitle),
            std::make_unique<ServGotChatEvent>(connection, purpleChatId, userFirstNames[0] + " " + userLastNames[0],
                                               "Hello", PURPLE_MESSAGE_RECV, date)
        },
        {
            make_object<getSupergroupFullInfo>(groupId),
            make_object<getSupergroupMembers>(
                groupId,
                make_object<supergroupMembersFilterRecent>(),
                0,
                200
            ),
            make_object<viewMessages>(groupChatId, std::vector<int64_t>(1, messageId), true),
            std::move(fullInfo),
            std::move(members),
            make_object<getSupergroupMembers>(
                groupId,
                make_object<supergroupMembersFilterAdministrators>(),
                0, 200
            ),
            std::move(admins)
        },
        {
            std::make_unique<ChatSetTopicEvent>(groupChatPurpleName, "Description", ""),
            std::make_unique<ChatClearUsersEvent>(groupChatPurpleName),
            std::make_unique<ChatAddUserEvent>(
                groupChatPurpleName,
                // This user is not in our contact list so first/last name is used
                userFirstNames[1] + " " + userLastNames[1],
                "", PURPLE_CBFLAGS_FOUNDER, false
            ),
            std::make_unique<ChatAddUserEvent>(
                groupChatPurpleName,
                // This is us (with + to match account name)
                "+" + selfPhoneNumber,
                "", PURPLE_CBFLAGS_NONE, false
            ),
            std::make_unique<ChatAddUserEvent>(
                groupChatPurpleName,
                // This user is not in our contact list so first/last name is used
                userFirstNames[0] + " " + userLastNames[0],
                "", PURPLE_CBFLAGS_OP, false
            ),
            std::make_unique<AccountSetAliasEvent>(account, selfFirstName + " " + selfLastName),
            std::make_unique<ShowAccountEvent>(account)
        }
    );
}

TEST_F(SupergroupTest, DeleteSupergroup_Fail)
{
    loginWithSupergroup();
    PurpleChat *chat = purple_blist_find_chat(account, groupChatPurpleName.c_str());
    ASSERT_NE(nullptr, chat);
    GList *actions = pluginInfo().blist_node_menu(&chat->node);

    nodeMenuAction(&chat->node, actions, "Delete group");
    prpl.verifyEvents(RequestActionEvent(connection, account, NULL, NULL, 2));
    prpl.requestedAction("_No");
    tgl.verifyNoRequests();
    prpl.verifyNoEvents();

    nodeMenuAction(&chat->node, actions, "Delete group");
    prpl.verifyEvents(RequestActionEvent(connection, account, NULL, NULL, 2));
    prpl.requestedAction("_Yes");
    tgl.verifyRequest(deleteSupergroup(groupId));
    tgl.reply(make_object<error>(100, "error"));

    g_list_free_full(actions, (GDestroyNotify)purple_menu_action_free);
}

TEST_F(SupergroupTest, LeaveSupergroup)
{
    const int32_t date         = 12345;
    const int64_t messageId    = 10001;
    constexpr int purpleChatId = 1;

    loginWithSupergroup();
    PurpleChat *chat = purple_blist_find_chat(account, groupChatPurpleName.c_str());
    ASSERT_NE(nullptr, chat);
    GList *actions = pluginInfo().blist_node_menu(&chat->node);

    nodeMenuAction(&chat->node, actions, "Leave group");
    prpl.verifyEvents(RequestActionEvent(connection, account, NULL, NULL, 2));
    prpl.requestedAction("_No");
    tgl.verifyNoRequests();
    prpl.verifyNoEvents();

    nodeMenuAction(&chat->node, actions, "Leave group");
    prpl.verifyEvents(RequestActionEvent(connection, account, NULL, NULL, 2));
    prpl.requestedAction("_Yes");
    tgl.verifyRequests({
        make_object<leaveChat>(groupChatId),
        make_object<deleteChatHistory>(groupChatId, true, false)
    });

    tgl.update(make_object<updateChatChatList>(groupChatId, nullptr));
    prpl.verifyEvents(RemoveChatEvent(groupChatPurpleName, ""));
    tgl.update(make_object<updateSupergroup>(make_object<supergroup>(
        groupId, "", 0, make_object<chatMemberStatusBanned>(0), 0,
        false, false, false, false, false, false, "", false
    )));
    tgl.update(make_object<updateSupergroup>(make_object<supergroup>(
        groupId, "", 0, make_object<chatMemberStatusLeft>(), 0,
        false, false, false, false, false, false, "", false
    )));

    prpl.verifyNoEvents();
    tgl.update(make_object<updateNewMessage>(
        makeMessage(messageId, selfId, groupChatId, true, date,
                    make_object<messageChatDeleteMember>(selfId))
    ));
    tgl.verifyRequest(viewMessages(groupChatId, {messageId}, true));
    prpl.verifyEvents(
        ServGotJoinedChatEvent(connection, purpleChatId, groupChatPurpleName, groupChatPurpleName),
        ConvSetTitleEvent(groupChatPurpleName, groupChatTitle),
        ChatSetTopicEvent(groupChatPurpleName, "", ""),
        ChatClearUsersEvent(groupChatPurpleName),
        ConversationWriteEvent(groupChatPurpleName, NotificationWho,
                               selfFirstName + " " + selfLastName +
                               ": Received unsupported message type messageChatDeleteMember",
                               PURPLE_MESSAGE_SYSTEM, date)
    );

    // There is a check that fails message sending if we are not a group member
    ASSERT_LT(pluginInfo().chat_send(connection, purpleChatId, "message", PURPLE_MESSAGE_SEND), 0);

    g_list_free_full(actions, (GDestroyNotify)purple_menu_action_free);
}

TEST_F(SupergroupTest, GetInviteLink)
{
    constexpr int     purpleChatId = 1;
    loginWithSupergroup();

    PurpleChat *chat = purple_blist_find_chat(account, groupChatPurpleName.c_str());
    ASSERT_NE(nullptr, chat);
    GList *actions = pluginInfo().blist_node_menu(&chat->node);

    nodeMenuAction(&chat->node, actions, "Show invite link");
    tgl.verifyRequest(generateChatInviteLink(groupChatId));

    tgl.reply(make_object<error>(100, "error"));
    prpl.verifyEvents(
        ServGotJoinedChatEvent(connection, purpleChatId, groupChatPurpleName, groupChatTitle),
        ChatSetTopicEvent(groupChatPurpleName, "", ""),
        ChatClearUsersEvent(groupChatPurpleName),
        ConversationWriteEvent(
            groupChatPurpleName, NotificationWho,
            "Cannot generate invite link: code 100 (error)",
            PURPLE_MESSAGE_SYSTEM, 0
        )
    );

    nodeMenuAction(&chat->node, actions, "Show invite link");
    tgl.verifyRequest(generateChatInviteLink(groupChatId));
    auto fullInfo = make_object<supergroupFullInfo>();
    fullInfo->invite_link_ = "http://invite";
    tgl.update(make_object<updateSupergroupFullInfo>(
        groupChatId,
        std::move(fullInfo)
    ));
    prpl.verifyNoEvents();

    tgl.reply(make_object<chatInviteLink>("http://invite"));
    prpl.verifyEvents(
        ConversationWriteEvent(
            groupChatPurpleName, NotificationWho,
            "http://invite",
            PURPLE_MESSAGE_SYSTEM, 0
        )
    );

    g_list_free_full(actions, (GDestroyNotify)purple_menu_action_free);
}

TEST_F(SupergroupTest, JoinByLink_InvalidLink1)
{
    login();

    GHashTable *components = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
    g_hash_table_insert(components, (char *)"id", g_strdup(""));
    g_hash_table_insert(components, (char *)"link", g_strdup("http://example.com"));

    pluginInfo().join_chat(connection, components);
    // Not a join link nor a group link - fail immediately
    tgl.verifyNoRequests();

    g_hash_table_destroy(components);
}

TEST_F(SupergroupTest, JoinByLink_InvalidLink2)
{
    login();

    GHashTable *components = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
    g_hash_table_insert(components, (char *)"id", g_strdup(""));
    g_hash_table_insert(components, (char *)"link", g_strdup("https://t.me/invalid/link"));

    pluginInfo().join_chat(connection, components);
    // Not a join link nor a group link - fail immediately
    tgl.verifyNoRequests();

    g_hash_table_destroy(components);
}

TEST_F(SupergroupTest, JoinByPublicLink1)
{
    const char *const NAME         = "groupname";
    const char *const LINK         = "https://t.me/groupname";
    constexpr int     purpleChatId = 1;

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

    tgl.verifyRequest(searchPublicChat(NAME));
    tgl.update(make_object<updateSupergroup>(make_object<supergroup>(
        groupId, "", 0, make_object<chatMemberStatusLeft>(), 2,
        false, false, false, false, false, false, "", false
    )));
    tgl.update(make_object<updateNewChat>(makeChat(
        groupChatId, make_object<chatTypeSupergroup>(groupId, false), groupChatTitle,
        nullptr, 0, 0, 0
    )));
    tgl.reply(makeChat(
        groupChatId, make_object<chatTypeSupergroup>(groupId, false), groupChatTitle,
        nullptr, 0, 0, 0
    ));

    uint64_t joinRequestId = tgl.verifyRequest(joinChat(groupChatId));
    prpl.verifyNoEvents();

    tgl.update(make_object<updateSupergroup>(make_object<supergroup>(
        groupId, "", 0, make_object<chatMemberStatusMember>(), 2,
        false, false, false, false, false, false, "", false
    )));
    tgl.update(make_object<updateChatChatList>(groupChatId, make_object<chatListMain>()));
    prpl.verifyEvents(AddChatEvent(
        groupChatPurpleName, groupChatTitle, account, NULL, NULL
    ));
    tgl.verifyRequests({
        make_object<getSupergroupFullInfo>(groupId),
        make_object<getSupergroupMembers>(
            groupId,
            make_object<supergroupMembersFilterRecent>(),
            0,
            200
        ),
    });

    tgl.reply(joinRequestId, make_object<ok>());
    prpl.verifyEvents(
        RemoveChatEvent("", LINK),
        ServGotJoinedChatEvent(
            connection, purpleChatId, groupChatPurpleName, groupChatTitle
        )
    );
}

TEST_F(SupergroupTest, JoinByPublicLink2)
{
    login();

    GHashTable *components = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
    g_hash_table_insert(components, (char *)"id", g_strdup(""));
    g_hash_table_insert(components, (char *)"link", g_strdup("https://t.me/s/groupname"));

    pluginInfo().join_chat(connection, components);
    g_hash_table_destroy(components);
    tgl.verifyRequest(searchPublicChat("groupname"));
}

TEST_F(SupergroupTest, JoinByPublicLink_SearchFail)
{
    login();

    GHashTable *components = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
    g_hash_table_insert(components, (char *)"id", g_strdup(""));
    g_hash_table_insert(components, (char *)"link", g_strdup("https://t.me/s/groupname"));

    pluginInfo().join_chat(connection, components);
    g_hash_table_destroy(components);
    tgl.verifyRequest(searchPublicChat("groupname"));
    tgl.reply(make_object<error>(100, "error"));

    tgl.verifyNoRequests();
    prpl.verifyNoEvents();
}

TEST_F(SupergroupTest, JoinByPublicLink_NotGroupChat)
{
    login();

    GHashTable *components = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
    g_hash_table_insert(components, (char *)"id", g_strdup(""));
    g_hash_table_insert(components, (char *)"link", g_strdup("https://t.me/s/groupname"));

    pluginInfo().join_chat(connection, components);
    g_hash_table_destroy(components);
    tgl.verifyRequest(searchPublicChat("groupname"));

    tgl.update(make_object<updateUser>(makeUser(
        userIds[0],
        userFirstNames[0],
        userLastNames[0],
        "",
        make_object<userStatusOffline>()
    )));
    tgl.update(standardPrivateChat(0));
    tgl.reply(std::move(standardPrivateChat(0)->chat_));

    tgl.verifyNoRequests();
    prpl.verifyNoEvents();
}

TEST_F(SupergroupTest, JoinByPublicLink_JoinFail)
{
    login();

    GHashTable *components = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
    g_hash_table_insert(components, (char *)"id", g_strdup(""));
    g_hash_table_insert(components, (char *)"link", g_strdup("https://t.me/s/groupname"));

    pluginInfo().join_chat(connection, components);
    g_hash_table_destroy(components);
    tgl.verifyRequest(searchPublicChat("groupname"));

    tgl.update(make_object<updateSupergroup>(make_object<supergroup>(
        groupId, "", 0, make_object<chatMemberStatusLeft>(), 2,
        false, false, false, false, false, false, "", false
    )));
    tgl.update(make_object<updateNewChat>(makeChat(
        groupChatId, make_object<chatTypeSupergroup>(groupId, false), groupChatTitle,
        nullptr, 0, 0, 0
    )));
    tgl.reply(makeChat(
        groupChatId, make_object<chatTypeSupergroup>(groupId, false), groupChatTitle,
        nullptr, 0, 0, 0
    ));

    tgl.verifyRequest(joinChat(groupChatId));
    tgl.reply(make_object<error>(100, "error"));
    prpl.verifyNoEvents();
}

TEST_F(SupergroupTest, ReceiveChannelPost)
{
    constexpr int32_t date         = 12345;
    constexpr int64_t messageId    = 10000;
    constexpr int     purpleChatId = 1;
    loginWithSupergroup();

    auto message = makeMessage(messageId, 0, groupChatId, false, date, makeTextMessage("Hello"));
    message->is_channel_post_ = true;
    tgl.update(make_object<updateNewMessage>(std::move(message)));

    tgl.verifyRequest(viewMessages(
        groupChatId,
        {messageId},
        true
    ));
    prpl.verifyEvents(
        ServGotJoinedChatEvent(connection, purpleChatId, groupChatPurpleName, groupChatTitle),
        ChatSetTopicEvent(groupChatPurpleName, "", ""),
        ChatClearUsersEvent(groupChatPurpleName),
        ServGotChatEvent(connection, purpleChatId, "Channel post",
                         "Hello", PURPLE_MESSAGE_RECV, date)
    );

}
