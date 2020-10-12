#include "fixture.h"
#include "libpurple-mock.h"
#include <fmt/format.h>

class MessageOrderTest: public CommTest {};

TEST_F(MessageOrderTest, ReplyOrdering)
{
    const int32_t dates[2]  = {10002, 10003};
    const int64_t msgIds[2] = {2, 3};
    const int32_t srcDate  = 10001;
    const int64_t srcMsgId = 1;
    loginWithOneContact();

    object_ptr<message> message = makeMessage(
        msgIds[0], userIds[0], chatIds[0], false, dates[0], makeTextMessage("reply")
    );
    message->reply_to_message_id_ = srcMsgId;

    tgl.update(make_object<updateNewMessage>(std::move(message)));
    std::vector<uint64_t> requestIds = tgl.verifyRequests({
        make_object<viewMessages>(chatIds[0], std::vector<int64_t>(1, msgIds[0]), true),
        make_object<getMessage>(chatIds[0], srcMsgId)
    });
    prpl.verifyNoEvents();

    tgl.update(make_object<updateNewMessage>(makeMessage(
        msgIds[1], userIds[0], chatIds[0], false, dates[1], makeTextMessage("followUp")
    )));
    tgl.verifyRequest(viewMessages(chatIds[0], {msgIds[1]}, true));
    prpl.verifyNoEvents();

    tgl.reply(requestIds.at(1), makeMessage(srcMsgId, userIds[0], chatIds[0], false, srcDate, makeTextMessage("original")));
    prpl.verifyEvents(
        ServGotImEvent(
            connection, purpleUserName(0),
            fmt::format(replyPattern, userFirstNames[0] + " " + userLastNames[0], "original", "reply"),
            PURPLE_MESSAGE_RECV, dates[0]
        ),
        ServGotImEvent(connection, purpleUserName(0), "followUp", PURPLE_MESSAGE_RECV, dates[1])
    );
}

TEST_F(MessageOrderTest, Reply_FlushAtLogout)
{
    const int32_t dates[2]  = {10002, 10003};
    const int64_t msgIds[2] = {2, 3};
    const int64_t srcMsgId  = 1;
    loginWithOneContact();

    object_ptr<message> message = makeMessage(
        msgIds[0], userIds[0], chatIds[0], false, dates[0], makeTextMessage("reply")
    );
    message->reply_to_message_id_ = srcMsgId;

    tgl.update(make_object<updateNewMessage>(std::move(message)));
    tgl.verifyRequests({
        make_object<viewMessages>( chatIds[0], std::vector<int64_t>(1, msgIds[0]), true),
        make_object<getMessage>(chatIds[0], srcMsgId)
    });
    prpl.verifyNoEvents();

    tgl.reply(make_object<ok>()); // reply to viewMessages

    tgl.update(make_object<updateNewMessage>(makeMessage(
        msgIds[1], userIds[0], chatIds[0], false, dates[1], makeTextMessage("followUp")
    )));
    tgl.verifyRequest(viewMessages(chatIds[0], {msgIds[1]}, true));
    prpl.verifyNoEvents();

    pluginInfo().close(connection);
    prpl.verifyEvents(
        ServGotImEvent(
            connection, purpleUserName(0),
            fmt::format(replyPattern, "Unknown user", "[message unavailable]", "reply"),
            PURPLE_MESSAGE_RECV, dates[0]
        ),
        ServGotImEvent(connection, purpleUserName(0), "followUp", PURPLE_MESSAGE_RECV, dates[1])
    );
}

TEST_F(MessageOrderTest, Photo_Download_FlushAtLogout)
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
    prpl.verifyNoEvents();

    pluginInfo().close(connection);
    prpl.verifyEvents(
        ServGotImEvent(connection, purpleUserName(0), "photo", PURPLE_MESSAGE_RECV, date),
        ConversationWriteEvent(
            purpleUserName(0), purpleUserName(0),
            userFirstNames[0] + " " + userLastNames[0] + ": Downloading photo",
            PURPLE_MESSAGE_SYSTEM, date
        )
    );
}
