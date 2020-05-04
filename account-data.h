#ifndef _ACCOUNT_DATA_H
#define _ACCOUNT_DATA_H

#include <td/telegram/td_api.h>
#include <map>
#include <mutex>

enum {
    CHAT_HISTORY_REQUEST_LIMIT  = 50,
    CHAT_HISTORY_RETRIEVE_LIMIT = 100
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
    const td::td_api::user *getUserByPrivateChat(const td::td_api::chat &chat);
    void getPrivateChats(std::vector<const td::td_api::chat *> &chats) const;
    void addNewContactRequest(uint64_t requestId, const char *phoneNumber, const char *alias, int32_t userId = 0);
    bool extractContactRequest(uint64_t requestId, std::string &phoneNumber, std::string &alias, int32_t &userId);
private:
    using UserInfoMap = std::map<int32_t, TdUserPtr>;
    using ChatInfoMap = std::map<int64_t, TdChatPtr>;

    struct ContactRequest {
        uint64_t    requestId;
        std::string phoneNumber;
        std::string alias;
        int32_t     userId;
    };

    UserInfoMap                         m_userInfo;
    ChatInfoMap                         m_chatInfo;
    // List of contacts for which private chat is not known yet.
    std::vector<int32_t>                m_contactUserIdsNoChat;
    // m_chatInfo can contain chats that are not in m_activeChats if some other chat contains
    // messages forwarded from another channel
    std::vector<int64_t>                m_activeChats;
    std::vector<ContactRequest>         m_addContactRequests;
};

#endif
