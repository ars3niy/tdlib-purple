#include "test-transceiver.h"
#include "tdlib-purple.h"
#include "purple-events.h"
#include <gtest/gtest.h>

using namespace td::td_api;

class CommTest: public testing::Test {
public:
    CommTest();

private:
    const std::string phoneNumber   = "1234567";
    const int         selfId        = 1;
    const std::string selfFirstName = "Isaac";
    const std::string selfLastName  = "Newton";

    PurplePlugin      purplePlugin;
    PurpleAccount    *account;
    PurpleConnection *connection;

protected:
    TestTransceiver      tgl;
    PurpleEventReceiver &prpl = g_purpleEvents;

    void SetUp() override;
    void TearDown() override;
    void login(std::vector<object_ptr<Object>> extraUpdates, object_ptr<users> getContactsReply,
               object_ptr<chats> getChatsReply);
};

CommTest::CommTest()
{
    tgprpl_set_test_backend(&tgl);
    purple_init_plugin(&purplePlugin);
}

void CommTest::SetUp()
{
    account = purple_account_new(("+" + phoneNumber).c_str(), NULL);
    connection = new PurpleConnection;
    account->gc = connection;
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

void CommTest::login(std::vector<object_ptr<Object>> extraUpdates, object_ptr<users> getContactsReply,
                     object_ptr<chats> getChatsReply)
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

    tgl.update(make_object<updateConnectionState>(make_object<connectionStateReady>()));
    tgl.verifyRequest(getContacts());

    tgl.update(make_object<updateUser>(make_object<user>(
        selfId,
        selfFirstName,
        selfLastName,
        "",
        phoneNumber, // Phone number here without + to make it more interesting
        make_object<userStatusOffline>(),
        nullptr,
        false,
        false,
        false,
        false,
        "",
        false,
        true,
        make_object<userTypeRegular>(),
        ""
    )));
    for (object_ptr<Object> &update: extraUpdates)
        tgl.update(std::move(update));
    tgl.verifyNoRequests();
    tgl.reply(std::move(getContactsReply));

    tgl.verifyRequest(getChats());
    prpl.verifyNoEvents();
    tgl.reply(std::move(getChatsReply));
    prpl.verifyEvents({
        std::make_unique<ConnectionSetStateEvent>(connection, PURPLE_CONNECTED),
        std::make_unique<AccountSetAliasEvent>(account, selfFirstName + " " + selfLastName),
        std::make_unique<ShowAccountEvent>(account)
    });
}

TEST_F(CommTest, login)
{
    login({}, make_object<users>(), make_object<chats>());
}
