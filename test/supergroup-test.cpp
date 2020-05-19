#include "fixture.h"
#include <glib/gstrfuncs.h>

class SupergroupTest: public CommTest {
protected:
    const int32_t     groupId             = 700;
    const int64_t     groupChatId         = -7000;
    const std::string groupChatTitle      = "Title";
    const std::string groupChatPurpleName = "chat" + std::to_string(groupChatId);
};

TEST_F(SupergroupTest, AddSupergroupChatAtLogin)
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
}
