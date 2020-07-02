#include "fixture.h"
#include "libpurple-mock.h"
#include <fmt/format.h>

class PrivateChatTest: public CommTest {};

TEST_F(PrivateChatTest, AddContactByPhone)
{
    login();

    // Adding new buddy
    PurpleBuddy *buddy = purple_buddy_new(account, userPhones[0].c_str(), "Local Alias");
    purple_blist_add_buddy(buddy, NULL, &standardPurpleGroup, NULL);
    prpl.discardEvents();
    pluginInfo().add_buddy(connection, buddy, &standardPurpleGroup);

    // The buddy is deleted right away, to be replaced later
    prpl.verifyEvents(RemoveBuddyEvent(account, userPhones[0]));

    // Adding user to contact list by phone number
    std::vector<object_ptr<contact>> contacts;
    contacts.push_back(make_object<contact>(
        userPhones[0],
        "",
        "",
        "",
        0
    ));
    tgl.verifyRequest(importContacts(std::move(contacts)));

    tgl.update(make_object<updateUser>(makeUser(
        userIds[0],
        "",
        "",
        userPhones[0],
        make_object<userStatusOffline>()
    )));
    tgl.reply(make_object<importedContacts>(
        std::vector<int32_t>(1, userIds[0]),
        std::vector<int32_t>()
    ));

    tgl.verifyRequest(addContact(
        make_object<contact>(
            userPhones[0],
            "Local",
            "Alias",
            "",
            userIds[0]
        ), true
    ));

    // We are notified that the user is now a contact
    object_ptr<user> userInfo = makeUser(
        userIds[0],
        "Local",
        "Alias",
        userPhones[0],
        make_object<userStatusOffline>()
    );
    userInfo->is_contact_ = true;
    tgl.update(make_object<updateUser>(std::move(userInfo)));

    tgl.reply(make_object<ok>());

    tgl.verifyRequest(createPrivateChat(userIds[0], false));
    prpl.verifyNoEvents();

    tgl.update(make_object<updateNewChat>(makeChat(
        chatIds[0],
        make_object<chatTypePrivate>(userIds[0]),
        "Local Alias",
        nullptr, 0, 0, 0
    )));

    // is_contact was true, so add buddy
    prpl.verifyEvents(AddBuddyEvent(
        purpleUserName(0),
        "Local Alias",
        account,
        NULL,
        &standardPurpleGroup,
        NULL
    ));
    tgl.reply(makeChat(
        chatIds[0],
        make_object<chatTypePrivate>(userIds[0]),
        userFirstNames[0] + " " + userLastNames[0],
        nullptr, 0, 0, 0
    ));
}

TEST_F(PrivateChatTest, ContactedByNew)
{
    login();
    constexpr int64_t messageId = 10000;
    constexpr int32_t date      = 123456;

    // Seems to happen when they add us to contacts
    tgl.update(make_object<updateUser>(makeUser(
        userIds[0],
        userFirstNames[0],
        userLastNames[0],
        "", // No phone number yet
        make_object<userStatusOffline>()
    )));

    // They message us
    object_ptr<updateNewChat> chatUpdate = standardPrivateChat(0);
    chatUpdate->chat_->chat_list_ = make_object<chatListMain>();
    tgl.update(std::move(chatUpdate));
    prpl.verifyEvents(AddBuddyEvent(
        purpleUserName(0),
        userFirstNames[0] + " " + userLastNames[0],
        account,
        nullptr, nullptr, nullptr
    ));

    tgl.update(make_object<updateNewMessage>(makeMessage(
        messageId,
        userIds[0],
        chatIds[0],
        false,
        date,
        makeTextMessage("text")
    )));
    prpl.verifyEvents(ServGotImEvent(
        connection,
        purpleUserName(0),
        "text",
        PURPLE_MESSAGE_RECV,
        date
    ));
    tgl.verifyRequest(viewMessages(
        chatIds[0],
        {messageId},
        true
    ));

    // And only now we get phone number
    tgl.update(make_object<updateUser>(makeUser(
        userIds[0],
        userFirstNames[0],
        userLastNames[0],
        "+" + userPhones[0],
        make_object<userStatusOffline>()
    )));
}

TEST_F(PrivateChatTest, ContactedByNew_ImmediatePhoneNumber)
{
    login();
    constexpr int64_t messageId = 10000;
    constexpr int32_t date      = 123456;

    // Phone number sent right away - this has not been observed in real life
    tgl.update(standardUpdateUser(0));
    prpl.verifyNoEvents();

    object_ptr<updateNewChat> chatUpdate = standardPrivateChat(0);
    chatUpdate->chat_->chat_list_ = make_object<chatListMain>();
    tgl.update(std::move(chatUpdate));
    prpl.verifyEvents(AddBuddyEvent(
        purpleUserName(0),
        userFirstNames[0] + " " + userLastNames[0],
        account,
        nullptr, nullptr, nullptr
    ));

    tgl.update(make_object<updateNewMessage>(makeMessage(
        messageId,
        userIds[0],
        chatIds[0],
        false,
        date,
        makeTextMessage("text")
    )));
    prpl.verifyEvents(ServGotImEvent(
        connection,
        purpleUserName(0),
        "text",
        PURPLE_MESSAGE_RECV,
        date
    ));
    tgl.verifyRequest(viewMessages(
        chatIds[0],
        {messageId},
        true
    ));
}

TEST_F(PrivateChatTest, ContactWithoutChatAtLogin)
{
    auto userUpdate = standardUpdateUser(0);
    userUpdate->user_->is_contact_ = true;
    login(
        {std::move(userUpdate)},
        make_object<users>(1, std::vector<int32_t>(1, userIds[0])),
        make_object<chats>(),
        {}, {}, {}
    );
    tgl.verifyRequest(createPrivateChat(userIds[0], false));

    tgl.update(standardPrivateChat(0));
    prpl.verifyNoEvents();

    tgl.reply(makeChat(
        chatIds[0],
        make_object<chatTypePrivate>(userIds[0]),
        userFirstNames[0] + " " + userLastNames[0],
        nullptr, 0, 0, 0
    ));
    prpl.verifyEvents(
        ConnectionSetStateEvent(connection, PURPLE_CONNECTED),
        AddBuddyEvent(purpleUserName(0), userFirstNames[0] + " " + userLastNames[0],
                      account, nullptr, nullptr, nullptr),
        UserStatusEvent(account, purpleUserName(0), PURPLE_STATUS_OFFLINE),
        AccountSetAliasEvent(account, selfFirstName + " " + selfLastName),
        ShowAccountEvent(account)
    );
}

TEST_F(PrivateChatTest, Document)
{
    const int64_t messageId = 1;
    const int32_t date      = 10001;
    const int32_t fileId    = 1234;
    loginWithOneContact();

    tgl.update(make_object<updateNewMessage>(makeMessage(
        messageId,
        userIds[0],
        chatIds[0],
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
            chatIds[0],
            std::vector<int64_t>(1, messageId),
            true
        ),
        make_object<downloadFile>(fileId, 1, 0, 0, true)
    });
    prpl.verifyEvents(
        ServGotImEvent(connection, purpleUserName(0), "document", PURPLE_MESSAGE_RECV, date),
        ConversationWriteEvent(
            purpleUserName(0), purpleUserName(0),
            userFirstNames[0] + " " + userLastNames[0] + ": Downloading doc.file.name [mime/type]",
            PURPLE_MESSAGE_SYSTEM, date
        )
    );

    tgl.reply(make_object<ok>());
    tgl.reply(make_object<file>(
        fileId, 10000, 10000,
        make_object<localFile>("/path", true, true, false, true, 0, 10000, 10000),
        make_object<remoteFile>("beh", "bleh", false, true, 10000)
    ));
    prpl.verifyEvents(ServGotImEvent(
        connection,
        purpleUserName(0),
        "<a href=\"file:///path\">doc.file.name [mime/type]</a>",
        PURPLE_MESSAGE_RECV,
        date
    ));
}

TEST_F(PrivateChatTest, Video)
{
    const int64_t messageId = 1;
    const int32_t date      = 10001;
    const int32_t fileId    = 1234;
    loginWithOneContact();

    tgl.update(make_object<updateNewMessage>(makeMessage(
        messageId,
        userIds[0],
        chatIds[0],
        false,
        date,
        make_object<messageVideo>(
            make_object<video>(
                120, 640, 480, "video.avi", "video/whatever", false, false, nullptr, nullptr,
                make_object<file>(
                    fileId, 10000, 10000,
                    make_object<localFile>("", true, true, false, false, 0, 0, 0),
                    make_object<remoteFile>("beh", "bleh", false, true, 10000)
                )
            ),
            make_object<formattedText>("video", std::vector<object_ptr<textEntity>>()),
            false
        )
    )));
    tgl.verifyRequests({
        make_object<viewMessages>(
            chatIds[0],
            std::vector<int64_t>(1, messageId),
            true
        ),
        make_object<downloadFile>(fileId, 1, 0, 0, true)
    });
    prpl.verifyEvents(
        ServGotImEvent(connection, purpleUserName(0), "video", PURPLE_MESSAGE_RECV, date),
        ConversationWriteEvent(
            purpleUserName(0), purpleUserName(0),
            userFirstNames[0] + " " + userLastNames[0] + ": Downloading video.avi [video/whatever]",
            PURPLE_MESSAGE_SYSTEM, date
        )
    );

    tgl.reply(make_object<ok>());
    tgl.reply(make_object<file>(
        fileId, 10000, 10000,
        make_object<localFile>("/path", true, true, false, true, 0, 10000, 10000),
        make_object<remoteFile>("beh", "bleh", false, true, 10000)
    ));
    prpl.verifyEvents(ServGotImEvent(
        connection,
        purpleUserName(0),
        "<a href=\"file:///path\">video.avi [video/whatever]</a>",
        PURPLE_MESSAGE_RECV,
        date
    ));
}

TEST_F(PrivateChatTest, Audio)
{
    const int64_t messageId = 1;
    const int32_t date      = 10001;
    const int32_t fileId    = 1234;
    loginWithOneContact();

    tgl.update(make_object<updateNewMessage>(makeMessage(
        messageId,
        userIds[0],
        chatIds[0],
        false,
        date,
        make_object<messageAudio>(
            make_object<audio>(
                25*60, "Symphony #40", "Wolfgang Amadeus Mozart",
                "symphony.ogg", "audio/whatever", nullptr, nullptr,
                make_object<file>(
                    fileId, 10000, 10000,
                    make_object<localFile>("", true, true, false, false, 0, 0, 0),
                    make_object<remoteFile>("beh", "bleh", false, true, 10000)
                )
            ),
            make_object<formattedText>("audio", std::vector<object_ptr<textEntity>>())
        )
    )));
    tgl.verifyRequests({
        make_object<viewMessages>(
            chatIds[0],
            std::vector<int64_t>(1, messageId),
            true
        ),
        make_object<downloadFile>(fileId, 1, 0, 0, true)
    });
    prpl.verifyEvents(
        ServGotImEvent(connection, purpleUserName(0), "audio", PURPLE_MESSAGE_RECV, date),
        ConversationWriteEvent(
            purpleUserName(0), purpleUserName(0),
            userFirstNames[0] + " " + userLastNames[0] + ": Downloading symphony.ogg [audio/whatever]",
            PURPLE_MESSAGE_SYSTEM, date
        )
    );

    tgl.reply(make_object<ok>());
    tgl.reply(make_object<file>(
        fileId, 10000, 10000,
        make_object<localFile>("/path", true, true, false, true, 0, 10000, 10000),
        make_object<remoteFile>("beh", "bleh", false, true, 10000)
    ));
    prpl.verifyEvents(ServGotImEvent(
        connection,
        purpleUserName(0),
        "<a href=\"file:///path\">symphony.ogg [audio/whatever]</a>",
        PURPLE_MESSAGE_RECV,
        date
    ));
}

TEST_F(PrivateChatTest, Sticker_AnimatedDisabled)
{
    const int32_t date      = 10001;
    const int32_t fileId[2] = {1234, 1235};
    const int32_t thumbId   = 1236;
    purple_account_set_bool(account, "animated-stickers", FALSE);
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
                fileId[0], 10000, 10000,
                nullptr,
                make_object<remoteFile>("beh", "bleh", false, true, 10000)
            )
        ))
    )));
    tgl.verifyRequests({
        make_object<viewMessages>(chatIds[0], std::vector<int64_t>(1, 1), true),
        make_object<downloadFile>(fileId[0], 1, 0, 0, true)
    });
    prpl.verifyNoEvents();

    tgl.reply(make_object<ok>()); // reply to viewMessages
    tgl.reply(make_object<file>(
        fileId[0], 10000, 10000,
        make_object<localFile>("/sticker", true, true, false, true, 0, 10000, 10000),
        make_object<remoteFile>("beh", "bleh", false, true, 10000)
    ));

    prpl.verifyEvents(ServGotImEvent(
        connection,
        purpleUserName(0),
        "<a href=\"file:///sticker\">sticker</a>",
        PURPLE_MESSAGE_RECV,
        date
    ));

    // Now with thumbnail and main file, both already downloaded
    tgl.update(make_object<updateNewMessage>(makeMessage(
        1,
        userIds[0],
        chatIds[0],
        false,
        date,
        make_object<messageSticker>(make_object<sticker>(
            0, 320, 200, "", true, false, nullptr,
            make_object<photoSize>(
                "whatever",
                make_object<file>(
                    thumbId, 10000, 10000,
                    make_object<localFile>("/thumb", true, true, false, true, 0, 10000, 10000),
                    make_object<remoteFile>("beh", "bleh", false, true, 10000)
                ),
                320, 200
            ),
            make_object<file>(
                fileId[1], 10000, 10000,
                make_object<localFile>("/sticker2.tgs", true, true, false, true, 0, 10000, 10000),
                make_object<remoteFile>("beh", "bleh", false, true, 10000)
            )
        ))
    )));
    tgl.verifyRequests({
        make_object<viewMessages>(chatIds[0], std::vector<int64_t>(1, 1), true),
    });

    tgl.reply(make_object<ok>()); // reply to viewMessages

    prpl.verifyEvents(ServGotImEvent(
        connection,
        purpleUserName(0),
        // Sticker replaced with thumbnail because it's .tgs
        "<a href=\"file:///thumb\">sticker</a>",
        PURPLE_MESSAGE_RECV,
        date
    ));
}

TEST_F(PrivateChatTest, OtherMessage)
{
    const int32_t date = 10001;
    loginWithOneContact();

    tgl.update(make_object<updateNewMessage>(makeMessage(
        1,
        userIds[0],
        chatIds[0],
        false,
        date,
        make_object<messageGame>()
    )));
    tgl.verifyRequest(viewMessages(
        chatIds[0],
        {1},
        true
    ));
    prpl.verifyEvents(
        NewConversationEvent(PURPLE_CONV_TYPE_IM, account, purpleUserName(0)),
        ConversationWriteEvent(
            purpleUserName(0), purpleUserName(0),
            userFirstNames[0] + " " + userLastNames[0] + ": Received unsupported message type messageGame",
            PURPLE_MESSAGE_SYSTEM, date
        )
    );
}

TEST_F(PrivateChatTest, Photo)
{
    purple_account_set_string(account, "media-size-threshold", "0");
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

    tgl.update(make_object<updateFile>(make_object<file>(
        fileId, 10000, 10000,
        make_object<localFile>("/path", true, true, true, false, 0, 0, 5000),
        make_object<remoteFile>("beh", "bleh", false, true, 10000)
    )));

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

    tgl.update(make_object<updateFile>(make_object<file>(
        fileId, 10000, 10000,
        make_object<localFile>("/path", true, true, false, true, 0, 10000, 10000),
        make_object<remoteFile>("beh", "bleh", false, true, 10000)
    )));
}

TEST_F(PrivateChatTest, AlreadyDownloadedPhoto)
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
            make_object<formattedText>("photo", std::vector<object_ptr<textEntity>>()),
            false
        )
    )));
    tgl.verifyRequest(
        viewMessages(chatIds[0], std::vector<int64_t>(1, 1), true)
    );
    prpl.verifyEvents(
        ServGotImEvent(
            connection,
            purpleUserName(0),
            "<img src=\"file:///path\">\n"
            "photo",
            (PurpleMessageFlags)(PURPLE_MESSAGE_RECV | PURPLE_MESSAGE_IMAGES),
            date
        )
    );

    tgl.reply(make_object<ok>()); // reply to viewMessages
}

TEST_F(PrivateChatTest, IgnoredUpdateUserAndNewPrivateChat)
{
    loginWithOneContact();

    tgl.update(standardUpdateUser(0));
    tgl.update(standardPrivateChat(0));
    tgl.update(standardUpdateUser(0));
    tgl.update(standardPrivateChat(0));

    tgl.verifyNoRequests();
    prpl.verifyNoEvents();
}

TEST_F(PrivateChatTest, SendImage)
{
    loginWithOneContact();

    const int64_t msgIdOld[3] = {10, 11, 12};
    const int64_t msgIdNew[3] = {20, 21, 22};
    const int32_t fileId[2] = {101, 102};
    const int32_t messageFailureDate = 1234;
    uint8_t data1[] = {1, 2, 3, 4, 5};
    uint8_t data2[] = {11, 12, 13, 14};

    const int id1 = purple_imgstore_add_with_id(arrayDup(data1, sizeof(data1)), sizeof(data1), "filename1");
    const int id2 = purple_imgstore_add_with_id(arrayDup(data2, sizeof(data2)), sizeof(data2), "filename2");
    const std::string messageText = fmt::format("prefix<img id=\"{}\">caption1<img id=\"{}\">\ncaption2", id1, id2);

    ASSERT_EQ(0, pluginInfo().send_im(connection, purpleUserName(0).c_str(), messageText.c_str(), PURPLE_MESSAGE_SEND));
    tgl.verifyRequests({
        make_object<sendMessage>(
            chatIds[0],
            0,
            nullptr,
            nullptr,
            make_object<inputMessageText>(
                make_object<formattedText>("prefix", std::vector<object_ptr<textEntity>>()),
                false, false
            )
        ),
        make_object<sendMessage>(
            chatIds[0],
            0,
            nullptr,
            nullptr,
            make_object<inputMessagePhoto>(
                make_object<inputFileLocal>(),
                nullptr, std::vector<std::int32_t>(), 0, 0,
                make_object<formattedText>("caption1", std::vector<object_ptr<textEntity>>()),
                0
            )
        ),
        make_object<sendMessage>(
            chatIds[0],
            0,
            nullptr,
            nullptr,
            make_object<inputMessagePhoto>(
                make_object<inputFileLocal>(),
                nullptr, std::vector<std::int32_t>(), 0, 0,
                make_object<formattedText>("caption2", std::vector<object_ptr<textEntity>>()),
                0
            )
        )
    });

    object_ptr<message> msg = makeMessage(
        msgIdOld[0],
        userIds[0],
        chatIds[0],
        true,
        1,
        makeTextMessage("prefix")
    );
    tgl.reply(std::move(msg));

    msg = makeMessage(
        msgIdOld[1],
        userIds[0],
        chatIds[0],
        true,
        1,
        make_object<messagePhoto>(
            makePhotoUploading(fileId[0], sizeof(data1), 0, "/path", 0, 0),
            make_object<formattedText>("caption1", std::vector<object_ptr<textEntity>>()),
            false
        )
    );
    msg->sending_state_ = make_object<messageSendingStatePending>();
    tgl.reply(std::move(msg));

    msg = makeMessage(
        msgIdOld[2],
        userIds[0],
        chatIds[0],
        true,
        1,
        make_object<messagePhoto>(
            makePhotoUploading(fileId[1], sizeof(data2), 0, "/path", 0, 0),
            make_object<formattedText>("caption2", std::vector<object_ptr<textEntity>>()),
            false
        )
    );
    msg->sending_state_ = make_object<messageSendingStatePending>();
    tgl.reply(std::move(msg));

    checkFile(tgl.getInputPhotoPath(0).c_str(), data1, sizeof(data1));
    tgl.update(make_object<updateMessageSendSucceeded>(
        makeMessage(
            msgIdNew[1],
            userIds[0],
            chatIds[0],
            true,
            1,
            make_object<messagePhoto>(
                makePhotoLocal(fileId[0], sizeof(data1), "/path", 0, 0),
                make_object<formattedText>("caption1", std::vector<object_ptr<textEntity>>()),
                false
            )
        ),
        msgIdOld[1]
    ));
    ASSERT_FALSE(g_file_test(tgl.getInputPhotoPath(0).c_str(), G_FILE_TEST_EXISTS));

    checkFile(tgl.getInputPhotoPath(1).c_str(), data2, sizeof(data2));
    tgl.update(make_object<updateMessageSendFailed>(
        makeMessage(
            msgIdNew[2],
            userIds[0],
            chatIds[0],
            true,
            messageFailureDate,
            make_object<messagePhoto>(
                makePhotoLocal(fileId[1], sizeof(data1), "/path", 0, 0),
                make_object<formattedText>("caption2", std::vector<object_ptr<textEntity>>()),
                false
            )
        ),
        msgIdOld[2],
        100, "whatever error"
    ));
    ASSERT_FALSE(g_file_test(tgl.getInputPhotoPath(1).c_str(), G_FILE_TEST_EXISTS));

    prpl.verifyEvents(
        NewConversationEvent(PURPLE_CONV_TYPE_IM, account, purpleUserName(0)),
        ConversationWriteEvent(purpleUserName(0), purpleUserName(0),
                               "Failed to send message: code 100 (whatever error)",
                               PURPLE_MESSAGE_SYSTEM, messageFailureDate)
    );
}

TEST_F(PrivateChatTest, ReplyToOldMessage)
{
    const int32_t date     = 10002;
    const int64_t msgId    = 2;
    const int32_t srcDate  = 10001;
    const int64_t srcMsgId = 1;
    loginWithOneContact();

    object_ptr<message> message = makeMessage(
        msgId,
        userIds[0],
        chatIds[0],
        false,
        date,
        makeTextMessage("reply")
    );
    message->reply_to_message_id_ = srcMsgId;

    tgl.update(make_object<updateNewMessage>(std::move(message)));
    tgl.verifyRequests({
        make_object<viewMessages>(
            chatIds[0],
            std::vector<int64_t>(1, msgId),
            true
        ),
        make_object<getMessage>(
            chatIds[0], srcMsgId
        )
    });
    prpl.verifyNoEvents();

    tgl.reply(make_object<ok>()); // reply to viewMessages
    tgl.reply(makeMessage(
        srcMsgId,
        userIds[0],
        chatIds[0],
        false,
        srcDate,
        makeTextMessage("1<2")
    ));
    prpl.verifyEvents(
        ServGotImEvent(
            connection,
            purpleUserName(0),
            fmt::format(replyPattern, userFirstNames[0] + " " + userLastNames[0], "1&lt;2", "reply"),
            PURPLE_MESSAGE_RECV,
            date
        )
    );
}

TEST_F(PrivateChatTest, ReplyToOldMessage_FetchFailed)
{
    const int32_t date     = 10002;
    const int64_t msgId    = 2;
    const int64_t srcMsgId = 1;
    loginWithOneContact();

    object_ptr<message> message = makeMessage(
        msgId,
        userIds[0],
        chatIds[0],
        false,
        date,
        makeTextMessage("reply")
    );
    message->reply_to_message_id_ = srcMsgId;

    tgl.update(make_object<updateNewMessage>(std::move(message)));
    tgl.verifyRequests({
        make_object<viewMessages>(
            chatIds[0],
            std::vector<int64_t>(1, msgId),
            true
        ),
        make_object<getMessage>(
            chatIds[0], srcMsgId
        )
    });
    prpl.verifyNoEvents();

    runTimeouts();
    prpl.verifyEvents(
        ServGotImEvent(
            connection,
            purpleUserName(0),
            fmt::format(replyPattern, "unknown user", "[message unavailable]", "reply"),
            PURPLE_MESSAGE_RECV,
            date
        )
    );
}

TEST_F(PrivateChatTest, TypingNotification)
{
    loginWithOneContact();

    pluginInfo().send_typing(connection, purpleUserName(0).c_str(), PURPLE_TYPING);
    tgl.verifyRequest(sendChatAction(chatIds[0], make_object<chatActionTyping>()));

    pluginInfo().send_typing(connection, purpleUserName(0).c_str(), PURPLE_TYPED);
    tgl.verifyRequest(sendChatAction(chatIds[0], make_object<chatActionCancel>()));

    pluginInfo().send_typing(connection, purpleUserName(0).c_str(), PURPLE_TYPING);
    tgl.verifyRequest(sendChatAction(chatIds[0], make_object<chatActionTyping>()));

    pluginInfo().send_typing(connection, purpleUserName(0).c_str(), PURPLE_NOT_TYPING);
    tgl.verifyRequest(sendChatAction(chatIds[0], make_object<chatActionCancel>()));
}

TEST_F(PrivateChatTest, DeleteContact)
{
    loginWithOneContact();

    PurpleBuddy *buddy = purple_find_buddy(account, purpleUserName(0).c_str());
    ASSERT_NE(nullptr, buddy);
    PurpleBuddy *dup   = purple_buddy_new(account, buddy->name, buddy->alias);
    purple_blist_remove_buddy(buddy);
    prpl.discardEvents();

    pluginInfo().remove_buddy(connection, dup, NULL);
    purple_buddy_destroy(dup);
    //prpl.verifyEvents(RequestActionEvent(connection, account, purpleUserName(0).c_str(), NULL, 2));
    //prpl.requestedAction("_Yes");

    tgl.verifyRequests({
        make_object<deleteChatHistory>(chatIds[0], true, false),
        make_object<removeContacts>(std::vector<std::int32_t>(1, userIds[0]))
    });

    auto userUpdate1 = standardUpdateUser(0);
    userUpdate1->user_->is_contact_ = true;
    tgl.update(std::move(userUpdate1));
    tgl.update(standardUpdateUser(0));
    tgl.update(make_object<updateChatTitle>(chatIds[0], "New Title"));
    tgl.update(make_object<updateChatChatList>(chatIds[0], make_object<chatListArchive>()));
    tgl.update(make_object<updateChatChatList>(chatIds[0], nullptr));
}

TEST_F(PrivateChatTest, MessageSendResponseError)
{
    loginWithOneContact();

    ASSERT_EQ(0, pluginInfo().send_im(connection, purpleUserName(0).c_str(), "message", PURPLE_MESSAGE_SEND));
    tgl.verifyRequest(sendMessage(
        chatIds[0],
        0,
        nullptr,
        nullptr,
        make_object<inputMessageText>(
            make_object<formattedText>("message", std::vector<object_ptr<textEntity>>()),
            false, false
        )
    ));

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

TEST_F(PrivateChatTest, SendMessage_SpecialCharacters)
{
    loginWithOneContact();

    ASSERT_EQ(0, pluginInfo().send_im(
        connection,
        purpleUserName(0).c_str(),
        "1&lt;2 3&gt;2",
        PURPLE_MESSAGE_SEND
    ));
    tgl.verifyRequest(sendMessage(
        chatIds[0],
        0,
        nullptr,
        nullptr,
        make_object<inputMessageText>(
            // Our mock purple_unescape_html handles these two characters
            make_object<formattedText>("1<2 3>2", std::vector<object_ptr<textEntity>>()),
            false, false
        )
    ));
}

TEST_F(PrivateChatTest, ReceiveMessage_SpecialCharacters)
{
    constexpr int64_t messageId = 10000;
    constexpr int32_t date      = 123456;

    loginWithOneContact();
    tgl.update(make_object<updateNewMessage>(makeMessage(
        messageId,
        userIds[0],
        chatIds[0],
        false,
        date,
        makeTextMessage("1<2 3>2")
    )));
    prpl.verifyEvents(ServGotImEvent(
        connection,
        purpleUserName(0),
        "1&lt;2 3&gt;2", // mock purple_markup_escape_text handles these two
        PURPLE_MESSAGE_RECV,
        date
    ));
    tgl.verifyRequest(viewMessages(chatIds[0], {messageId}, true));
}

TEST_F(PrivateChatTest, WriteToNonContact_CreatePrivateChatFail)
{
    login();
    // Normaly users without chats have to be group chat members, but for the test it doesn't matter
    tgl.update(standardUpdateUser(1));

    ASSERT_EQ(0, pluginInfo().send_im(
        connection,
        (userFirstNames[1] + " " + userLastNames[1]).c_str(),
        "message",
        PURPLE_MESSAGE_SEND
    ));
    tgl.verifyRequest(createPrivateChat(userIds[1], false));

    tgl.reply(make_object<error>(100, "error"));
    prpl.verifyEvents(
        NewConversationEvent(
            PURPLE_CONV_TYPE_IM, account,
            userFirstNames[1] + " " + userLastNames[1]
        ),
        ConversationWriteEvent(
            userFirstNames[1] + " " + userLastNames[1],
            userFirstNames[1] + " " + userLastNames[1],
            "Failed to open chat: code 100 (error)",
            PURPLE_MESSAGE_ERROR, 0
        )
    );
}

TEST_F(PrivateChatTest, WriteToUnknownUser)
{
    login();
    ASSERT_EQ(-1, pluginInfo().send_im(
        connection,
        "Antonie van Leeuwenhoek",
        "message",
        PURPLE_MESSAGE_SEND
    ));

    prpl.verifyEvents(
        NewConversationEvent(
            PURPLE_CONV_TYPE_IM, account,
            "Antonie van Leeuwenhoek"
        ),
        ConversationWriteEvent(
            "Antonie van Leeuwenhoek", "Antonie van Leeuwenhoek",
            "User not found",
            PURPLE_MESSAGE_ERROR, 0
        )
    );
}

TEST_F(PrivateChatTest, BuddyWithNullAlias)
{
    PurpleBuddy *buddy = purple_buddy_new(account, purpleUserName(0).c_str(), NULL);
    purple_blist_add_buddy(buddy, NULL, &standardPurpleGroup, NULL);
    prpl.discardEvents();

    login(
        {standardUpdateUser(0), standardPrivateChat(0), makeUpdateChatListMain(chatIds[0])},
        make_object<users>(1, std::vector<int32_t>(1, userIds[0])),
        make_object<chats>(std::vector<int64_t>(1, chatIds[0])),
        {}, {},
        {
            std::make_unique<ConnectionSetStateEvent>(connection, PURPLE_CONNECTED),
            std::make_unique<AliasBuddyEvent>(purpleUserName(0), userFirstNames[0] + " " + userLastNames[0]),
            std::make_unique<UserStatusEvent>(account, purpleUserName(0), PURPLE_STATUS_OFFLINE),
            std::make_unique<AccountSetAliasEvent>(account, selfFirstName + " " + selfLastName),
            std::make_unique<ShowAccountEvent>(account)
        }
    );
}
