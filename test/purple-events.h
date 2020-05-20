#ifndef _PURPLE_EVENTS_H
#define _PURPLE_EVENTS_H

#include <purple.h>
#include <stdint.h>
#include <string>
#include <memory>
#include <queue>
#include <iostream>

struct PurpleEvent;

extern "C" {
    extern PurpleGroup standardPurpleGroup;
}

class PurpleEventReceiver {
public:
    void addEvent(std::unique_ptr<PurpleEvent> event);

    // Check that given events in that order, and no others, are in the queue, and clear the queue
    template<typename... EventTypes>
    void verifyEvents(const PurpleEvent &event, EventTypes... args)
    {
        verifyEvent(event);
        verifyEvents(args...);
    }

    void verifyEvents2(std::initializer_list<std::unique_ptr<PurpleEvent>> events);
    void verifyNoEvents();
    void discardEvents();

    void inputEnter(const gchar *value);
    void inputCancel();
private:
    void verifyEvent(const PurpleEvent &event);
    void verifyEvents()
    {
        verifyNoEvents();
    }

    std::queue<std::unique_ptr<PurpleEvent>> m_events;
    void      *inputUserData = NULL;
    GCallback  inputOkCb     = NULL;
    GCallback  inputCancelCb = NULL;
};

extern PurpleEventReceiver g_purpleEvents;

enum class PurpleEventType: uint8_t {
    AccountSetAlias,
    ShowAccount,
    AddBuddy,
    AddChat,
    RemoveChat,
    AliasChat,
    HideAccount,
    RemoveBuddy,
    AliasBuddy,
    ConnectionError,
    ConnectionSetState,
    ConnectionUpdateProgress,
    NewConversation,
    ConversationWrite,
    ConvSetTitle,
    NotifyMessage,
    UserStatus,
    RequestInput,
    JoinChatFailed,
    ServGotChat,
    ServGotIm,
    ServGotJoinedChat,
    BuddyTypingStart,
    BuddyTypingStop,
    PresentConversation,
    ChatAddUser,
    ChatClearUsers,
};

struct PurpleEvent {
    PurpleEventType type;

    PurpleEvent(PurpleEventType type) : type(type) {}
    virtual ~PurpleEvent() {}
    std::string toString() const;
};

struct AccountSetAliasEvent: PurpleEvent {
    PurpleAccount *account;
    std::string    alias;

    AccountSetAliasEvent(PurpleAccount *account, const std::string &alias)
    : PurpleEvent(PurpleEventType::AccountSetAlias), account(account), alias(alias) {}
};

struct ShowAccountEvent: PurpleEvent {
    PurpleAccount *account;

    ShowAccountEvent(PurpleAccount *account)
    : PurpleEvent(PurpleEventType::ShowAccount), account(account) {}
};

struct AddBuddyEvent: PurpleEvent {
    std::string      username;
    std::string      alias;
    PurpleAccount   *account;
    PurpleContact   *contact;
    PurpleGroup     *group;
    PurpleBlistNode *node;

    AddBuddyEvent(const std::string &username, const std::string &alias, PurpleAccount *account,
                  PurpleContact *contact, PurpleGroup *group, PurpleBlistNode *node)
    : PurpleEvent(PurpleEventType::AddBuddy), username(username), alias(alias), account(account),
      contact(contact), group(group), node(node) {}
};

struct AddChatEvent: PurpleEvent {
    std::string      name;
    std::string      alias;
    PurpleAccount   *account;
    PurpleGroup     *group;
    PurpleBlistNode *node;

    AddChatEvent(const std::string &name, const std::string &alias, PurpleAccount *account,
                 PurpleGroup *group, PurpleBlistNode *node)
    : PurpleEvent(PurpleEventType::AddChat), name(name), alias(alias), account(account),
      group(group), node(node) {}
};

struct RemoveChatEvent: public PurpleEvent {
    std::string name;
    std::string inviteLink;

    RemoveChatEvent(const std::string &name, const std::string &inviteLink)
    : PurpleEvent(PurpleEventType::RemoveChat), name(name), inviteLink(inviteLink) {}
};

struct AliasChatEvent: PurpleEvent {
    std::string name;
    std::string newAlias;

    AliasChatEvent(const std::string &username, const std::string &newAlias)
    : PurpleEvent(PurpleEventType::AliasChat), name(username), newAlias(newAlias) {}
};

struct HideAccountEvent: PurpleEvent {
    PurpleAccount *account;
};

struct RemoveBuddyEvent: PurpleEvent {
    PurpleAccount *account;
    std::string    username;

    RemoveBuddyEvent(PurpleAccount *account, const std::string &username)
    : PurpleEvent(PurpleEventType::RemoveBuddy), account(account), username(username) {}
};

struct AliasBuddyEvent: PurpleEvent {
    std::string username;
    std::string newAlias;

    AliasBuddyEvent(const std::string &username, const std::string &newAlias)
    : PurpleEvent(PurpleEventType::AliasBuddy), username(username), newAlias(newAlias) {}
};

struct ConnectionErrorEvent: PurpleEvent {
    PurpleConnection *connection;
    std::string       message;
};

struct ConnectionSetStateEvent: PurpleEvent {
    PurpleConnection      *connection;
    PurpleConnectionState  state;

    ConnectionSetStateEvent(PurpleConnection *connection, PurpleConnectionState state)
    : PurpleEvent(PurpleEventType::ConnectionSetState), connection(connection), state(state) {}
};

struct ConnectionUpdateProgressEvent: PurpleEvent {
    PurpleConnection *connection;
    size_t            step, stepCount;

    ConnectionUpdateProgressEvent(PurpleConnection *connection,
                                  size_t step, size_t stepCount)
    : PurpleEvent(PurpleEventType::ConnectionUpdateProgress), connection(connection),
      step(step), stepCount(stepCount) {}
};

struct NewConversationEvent: PurpleEvent {
    PurpleConversationType  type;
    PurpleAccount          *account;
    std::string             name;

    NewConversationEvent(PurpleConversationType type, PurpleAccount *account, const std::string &name)
    : PurpleEvent(PurpleEventType::NewConversation), type(type), account(account), name(name) {}
};

struct ConversationWriteEvent: PurpleEvent {
    std::string        conversation;
    std::string        username;
    std::string        message;
    PurpleMessageFlags flags;
    time_t             mtime;

    ConversationWriteEvent(const std::string &conversationName, const std::string &username,
                           const std::string &message, PurpleMessageFlags flags, time_t mtime)
    : PurpleEvent(PurpleEventType::ConversationWrite), conversation(conversationName),
    username(username), message(message), flags(flags), mtime(mtime) {}
};

struct ConvSetTitleEvent: PurpleEvent {
    std::string name;
    std::string newTitle;

    ConvSetTitleEvent(const std::string name, const std::string &newTitle)
    : PurpleEvent(PurpleEventType::ConvSetTitle), name(name), newTitle(newTitle) {}
};

struct NotifyMessageEvent: PurpleEvent {
    void                *handle;
    PurpleNotifyMsgType  type;
    std::string          title;
    std::string          primary;
    std::string          secondary;
};

struct UserStatusEvent: PurpleEvent {
    PurpleAccount        *account;
    std::string           username;
    PurpleStatusPrimitive status;

    UserStatusEvent(PurpleAccount *account, const std::string &username, PurpleStatusPrimitive status)
    : PurpleEvent(PurpleEventType::UserStatus), account(account), username(username), status(status) {}
};

struct RequestInputEvent: PurpleEvent {
    void               *handle;
    std::string         title, primary;
	std::string         secondary, default_value;
	GCallback           ok_cb;
	GCallback           cancel_cb;
	PurpleAccount      *account;
    std::string         username;
    PurpleConversation *conv;
	void               *user_data;

    RequestInputEvent(void *handle, const char *title, const char *primary,
                      const char *secondary, const char *default_value,
                      const char *ok_text, GCallback ok_cb,
                      const char *cancel_text, GCallback cancel_cb,
                      PurpleAccount *account, const char *who, PurpleConversation *conv,
                      void *user_data)
    : PurpleEvent(PurpleEventType::RequestInput),
      handle(handle),
      title(title ? title : ""),
      primary(primary ? primary : ""),
      secondary(secondary ? secondary : ""),
      default_value(default_value ? default_value : ""),
      ok_cb(ok_cb),
      cancel_cb(cancel_cb),
      account(account),
      username(who ? who : ""),
      conv(conv),
      user_data(user_data)
      {}
    RequestInputEvent(void *handle, PurpleAccount *account, const char *who, PurpleConversation *conv)
    : PurpleEvent(PurpleEventType::RequestInput),
      handle(handle),
      account(account),
      username(who ? who : ""),
      conv(conv)
      {}
};

struct JoinChatFailedEvent: PurpleEvent {
    PurpleConnection *connection;
};

struct ServGotChatEvent: public PurpleEvent {
    PurpleConnection  *connection;
    int                id;
    std::string        username;
    std::string        message;
    PurpleMessageFlags flags;
    time_t             mtime;

    ServGotChatEvent(PurpleConnection *connection, int id, const std::string &username, const std::string &message,
                   PurpleMessageFlags flags, time_t mtime)
    : PurpleEvent(PurpleEventType::ServGotChat), connection(connection), id(id), username(username),
      message(message), flags(flags), mtime(mtime) {}
};

struct ServGotImEvent: PurpleEvent {
    PurpleConnection  *connection;
    std::string        username;
    std::string        message;
    PurpleMessageFlags flags;
    time_t             mtime;

    ServGotImEvent(PurpleConnection *connection, const std::string &username, const std::string &message,
                   PurpleMessageFlags flags, time_t mtime)
    : PurpleEvent(PurpleEventType::ServGotIm), connection(connection), username(username),
      message(message), flags(flags), mtime(mtime) {}
};

struct ServGotJoinedChatEvent: public PurpleEvent {
    PurpleConnection  *connection;
    int                id;
    std::string        chatName;
    std::string        conversationTitle;

    ServGotJoinedChatEvent(PurpleConnection *connection, int id, const std::string &chatName,
                           const std::string &chatAlias)
    : PurpleEvent(PurpleEventType::ServGotJoinedChat), connection(connection), id(id),
      chatName(chatName), conversationTitle(chatAlias) {}
};

struct BuddyTypingStartEvent: PurpleEvent {
    PurpleConnection *connection;
    std::string       username;
    int               timeout;
    PurpleTypingState state;
};

struct BuddyTypingStopEvent: PurpleEvent {
    PurpleConnection *connection;
    std::string       username;
};

struct PresentConversationEvent: PurpleEvent {
    std::string name;
    PresentConversationEvent(const std::string &name)
    : PurpleEvent(PurpleEventType::PresentConversation), name(name) {}
};

struct ChatAddUserEvent: PurpleEvent {
    std::string chatName;
    std::string user;
    std::string extra_msg;
    PurpleConvChatBuddyFlags flags;
    bool new_arrival;

    ChatAddUserEvent(const std::string &chatName, const std::string &user, const std::string &extra_msg,
                      PurpleConvChatBuddyFlags flags, gboolean new_arrival)
    : PurpleEvent(PurpleEventType::ChatAddUser), chatName(chatName), user(user), extra_msg(extra_msg),
      flags(flags), new_arrival(new_arrival) {}
};

struct ChatClearUsersEvent: PurpleEvent {
    std::string chatName;

    ChatClearUsersEvent(const std::string &chatName)
    : PurpleEvent(PurpleEventType::ChatClearUsers), chatName(chatName) {}
};

#endif
