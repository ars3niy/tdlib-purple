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

TEST_F(MessageOrderTest, FlushAtLogout)
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
