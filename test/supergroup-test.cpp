#include "fixture.h"
#include <glib/gstrfuncs.h>

class SupergroupTest: public CommTest {
protected:
    const int32_t     groupId             = 700;
    const int64_t     groupChatId         = -7000;
    const std::string groupChatTitle      = "Title";
    const std::string groupChatPurpleName = "chat" + std::to_string(groupChatId);

    void loginWithSupergroup();
};

void SupergroupTest::loginWithSupergroup()
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

    tgl.verifyRequest(getSupergroupFullInfo(groupId));
}

TEST_F(SupergroupTest, AddSupergroupChatAtLogin)
{
    loginWithSupergroup();
}

TEST_F(SupergroupTest, AddSupergroupChatAtLogin_OpenChatAfterFullInfo)
{
    constexpr int     purpleChatId = 1;

    loginWithSupergroup();
    auto fullInfo = make_object<supergroupFullInfo>();
    fullInfo->description_ = "Description";
    tgl.reply(std::move(fullInfo));

    GHashTable *components = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
    g_hash_table_insert(components, (char *)"id", g_strdup((groupChatPurpleName).c_str()));
    pluginInfo().join_chat(connection, components);
    g_hash_table_destroy(components);

    prpl.verifyEvents(
        ServGotJoinedChatEvent(connection, purpleChatId, groupChatPurpleName, groupChatTitle),
        ChatSetTopicEvent(groupChatPurpleName, "Description", ""),
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

    login(
        {
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
        make_object<chats>(std::vector<int64_t>(1, groupChatId))
    );

    tgl.verifyRequest(getSupergroupFullInfo(groupId));
}

TEST_F(SupergroupTest, ExistingSupergroupReceiveMessageAtLogin_OpenChatBeforeFullInfo)
{
    constexpr int64_t messageId    = 10001;
    constexpr int32_t date         = 12345;
    constexpr int     purpleChatId = 1;

    GHashTable *components = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
    g_hash_table_insert(components, (char *)"id", g_strdup((groupChatPurpleName).c_str()));
    purple_blist_add_chat(purple_chat_new(account, groupChatTitle.c_str(), components), NULL, NULL);
    prpl.discardEvents();

    login(
        {
            make_object<updateSupergroup>(make_object<supergroup>(
                groupId, "", 0, make_object<chatMemberStatusMember>(), 2,
                false, false, false, false, false, false, "", false
            )),
            make_object<updateNewChat>(makeChat(
                groupChatId, make_object<chatTypeSupergroup>(groupId, false), groupChatTitle,
                nullptr, 0, 0, 0
            )),
            makeUpdateChatListMain(groupChatId),
            standardUpdateUser(0), // Incoming message will be from this guy
            make_object<updateNewMessage>(
                makeMessage(messageId, userIds[0], groupChatId, false, date, makeTextMessage("Hello"))
            )
        },
        make_object<users>(),
        make_object<chats>(std::vector<int64_t>(1, groupChatId)),
        {
            // chat title is wrong at this point because libpurple doesn't find the chat in contact
            // list while the contact is not online, and thus has no way of knowing the chat alias.
            // Real libpurple works like that and our mock version mirrors the behaviour.
            std::make_unique<ServGotJoinedChatEvent>(connection, purpleChatId, groupChatPurpleName,
                                                     groupChatPurpleName),
            // Now chat title is corrected
            std::make_unique<ConvSetTitleEvent>(groupChatPurpleName, groupChatTitle),
            std::make_unique<ServGotChatEvent>(connection, purpleChatId, userFirstNames[0] + " " + userLastNames[0],
                                               "Hello", PURPLE_MESSAGE_RECV, date)
        },
        {make_object<viewMessages>(groupChatId, std::vector<int64_t>(1, messageId), true)}
    );

    tgl.verifyRequest(getSupergroupFullInfo(groupId));
    auto fullInfo = make_object<supergroupFullInfo>();
    fullInfo->description_ = "Description";
    tgl.reply(std::move(fullInfo));

    prpl.verifyEvents(ChatSetTopicEvent(groupChatPurpleName, "Description", ""));
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
        ConversationWriteEvent(groupChatPurpleName, "",
                               selfFirstName + " " + selfLastName +
                               ": Received unsupported message type messageChatDeleteMember",
                               PURPLE_MESSAGE_SYSTEM, date)
    );

    // There is a check that fails message sending if we are not a group member
    ASSERT_LT(pluginInfo().chat_send(connection, purpleChatId, "message", PURPLE_MESSAGE_SEND), 0);

    g_list_free_full(actions, (GDestroyNotify)purple_menu_action_free);
}
