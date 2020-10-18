#include "fixture.h"

class SecretChatTest: public CommTest {
protected:
    const int32_t secretChatId = 700;
    const int64_t secretChatChatId = 70000;
    const std::string secretChatTitle = "Secret Chat Title";

    const std::string secretChatBuddyName = "secret" + std::to_string(secretChatId);
    const std::string secretChatBuddyAlias = "Secret chat: " + secretChatTitle;

    object_ptr<chat> makeChatForSecret(int32_t userId);
    void loginWithSecretChat();
};

object_ptr<td::td_api::chat> SecretChatTest::makeChatForSecret(int32_t userId)
{
    return makeChat(secretChatChatId, make_object<chatTypeSecret>(secretChatId, userId),
                    secretChatTitle, nullptr, 0, 0, 0);
}

void SecretChatTest::loginWithSecretChat()
{
    loginWithOneContact();

    tgl.update(make_object<updateSecretChat>(make_object<secretChat>(
        secretChatId, userIds[0], make_object<secretChatStatePending>(), false, 60, "whatever", 0
    )));
    tgl.update(make_object<updateNewChat>(makeChatForSecret(userIds[0])));
    prpl.verifyEvents(
        AddBuddyEvent(secretChatBuddyName, secretChatBuddyAlias, account, NULL, NULL, NULL),
        UserStatusEvent(account, secretChatBuddyName, PURPLE_STATUS_OFFLINE)
    );

    tgl.update(make_object<updateSecretChat>(make_object<secretChat>(
        secretChatId, userIds[0], make_object<secretChatStateReady>(), false, 60, "whatever", 0
    )));
    prpl.verifyEvents(
        UserStatusEvent(account, secretChatBuddyName, PURPLE_STATUS_AVAILABLE)
    );
}

TEST_F(SecretChatTest, ReceiveMessage)
{
    const int32_t date = 123;
    loginWithSecretChat();

    tgl.update(make_object<updateNewMessage>(
        makeMessage(1, userIds[0], secretChatChatId, false, date, makeTextMessage("text"))
    ));
    tgl.verifyRequest(viewMessages(secretChatChatId, {1}, true));
    prpl.verifyEvents(ServGotImEvent(
        connection, secretChatBuddyName, "text", PURPLE_MESSAGE_RECV, date
    ));
}

TEST_F(SecretChatTest, CreateSecretChat_Lifecycle)
{
    loginWithOneContact();

    PurpleBuddy *buddy = purple_find_buddy(account, purpleUserName(0).c_str());
    ASSERT_NE(nullptr, buddy);
    GList *actions = pluginInfo().blist_node_menu(&buddy->node);
    nodeMenuAction(&buddy->node, actions, "Start secret chat");

    tgl.verifyRequest(createNewSecretChat(userIds[0]));
    tgl.update(make_object<updateSecretChat>(make_object<secretChat>(
        secretChatId, userIds[0], make_object<secretChatStatePending>(), true, 60, "whatever", 0
    )));
    tgl.update(make_object<updateNewChat>(makeChatForSecret(userIds[0])));
    prpl.verifyEvents(
        AddBuddyEvent(secretChatBuddyName, secretChatBuddyAlias, account, NULL, NULL, NULL),
        NewConversationEvent(PURPLE_CONV_TYPE_IM, account, secretChatBuddyName),
        ConversationWriteEvent(
            secretChatBuddyName, secretChatBuddyName,
            "The secret chat will be available when activated by the peer",
            PURPLE_MESSAGE_NO_LOG, 0
        ),
        UserStatusEvent(account, secretChatBuddyName, PURPLE_STATUS_OFFLINE)
    );

    tgl.update(make_object<updateSecretChat>(make_object<secretChat>(
        secretChatId, userIds[0], make_object<secretChatStateReady>(), false, 60, "whatever", 0
    )));
    prpl.verifyEvents(
        UserStatusEvent(account, secretChatBuddyName, PURPLE_STATUS_AVAILABLE)
    );

    purple_blist_remove_buddy(purple_find_buddy(account, secretChatBuddyName.c_str()));
    prpl.discardEvents();
    PurpleBuddy *dup = purple_buddy_new(account, secretChatBuddyName.c_str(),
                                        secretChatBuddyAlias.c_str());
    pluginInfo().remove_buddy(connection, dup, NULL);
    purple_buddy_destroy(dup);
    tgl.verifyRequests({
        make_object<deleteChatHistory>(secretChatChatId, true, false),
        make_object<closeSecretChat>(secretChatId)
    });
    tgl.update(make_object<updateSecretChat>(make_object<secretChat>(
        secretChatId, userIds[0], make_object<secretChatStateClosed>(), true, 60, "whatever", 0
    )));

    g_list_free_full(actions, (GDestroyNotify)purple_menu_action_free);
}

TEST_F(SecretChatTest, SecretChatsDisabled)
{
    purple_account_set_bool(account, "enable-secret-chats", FALSE);
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
            false, // use secret chats
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
}

TEST_F(SecretChatTest, SendMessage)
{
    const int32_t date = 123;
    loginWithSecretChat();

    ASSERT_EQ(0, pluginInfo().send_im(connection, secretChatBuddyName.c_str(), "message", PURPLE_MESSAGE_SEND));
    tgl.verifyRequest(sendMessage(
        secretChatChatId,
        0,
        nullptr,
        nullptr,
        make_object<inputMessageText>(
            make_object<formattedText>("message", std::vector<object_ptr<textEntity>>()),
            false, false
        )
    ));

    tgl.update(make_object<updateNewMessage>(
        makeMessage(1, userIds[0], secretChatChatId, true, date, makeTextMessage("message"))
    ));
    tgl.verifyRequest(viewMessages(secretChatChatId, {1}, true));
    prpl.verifyEvents(
        NewConversationEvent(PURPLE_CONV_TYPE_IM, account, secretChatBuddyName),
        ConversationWriteEvent(
            secretChatBuddyName, selfFirstName + " " + selfLastName, "message",
            PURPLE_MESSAGE_SEND, date
        )
    );
}

TEST_F(SecretChatTest, TypingNotification)
{
    loginWithSecretChat();

    pluginInfo().send_typing(connection, secretChatBuddyName.c_str(), PURPLE_TYPING);
    tgl.verifyRequest(sendChatAction(secretChatChatId, make_object<chatActionTyping>()));

    pluginInfo().send_typing(connection, secretChatBuddyName.c_str(), PURPLE_TYPED);
    tgl.verifyRequest(sendChatAction(secretChatChatId, make_object<chatActionCancel>()));

    pluginInfo().send_typing(connection, secretChatBuddyName.c_str(), PURPLE_TYPING);
    tgl.verifyRequest(sendChatAction(secretChatChatId, make_object<chatActionTyping>()));

    pluginInfo().send_typing(connection, secretChatBuddyName.c_str(), PURPLE_NOT_TYPING);
    tgl.verifyRequest(sendChatAction(secretChatChatId, make_object<chatActionCancel>()));
}

TEST_F(SecretChatTest, SendFile)
{
    const char *const PATH   = "/path";
    const int32_t     fileId = 1234;
    loginWithSecretChat();

    setFakeFileSize(PATH, 9000);
    pluginInfo().send_file(connection, secretChatBuddyName.c_str(), PATH);
    prpl.verifyEvents(XferAcceptedEvent(secretChatBuddyName, PATH));
    tgl.verifyRequest(uploadFile(
        make_object<inputFileLocal>(PATH),
        make_object<fileTypeDocument>(),
        1
    ));

    tgl.reply(make_object<file>(
        fileId, 10000, 10000,
        make_object<localFile>(PATH, false, false, false, true, 0, 10000, 10000),
        make_object<remoteFile>("", "", true, false, 0)
    ));
    prpl.verifyEvents(
        XferStartEvent(PATH),
        XferProgressEvent(PATH, 0)
    );

    tgl.update(make_object<updateFile>(make_object<file>(
        fileId, 10000, 10000,
        make_object<localFile>(PATH, false, false, false, true, 0, 10000, 10000),
        make_object<remoteFile>("", "", true, false, 5000)
    )));
    prpl.verifyEvents(XferProgressEvent(PATH, 5000));

    tgl.update(make_object<updateFile>(make_object<file>(
        fileId, 10000, 10000,
        make_object<localFile>(PATH, false, false, false, true, 0, 10000, 10000),
        make_object<remoteFile>("", "", true, false, 9500)
    )));
    prpl.verifyEvents(XferProgressEvent(PATH, 9000));

    tgl.update(make_object<updateFile>(make_object<file>(
        fileId, 10000, 10000,
        make_object<localFile>(PATH, false, false, false, true, 0, 10000, 10000),
        make_object<remoteFile>("", "", false, false, 10000)
    )));
    prpl.verifyEvents(
        XferCompletedEvent(PATH, TRUE, 9000),
        XferEndEvent(PATH)
    );
    tgl.verifyRequest(sendMessage(
        secretChatChatId,
        0,
        nullptr,
        nullptr,
        make_object<inputMessageDocument>(
            make_object<inputFileId>(fileId),
            nullptr,
            make_object<formattedText>()
        )
    ));
}

TEST_F(SecretChatTest, Download_Inline_Progress)
{
    const int64_t messageId = 1;
    const int32_t date      = 10001;
    const int32_t fileId    = 1234;
    loginWithSecretChat();

    tgl.update(make_object<updateNewMessage>(makeMessage(
        messageId,
        userIds[0],
        secretChatChatId,
        false,
        date,
        make_object<messageDocument>(
            make_object<document>(
                "doc.file.name", "mime/type", nullptr, nullptr,
                make_object<file>(
                    fileId, 10000, 10000,
                    make_object<localFile>("", true, true, false, false, 0, 0, 0),
                    make_object<remoteFile>("beh", "bleh", false, true, 10000)
                )
            ),
            make_object<formattedText>("document", std::vector<object_ptr<textEntity>>())
        )
    )));
    tgl.verifyRequests({
        make_object<viewMessages>(
            secretChatChatId,
            std::vector<int64_t>(1, messageId),
            true
        ),
        make_object<downloadFile>(fileId, 1, 0, 0, true)
    });
    prpl.verifyNoEvents();

    tgl.reply(make_object<ok>());

    tgl.runTimeouts();
    std::string tempFileName;
    prpl.verifyEvents(
        XferAcceptedEvent(secretChatBuddyName, &tempFileName),
        ServGotImEvent(connection, secretChatBuddyName, "document", PURPLE_MESSAGE_RECV, date),
        ConversationWriteEvent(
            secretChatBuddyName, secretChatBuddyName,
            secretChatTitle + ": Downloading doc.file.name [mime/type]",
            PURPLE_MESSAGE_SYSTEM, date
        )
    );

    tgl.update(make_object<updateFile>(make_object<file>(
        fileId, 10000, 10000,
        make_object<localFile>("/path", true, true, false, true, 0, 10000, 10000),
        make_object<remoteFile>("beh", "bleh", false, true, 10000)
    )));
    prpl.verifyEvents(
        XferStartEvent(&tempFileName),
        XferProgressEvent(tempFileName, 10000)
    );
    tgl.reply(make_object<file>(
        fileId, 10000, 10000,
        make_object<localFile>("/path", true, true, false, true, 0, 10000, 10000),
        make_object<remoteFile>("beh", "bleh", false, true, 10000)
    ));
    ASSERT_FALSE(g_file_test(tempFileName.c_str(), G_FILE_TEST_EXISTS));
    prpl.verifyEvents(
        XferCompletedEvent(tempFileName, TRUE, 10000),
        XferEndEvent(tempFileName),
        ServGotImEvent(
            connection,
            secretChatBuddyName,
            "<a href=\"file:///path\">doc.file.name [mime/type]</a>",
            PURPLE_MESSAGE_RECV,
            date
        )
    );
}

TEST_F(SecretChatTest, Download_StandardTransfer)
{
    const int64_t messageId = 1;
    const int32_t date      = 10001;
    const int32_t fileId    = 1234;
    uint8_t       data[]    = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    const char *outputFileName = ".test_download";

    setUiName("spectrum"); // No longer pidgin - now downloads will use libpurple transfers
    loginWithSecretChat();

    tgl.update(make_object<updateNewMessage>(makeMessage(
        messageId,
        userIds[0],
        secretChatChatId,
        false,
        date,
        make_object<messageDocument>(
            make_object<document>(
                "doc.file.name", "mime/type", nullptr, nullptr,
                make_object<file>(
                    fileId, 10000, 10000,
                    make_object<localFile>("", true, true, false, false, 0, 0, 0),
                    make_object<remoteFile>("beh", "bleh", false, true, 10000)
                )
            ),
            make_object<formattedText>("document", std::vector<object_ptr<textEntity>>())
        )
    )));
    tgl.verifyRequest(viewMessages(
        secretChatChatId,
        std::vector<int64_t>(1, messageId),
        true
    ));
    prpl.verifyEvents(
        XferRequestEvent(PURPLE_XFER_RECEIVE, secretChatBuddyName.c_str(), "doc.file.name")
    );

    purple_xfer_request_accepted(prpl.getLastXfer(), outputFileName);
    prpl.verifyEvents(
        XferAcceptedEvent(secretChatBuddyName, outputFileName),
        XferStartEvent(outputFileName)
    );

    tgl.verifyRequest(downloadFile(fileId, 1, 0, 0, true));

    char *tdlibFileName = NULL;
    int fd = g_file_open_tmp("tdlib_test_XXXXXX", &tdlibFileName, NULL);
    ASSERT_TRUE(fd >= 0);
    ASSERT_EQ((ssize_t)sizeof(data), write(fd, data, sizeof(data)));
    ::close(fd);

    tgl.update(make_object<updateFile>(make_object<file>(
        fileId, 10000, 10000,
        make_object<localFile>(tdlibFileName, true, true, true, false, 0, 0, 2000),
        make_object<remoteFile>("beh", "bleh", false, true, 10000)
    )));
    prpl.verifyEvents(XferProgressEvent(outputFileName, 2000));

    tgl.reply(make_object<file>(
        fileId, 10000, 10000,
        make_object<localFile>(tdlibFileName, true, true, false, true, 0, 10000, 10000),
        make_object<remoteFile>("beh", "bleh", false, true, 10000)
    ));
    tgl.update(make_object<file>(
        fileId, 10000, 10000,
        make_object<localFile>(tdlibFileName, true, true, false, true, 0, 10000, 10000),
        make_object<remoteFile>("beh", "bleh", false, true, 10000)
    ));

    prpl.verifyEvents(
        XferWriteFileEvent(outputFileName, data, 10),
        XferWriteFileEvent(outputFileName, data+10, sizeof(data)-10),
        XferCompletedEvent(outputFileName, TRUE, sizeof(data)),
        XferEndEvent(outputFileName)
    );

    remove(tdlibFileName);
    g_free(tdlibFileName);
}
