#include "purple-events.h"
#include <gtest/gtest.h>

PurpleEventReceiver g_purpleEvents;

void PurpleEventReceiver::addEvent(std::unique_ptr<PurpleEvent> event)
{
    std::cout << "Libpurple event: " << event->toString() << "\n";
    m_events.push(std::move(event));
}

#define COMPARE(param) ASSERT_EQ(expected.param, actual.param)

static void compare(const AccountSetAliasEvent &actual, const AccountSetAliasEvent &expected)
{
    COMPARE(account);
    COMPARE(alias);
}

static void compare(const ShowAccountEvent &actual, const ShowAccountEvent &expected)
{
    COMPARE(account);
}

static void compare(const AddBuddyEvent &actual, const AddBuddyEvent &expected)
{
    COMPARE(username);
    COMPARE(alias);
    COMPARE(account);
    COMPARE(contact);
    COMPARE(group);
    COMPARE(node);
}

static void compare(const AddChatEvent &actual, const AddChatEvent &expected)
{
    COMPARE(name);
    COMPARE(alias);
    COMPARE(account);
    COMPARE(group);
    COMPARE(node);
}

static void compare(const RemoveChatEvent &actual, const RemoveChatEvent &expected)
{
    COMPARE(name);
    COMPARE(inviteLink);
}

static void compare(const HideAccountEvent &actual, const HideAccountEvent &expected)
{
    COMPARE(account);
}

static void compare(const RemoveBuddyEvent &actual, const RemoveBuddyEvent &expected)
{
    COMPARE(account);
    COMPARE(username);
}

static void compare(const ConnectionErrorEvent &actual, const ConnectionErrorEvent &expected)
{
    COMPARE(connection);
    COMPARE(message);
}

static void compare(const ConnectionSetStateEvent &actual, const ConnectionSetStateEvent &expected)
{
    COMPARE(connection);
    COMPARE(state);
}

static void compare(const ConnectionUpdateProgressEvent &actual, const ConnectionUpdateProgressEvent &expected)
{
    COMPARE(connection);
    COMPARE(step);
    COMPARE(stepCount);
}

static void compare(const NewConversationEvent &actual, const NewConversationEvent &expected)
{
    COMPARE(type);
    COMPARE(account);
    COMPARE(name);
}

static void compare(const ConversationWriteEvent &actual, const ConversationWriteEvent &expected)
{
    COMPARE(conversation);
    COMPARE(username);
    COMPARE(message);
    COMPARE(flags);
    COMPARE(mtime);
}

static void compare(const NotifyMessageEvent &actual, const NotifyMessageEvent &expected)
{
    COMPARE(handle);
    COMPARE(type);
    COMPARE(title);
    COMPARE(primary);
    COMPARE(secondary);
}

static void compare(const UserStatusEvent &actual, const UserStatusEvent &expected)
{
    COMPARE(account);
    COMPARE(username);
    COMPARE(status);
}

static void compare(const RequestInputEvent &actual, const RequestInputEvent &expected)
{
    COMPARE(handle);
    COMPARE(title);
    COMPARE(primary);
    COMPARE(account);
    COMPARE(username);
    COMPARE(conv);
}

static void compare(const JoinChatFailedEvent &actual, const JoinChatFailedEvent &expected)
{
    COMPARE(connection);
}

static void compare(const ServGotChatEvent &actual, const ServGotChatEvent &expected)
{
    COMPARE(connection);
    COMPARE(id);
    COMPARE(username);
    COMPARE(message);
    COMPARE(flags);
    COMPARE(mtime);
}

static void compare(const ServGotImEvent &actual, const ServGotImEvent &expected)
{
    COMPARE(connection);
    COMPARE(username);
    COMPARE(message);
    COMPARE(flags);
    COMPARE(mtime);
}

static void compare(const ServGotJoinedChatEvent &actual, const ServGotJoinedChatEvent &expected)
{
    COMPARE(connection);
    COMPARE(id);
    COMPARE(chatName);
    COMPARE(chatAlias);
}

static void compare(const BuddyTypingStartEvent &actual, const BuddyTypingStartEvent &expected)
{
    COMPARE(connection);
    COMPARE(username);
    COMPARE(state);
}

static void compare(const BuddyTypingStopEvent &actual, const BuddyTypingStopEvent &expected)
{
    COMPARE(connection);
    COMPARE(username);
}

static void compare(const PresentConversationEvent &actual, const PresentConversationEvent &expected)
{
    COMPARE(name);
}

static void compare(const ChatAddUserEvent &actual, const ChatAddUserEvent &expected)
{
    COMPARE(chatName);
    COMPARE(user);
    COMPARE(extra_msg);
    COMPARE(flags);
    COMPARE(new_arrival);
}

static void compareEvents(const PurpleEvent &actual, const PurpleEvent &expected)
{
    ASSERT_EQ(expected.type, actual.type) << "Unexpected libpurple event " << actual.toString() <<
                                             ": expected " << expected.toString();

#define C(type) case PurpleEventType::type: \
    compare(static_cast<const type ## Event &>(actual), \
            static_cast<const type ## Event &>(expected)); \
    break;
                                                    
    switch (actual.type) {
        C(AccountSetAlias)
        C(ShowAccount)
        C(AddBuddy)
        C(AddChat)
        C(RemoveChat)
        C(HideAccount)
        C(RemoveBuddy)
        C(ConnectionError)
        C(ConnectionSetState)
        C(ConnectionUpdateProgress)
        C(NewConversation)
        C(ConversationWrite)
        C(NotifyMessage)
        C(UserStatus)
        C(RequestInput)
        C(JoinChatFailed)
        C(ServGotChat)
        C(ServGotIm)
        C(ServGotJoinedChat)
        C(BuddyTypingStart)
        C(BuddyTypingStop)
        C(PresentConversation)
        C(ChatAddUser)
        default:
            ASSERT_TRUE(false) << "Unsupported libpurple event " << actual.toString();
    };
#undef C
}

void PurpleEventReceiver::verifyEvent(const PurpleEvent &event)
{
    ASSERT_FALSE(m_events.empty()) << "Missing libpurple event " << event.toString();
    if (!m_events.empty()) {
        compareEvents(*m_events.front(), event);
        m_events.pop();
    }
}

void PurpleEventReceiver::verifyEvents2(std::initializer_list<std::unique_ptr<PurpleEvent>> events)
{
    for (auto &pEvent: events)
        verifyEvent(*pEvent);
    verifyNoEvents();
}

void PurpleEventReceiver::verifyNoEvents()
{
    ASSERT_TRUE(m_events.empty()) << "Unexpected libpurple event: " << m_events.front()->toString();
}

void PurpleEventReceiver::discardEvents()
{
    while (!m_events.empty()) m_events.pop();
}

std::string PurpleEvent::toString() const
{
#define C(type) case PurpleEventType::type: return #type;
    switch (type) {
    C(AccountSetAlias)
    C(ShowAccount)
    C(AddBuddy)
    C(AddChat)
    C(RemoveChat)
    C(HideAccount)
    C(RemoveBuddy)
    C(ConnectionError)
    C(ConnectionSetState)
    C(ConnectionUpdateProgress)
    C(NewConversation)
    C(ConversationWrite)
    C(NotifyMessage)
    C(UserStatus)
    C(RequestInput)
    C(JoinChatFailed)
    C(ServGotChat)
    C(ServGotIm)
    C(ServGotJoinedChat)
    C(BuddyTypingStart)
    C(BuddyTypingStop)
    C(PresentConversation)
    C(ChatAddUser)
    }
    return std::to_string((unsigned)type);
#undef C
}
