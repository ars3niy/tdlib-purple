#include "fixture.h"
#include <glib/gstrfuncs.h>

class GroupChatTest: public CommTest {};

TEST_F(GroupChatTest, AddBasicGroupChatAtLogin)
{
    constexpr int32_t groupId   = 700;
    constexpr int64_t chatId    = 7000;
    const std::string chatTitle = "Title";

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
        make_object<chats>(std::vector<int64_t>(1, chatId)),
        {},
        {
            std::make_unique<ConnectionSetStateEvent>(connection, PURPLE_CONNECTED),
            std::make_unique<AddChatEvent>(
                std::to_string(chatId), chatTitle, account, nullptr, nullptr
            ),
            std::make_unique<AccountSetAliasEvent>(account, selfFirstName + " " + selfLastName),
            std::make_unique<ShowAccountEvent>(account)
        }
    );
}

TEST_F(GroupChatTest, AddBasicGroupChat)
{
    constexpr int32_t groupId   = 700;
    constexpr int64_t chatId    = 7000;
    const std::string chatTitle = "Title";

    login();
    tgl.update(make_object<updateBasicGroup>(make_object<basicGroup>(
        groupId, 2, make_object<chatMemberStatusMember>(), true, 0
    )));
    tgl.update(make_object<updateNewChat>(makeChat(
        chatId, make_object<chatTypeBasicGroup>(groupId), chatTitle, nullptr, 0, 0, 0
    )));

    prpl.verifyEvent(AddChatEvent(
        std::to_string(chatId), chatTitle, account, NULL, NULL
    ));
}

TEST_F(GroupChatTest, ExistingGroupChatAtLogin)
{
    constexpr int32_t groupId   = 700;
    constexpr int64_t chatId    = 7000;
    const std::string chatTitle = "Title";

    GHashTable *table = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
    g_hash_table_insert(table, (char *)"id", g_strdup(std::to_string(chatId).c_str()));
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
