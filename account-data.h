#ifndef _ACCOUNT_DATA_H
#define _ACCOUNT_DATA_H

#include <td/telegram/td_api.h>
#include <map>
#include <mutex>

enum {
    CHAT_HISTORY_REQUEST_LIMIT  = 50,
    CHAT_HISTORY_RETRIEVE_LIMIT = 100
};

struct PrivateChat {
    const td::td_api::chat &chat;
    const td::td_api::user &user;

    PrivateChat(const td::td_api::chat &chat, const td::td_api::user &user)
    : chat(chat), user(user) {}
};

struct UserUpdate {
    int32_t userId;
    struct {
        bool status;
    } updates;
};

struct UnreadChat {
    int64_t chatId;
    // Messages are in chronological order, hopefully
    std::vector<td::td_api::object_ptr<td::td_api::message>> messages;
};

struct UserAction {
    int32_t userId;
    bool    isTyping;
};

struct FailedContact {
    std::string phoneNumber;
    td::td_api::object_ptr<td::td_api::error> error;
};

class TdAccountData {
public:
    using TdUserPtr    = td::td_api::object_ptr<td::td_api::user>;
    using TdChatPtr    = td::td_api::object_ptr<td::td_api::chat>;
    using TdMessagePtr = td::td_api::object_ptr<td::td_api::message>;

    void updateUser(TdUserPtr user);
    void addChat(TdChatPtr chat); // Updates existing chat if any
    void setContacts(const std::vector<std::int32_t> &userIds);
    void setActiveChats(std::vector<std::int64_t> &&chats);
    void getContactsWithNoChat(std::vector<std::int32_t> &userIds);
    const td::td_api::chat *getChat(int64_t chatId) const;
    const td::td_api::chat *getPrivateChatByUserId(int32_t userId) const;
    const td::td_api::user *getUser(int32_t userId) const;
    const td::td_api::user *getUserByPhone(const char *phoneNumber) const;
    void updateUserStatus(int32_t userId, td::td_api::object_ptr<td::td_api::UserStatus> status);
    void getPrivateChats(std::vector<PrivateChat> &chats) const;
    void addNewMessage(td::td_api::object_ptr<td::td_api::message> message);
    void getUnreadChatMessages(std::vector<UnreadChat> &chats);
    void getUpdatedUsers(std::vector<UserUpdate> &updates);
    void addUserAction(int32_t userId, bool isTyping);
    void getNewUserActions(std::vector<UserAction> &actions);
    void addNewContactRequest(uint64_t requestId, const char *phoneNumber, int32_t userId = 0);
    bool extractContactRequest(uint64_t requestId, std::string &phoneNumber, int32_t &userId);
    void addFailedContact(std::string &&phoneNumber, td::td_api::object_ptr<td::td_api::error> &&error);
    void getFailedContacts(std::vector<FailedContact> &failedContacts);
private:
    using UserInfoMap = std::map<int32_t, TdUserPtr>;
    using ChatInfoMap = std::map<int64_t, TdChatPtr>;

    struct ContactRequest {
        uint64_t    requestId;
        std::string phoneNumber;
        int32_t     userId;
    };

    UserInfoMap                         m_userInfo;
    ChatInfoMap                         m_chatInfo;
    // List of contacts for which private chat is not known yet.
    std::vector<int32_t>                m_contactUserIdsNoChat;
    // m_chatInfo can contain chats that are not in m_activeChats if some other chat contains
    // messages forwarded from another channel
    std::vector<int64_t>                m_activeChats;
    std::vector<TdMessagePtr>           m_newMessages;
    std::vector<UserUpdate>             m_updatedUsers;
    std::vector<UserAction>             m_userActions;
    std::vector<ContactRequest>         m_addContactRequests;
    std::vector<FailedContact>          m_failedContacts;

    UserUpdate &addUserUpdate(int32_t userId);
};

#endif
