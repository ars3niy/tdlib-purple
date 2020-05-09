#include "fixture.h"

class GroupChatTest: public CommTest {};

TEST_F(GroupChatTest, DISABLED_AddBasicGroupChatAtLogin)
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
