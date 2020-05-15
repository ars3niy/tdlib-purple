#include "fixture.h"

class PrivateChatTest: public CommTest {};

TEST_F(PrivateChatTest, AddContactByPhone)
{
    login();

    PurpleBuddy *buddy = purple_buddy_new(account, userPhones[0].c_str(), "LocalAlias");
    purple_blist_add_buddy(buddy, NULL, &standardPurpleGroup, NULL);
    prpl.discardEvents();

    pluginInfo().add_buddy(connection, buddy, &standardPurpleGroup);
    // The buddy is deleted right away, to be replaced later
    prpl.verifyEvents(RemoveBuddyEvent(account, userPhones[0]));

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
            "LocalAlias",
            "",
            "",
            userIds[0]
        ), true
    ));

    tgl.update(make_object<updateUser>(makeUser(
        userIds[0],
        "LocalAlias",
        "",
        userPhones[0],
        make_object<userStatusOffline>()
    )));
    tgl.reply(make_object<ok>());

    tgl.verifyRequest(createPrivateChat(userIds[0], false));
    prpl.verifyNoEvents();

    tgl.update(make_object<updateNewChat>(makeChat(
        chatIds[0],
        make_object<chatTypePrivate>(userIds[0]),
        "LocalAlias",
        nullptr, 0, 0, 0
    )));

    prpl.verifyEvents(AddBuddyEvent(
        purpleUserName(0),
        "LocalAlias",
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
    tgl.update(standardPrivateChat(0));
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

    tgl.update(standardPrivateChat(0));
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

TEST_F(PrivateChatTest, Document)
{
    const int32_t date = 10001;
    loginWithOneContact();

    tgl.update(make_object<updateNewMessage>(makeMessage(
        1,
        userIds[0],
        chatIds[0],
        false,
        date,
        make_object<messageDocument>(
            make_object<document>("doc.file.name", "mime/type", nullptr, nullptr, nullptr),
            make_object<formattedText>("document", std::vector<object_ptr<textEntity>>())
        )
    )));
    tgl.verifyRequest(viewMessages(
        chatIds[0],
        {1},
        true
    ));
    prpl.verifyEvents(
        ServGotImEvent(connection, purpleUserName(0), "document", PURPLE_MESSAGE_RECV, date),
        ConversationWriteEvent(purpleUserName(0), "", "Sent a file: doc.file.name [mime/type]",
                               PURPLE_MESSAGE_SYSTEM, date)
    );
}

TEST_F(PrivateChatTest, Video)
{
    const int32_t date = 10001;
    loginWithOneContact();

    tgl.update(make_object<updateNewMessage>(makeMessage(
        1,
        userIds[0],
        chatIds[0],
        false,
        date,
        make_object<messageVideo>(
            make_object<video>(120, 640, 480, "video.avi", "video/whatever", false, false, nullptr, nullptr, nullptr),
            make_object<formattedText>("video", std::vector<object_ptr<textEntity>>()),
            false
        )
    )));
    tgl.verifyRequest(viewMessages(
        chatIds[0],
        {1},
        true
    ));
    prpl.verifyEvents(
        ServGotImEvent(connection, purpleUserName(0), "video", PURPLE_MESSAGE_RECV, date),
        ConversationWriteEvent(purpleUserName(0), "", "Sent a video: video.avi [640x480, 120s]",
                               PURPLE_MESSAGE_SYSTEM, date)
    );
}

TEST_F(PrivateChatTest, Audio)
{
    const int32_t date = 10001;
    loginWithOneContact();

    tgl.update(make_object<updateNewMessage>(makeMessage(
        1,
        userIds[0],
        chatIds[0],
        false,
        date,
        make_object<messageAudio>(
            make_object<audio>(25*60, "Symphony #40", "Wolfgang Amadeus Mozart", "symphony.ogg", "audio/whatever", nullptr, nullptr, nullptr),
            make_object<formattedText>("audio", std::vector<object_ptr<textEntity>>())
        )
    )));
    tgl.verifyRequest(viewMessages(
        chatIds[0],
        {1},
        true
    ));
    prpl.verifyEvents(
        NewConversationEvent(PURPLE_CONV_TYPE_IM, account, purpleUserName(0)),
        ConversationWriteEvent(purpleUserName(0), "", "Received unsupported message type messageAudio",
                               PURPLE_MESSAGE_SYSTEM, date)
    );
}

TEST_F(PrivateChatTest, Sticker)
{
    const int32_t date      = 10001;
    const int32_t fileId[2] = {1234, 1235};
    const int32_t thumbId   = 1236;
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

    tgl.reply(make_object<ok>()); // reply to viewMessages
    tgl.reply(make_object<file>(
        fileId[0], 10000, 10000,
        make_object<localFile>("/sticker", true, true, false, true, 0, 10000, 10000),
        make_object<remoteFile>("beh", "bleh", false, true, 10000)
    ));

    prpl.verifyEvents(ServGotImEvent(
        connection,
        purpleUserName(0),
        "<a href=\"file:///sticker\">Sticker</a>",
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
        "<a href=\"file:///thumb\">Sticker</a>",
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
        ConversationWriteEvent(purpleUserName(0), "", "Received unsupported message type messageGame",
                               PURPLE_MESSAGE_SYSTEM, date)
    );
}

TEST_F(PrivateChatTest, Photo)
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
        ConversationWriteEvent(purpleUserName(0), "", "Downloading image", PURPLE_MESSAGE_SYSTEM, date)
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

TEST_F(PrivateChatTest, RenameBuddyAtConnect)
{
    purple_blist_add_buddy(purple_buddy_new(account, purpleUserName(0).c_str(), "whatever"), NULL,
                           &standardPurpleGroup, NULL);
    prpl.discardEvents();

    login(
        {standardUpdateUser(0), standardPrivateChat(0)},
        make_object<users>(1, std::vector<int32_t>(1, userIds[0])),
        make_object<chats>(std::vector<int64_t>(1, chatIds[0])),
        {}, {},
        {
            std::make_unique<ConnectionSetStateEvent>(connection, PURPLE_CONNECTED),
            std::make_unique<RemoveBuddyEvent>(account, purpleUserName(0)),
            std::make_unique<AddBuddyEvent>(purpleUserName(0), userFirstNames[0] + " " + userLastNames[0],
                                            account, nullptr, &standardPurpleGroup, nullptr),
            std::make_unique<UserStatusEvent>(account, purpleUserName(0), PURPLE_STATUS_OFFLINE),
            std::make_unique<AccountSetAliasEvent>(account, selfFirstName + " " + selfLastName),
            std::make_unique<ShowAccountEvent>(account)
        }
    );
}
