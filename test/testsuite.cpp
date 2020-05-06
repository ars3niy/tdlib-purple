#include "test-transceiver.h"
#include "tdlib-purple.h"
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

    TestTransceiver   tgl;
    PurplePlugin      purplePlugin;
    PurpleAccount    *account;
    PurpleConnection *connection;

protected:
    void SetUp() override;
    void TearDown() override;
    void login();
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
    if (purple_connection_get_protocol_data(connection))
        ((PurplePluginProtocolInfo *)purplePlugin.info->extra_info)->close(connection);
    delete connection;
    purple_account_destroy(account);
}

void CommTest::login()
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
    // TODO: verify purple_connection_set_state and purple_connection_update_progress

    tgl.update(make_object<updateConnectionState>(make_object<connectionStateUpdating>()));
    tgl.verifyNoRequests();
    // TODO: verify purple_connection_update_progress

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
    tgl.verifyNoRequests();
    // TODO: test sending some users and chats
    tgl.reply(make_object<users>());

    tgl.verifyRequest(getChats());
    // TODO: test sending some chats
    tgl.reply(make_object<chats>());
    // TODO: verfy purple_account_set_alias
}

TEST_F(CommTest, login)
{
    login();
}
