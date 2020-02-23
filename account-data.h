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
    // Messages are in the same order as the one in which Telegram retrieves history -
    // that is, reverse chronological order
    std::vector<td::td_api::object_ptr<td::td_api::message>> messages;
};

enum class ChatHistoryResult: uint8_t {
    Finished,
    Unfinished,
};

class TdAccountData {
public:
    using TdUserPtr = td::td_api::object_ptr<td::td_api::user>;
    using TdChatPtr = td::td_api::object_ptr<td::td_api::chat>;

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
    void addUnreadHistoryRequest(uint64_t queryId, int64_t chatId, int64_t lastReadInId,
                                 int64_t lastReadOutId);
    void addFollowUpHistoryRequest(uint64_t lastQueryId, uint64_t newQueryId);
    ChatHistoryResult handleHistoryResponse(uint64_t requestId,
                                            td::td_api::object_ptr<td::td_api::Object> object,
                                            int64_t &chatId,
                                            int64_t &oldestMessageId);
    void getUnreadChatMessages(std::vector<UnreadChat> &chats);
private:
    struct RequestHistoryState {
        uint64_t queryId;
        int64_t  chatId;
        int64_t  lastReadInId;
        int64_t  lastReadOutId;
        int64_t  oldestSeenInId;
        int64_t  oldestSeenOutId;
        bool     inboxFinished;
        bool     outboxFinished;

        std::vector<td::td_api::object_ptr<td::td_api::message>> messages;
    };

    std::map<int32_t, TdUserPtr>        m_userInfo;
    std::map<int64_t, TdChatPtr>        m_chatInfo;
    // m_chatInfo can contain chats that are not in m_activeChats if some other chat contains
    // messages forwarded from another channel
    std::vector<int64_t>                m_activeChats;
    std::vector<RequestHistoryState>    m_chatHistoryRequests;
    std::mutex                          m_dataMutex;
};

#endif
