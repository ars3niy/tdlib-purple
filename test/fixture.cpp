#include "fixture.h"
#include "tdlib-purple.h"
#include "libpurple-mock.h"

CommTest::CommTest()
{
    tgprpl_set_test_backend(&tgl);
    tgprpl_set_single_thread();
    purple_init_plugin(&purplePlugin);
    purplePlugin.info->load(&purplePlugin);
}

void CommTest::SetUp()
{
    account = purple_account_new(("+" + selfPhoneNumber).c_str(), NULL);
    connection = new PurpleConnection;
    connection->state = PURPLE_DISCONNECTED;
    connection->account = account;
    account->gc = connection;
    prpl.discardEvents();
    setUiName("Pidgin");
}

void CommTest::TearDown()
{
    tgl.verifyNoRequests();
    prpl.verifyNoEvents();
    if (purple_connection_get_protocol_data(connection))
        pluginInfo().close(connection);
    delete connection;
    account->gc = NULL;
    tgl.runTimeouts();
    purple_account_destroy(account);
    account = NULL;
    clearFakeFiles();
}

void CommTest::login(std::initializer_list<object_ptr<Object>> extraUpdates, object_ptr<users> getContactsReply,
                     object_ptr<chats> getChatsReply,
                     std::initializer_list<std::unique_ptr<PurpleEvent>> postUpdateEvents,
                     std::initializer_list<object_ptr<Function>> postUpdateRequests,
                     std::initializer_list<std::unique_ptr<PurpleEvent>> postLoginEvents)
{
    pluginInfo().login(account);

    tgl.update(make_object<updateAuthorizationState>(make_object<authorizationStateWaitTdlibParameters>()));
    tgl.verifyRequests({
        make_object<disableProxy>(),
        make_object<getProxies>(),
        make_object<setTdlibParameters>(make_object<tdlibParameters>(
            false,
            std::string(purple_user_dir()) + G_DIR_SEPARATOR_S +
            "tdlib" + G_DIR_SEPARATOR_S + "+" + selfPhoneNumber,
            "",
            false,
            false,
            false,
            false,
            0,
            "",
            "",
            "",
            "",
            "",
            false,
            false
        ))
    });
    tgl.reply(make_object<ok>());

    // TODO: what if is_encrypted = false?
    tgl.update(make_object<updateAuthorizationState>(make_object<authorizationStateWaitEncryptionKey>(true)));
    tgl.verifyRequest(checkDatabaseEncryptionKey(""));
    tgl.reply(make_object<ok>());

    tgl.update(make_object<updateAuthorizationState>(make_object<authorizationStateReady>()));
    tgl.verifyNoRequests();

    tgl.update(make_object<updateConnectionState>(make_object<connectionStateConnecting>()));
    tgl.verifyNoRequests();
    prpl.verifyEvents(
        ConnectionSetStateEvent(connection, PURPLE_CONNECTING),
        ConnectionUpdateProgressEvent(connection, 1, 3)
    );

    tgl.update(make_object<updateConnectionState>(make_object<connectionStateUpdating>()));
    tgl.verifyNoRequests();
    prpl.verifyEvents(ConnectionUpdateProgressEvent(connection, 2, 3));

    tgl.update(make_object<updateUser>(makeUser(
        selfId,
        selfFirstName,
        selfLastName,
        selfPhoneNumber, // Phone number here without + to make it more interesting
        make_object<userStatusOffline>()
    )));
    for (const object_ptr<Object> &update: extraUpdates)
        tgl.update(std::move(const_cast<object_ptr<Object> &>(update))); // Take that!
    prpl.verifyEvents2(std::move(postUpdateEvents));
    tgl.verifyRequests(std::move(postUpdateRequests));

    tgl.update(make_object<updateConnectionState>(make_object<connectionStateReady>()));
    tgl.verifyRequest(getContacts());

    tgl.reply(std::move(getContactsReply));

    tgl.verifyRequest(getChats());
    bool hasChats = !getChatsReply->chat_ids_.empty();
    tgl.reply(std::move(getChatsReply));
    if (hasChats) {
        tgl.verifyRequest(getChats());
        tgl.reply(make_object<chats>());
    }

    if ((postLoginEvents.size() == 1) && (*postLoginEvents.begin() == nullptr))
        prpl.verifyEvents(
            ConnectionSetStateEvent(connection, PURPLE_CONNECTED),
            AccountSetAliasEvent(account, selfFirstName + " " + selfLastName),
            ShowAccountEvent(account)
        );
    else
        prpl.verifyEvents2(std::move(postLoginEvents));
}

void CommTest::loginWithOneContact()
{
    login(
        {standardUpdateUser(0), standardPrivateChat(0), makeUpdateChatListMain(chatIds[0])},
        make_object<users>(1, std::vector<int32_t>(1, userIds[0])),
        make_object<chats>(std::vector<int64_t>(1, chatIds[0])),
        {}, {},
        {
            std::make_unique<ConnectionSetStateEvent>(connection, PURPLE_CONNECTED),
            std::make_unique<AddBuddyEvent>(purpleUserName(0), userFirstNames[0] + " " + userLastNames[0],
                                            account, nullptr, nullptr, nullptr),
            std::make_unique<UserStatusEvent>(account, purpleUserName(0), PURPLE_STATUS_OFFLINE),
            std::make_unique<AccountSetAliasEvent>(account, selfFirstName + " " + selfLastName),
            std::make_unique<ShowAccountEvent>(account)
        }
    );
}

object_ptr<updateUser> CommTest::standardUpdateUser(unsigned index)
{
    return make_object<updateUser>(makeUser(
        userIds[index],
        userFirstNames[index],
        userLastNames[index],
        userPhones[index],
        make_object<userStatusOffline>()
    ));
}

object_ptr<updateUser> CommTest::standardUpdateUserNoPhone(unsigned index)
{
    return make_object<updateUser>(makeUser(
        userIds[index],
        userFirstNames[index],
        userLastNames[index],
        "",
        make_object<userStatusOffline>()
    ));
}

object_ptr<updateNewChat> CommTest::standardPrivateChat(unsigned index, object_ptr<ChatList> chatList)
{
    object_ptr<chat> chat = makeChat(
        chatIds[0],
        make_object<chatTypePrivate>(userIds[0]),
        userFirstNames[0] + " " + userLastNames[0],
        nullptr, 0, 0, 0
    );
    chat->chat_list_ = std::move(chatList);
    return make_object<updateNewChat>(std::move(chat));
}

PurplePluginProtocolInfo &CommTest::pluginInfo()
{
    return *(PurplePluginProtocolInfo *)purplePlugin.info->extra_info;
}

void checkFile(const char *filename, void *content, unsigned size)
{
    gchar *actualContent;
    gsize  actualSize;
    ASSERT_TRUE(g_file_get_contents(filename, &actualContent, &actualSize, NULL)) << filename << " does not exist";
    ASSERT_EQ(actualSize, size) << "Wrong file size for " << filename;
    ASSERT_EQ(0, memcmp(content, actualContent, size)) << "Wrong content for " << filename;
    g_free(actualContent);
}
