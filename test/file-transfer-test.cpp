#include "fixture.h"

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
            purpleUserName(0), "",
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
            purpleUserName(0), "",
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
            purpleUserName(0), "",
            userFirstNames[0] + " " + userLastNames[0] + ": Ignoring secret photo",
            PURPLE_MESSAGE_SYSTEM, date
        )
    );
}
