#include "fixture.h"

class LoginTest: public CommTest {};

TEST_F(LoginTest, Login)
{
    login();
}

TEST_F(LoginTest, ConnectionReadyBeforeAuthReady)
{
    pluginInfo().login(account);
    prpl.verifyEvents(
        ConnectionSetStateEvent(connection, PURPLE_CONNECTING),
        ConnectionUpdateProgressEvent(connection, 1, 2)
    );

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
            true, // use secret chats
            0,
            "",
            "",
            "",
            "",
            "",
            true,
            false
        ))
    });
    tgl.update(make_object<updateAuthorizationState>(make_object<authorizationStateWaitEncryptionKey>(true)));
    tgl.reply(make_object<ok>());

    // TODO: what if is_encrypted = false?
    tgl.verifyRequest(checkDatabaseEncryptionKey(""));
    tgl.update(make_object<updateAuthorizationState>(make_object<authorizationStateWaitPhoneNumber>()));
    tgl.reply(make_object<ok>());

    tgl.verifyRequest(setAuthenticationPhoneNumber("+" + selfPhoneNumber, nullptr));
    tgl.update(make_object<updateConnectionState>(make_object<connectionStateConnecting>()));
    tgl.update(make_object<updateConnectionState>(make_object<connectionStateReady>()));
    tgl.verifyNoRequests();
    prpl.verifyNoEvents();

    tgl.update(make_object<updateAuthorizationState>(make_object<authorizationStateReady>()));
    prpl.verifyEvents(ConnectionSetStateEvent(connection, PURPLE_CONNECTED));
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

    tgl.verifyRequest(getChatsRequest());
    prpl.verifyNoEvents();
    tgl.reply(getChatsNoChatsResponse());

    prpl.verifyEvents(
        AccountSetAliasEvent(account, selfFirstName + " " + selfLastName),
        ShowAccountEvent(account)
    );
}

TEST_F(LoginTest, RegisterNewAccount_WithAlias_ConnectionReadyBeforeAuthReady)
{
    purple_account_set_alias(account, (selfFirstName + " " + selfLastName).c_str());
    prpl.discardEvents();
    pluginInfo().login(account);
    prpl.verifyEvents(
        ConnectionSetStateEvent(connection, PURPLE_CONNECTING),
        ConnectionUpdateProgressEvent(connection, 1, 2)
    );

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
            true, // use secret chats
            0,
            "",
            "",
            "",
            "",
            "",
            true,
            false
        ))
    });
    tgl.update(make_object<updateAuthorizationState>(make_object<authorizationStateWaitEncryptionKey>(true)));
    tgl.reply(make_object<ok>());

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
    prpl.verifyEvents(ConnectionSetStateEvent(connection, PURPLE_CONNECTED));
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

    tgl.verifyRequest(getChatsRequest());
    prpl.verifyNoEvents();
    tgl.reply(getChatsNoChatsResponse());

    prpl.verifyEvents(
        AccountSetAliasEvent(account, selfFirstName + " " + selfLastName),
        ShowAccountEvent(account)
    );
}

TEST_F(LoginTest, RegisterNewAccount_NoAlias)
{
    pluginInfo().login(account);
    prpl.verifyEvents(
        ConnectionSetStateEvent(connection, PURPLE_CONNECTING),
        ConnectionUpdateProgressEvent(connection, 1, 2)
    );

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
            true, // use secret chats
            0,
            "",
            "",
            "",
            "",
            "",
            true,
            false
        ))
    });
    tgl.update(make_object<updateAuthorizationState>(make_object<authorizationStateWaitEncryptionKey>(true)));
    tgl.reply(make_object<ok>());

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

    tgl.verifyNoRequests();
    prpl.verifyEvents(RequestInputEvent(connection, account, NULL, NULL));

    prpl.inputEnter((selfFirstName + "     " + selfLastName).c_str());
    tgl.verifyRequest(registerUser(selfFirstName, selfLastName));

    tgl.update(make_object<updateAuthorizationState>(make_object<authorizationStateReady>()));
    prpl.verifyEvents(ConnectionSetStateEvent(connection, PURPLE_CONNECTED));
    tgl.reply(make_object<ok>());
    tgl.update(make_object<updateConnectionState>(make_object<connectionStateReady>()));
    tgl.verifyRequest(getContacts());
}

TEST_F(LoginTest, TwoFactorAuthentication)
{
    purple_account_set_alias(account, (selfFirstName + " " + selfLastName).c_str());
    prpl.discardEvents();
    pluginInfo().login(account);
    prpl.verifyEvents(
        ConnectionSetStateEvent(connection, PURPLE_CONNECTING),
        ConnectionUpdateProgressEvent(connection, 1, 2)
    );

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
            true, // use secret chats
            0,
            "",
            "",
            "",
            "",
            "",
            true,
            false
        ))
    });
    tgl.update(make_object<updateAuthorizationState>(make_object<authorizationStateWaitEncryptionKey>(true)));
    tgl.reply(make_object<ok>());

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
    tgl.update(make_object<updateConnectionState>(make_object<connectionStateReady>()));
    prpl.verifyNoEvents();

    tgl.update(make_object<updateAuthorizationState>(make_object<authorizationStateWaitPassword>(
        "hint", true, "user@example.com"
    )));
    tgl.reply(make_object<ok>());

    prpl.verifyEvents(RequestInputEvent(connection, account, NULL, NULL));
    prpl.inputEnter("password");
    tgl.verifyRequest(checkAuthenticationPassword("password"));

    tgl.update(make_object<updateAuthorizationState>(make_object<authorizationStateReady>()));
    prpl.verifyEvents(ConnectionSetStateEvent(connection, PURPLE_CONNECTED));
    tgl.reply(make_object<ok>());
    tgl.verifyRequest(getContacts());
}

TEST_F(LoginTest, RenameBuddyAtConnect)
{
    purple_blist_add_buddy(purple_buddy_new(account, purpleUserName(0).c_str(), "whatever"), NULL,
                           &standardPurpleGroup, NULL);
    prpl.discardEvents();

    login(
        {standardUpdateUser(0), standardPrivateChat(0), makeUpdateChatListMain(chatIds[0])},
        make_object<users>(1, std::vector<int32_t>(1, userIds[0])),
        make_object<chats>(std::vector<int64_t>(1, chatIds[0])),
        {
            std::make_unique<AliasBuddyEvent>(purpleUserName(0), userFirstNames[0] + " " + userLastNames[0]),
        }, {},
        {
            std::make_unique<UserStatusEvent>(account, purpleUserName(0), PURPLE_STATUS_AWAY),
            std::make_unique<AccountSetAliasEvent>(account, selfFirstName + " " + selfLastName),
            std::make_unique<ShowAccountEvent>(account)
        }
    );
}

TEST_F(LoginTest, RenameBuddy)
{
    loginWithOneContact();

    purple_blist_alias_buddy(purple_find_buddy(account, purpleUserName(0).c_str()), "New Name");
    prpl.discardEvents();
    pluginInfo().alias_buddy(connection, purpleUserName(0).c_str(), "New Name");

    tgl.verifyRequest(addContact(make_object<contact>(
        "", "New", "Name", "", userIds[0]
    ), true));

    tgl.update(make_object<updateChatTitle>(chatIds[0], "New Name"));
    object_ptr<updateUser> updateUser = standardUpdateUser(0);
    updateUser->user_->first_name_ = "New";
    updateUser->user_->last_name_ = "Name";
    tgl.update(std::move(updateUser));
}

TEST_F(LoginTest, BuddyRenamedByServer)
{
    loginWithOneContact();

    tgl.update(make_object<updateChatTitle>(chatIds[0], "New Name"));
    prpl.verifyEvents(AliasBuddyEvent(purpleUserName(0), "New Name"));

    object_ptr<updateUser> updateUser = standardUpdateUser(0);
    updateUser->user_->first_name_ = "New";
    updateUser->user_->last_name_ = "Name";
    tgl.update(std::move(updateUser));
}

TEST_F(LoginTest, AddedProxyCofiguration)
{
    char host[] = "host";
    const int port = 10;
    char username[] = "username";
    char password[] = "password";
    PurpleProxyInfo purpleProxy;
    purpleProxy.type = PURPLE_PROXY_SOCKS5;
    purpleProxy.host = host;
    purpleProxy.port = port;
    purpleProxy.username = username;
    purpleProxy.password = password;
    account->proxy_info = &purpleProxy;

    pluginInfo().login(account);
    prpl.verifyEvents(
        ConnectionSetStateEvent(connection, PURPLE_CONNECTING),
        ConnectionUpdateProgressEvent(connection, 1, 2)
    );

    tgl.update(make_object<updateAuthorizationState>(make_object<authorizationStateWaitTdlibParameters>()));
    tgl.verifyRequests({
        make_object<disableProxy>(),
        make_object<addProxy>(host, port, true, make_object<proxyTypeSocks5>(username, password)),
        make_object<getProxies>(),
        make_object<setTdlibParameters>(make_object<tdlibParameters>(
            false,
            std::string(purple_user_dir()) + G_DIR_SEPARATOR_S +
            "tdlib" + G_DIR_SEPARATOR_S + "+" + selfPhoneNumber,
            "",
            false,
            false,
            false,
            true, // use secret chats
            0,
            "",
            "",
            "",
            "",
            "",
            true,
            false
        ))
    });

    tgl.reply(make_object<ok>()); // reply to disableProxy
    tgl.reply(make_object<proxy>(2, "", 0, 0, false, nullptr));
    std::vector<object_ptr<proxy>> proxyList;
    proxyList.push_back(make_object<proxy>(2, "", 0, 0, true, nullptr));
    tgl.reply(make_object<proxies>(std::move(proxyList)));
    tgl.reply(make_object<ok>());
}

TEST_F(LoginTest, ChangedProxyCofiguration)
{
    char host[] = "host";
    const int port = 10;
    char username[] = "username";
    char password[] = "password";
    PurpleProxyInfo purpleProxy;
    purpleProxy.type = PURPLE_PROXY_SOCKS5;
    purpleProxy.host = host;
    purpleProxy.port = port;
    purpleProxy.username = username;
    purpleProxy.password = password;
    account->proxy_info = &purpleProxy;

    pluginInfo().login(account);
    prpl.verifyEvents(
        ConnectionSetStateEvent(connection, PURPLE_CONNECTING),
        ConnectionUpdateProgressEvent(connection, 1, 2)
    );

    tgl.update(make_object<updateAuthorizationState>(make_object<authorizationStateWaitTdlibParameters>()));
    tgl.verifyRequests({
        make_object<disableProxy>(),
        make_object<addProxy>(host, port, true, make_object<proxyTypeSocks5>(username, password)),
        make_object<getProxies>(),
        make_object<setTdlibParameters>(make_object<tdlibParameters>(
            false,
            std::string(purple_user_dir()) + G_DIR_SEPARATOR_S +
            "tdlib" + G_DIR_SEPARATOR_S + "+" + selfPhoneNumber,
            "",
            false,
            false,
            false,
            true, // use secret chats
            0,
            "",
            "",
            "",
            "",
            "",
            true,
            false
        ))
    });

    tgl.reply(make_object<ok>()); // reply to disableProxy
    tgl.reply(make_object<proxy>(2, "", 0, 0, false, nullptr));
    std::vector<object_ptr<proxy>> proxyList;
    proxyList.push_back(make_object<proxy>(1, "", 0, 0, false, nullptr));
    proxyList.push_back(make_object<proxy>(2, "", 0, 0, true, nullptr));
    tgl.reply(make_object<proxies>(std::move(proxyList)));
    tgl.reply(make_object<ok>());

    tgl.verifyRequest(removeProxy(1));
}

TEST_F(LoginTest, RemovedProxyCofiguration)
{
    pluginInfo().login(account);
    prpl.verifyEvents(
        ConnectionSetStateEvent(connection, PURPLE_CONNECTING),
        ConnectionUpdateProgressEvent(connection, 1, 2)
    );

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
            true, // use secret chats
            0,
            "",
            "",
            "",
            "",
            "",
            true,
            false
        ))
    });

    tgl.reply(make_object<ok>()); // reply to disableProxy
    std::vector<object_ptr<proxy>> proxyList;
    proxyList.push_back(make_object<proxy>(1, "", 0, 0, false, nullptr));
    tgl.reply(make_object<proxies>(std::move(proxyList)));
    tgl.reply(make_object<ok>());

    tgl.verifyRequest(removeProxy(1));
}

TEST_F(LoginTest, getChatsSequence)
{
    pluginInfo().login(account);
    prpl.verifyEvents(
        ConnectionSetStateEvent(connection, PURPLE_CONNECTING),
        ConnectionUpdateProgressEvent(connection, 1, 2)
    );

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
            true, // use secret chats
            0,
            "",
            "",
            "",
            "",
            "",
            true,
            false
        ))
    });
    tgl.update(make_object<updateAuthorizationState>(make_object<authorizationStateWaitEncryptionKey>(true)));
    tgl.reply(make_object<ok>());

    tgl.verifyRequest(checkDatabaseEncryptionKey(""));
    tgl.update(make_object<updateAuthorizationState>(make_object<authorizationStateWaitPhoneNumber>()));
    tgl.reply(make_object<ok>());

    tgl.verifyRequest(setAuthenticationPhoneNumber("+" + selfPhoneNumber, nullptr));
    tgl.update(make_object<updateConnectionState>(make_object<connectionStateConnecting>()));
    tgl.update(make_object<updateConnectionState>(make_object<connectionStateReady>()));
    tgl.verifyNoRequests();
    prpl.verifyNoEvents();

    tgl.update(make_object<updateAuthorizationState>(make_object<authorizationStateReady>()));
    prpl.verifyEvents(ConnectionSetStateEvent(connection, PURPLE_CONNECTED));
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

    tgl.verifyRequest(loadChats(make_object<chatListMain>(), 200));

    object_ptr<updateNewChat> chat1 = standardPrivateChat(0, make_object<chatListMain>());
    object_ptr<updateNewChat> chat2 = standardPrivateChat(1, make_object<chatListMain>());
    object_ptr<updateNewChat> chat3 = standardPrivateChat(1, make_object<chatListMain>());
    chat1->chat_->order_ = 10;
    chat2->chat_->order_ = 20;
    chat3->chat_->order_ = 30;
    chat3->chat_->id_ = chatIds[1]+1;
    tgl.update(std::move(chat1));
    tgl.update(std::move(chat2));
    tgl.update(std::move(chat3));
    tgl.update(make_object<updateChatPosition>(
        chatIds[1]+1, make_object<chatPosition>(
            make_object<chatListMain>(), 15, false, nullptr
        )
    ));
    tgl.update(make_object<updateChatPosition>(
        chatIds[0], make_object<chatPosition>(
            make_object<chatListArchive>(), 10, false, nullptr
        )
    ));
    tgl.reply(make_object<ok>());

    tgl.verifyRequest(getChatsRequest());
    tgl.update(standardPrivateChat(1));
    tgl.reply(getChatsNoChatsResponse());

    // updateUser were missing (not realistic though), so no buddies
    prpl.verifyEvents(
        AccountSetAliasEvent(account, selfFirstName + " " + selfLastName),
        ShowAccountEvent(account)
    );
}

TEST_F(LoginTest, KeepInlineDownloads)
{
    purple_account_set_bool(account, "keep-inline-downloads", TRUE);
    pluginInfo().login(account);
    prpl.verifyEvents(
        ConnectionSetStateEvent(connection, PURPLE_CONNECTING),
        ConnectionUpdateProgressEvent(connection, 1, 2)
    );

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
            true, // use secret chats
            0,
            "",
            "",
            "",
            "",
            "",
            false, // enable storage optimizer, false due to configuration
            false
        ))
    });
}

TEST_F(LoginTest, IncomingGroupChatMessageAtLoginWhileChatListStillNull)
{
    const int32_t     groupId             = 700;
    const int64_t     groupChatId         = 7000;
    const std::string groupChatTitle      = "Title";
    const std::string groupChatPurpleName = "chat" + std::to_string(groupChatId);
    constexpr int64_t messageId    = 10000;
    constexpr int32_t date         = 123456;
    constexpr int     purpleChatId = 1;

    GHashTable *components = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
    g_hash_table_insert(components, (char *)"id", g_strdup((groupChatPurpleName).c_str()));
    purple_blist_add_chat(purple_chat_new(account, groupChatTitle.c_str(), components), NULL, NULL);
    prpl.discardEvents();

    login(
        {
            standardUpdateUser(0),
            make_object<updateBasicGroup>(make_object<basicGroup>(
                groupId, 2, make_object<chatMemberStatusMember>(), true, 0
            )),
            make_object<updateNewChat>(makeChat(
                groupChatId, make_object<chatTypeBasicGroup>(groupId), groupChatTitle, nullptr, 0, 0, 0
            )),
            make_object<updateNewMessage>(makeMessage(
                messageId, userIds[0], groupChatId, false, date, makeTextMessage("text")
            )),
            makeUpdateChatListMain(groupChatId),
        },
        make_object<users>(), make_object<chats>(),
        {
            // Removal is unnecessary but nothing too bad is happening
            std::make_unique<RemoveChatEvent>(groupChatPurpleName, ""),
            std::make_unique<ServGotJoinedChatEvent>(connection, purpleChatId, groupChatPurpleName, groupChatPurpleName),
            std::make_unique<ConvSetTitleEvent>(groupChatPurpleName, groupChatTitle),
            std::make_unique<ServGotChatEvent>(connection, purpleChatId, userFirstNames[0] + " " + userLastNames[0],
                             "text", PURPLE_MESSAGE_RECV, date),
            std::make_unique<AddChatEvent>(groupChatPurpleName, groupChatTitle, account, nullptr, nullptr)
        },
        {
            make_object<viewMessages>(groupChatId, std::vector<int64_t>(1, messageId), true),
            make_object<getBasicGroupFullInfo>(groupId)
        }
    );
}
