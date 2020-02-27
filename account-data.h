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

struct UnreadChat {
    int64_t chatId;
    // Messages are in chronological order, hopefully
    std::vector<td::td_api::object_ptr<td::td_api::message>> messages;
};

enum class ChatHistoryResult: uint8_t {
    Finished,
    Unfinished,
};

class TdAccountData {
public:
    using TdUserPtr    = td::td_api::object_ptr<td::td_api::user>;
    using TdChatPtr    = td::td_api::object_ptr<td::td_api::chat>;
    using TdMessagePtr = td::td_api::object_ptr<td::td_api::message>;

    class Lock: public std::unique_lock<std::mutex> {
    public:
        Lock(TdAccountData &data)
        : std::unique_lock<std::mutex>(data.m_dataMutex)
        {}
    };

    void updateUser(TdUserPtr user);
    void addNewChat(TdChatPtr chat);
    void setActiveChats(std::vector<std::int64_t> &&chats);
    const td::td_api::chat *getChat(int64_t chatId) const;
    const td::td_api::user *getUser(int32_t userId) const;
    void getPrivateChats(std::vector<PrivateChat> &chats) const;

    // This is for showing newly arrived messages
    void addNewMessage(td::td_api::object_ptr<td::td_api::message> message);
    void getUnreadChatMessages(std::vector<UnreadChat> &chats);
private:
    std::map<int32_t, TdUserPtr>        m_userInfo;
    std::map<int64_t, TdChatPtr>        m_chatInfo;
    // m_chatInfo can contain chats that are not in m_activeChats if some other chat contains
    // messages forwarded from another channel
    std::vector<int64_t>                m_activeChats;
    std::vector<TdMessagePtr>           m_newMessages;
    std::mutex                          m_dataMutex;
};

#endif
