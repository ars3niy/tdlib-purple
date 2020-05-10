#include "fixture.h"
#include "tdlib-purple.h"

CommTest::CommTest()
{
    tgprpl_set_test_backend(&tgl);
    purple_init_plugin(&purplePlugin);
}

void CommTest::SetUp()
{
    account = purple_account_new(("+" + phoneNumber).c_str(), NULL);
    connection = new PurpleConnection;
    connection->state = PURPLE_DISCONNECTED;
    connection->account = account;
    account->gc = connection;
    prpl.discardEvents();
}

void CommTest::TearDown()
{
    tgl.verifyNoRequests();
    prpl.verifyNoEvents();
    if (purple_connection_get_protocol_data(connection))
        ((PurplePluginProtocolInfo *)purplePlugin.info->extra_info)->close(connection);
    delete connection;
    purple_account_destroy(account);
}

void CommTest::login(std::initializer_list<object_ptr<Object>> extraUpdates, object_ptr<users> getContactsReply,
                     object_ptr<chats> getChatsReply,
                     std::initializer_list<std::unique_ptr<PurpleEvent>> postUpdateEvents,
                     std::initializer_list<std::unique_ptr<PurpleEvent>> postLoginEvents,
                     std::initializer_list<object_ptr<Function>> postUpdateRequests)
{
    ((PurplePluginProtocolInfo *)purplePlugin.info->extra_info)->login(account);

    tgl.update(make_object<updateAuthorizationState>(make_object<authorizationStateWaitTdlibParameters>()));
    tgl.verifyRequest(setTdlibParameters(make_object<tdlibParameters>(
        false,
        std::string(purple_user_dir()) + G_DIR_SEPARATOR_S +
        "tdlib" + G_DIR_SEPARATOR_S + "+" + phoneNumber,
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
    )));
    tgl.reply(make_object<ok>());

    // TODO: what if is_encrypted = false?
    tgl.update(make_object<updateAuthorizationState>(make_object<authorizationStateWaitEncryptionKey>(true)));
    tgl.verifyRequest(checkDatabaseEncryptionKey(""));
    tgl.reply(make_object<ok>());

    tgl.update(make_object<updateAuthorizationState>(make_object<authorizationStateWaitPhoneNumber>()));
    tgl.verifyRequest(setAuthenticationPhoneNumber("+" + phoneNumber, nullptr));
    tgl.reply(make_object<ok>());

    tgl.update(make_object<updateAuthorizationState>(make_object<authorizationStateReady>()));
    tgl.verifyNoRequests();

    tgl.update(make_object<updateConnectionState>(make_object<connectionStateConnecting>()));
    tgl.verifyNoRequests();
    prpl.verifyEvents({
        std::make_unique<ConnectionSetStateEvent>(connection, PURPLE_CONNECTING),
        std::make_unique<ConnectionUpdateProgressEvent>(connection, 1, 3)
    });

    tgl.update(make_object<updateConnectionState>(make_object<connectionStateUpdating>()));
    tgl.verifyNoRequests();
    prpl.verifyEvent(ConnectionUpdateProgressEvent(connection, 2, 3));

    tgl.update(make_object<updateUser>(makeUser(
        selfId,
        selfFirstName,
        selfLastName,
        phoneNumber, // Phone number here without + to make it more interesting
        make_object<userStatusOffline>()
    )));
    for (const object_ptr<Object> &update: extraUpdates)
        tgl.update(std::move(const_cast<object_ptr<Object> &>(update))); // Take that!
    prpl.verifyEvents(std::move(postUpdateEvents));
    tgl.verifyRequests(std::move(postUpdateRequests));

    tgl.update(make_object<updateConnectionState>(make_object<connectionStateReady>()));
    tgl.verifyRequest(getContacts());

    tgl.reply(std::move(getContactsReply));

    tgl.verifyRequest(getChats());

    tgl.reply(std::move(getChatsReply));
    if (postLoginEvents.size() != 0)
        prpl.verifyEvents(std::move(postLoginEvents));
    else
        prpl.verifyEvents({
            std::make_unique<ConnectionSetStateEvent>(connection, PURPLE_CONNECTED),
            std::make_unique<AccountSetAliasEvent>(account, selfFirstName + " " + selfLastName),
            std::make_unique<ShowAccountEvent>(account)
        });
}

void CommTest::loginWithOneContact()
{
    login(
        {standardUpdateUser(0), standardPrivateChat(0)},
        make_object<users>(1, std::vector<int32_t>(1, userIds[0])),
        make_object<chats>(std::vector<int64_t>(1, chatIds[0])),
        {},
        {
            std::make_unique<ConnectionSetStateEvent>(connection, PURPLE_CONNECTED),
            std::make_unique<AddBuddyEvent>(userPhones[0], userFirstNames[0] + " " + userLastNames[0],
                                            account, nullptr, nullptr, nullptr),
            std::make_unique<UserStatusEvent>(account, userPhones[0], PURPLE_STATUS_OFFLINE),
            std::make_unique<AccountSetAliasEvent>(account, selfFirstName + " " + selfLastName),
            std::make_unique<ShowAccountEvent>(account)
        }
    );
}

object_ptr<updateUser> CommTest::standardUpdateUser(unsigned index)
{
    return make_object<updateUser>(makeUser(
        userIds[0],
        userFirstNames[0],
        userLastNames[0],
        userPhones[0],
        make_object<userStatusOffline>()
    ));
}

object_ptr<updateNewChat> CommTest::standardPrivateChat(unsigned index)
{
    return make_object<updateNewChat>(makeChat(
        chatIds[0],
        make_object<chatTypePrivate>(userIds[0]),
        userFirstNames[0] + " " + userLastNames[0],
        nullptr, 0, 0, 0
    ));
}
