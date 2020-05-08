#include "test-transceiver.h"
#include "tdlib-purple.h"
#include "purple-events.h"
#include <gtest/gtest.h>

using namespace td::td_api;

class CommTest: public testing::Test {
public:
    CommTest();

private:
    PurplePlugin      purplePlugin;
protected:
    TestTransceiver      tgl;
    PurpleEventReceiver &prpl = g_purpleEvents;
    PurpleAccount       *account;
    PurpleConnection    *connection;

    const std::string phoneNumber       = "1234567";
    const int         selfId            = 1;
    const std::string selfFirstName     = "Isaac";
    const std::string selfLastName      = "Newton";

    const int32_t     userIds[2]        = {100, 101};
    const int64_t     chatIds[2]        = {1000, 1001};
    const std::string userPhones[2]     = {"00001", "00002"};
    const std::string userFirstNames[2] = {"Gottfried", "Galileo"};
    const std::string userLastNames[2]  = {"Leibniz", "Galilei"};

    void SetUp() override;
    void TearDown() override;
    void login(std::initializer_list<object_ptr<Object>> extraUpdates, object_ptr<users> getContactsReply,
               object_ptr<chats> getChatsReply,
               std::initializer_list<std::unique_ptr<PurpleEvent>> postLoginEvents = {});
    void loginWithOneContact();

    object_ptr<updateUser>    standardUpdateUser(unsigned index);
    object_ptr<updateNewChat> standardPrivateChat(unsigned index);
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
    connection->state = PURPLE_DISCONNECTED;
    connection->account = account;
    account->gc = connection;
    prpl.discardEvents();
}

void CommTest::TearDown()
{
    tgl.verifyNoRequests();
    prpl.verifyNoEvents();
    if (purple_connection_get_protocol_data(connection))
        ((PurplePluginProtocolInfo *)purplePlugin.info->extra_info)->close(connection);
    delete connection;
    purple_account_destroy(account);
}

void CommTest::login(std::initializer_list<object_ptr<Object>> extraUpdates, object_ptr<users> getContactsReply,
                     object_ptr<chats> getChatsReply,
                     std::initializer_list<std::unique_ptr<PurpleEvent>> postLoginEvents)
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
    prpl.verifyEvents({
        std::make_unique<ConnectionSetStateEvent>(connection, PURPLE_CONNECTING),
        std::make_unique<ConnectionUpdateProgressEvent>(connection, 1, 3)
    });

    tgl.update(make_object<updateConnectionState>(make_object<connectionStateUpdating>()));
    tgl.verifyNoRequests();
    prpl.verifyEvent(ConnectionUpdateProgressEvent(connection, 2, 3));

    tgl.update(make_object<updateConnectionState>(make_object<connectionStateReady>()));
    tgl.verifyRequest(getContacts());

    tgl.update(make_object<updateUser>(makeUser(
        selfId,
        selfFirstName,
        selfLastName,
        phoneNumber, // Phone number here without + to make it more interesting
        make_object<userStatusOffline>()
    )));
    for (const object_ptr<Object> &update: extraUpdates)
        tgl.update(std::move(const_cast<object_ptr<Object> &>(update))); // Take that!
    tgl.verifyNoRequests();
    tgl.reply(std::move(getContactsReply));

    tgl.verifyRequest(getChats());
    prpl.verifyNoEvents();
    tgl.reply(std::move(getChatsReply));
    if (postLoginEvents.size() == 0)
        prpl.verifyEvents({
            std::make_unique<ConnectionSetStateEvent>(connection, PURPLE_CONNECTED),
            std::make_unique<AccountSetAliasEvent>(account, selfFirstName + " " + selfLastName),
            std::make_unique<ShowAccountEvent>(account)
        });
    else
        prpl.verifyEvents(std::move(postLoginEvents));
}

void CommTest::loginWithOneContact()
{
    login(
        {standardUpdateUser(0), standardPrivateChat(0)},
        make_object<users>(1, std::vector<int32_t>(1, userIds[0])),
        make_object<chats>(std::vector<int64_t>(1, chatIds[0])),
        {
            std::make_unique<ConnectionSetStateEvent>(connection, PURPLE_CONNECTED),
            std::make_unique<AddBuddyEvent>(userPhones[0], userFirstNames[0] + " " + userLastNames[0],
                                            account, nullptr, nullptr, nullptr),
            std::make_unique<UserStatusEvent>(account, userPhones[0], PURPLE_STATUS_OFFLINE),
            std::make_unique<AccountSetAliasEvent>(account, selfFirstName + " " + selfLastName),
            std::make_unique<ShowAccountEvent>(account)
        }
    );
}

object_ptr<updateUser> CommTest::standardUpdateUser(unsigned index)
{
    return make_object<updateUser>(makeUser(
        userIds[0],
        userFirstNames[0],
        userLastNames[0],
        userPhones[0],
        make_object<userStatusOffline>()
    ));
}

object_ptr<updateNewChat> CommTest::standardPrivateChat(unsigned index)
{
    return make_object<updateNewChat>(makeChat(
        chatIds[0],
        make_object<chatTypePrivate>(userIds[0]),
        userFirstNames[0] + " " + userLastNames[0],
        nullptr, 0, 0, 0
    ));
}

TEST_F(CommTest, Login)
{
    login({}, make_object<users>(), make_object<chats>());
}

TEST_F(CommTest, ContactedByNew)
{
    login({}, make_object<users>(), make_object<chats>());
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
    prpl.verifyEvents({
        std::make_unique<AddBuddyEvent>(
            userPhones[0],
            userFirstNames[0] + " " + userLastNames[0],
            account,
            nullptr, nullptr, nullptr
        ),
        std::make_unique<ServGotImEvent>(
            connection,
            userPhones[0],
            "text",
            PURPLE_MESSAGE_RECV,
            date
        ),
    });
    tgl.verifyRequest(viewMessages(
        chatIds[0],
        {messageId},
        true
    ));
}

TEST_F(CommTest, Document)
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
    prpl.verifyEvents({
        std::make_unique<ServGotImEvent>(connection, userPhones[0], "document", PURPLE_MESSAGE_RECV, date),
        std::make_unique<ConversationWriteEvent>(userPhones[0], "",
                                                 "Sent a file: doc.file.name [mime/type]",
                                                 PURPLE_MESSAGE_SYSTEM, date)
    });
}

TEST_F(CommTest, Video)
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
    prpl.verifyEvents({
        std::make_unique<ServGotImEvent>(connection, userPhones[0], "video", PURPLE_MESSAGE_RECV, date),
        std::make_unique<ConversationWriteEvent>(userPhones[0], "",
                                                 "Sent a video: video.avi [640x480, 120s]",
                                                 PURPLE_MESSAGE_SYSTEM, date)
    });
}

TEST_F(CommTest, Audio)
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
    prpl.verifyEvents({
        std::make_unique<NewConversationEvent>(PURPLE_CONV_TYPE_IM, account, userPhones[0]),
        std::make_unique<ConversationWriteEvent>(userPhones[0], "",
                                                 "Received unsupported message type messageAudio",
                                                 PURPLE_MESSAGE_SYSTEM, date)
    });
}

TEST_F(CommTest, OtherMessage)
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
    prpl.verifyEvents({
        std::make_unique<NewConversationEvent>(PURPLE_CONV_TYPE_IM, account, userPhones[0]),
        std::make_unique<ConversationWriteEvent>(userPhones[0], "",
                                                 "Received unsupported message type messageGame",
                                                 PURPLE_MESSAGE_SYSTEM, date)
    });
}
