#include "purple-events.h"
#include <gtest/gtest.h>

PurpleEventReceiver g_purpleEvents;

void PurpleEventReceiver::addEvent(std::unique_ptr<PurpleEvent> event)
{
    std::cout << "Libpurple event: " << event->toString() << "\n";
    m_events.push(std::move(event));
}

static void compare(const AccountSetAliasEvent &actual, const AccountSetAliasEvent &expected)
{
    ASSERT_EQ(expected.account, actual.account);
    ASSERT_EQ(expected.alias, actual.alias);
}

static void compare(const ShowAccountEvent &actual, const ShowAccountEvent &expected)
{
    ASSERT_EQ(expected.account, actual.account);
}

static void compare(const AddBuddyEvent &actual, const AddBuddyEvent &expected)
{
    ASSERT_EQ(expected.username, actual.username);
    ASSERT_EQ(expected.alias, actual.alias);
    ASSERT_EQ(expected.account, actual.account);
    ASSERT_EQ(expected.contact, actual.contact);
    ASSERT_EQ(expected.group, actual.group);
    ASSERT_EQ(expected.node, actual.node);
}

static void compare(const AddChatEvent &actual, const AddChatEvent &expected)
{
    ASSERT_EQ(expected.name, actual.name);
    ASSERT_EQ(expected.alias, actual.alias);
    ASSERT_EQ(expected.account, actual.account);
    ASSERT_EQ(expected.group, actual.group);
    ASSERT_EQ(expected.node, actual.node);
}

static void compare(const HideAccountEvent &actual, const HideAccountEvent &expected)
{
    ASSERT_EQ(expected.account, actual.account);
}

static void compare(const RemoveBuddyEvent &actual, const RemoveBuddyEvent &expected)
{
    ASSERT_EQ(expected.account, actual.account);
    ASSERT_EQ(expected.username, actual.username);
}

static void compare(const ConnectionErrorEvent &actual, const ConnectionErrorEvent &expected)
{
    ASSERT_EQ(expected.connection, actual.connection);
    ASSERT_EQ(expected.message, actual.message);
}

static void compare(const ConnectionSetStateEvent &actual, const ConnectionSetStateEvent &expected)
{
    ASSERT_EQ(expected.connection, actual.connection);
    ASSERT_EQ(expected.state, actual.state);
}

static void compare(const ConnectionUpdateProgressEvent &actual, const ConnectionUpdateProgressEvent &expected)
{
    ASSERT_EQ(expected.connection, actual.connection);
    ASSERT_EQ(expected.step, actual.step);
    ASSERT_EQ(expected.stepCount, actual.stepCount);
}

static void compare(const NewConversationEvent &actual, const NewConversationEvent &expected)
{
    ASSERT_EQ(expected.type, actual.type);
    ASSERT_EQ(expected.account, actual.account);
    ASSERT_EQ(expected.name, actual.name);
}

static void compare(const ConversationWriteEvent &actual, const ConversationWriteEvent &expected)
{
    ASSERT_EQ(expected.conversation, actual.conversation);
    ASSERT_EQ(expected.username, actual.username);
    ASSERT_EQ(expected.message, actual.message);
    ASSERT_EQ(expected.flags, actual.flags);
    ASSERT_EQ(expected.mtime, actual.mtime);
}

static void compare(const NotifyMessageEvent &actual, const NotifyMessageEvent &expected)
{
    ASSERT_EQ(expected.handle, actual.handle);
    ASSERT_EQ(expected.type, actual.type);
    ASSERT_EQ(expected.title, actual.title);
    ASSERT_EQ(expected.primary, actual.primary);
    ASSERT_EQ(expected.secondary, actual.secondary);
}

static void compare(const UserStatusEvent &actual, const UserStatusEvent &expected)
{
    ASSERT_EQ(expected.account, actual.account);
    ASSERT_EQ(expected.username, actual.username);
    ASSERT_EQ(expected.status, actual.status);
}

static void compare(const RequestInputEvent &actual, const RequestInputEvent &expected)
{
    ASSERT_EQ(expected.handle, actual.handle);
    ASSERT_EQ(expected.title, actual.title);
    ASSERT_EQ(expected.primary, actual.primary);
    ASSERT_EQ(expected.account, actual.account);
    ASSERT_EQ(expected.username, actual.username);
    ASSERT_EQ(expected.conv, actual.conv);
}

static void compare(const JoinChatFailedEvent &actual, const JoinChatFailedEvent &expected)
{
    ASSERT_EQ(expected.connection, actual.connection);
}

static void compare(const ServGotChatEvent &actual, const ServGotChatEvent &expected)
{
    ASSERT_EQ(expected.connection, actual.connection);
    ASSERT_EQ(expected.id, actual.id);
    ASSERT_EQ(expected.username, actual.username);
    ASSERT_EQ(expected.message, actual.message);
    ASSERT_EQ(expected.flags, actual.flags);
    ASSERT_EQ(expected.mtime, actual.mtime);
}

static void compare(const ServGotImEvent &actual, const ServGotImEvent &expected)
{
    ASSERT_EQ(expected.connection, actual.connection);
    ASSERT_EQ(expected.username, actual.username);
    ASSERT_EQ(expected.message, actual.message);
    ASSERT_EQ(expected.flags, actual.flags);
    ASSERT_EQ(expected.mtime, actual.mtime);
}

static void compare(const ServGotJoinedChatEvent &actual, const ServGotJoinedChatEvent &expected)
{
    ASSERT_EQ(expected.connection, actual.connection);
    ASSERT_EQ(expected.id, actual.id);
    ASSERT_EQ(expected.chatName, actual.chatName);
    ASSERT_EQ(expected.chatAlias, actual.chatAlias);
}

static void compare(const BuddyTypingStartEvent &actual, const BuddyTypingStartEvent &expected)
{
    ASSERT_EQ(expected.connection, actual.connection);
    ASSERT_EQ(expected.username, actual.username);
    ASSERT_EQ(expected.state, actual.state);
}

static void compare(const BuddyTypingStopEvent &actual, const BuddyTypingStopEvent &expected)
{
    ASSERT_EQ(expected.connection, actual.connection);
    ASSERT_EQ(expected.username, actual.username);
}

static void compare(const PresentConversationEvent &actual, const PresentConversationEvent &expected)
{
    ASSERT_EQ(expected.name, actual.name);
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
    }
    return std::to_string((unsigned)type);
#undef C
}
