#include "fixture.h"
#include "libpurple-mock.h"
#include "buildopt.h"

class FileTransferTest: public CommTest {};

TEST_F(FileTransferTest, BigPhoto_RequestDownload)
{
    purple_account_set_string(account, "media-size-threshold", "0.5");

    const int32_t date   = 10001;
    const int32_t fileId = 1234;
    loginWithOneContact();

    std::vector<object_ptr<photoSize>> sizes;
    sizes.push_back(make_object<photoSize>(
        "whatever",
        make_object<file>(
            fileId, 600000, 600000,
            make_object<localFile>("", true, true, false, false, 0, 0, 0),
            make_object<remoteFile>("beh", "bleh", false, true, 600000)
        ),
        640, 480
    ));
    tgl.update(make_object<updateNewMessage>(makeMessage(
        1,
        userIds[0],
        chatIds[0],
        false,
        date,
        make_object<messagePhoto>(
            make_object<photo>(false, nullptr, std::move(sizes)),
            make_object<formattedText>("caption", std::vector<object_ptr<textEntity>>()),
            false
        )
    )));
    tgl.verifyRequest(
        viewMessages(chatIds[0], std::vector<int64_t>(1, 1), true)
    );
    prpl.verifyEvents(
        ServGotImEvent(connection, purpleUserName(0), "caption", PURPLE_MESSAGE_RECV, date),
        ConversationWriteEvent(
            purpleUserName(0), purpleUserName(0),
            userFirstNames[0] + " " + userLastNames[0] + ": Requesting photo download",
            PURPLE_MESSAGE_SYSTEM, date
        ),
        RequestActionEvent(connection, account, NULL, NULL, 2)
    );

    prpl.requestedAction("_Yes");
    tgl.verifyRequest(downloadFile(fileId, 1, 0, 0, true));
    tgl.reply(make_object<file>(
        fileId, 10000, 10000,
        make_object<localFile>("/path", true, true, false, true, 0, 10000, 10000),
        make_object<remoteFile>("beh", "bleh", false, true, 10000)
    ));

    prpl.verifyEvents(ServGotImEvent(
        connection,
        purpleUserName(0),
        "<img src=\"file:///path\">",
        (PurpleMessageFlags)(PURPLE_MESSAGE_RECV | PURPLE_MESSAGE_IMAGES),
        date
    ));
}

TEST_F(FileTransferTest, BigPhoto_Ignore)
{
    purple_account_set_string(account, "media-size-threshold", "0.5");
    purple_account_set_string(account, "media-handling-behavior", "discard");

    const int32_t date   = 10001;
    const int32_t fileId = 1234;
    loginWithOneContact();

    std::vector<object_ptr<photoSize>> sizes;
    sizes.push_back(make_object<photoSize>(
        "whatever",
        make_object<file>(
            fileId, 655360, 655360,
            make_object<localFile>("", true, true, false, false, 0, 0, 0),
            make_object<remoteFile>("beh", "bleh", false, true, 655360)
        ),
        640, 480
    ));
    tgl.update(make_object<updateNewMessage>(makeMessage(
        1,
        userIds[0],
        chatIds[0],
        false,
        date,
        make_object<messagePhoto>(
            make_object<photo>(false, nullptr, std::move(sizes)),
            make_object<formattedText>("caption", std::vector<object_ptr<textEntity>>()),
            false
        )
    )));
    tgl.verifyRequest(
        viewMessages(chatIds[0], std::vector<int64_t>(1, 1), true)
    );
    prpl.verifyEvents(
        ServGotImEvent(connection, purpleUserName(0), "caption", PURPLE_MESSAGE_RECV, date),
        ConversationWriteEvent(
            purpleUserName(0), purpleUserName(0),
            userFirstNames[0] + " " + userLastNames[0] + ": Ignoring photo download (purple_str_size_to_units)",
            PURPLE_MESSAGE_SYSTEM, date
        )
    );
}

TEST_F(FileTransferTest, SecretPhoto_AlreadyDownloaded)
{
    const int32_t date   = 10001;
    const int32_t fileId = 1234;
    loginWithOneContact();

    std::vector<object_ptr<photoSize>> sizes;
    sizes.push_back(make_object<photoSize>(
        "whatever",
        make_object<file>(
            fileId, 10000, 10000,
            make_object<localFile>("/path", true, true, false, true, 0, 10000, 10000),
            make_object<remoteFile>("beh", "bleh", false, true, 10000)
        ),
        640, 480
    ));
    tgl.update(make_object<updateNewMessage>(makeMessage(
        1,
        userIds[0],
        chatIds[0],
        false,
        date,
        make_object<messagePhoto>(
            make_object<photo>(false, nullptr, std::move(sizes)),
            make_object<formattedText>("caption", std::vector<object_ptr<textEntity>>()),
            true
        )
    )));
    tgl.verifyRequest(
        viewMessages(chatIds[0], std::vector<int64_t>(1, 1), true)
    );

    // Secret photos are always ignored
    prpl.verifyEvents(
        ServGotImEvent(connection, purpleUserName(0), "caption", PURPLE_MESSAGE_RECV, date),
        ConversationWriteEvent(
            purpleUserName(0), purpleUserName(0),
            userFirstNames[0] + " " + userLastNames[0] + ": Ignoring secret photo",
            PURPLE_MESSAGE_SYSTEM, date
        )
    );
}

TEST_F(FileTransferTest, PhotoWithoutCaption)
{
    const int32_t date   = 10001;
    const int32_t fileId = 1234;
    loginWithOneContact();

    std::vector<object_ptr<photoSize>> sizes;
    sizes.push_back(make_object<photoSize>(
        "whatever",
        make_object<file>(
            fileId, 10000, 10000,
            make_object<localFile>("", true, true, false, false, 0, 0, 0),
            make_object<remoteFile>("beh", "bleh", false, true, 10000)
        ),
        640, 480
    ));
    tgl.update(make_object<updateNewMessage>(makeMessage(
        1,
        userIds[0],
        chatIds[0],
        false,
        date,
        make_object<messagePhoto>(
            make_object<photo>(false, nullptr, std::move(sizes)),
            make_object<formattedText>("", std::vector<object_ptr<textEntity>>()),
            false
        )
    )));
    tgl.verifyRequests({
        make_object<viewMessages>(chatIds[0], std::vector<int64_t>(1, 1), true),
        make_object<downloadFile>(fileId, 1, 0, 0, true)
    });

    prpl.verifyEvents(
        NewConversationEvent(PURPLE_CONV_TYPE_IM, account, purpleUserName(0)),
        ConversationWriteEvent(
            purpleUserName(0), purpleUserName(0),
            userFirstNames[0] + " " + userLastNames[0] + ": Downloading photo",
            PURPLE_MESSAGE_SYSTEM, date
        )
    );

    tgl.reply(make_object<ok>()); // reply to viewMessages
    tgl.reply(make_object<file>(
        fileId, 10000, 10000,
        make_object<localFile>("/path", true, true, false, true, 0, 10000, 10000),
        make_object<remoteFile>("beh", "bleh", false, true, 10000)
    ));

    prpl.verifyEvents(ServGotImEvent(
        connection,
        purpleUserName(0),
        "<img src=\"file:///path\">",
        (PurpleMessageFlags)(PURPLE_MESSAGE_RECV | PURPLE_MESSAGE_IMAGES),
        date
    ));
}

TEST_F(FileTransferTest, SendFile_ErrorInUploadResponse)
{
    const char *const PATH = "/path";
    loginWithOneContact();

    setFakeFileSize(PATH, 9000);
    pluginInfo().send_file(connection, purpleUserName(0).c_str(), PATH);
    prpl.verifyEvents(XferAcceptedEvent(PATH));
    tgl.verifyRequest(uploadFile(
        make_object<inputFileLocal>(PATH),
        make_object<fileTypeDocument>(),
        1
    ));

    tgl.reply(make_object<error>(1, "error"));
    prpl.verifyEvents(XferRemoteCancelEvent(PATH));
}

TEST_F(FileTransferTest, SendFile_SendMessageResponseError)
{
    const char *const PATH   = "/path";
    const int32_t     fileId = 1234;
    loginWithOneContact();

    setFakeFileSize(PATH, 9000);
    pluginInfo().send_file(connection, purpleUserName(0).c_str(), PATH);
    prpl.verifyEvents(XferAcceptedEvent(PATH));
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
        chatIds[0],
        0,
        nullptr,
        nullptr,
        make_object<inputMessageDocument>(
            make_object<inputFileId>(fileId),
            nullptr,
            make_object<formattedText>()
        )
    ));

    tgl.update(make_object<updateFile>(make_object<file>(
        fileId, 10000, 10000,
        make_object<localFile>(PATH, false, false, false, true, 0, 10000, 10000),
        make_object<remoteFile>("beh", "bleh", false, true, 10000)
    )));

    tgl.reply(make_object<error>(100, "error"));
    prpl.verifyEvents(
        NewConversationEvent(PURPLE_CONV_TYPE_IM, account, purpleUserName(0)),
        ConversationWriteEvent(
            purpleUserName(0), purpleUserName(0),
            "Failed to send message: code 100 (error)",
            PURPLE_MESSAGE_SYSTEM, 0
        )
    );
}

TEST_F(FileTransferTest, SendFile_UnknownUser)
{
    const char *const PATH = "/path";
    login();

    setFakeFileSize(PATH, 9000);
    pluginInfo().send_file(connection, "Antonie van Leeuwenhoek", PATH);
    prpl.verifyEvents(
        XferAcceptedEvent(PATH),
        XferLocalCancelEvent(PATH)
    );
}

#ifndef NoWebp
TEST_F(FileTransferTest, WebpStickerDecode)
#else
TEST_F(FileTransferTest, DISABLED_WebpStickerDecode)
#endif
{
    const int32_t date      = 10001;
    const int32_t fileId    = 1234;
    loginWithOneContact();

    tgl.update(make_object<updateNewMessage>(makeMessage(
        1,
        userIds[0],
        chatIds[0],
        false,
        date,
        make_object<messageSticker>(make_object<sticker>(
            0, 320, 200, "", true, false, nullptr,
            nullptr,
            make_object<file>(
                fileId, 10000, 10000,
                make_object<localFile>(TEST_SOURCE_DIR "/test.webp", true, true, false, true, 0, 10000, 10000),
                make_object<remoteFile>("beh", "bleh", false, true, 10000)
            )
        ))
    )));
    tgl.verifyRequest(viewMessages(chatIds[0], {1}, true));
    prpl.verifyEvents(ServGotImEvent(
        connection,
        purpleUserName(0),
        "\n<img id=\"" + std::to_string(getLastImgstoreId()) + "\">",
        PURPLE_MESSAGE_RECV,
        date
    ));
}

#ifndef NoLottie
TEST_F(FileTransferTest, AnimatedStickerDecode)
#else
TEST_F(FileTransferTest, DISABLED_AnimatedStickerDecode)
#endif
{
    const int32_t date    = 10001;
    const int32_t fileId  = 1234;
    loginWithOneContact();

    // No thumbnail, only .tgs
    tgl.update(make_object<updateNewMessage>(makeMessage(
        1,
        userIds[0],
        chatIds[0],
        false,
        date,
        make_object<messageSticker>(make_object<sticker>(
            0, 320, 200, "", true, false, nullptr,
            nullptr,
            make_object<file>(
                fileId, 10000, 10000,
                make_object<localFile>(TEST_SOURCE_DIR "/test.tgs", true, true, false, true, 0, 10000, 10000),
                make_object<remoteFile>("beh", "bleh", false, true, 10000)
            )
        ))
    )));
    tgl.verifyRequests({
        make_object<viewMessages>(chatIds[0], std::vector<int64_t>(1, 1), true),
    });

    tgl.reply(make_object<ok>()); // reply to viewMessages

    prpl.verifyEvents(
        NewConversationEvent(PURPLE_CONV_TYPE_IM, account, purpleUserName(0)),
        ConversationWriteEvent(
            purpleUserName(0), purpleUserName(0),
            userFirstNames[0] + " " + userLastNames[0] + ": Converting sticker",
            PURPLE_MESSAGE_SYSTEM, date
        ),
        ServGotImEvent(
            connection,
            purpleUserName(0),
            // Sticker was converted to gif
            "\n<img id=\"" + std::to_string(getLastImgstoreId()) + "\">",
            PURPLE_MESSAGE_RECV,
            date
        )
    );
}

TEST_F(FileTransferTest, Photo_DownloadProgress_StuckAtStart)
{
    const int32_t date   = 10001;
    const int32_t fileId = 1234;
    loginWithOneContact();

    std::vector<object_ptr<photoSize>> sizes;
    sizes.push_back(make_object<photoSize>(
        "whatever",
        make_object<file>(
            fileId, 10000, 10000,
            make_object<localFile>("", true, true, false, false, 0, 0, 0),
            make_object<remoteFile>("beh", "bleh", false, true, 10000)
        ),
        640, 480
    ));
    tgl.update(make_object<updateNewMessage>(makeMessage(
        1,
        userIds[0],
        chatIds[0],
        false,
        date,
        make_object<messagePhoto>(
            make_object<photo>(false, nullptr, std::move(sizes)),
            make_object<formattedText>("photo", std::vector<object_ptr<textEntity>>()),
            false
        )
    )));
    tgl.verifyRequests({
        make_object<viewMessages>(chatIds[0], std::vector<int64_t>(1, 1), true),
        make_object<downloadFile>(fileId, 1, 0, 0, true)
    });
    prpl.verifyEvents(
        ServGotImEvent(connection, purpleUserName(0), "photo", PURPLE_MESSAGE_RECV, date),
        ConversationWriteEvent(
            purpleUserName(0), purpleUserName(0),
            userFirstNames[0] + " " + userLastNames[0] + ": Downloading photo",
            PURPLE_MESSAGE_SYSTEM, date
        )
    );

    tgl.reply(make_object<ok>()); // reply to viewMessages
    tgl.runTimeouts();
    std::string tempFileName;
    prpl.verifyEvents(
        XferAcceptedEvent(&tempFileName)
    );

    tgl.update(make_object<updateFile>(make_object<file>(
        fileId, 10000, 10000,
        make_object<localFile>("/path", true, true, true, false, 0, 0, 2000),
        make_object<remoteFile>("beh", "bleh", false, true, 10000)
    )));
    prpl.verifyEvents(
        XferStartEvent(tempFileName),
        XferProgressEvent(tempFileName, 2000)
    );

    tgl.update(make_object<updateFile>(make_object<file>(
        fileId, 10000, 10000,
        make_object<localFile>("/path", true, true, true, false, 0, 0, 5000),
        make_object<remoteFile>("beh", "bleh", false, true, 10000)
    )));
    prpl.verifyEvents(
        XferProgressEvent(tempFileName, 5000)
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
            purpleUserName(0),
            "<img src=\"file:///path\">",
            (PurpleMessageFlags)(PURPLE_MESSAGE_RECV | PURPLE_MESSAGE_IMAGES),
            date
        )
    );

    tgl.update(make_object<updateFile>(make_object<file>(
        fileId, 10000, 10000,
        make_object<localFile>("/path", true, true, false, true, 0, 10000, 10000),
        make_object<remoteFile>("beh", "bleh", false, true, 10000)
    )));
}

TEST_F(FileTransferTest, Photo_DownloadProgress)
{
    const int32_t date   = 10001;
    const int32_t fileId = 1234;
    loginWithOneContact();

    std::vector<object_ptr<photoSize>> sizes;
    sizes.push_back(make_object<photoSize>(
        "whatever",
        make_object<file>(
            fileId, 10000, 10000,
            make_object<localFile>("", true, true, false, false, 0, 0, 0),
            make_object<remoteFile>("beh", "bleh", false, true, 10000)
        ),
        640, 480
    ));
    tgl.update(make_object<updateNewMessage>(makeMessage(
        1,
        userIds[0],
        chatIds[0],
        false,
        date,
        make_object<messagePhoto>(
            make_object<photo>(false, nullptr, std::move(sizes)),
            make_object<formattedText>("photo", std::vector<object_ptr<textEntity>>()),
            false
        )
    )));
    tgl.verifyRequests({
        make_object<viewMessages>(chatIds[0], std::vector<int64_t>(1, 1), true),
        make_object<downloadFile>(fileId, 1, 0, 0, true)
    });
    prpl.verifyEvents(
        ServGotImEvent(connection, purpleUserName(0), "photo", PURPLE_MESSAGE_RECV, date),
        ConversationWriteEvent(
            purpleUserName(0), purpleUserName(0),
            userFirstNames[0] + " " + userLastNames[0] + ": Downloading photo",
            PURPLE_MESSAGE_SYSTEM, date
        )
    );

    tgl.reply(make_object<ok>()); // reply to viewMessages

    tgl.update(make_object<updateFile>(make_object<file>(
        fileId, 10000, 10000,
        make_object<localFile>("/path", true, true, true, false, 0, 0, 2000),
        make_object<remoteFile>("beh", "bleh", false, true, 10000)
    )));

    tgl.runTimeouts();
    std::string tempFileName;
    prpl.verifyEvents(
        XferAcceptedEvent(&tempFileName),
        XferStartEvent(&tempFileName)
    );

    tgl.update(make_object<updateFile>(make_object<file>(
        fileId, 10000, 10000,
        make_object<localFile>("/path", true, true, true, false, 0, 0, 5000),
        make_object<remoteFile>("beh", "bleh", false, true, 10000)
    )));

    prpl.verifyEvents(
        XferProgressEvent(tempFileName, 5000)
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
            purpleUserName(0),
            "<img src=\"file:///path\">",
            (PurpleMessageFlags)(PURPLE_MESSAGE_RECV | PURPLE_MESSAGE_IMAGES),
            date
        )
    );

    tgl.update(make_object<updateFile>(make_object<file>(
        fileId, 10000, 10000,
        make_object<localFile>("/path", true, true, false, true, 0, 10000, 10000),
        make_object<remoteFile>("beh", "bleh", false, true, 10000)
    )));
}

TEST_F(FileTransferTest, Photo_DownloadProgress_StuckAtStart_Cancel)
{
    const int32_t date   = 10001;
    const int32_t fileId = 1234;
    loginWithOneContact();

    std::vector<object_ptr<photoSize>> sizes;
    sizes.push_back(make_object<photoSize>(
        "whatever",
        make_object<file>(
            fileId, 10000, 10000,
            make_object<localFile>("", true, true, false, false, 0, 0, 0),
            make_object<remoteFile>("beh", "bleh", false, true, 10000)
        ),
        640, 480
    ));
    tgl.update(make_object<updateNewMessage>(makeMessage(
        1,
        userIds[0],
        chatIds[0],
        false,
        date,
        make_object<messagePhoto>(
            make_object<photo>(false, nullptr, std::move(sizes)),
            make_object<formattedText>("photo", std::vector<object_ptr<textEntity>>()),
            false
        )
    )));
    tgl.verifyRequests({
        make_object<viewMessages>(chatIds[0], std::vector<int64_t>(1, 1), true),
        make_object<downloadFile>(fileId, 1, 0, 0, true)
    });
    prpl.verifyEvents(
        ServGotImEvent(connection, purpleUserName(0), "photo", PURPLE_MESSAGE_RECV, date),
        ConversationWriteEvent(
            purpleUserName(0), purpleUserName(0),
            userFirstNames[0] + " " + userLastNames[0] + ": Downloading photo",
            PURPLE_MESSAGE_SYSTEM, date
        )
    );

    tgl.reply(make_object<ok>()); // reply to viewMessages
    tgl.runTimeouts();
    std::string tempFileName;
    prpl.verifyEvents(
        XferAcceptedEvent(&tempFileName)
    );

    purple_xfer_cancel_local(prpl.getLastXfer());
    prpl.verifyEvents(XferLocalCancelEvent(tempFileName));
    tgl.reply(make_object<error>(400, "Download cancelled"));
    ASSERT_FALSE(g_file_test(tempFileName.c_str(), G_FILE_TEST_EXISTS));
    tgl.verifyRequest(cancelDownloadFile(fileId, false));

    tgl.update(make_object<updateFile>(make_object<file>(
        fileId, 10000, 10000,
        make_object<localFile>("/path", true, true, false, false, 0, 1000, 1000),
        make_object<remoteFile>("beh", "bleh", false, true, 10000)
    )));
}

TEST_F(FileTransferTest, Photo_DownloadProgress_Cancel)
{
    const int32_t date   = 10001;
    const int32_t fileId = 1234;
    loginWithOneContact();

    std::vector<object_ptr<photoSize>> sizes;
    sizes.push_back(make_object<photoSize>(
        "whatever",
        make_object<file>(
            fileId, 10000, 10000,
            make_object<localFile>("", true, true, false, false, 0, 0, 0),
            make_object<remoteFile>("beh", "bleh", false, true, 10000)
        ),
        640, 480
    ));
    tgl.update(make_object<updateNewMessage>(makeMessage(
        1,
        userIds[0],
        chatIds[0],
        false,
        date,
        make_object<messagePhoto>(
            make_object<photo>(false, nullptr, std::move(sizes)),
            make_object<formattedText>("photo", std::vector<object_ptr<textEntity>>()),
            false
        )
    )));
    tgl.verifyRequests({
        make_object<viewMessages>(chatIds[0], std::vector<int64_t>(1, 1), true),
        make_object<downloadFile>(fileId, 1, 0, 0, true)
    });
    prpl.verifyEvents(
        ServGotImEvent(connection, purpleUserName(0), "photo", PURPLE_MESSAGE_RECV, date),
        ConversationWriteEvent(
            purpleUserName(0), purpleUserName(0),
            userFirstNames[0] + " " + userLastNames[0] + ": Downloading photo",
            PURPLE_MESSAGE_SYSTEM, date
        )
    );

    tgl.reply(make_object<ok>()); // reply to viewMessages
    tgl.update(make_object<updateFile>(make_object<file>(
        fileId, 10000, 10000,
        make_object<localFile>("/path", true, true, true, false, 0, 0, 2000),
        make_object<remoteFile>("beh", "bleh", false, true, 10000)
    )));

    tgl.runTimeouts();
    std::string tempFileName;
    prpl.verifyEvents(
        XferAcceptedEvent(&tempFileName),
        XferStartEvent(&tempFileName)
    );

    tgl.update(make_object<updateFile>(make_object<file>(
        fileId, 10000, 10000,
        make_object<localFile>("/path", true, true, true, false, 0, 0, 5000),
        make_object<remoteFile>("beh", "bleh", false, true, 10000)
    )));

    prpl.verifyEvents(
        XferProgressEvent(tempFileName, 5000)
    );

    purple_xfer_cancel_local(prpl.getLastXfer());
    prpl.verifyEvents(XferLocalCancelEvent(tempFileName));
    tgl.reply(make_object<error>(400, "Download cancelled"));
    ASSERT_FALSE(g_file_test(tempFileName.c_str(), G_FILE_TEST_EXISTS));
    tgl.verifyRequest(cancelDownloadFile(fileId, false));

    tgl.update(make_object<updateFile>(make_object<file>(
        fileId, 10000, 10000,
        make_object<localFile>("/path", true, true, false, false, 0, 6000, 6000),
        make_object<remoteFile>("beh", "bleh", false, true, 10000)
    )));
}

TEST_F(FileTransferTest, Photo_Download_LogoutBeforeProgress)
{
    const int32_t date   = 10001;
    const int32_t fileId = 1234;
    loginWithOneContact();

    std::vector<object_ptr<photoSize>> sizes;
    sizes.push_back(make_object<photoSize>(
        "whatever",
        make_object<file>(
            fileId, 10000, 10000,
            make_object<localFile>("", true, true, false, false, 0, 0, 0),
            make_object<remoteFile>("beh", "bleh", false, true, 10000)
        ),
        640, 480
    ));
    tgl.update(make_object<updateNewMessage>(makeMessage(
        1,
        userIds[0],
        chatIds[0],
        false,
        date,
        make_object<messagePhoto>(
            make_object<photo>(false, nullptr, std::move(sizes)),
            make_object<formattedText>("photo", std::vector<object_ptr<textEntity>>()),
            false
        )
    )));
    tgl.verifyRequests({
        make_object<viewMessages>(chatIds[0], std::vector<int64_t>(1, 1), true),
        make_object<downloadFile>(fileId, 1, 0, 0, true)
    });
    prpl.verifyEvents(
        ServGotImEvent(connection, purpleUserName(0), "photo", PURPLE_MESSAGE_RECV, date),
        ConversationWriteEvent(
            purpleUserName(0), purpleUserName(0),
            userFirstNames[0] + " " + userLastNames[0] + ": Downloading photo",
            PURPLE_MESSAGE_SYSTEM, date
        )
    );
}

TEST_F(FileTransferTest, SendFileToNonContact)
{
    const char *const PATH   = "/path";
    const int32_t     fileId = 1234;
    setFakeFileSize(PATH, 10000);

    login();

    // Normaly users without chats have to be group chat members, but for the test it doesn't matter
    tgl.update(standardUpdateUser(0));

    // Send ﬁle successfully
    pluginInfo().send_file(
        connection,
        (userFirstNames[0] + " " + userLastNames[0]).c_str(),
        PATH
    );
    prpl.verifyEvents(
        XferAcceptedEvent(PATH)
    );
    tgl.verifyRequest(createPrivateChat(userIds[0], false));

    tgl.update(standardPrivateChat(0));
    tgl.reply(makeChat(
        chatIds[0],
        make_object<chatTypePrivate>(userIds[0]),
        userFirstNames[0] + " " + userLastNames[0],
        nullptr, 0, 0, 0
    ));
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
        make_object<remoteFile>("", "", false, false, 10000)
    )));
    prpl.verifyEvents(
        XferCompletedEvent(PATH, TRUE, 10000),
        XferEndEvent(PATH)
    );
    tgl.verifyRequest(sendMessage(
        chatIds[0],
        0,
        nullptr,
        nullptr,
        make_object<inputMessageDocument>(
            make_object<inputFileId>(fileId),
            nullptr,
            make_object<formattedText>()
        )
    ));
    ASSERT_EQ(0, pluginInfo().send_im(
        connection,
        (userFirstNames[0] + " " + userLastNames[0]).c_str(),
        "message2",
        PURPLE_MESSAGE_SEND
    ));
    tgl.verifyRequest(sendMessage(
        chatIds[0],
        0,
        nullptr,
        nullptr,
        make_object<inputMessageText>(
            make_object<formattedText>("message2", std::vector<object_ptr<textEntity>>()),
            false,
            false
        )
    ));
}

TEST_F(FileTransferTest, SendFileToNonContact_CreatePrivateChatFail)
{
    const char *const PATH   = "/path";
    setFakeFileSize(PATH, 10000);

    login();

    // Normaly users without chats have to be group chat members, but for the test it doesn't matter
    tgl.update(standardUpdateUser(1));

    pluginInfo().send_file(
        connection,
        (userFirstNames[1] + " " + userLastNames[1]).c_str(),
        PATH
    );
    prpl.verifyEvents(
        XferAcceptedEvent(PATH)
    );
    tgl.verifyRequest(createPrivateChat(userIds[1], false));

    tgl.reply(make_object<error>(100, "error"));
    prpl.verifyEvents(XferLocalCancelEvent(PATH));
}

TEST_F(FileTransferTest, SendFileToNonContact_TurboCancel)
{
    const char *const PATH   = "/path";
    setFakeFileSize(PATH, 10000);

    login();

    // Normaly users without chats have to be group chat members, but for the test it doesn't matter
    tgl.update(standardUpdateUser(1));

    pluginInfo().send_file(
        connection,
        (userFirstNames[1] + " " + userLastNames[1]).c_str(),
        PATH
    );
    prpl.verifyEvents(
        XferAcceptedEvent(PATH)
    );
    tgl.verifyRequest(createPrivateChat(userIds[1], false));

    purple_xfer_cancel_local(prpl.getLastXfer());
    prpl.discardEvents();

    tgl.update(standardPrivateChat(0));
    tgl.reply(makeChat(
        chatIds[0],
        make_object<chatTypePrivate>(userIds[0]),
        userFirstNames[0] + " " + userLastNames[0],
        nullptr, 0, 0, 0
    ));
}
