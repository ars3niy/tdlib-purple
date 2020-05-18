#include "fixture.h"

class LoginTest: public CommTest {};

TEST_F(LoginTest, Login)
{
    login();
}

TEST_F(LoginTest, ConnectionReadyBeforeAuthReady)
{
    pluginInfo().login(account);

    tgl.update(make_object<updateAuthorizationState>(make_object<authorizationStateWaitTdlibParameters>()));
    tgl.verifyRequest(setTdlibParameters(make_object<tdlibParameters>(
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
    )));
    tgl.update(make_object<updateAuthorizationState>(make_object<authorizationStateWaitEncryptionKey>(true)));
    tgl.reply(make_object<ok>());

    // TODO: what if is_encrypted = false?
    tgl.verifyRequest(checkDatabaseEncryptionKey(""));
    tgl.update(make_object<updateAuthorizationState>(make_object<authorizationStateWaitPhoneNumber>()));
    tgl.reply(make_object<ok>());

    tgl.verifyRequest(setAuthenticationPhoneNumber("+" + selfPhoneNumber, nullptr));
    tgl.update(make_object<updateConnectionState>(make_object<connectionStateConnecting>()));
    prpl.verifyEvents(
        ConnectionSetStateEvent(connection, PURPLE_CONNECTING),
        ConnectionUpdateProgressEvent(connection, 1, 3)
    );

    tgl.update(make_object<updateConnectionState>(make_object<connectionStateReady>()));
    tgl.verifyNoRequests();

    tgl.update(make_object<updateAuthorizationState>(make_object<authorizationStateReady>()));
    tgl.reply(make_object<ok>());

    tgl.verifyRequest(getContacts());
    tgl.update(make_object<updateUser>(makeUser(
        selfId,
        selfFirstName,
        selfLastName,
        selfPhoneNumber, // Phone number here without + to make it more interesting
        make_object<userStatusOffline>()
    )));
    tgl.reply(make_object<users>());

    tgl.verifyRequest(getChats());
    prpl.verifyNoEvents();
    tgl.reply(make_object<chats>());

    prpl.verifyEvents(
        ConnectionSetStateEvent(connection, PURPLE_CONNECTED),
        AccountSetAliasEvent(account, selfFirstName + " " + selfLastName),
        ShowAccountEvent(account)
    );
}

TEST_F(LoginTest, RegisterNewAccount_ConnectionReadyBeforeAuthReady)
{
    purple_account_set_alias(account, (selfFirstName + " " + selfLastName).c_str());
    prpl.discardEvents();
    pluginInfo().login(account);

    tgl.update(make_object<updateAuthorizationState>(make_object<authorizationStateWaitTdlibParameters>()));
    tgl.verifyRequest(setTdlibParameters(make_object<tdlibParameters>(
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
    )));
    tgl.update(make_object<updateAuthorizationState>(make_object<authorizationStateWaitEncryptionKey>(true)));
    tgl.reply(make_object<ok>());

    // TODO: what if is_encrypted = false?
    tgl.verifyRequest(checkDatabaseEncryptionKey(""));
    tgl.update(make_object<updateAuthorizationState>(make_object<authorizationStateWaitPhoneNumber>()));
    tgl.reply(make_object<ok>());

    tgl.verifyRequest(setAuthenticationPhoneNumber("+" + selfPhoneNumber, nullptr));

    tgl.update(make_object<updateAuthorizationState>(
        make_object<authorizationStateWaitCode>(
            make_object<authenticationCodeInfo>(
                selfPhoneNumber,
                make_object<authenticationCodeTypeTelegramMessage>(5),
                make_object<authenticationCodeTypeSms>(5),
                1800
            )
        )
    ));

    prpl.verifyEvents(RequestInputEvent(connection, account, NULL, NULL));
    prpl.inputEnter("12345");
    tgl.verifyRequest(checkAuthenticationCode("12345"));

    tgl.update(make_object<updateConnectionState>(make_object<connectionStateConnecting>()));
    prpl.verifyEvents(
        ConnectionSetStateEvent(connection, PURPLE_CONNECTING),
        ConnectionUpdateProgressEvent(connection, 1, 3)
    );

    tgl.update(make_object<updateConnectionState>(make_object<connectionStateReady>()));

    tgl.update(make_object<updateAuthorizationState>(
        make_object<authorizationStateWaitRegistration>(
            make_object<termsOfService>(
                make_object<formattedText>(
                    "Terms of service",
                    std::vector<object_ptr<textEntity>>()
                ),
                0, false
            )
        )
    ));
    tgl.reply(make_object<ok>());

    tgl.verifyRequest(registerUser(selfFirstName, selfLastName));
    prpl.verifyNoEvents();

    tgl.update(make_object<updateAuthorizationState>(make_object<authorizationStateReady>()));
    tgl.reply(make_object<ok>());

    tgl.verifyRequest(getContacts());
    tgl.update(make_object<updateUser>(makeUser(
        selfId,
        selfFirstName,
        selfLastName,
        selfPhoneNumber, // Phone number here without + to make it more interesting
        make_object<userStatusOffline>()
    )));
    tgl.reply(make_object<users>());

    tgl.verifyRequest(getChats());
    prpl.verifyNoEvents();
    tgl.reply(make_object<chats>());

    prpl.verifyEvents(
        ConnectionSetStateEvent(connection, PURPLE_CONNECTED),
        AccountSetAliasEvent(account, selfFirstName + " " + selfLastName),
        ShowAccountEvent(account)
    );
}
