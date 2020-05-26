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
            userFirstNames[0] + " " + userLastNames[0] + ": Requesting image download",
            PURPLE_MESSAGE_SYSTEM, date
        )
    );
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
            userFirstNames[0] + " " + userLastNames[0] + ": Ignoring image download of 640 kB",
            PURPLE_MESSAGE_SYSTEM, date
        )
    );
}
