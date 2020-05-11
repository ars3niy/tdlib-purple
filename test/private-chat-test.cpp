#include "fixture.h"

class PrivateChatTest: public CommTest {};

TEST_F(PrivateChatTest, Login)
{
    login();
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

    tgl.update(make_object<updateNewMessage>(makeMessage(
        messageId,
        userIds[0],
        chatIds[0],
        false,
        date,
        makeTextMessage("text")
    )));
    prpl.verifyNoEvents();
    tgl.verifyNoRequests();

    // And only now we get phone number (with +, though in reality it's without)
    tgl.update(make_object<updateUser>(makeUser(
        userIds[0],
        userFirstNames[0],
        userLastNames[0],
        "+" + userPhones[0],
        make_object<userStatusOffline>()
    )));
    prpl.verifyEvents(
        AddBuddyEvent(
            userPhones[0],
            userFirstNames[0] + " " + userLastNames[0],
            account,
            nullptr, nullptr, nullptr
        ),
        ServGotImEvent(
            connection,
            userPhones[0],
            "text",
            PURPLE_MESSAGE_RECV,
            date
        )
    );
    tgl.verifyRequest(viewMessages(
        chatIds[0],
        {messageId},
        true
    ));
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
        userPhones[0],
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
        userPhones[0],
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
        ServGotImEvent(connection, userPhones[0], "document", PURPLE_MESSAGE_RECV, date),
        ConversationWriteEvent(userPhones[0], "", "Sent a file: doc.file.name [mime/type]",
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
        ServGotImEvent(connection, userPhones[0], "video", PURPLE_MESSAGE_RECV, date),
        ConversationWriteEvent(userPhones[0], "", "Sent a video: video.avi [640x480, 120s]",
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
        NewConversationEvent(PURPLE_CONV_TYPE_IM, account, userPhones[0]),
        ConversationWriteEvent(userPhones[0], "", "Received unsupported message type messageAudio",
                               PURPLE_MESSAGE_SYSTEM, date)
    );
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
        NewConversationEvent(PURPLE_CONV_TYPE_IM, account, userPhones[0]),
        ConversationWriteEvent(userPhones[0], "", "Received unsupported message type messageGame",
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
        ServGotImEvent(connection, userPhones[0], "photo", PURPLE_MESSAGE_RECV, date),
        ConversationWriteEvent(userPhones[0], "", "Downloading image", PURPLE_MESSAGE_SYSTEM, date)
    );

    tgl.reply(make_object<ok>()); // reply to viewMessages
    tgl.reply(make_object<file>(
        fileId, 10000, 10000,
        make_object<localFile>("/path", true, true, false, true, 0, 10000, 10000),
        make_object<remoteFile>("beh", "bleh", false, true, 10000)
    ));

    prpl.verifyEvents(ServGotImEvent(connection, userPhones[0], "<img src=\"file:///path\">", PURPLE_MESSAGE_RECV, date));
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
    PurpleGroup group;
    purple_blist_add_buddy(purple_buddy_new(account, userPhones[0].c_str(), "whatever"), NULL, &group, NULL);
    prpl.discardEvents();

    login(
        {standardUpdateUser(0), standardPrivateChat(0)},
        make_object<users>(1, std::vector<int32_t>(1, userIds[0])),
        make_object<chats>(std::vector<int64_t>(1, chatIds[0])),
        {}, {},
        {
            std::make_unique<ConnectionSetStateEvent>(connection, PURPLE_CONNECTED),
            std::make_unique<RemoveBuddyEvent>(account, userPhones[0]),
            std::make_unique<AddBuddyEvent>(userPhones[0], userFirstNames[0] + " " + userLastNames[0],
                                            account, nullptr, &group, nullptr),
            std::make_unique<UserStatusEvent>(account, userPhones[0], PURPLE_STATUS_OFFLINE),
            std::make_unique<AccountSetAliasEvent>(account, selfFirstName + " " + selfLastName),
            std::make_unique<ShowAccountEvent>(account)
        }
    );
}
