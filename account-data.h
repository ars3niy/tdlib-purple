#ifndef _ACCOUNT_DATA_H
#define _ACCOUNT_DATA_H

#include <td/telegram/td_api.h>
#include <map>
#include <mutex>

bool        isPhoneNumber(const char *s);
const char *getCanonicalPhoneNumber(const char *s);
int32_t     stringToUserId(const char *s);
std::string getDisplayName(const td::td_api::user *user);
bool        isPrivateChat(const td::td_api::chat &chat);
int32_t     getUserIdByPrivateChat(const td::td_api::chat &chat); // return 0 if not private chat
int32_t     getBasicGroupId(const td::td_api::chat &chat); // returns 0 if not chatTypeBasicGroup
int32_t     getSupergroupId(const td::td_api::chat &chat); // returns 0 if not chatTypeSupergroup
bool        isGroupMember(const td::td_api::object_ptr<td::td_api::ChatMemberStatus> &status);

enum {
    CHAT_HISTORY_REQUEST_LIMIT  = 50,
    CHAT_HISTORY_RETRIEVE_LIMIT = 100
};

class PendingRequest {
public:
    uint64_t requestId;

    PendingRequest(uint64_t requestId) : requestId(requestId) {}
    virtual ~PendingRequest() {} // for dynamic_cast
};

class GroupInfoRequest: public PendingRequest {
public:
    int32_t groupId;

    GroupInfoRequest(uint64_t requestId, int32_t groupId)
    : PendingRequest(requestId), groupId(groupId) {}
};

class ContactRequest: public PendingRequest {
public:
    std::string phoneNumber;
    std::string alias;
    std::string groupName;
    int32_t     userId;

    ContactRequest(uint64_t requestId, const std::string &phoneNumber, const std::string &alias,
                   const std::string &groupName, int32_t userId)
    : PendingRequest(requestId), phoneNumber(phoneNumber), alias(alias), groupName(groupName),
      userId(userId) {}
};

class GroupJoinRequest: public PendingRequest {
public:
    std::string inviteLink;

    GroupJoinRequest(uint64_t requestId, const std::string &inviteLink)
    : PendingRequest(requestId), inviteLink(inviteLink) {}
};

class SendMessageRequest: public PendingRequest {
public:
    std::string tempFile;

    SendMessageRequest(uint64_t requestId, const std::string &tempFile)
    : PendingRequest(requestId), tempFile(tempFile) {}
};

class PendingMessage: public PendingRequest {
public:
    int64_t messageId;
    int64_t chatId;

    PendingMessage(uint64_t requestId, int64_t messageId, int64_t chatId)
    : PendingRequest(requestId), messageId(messageId), chatId(chatId) {}
};

class TdAccountData {
public:
    using TdUserPtr       = td::td_api::object_ptr<td::td_api::user>;
    using TdChatPtr       = td::td_api::object_ptr<td::td_api::chat>;
    using TdMessagePtr    = td::td_api::object_ptr<td::td_api::message>;
    using TdGroupPtr      = td::td_api::object_ptr<td::td_api::basicGroup>;
    using TdGroupInfoPtr  = td::td_api::object_ptr<td::td_api::basicGroupFullInfo>;
    using TdSupergroupPtr = td::td_api::object_ptr<td::td_api::supergroup>;

    void updateUser(TdUserPtr user);
    void setUserStatus(int32_t userId, td::td_api::object_ptr<td::td_api::UserStatus> status);
    void updateBasicGroup(TdGroupPtr group);
    void setBasicGroupInfoRequested(int32_t groupId);
    bool isBasicGroupInfoRequested(int32_t groupId);
    void updateBasicGroupInfo(int32_t groupId, TdGroupInfoPtr groupInfo);
    void updateSupergroup(TdSupergroupPtr group);

    void addChat(TdChatPtr chat); // Updates existing chat if any
    void updateChatChatList(int64_t chatId, td::td_api::object_ptr<td::td_api::ChatList> list);
    void updateChatTitle(int64_t chatId, const std::string &title);
    void setContacts(const std::vector<std::int32_t> &userIds);
    void getContactsWithNoChat(std::vector<std::int32_t> &userIds);
    void getChats(std::vector<const td::td_api::chat *> &chats) const;

    const td::td_api::chat       *getChat(int64_t chatId) const;
    int                           getPurpleChatId(int64_t tdChatId);
    const td::td_api::chat       *getChatByPurpleId(int32_t purpleChatId) const;
    const td::td_api::chat       *getPrivateChatByUserId(int32_t userId) const;
    const td::td_api::user       *getUser(int32_t userId) const;
    const td::td_api::user       *getUserByPhone(const char *phoneNumber) const;
    const td::td_api::user       *getUserByPrivateChat(const td::td_api::chat &chat);
    void                          getUsersByDisplayName(const char *displayName,
                                                        std::vector<const td::td_api::user*> &users);
    const td::td_api::basicGroup *getBasicGroup(int32_t groupId) const;
    const td::td_api::basicGroupFullInfo *getBasicGroupInfo(int32_t groupId) const;
    const td::td_api::supergroup *getSupergroup(int32_t groupId) const;
    const td::td_api::chat       *getBasicGroupChatByGroup(int32_t groupId) const;
    const td::td_api::chat       *getSupergroupChatByGroup(int32_t groupId) const;
    bool                          isGroupChatWithMembership(const td::td_api::chat &chat);

    template<typename ReqType, typename... ArgsType>
    void addPendingRequest(ArgsType... args)
    {
        m_requests.push_back(std::make_unique<ReqType>(args...));
    }
    template<typename ReqType>
    void addPendingRequest(uint64_t requestId, std::unique_ptr<ReqType> &&request)
    {
        m_requests.push_back(std::move(request));
        m_requests.back()->requestId = requestId;
    }
    template<typename ReqType>
    std::unique_ptr<ReqType> getPendingRequest(uint64_t requestId)
    {
        return std::unique_ptr<ReqType>(dynamic_cast<ReqType *>(getPendingRequestImpl(requestId).release()));
    }

    const ContactRequest      *findContactRequest(int32_t userId);
    void                       addTempFileUpload(int64_t messageId, const std::string &path);
    std::string                extractTempFileUpload(int64_t messageId);

    void                       saveMessage(TdMessagePtr message);
    td::td_api::message       *findMessage(int64_t messageId);
private:
    struct ChatInfo {
        int32_t   purpleId;
        TdChatPtr chat;

        ChatInfo() : purpleId(0), chat() {}
    };

    struct GroupInfo {
        TdGroupPtr     group;
        TdGroupInfoPtr fullInfo;
        bool           fullInfoRequested = false;
    };

    struct SendMessageInfo {
        int64_t     messageId;
        std::string tempFile;
    };

    using ChatMap = std::map<int64_t, ChatInfo>;
    using UserMap = std::map<int32_t, TdUserPtr>;
    UserMap                            m_userInfo;
    ChatMap                            m_chatInfo;
    std::map<int32_t, GroupInfo>       m_groups;
    std::map<int32_t, TdSupergroupPtr> m_supergroups;
    int                                m_lastChatPurpleId = 0;

    // List of contacts for which private chat is not known yet.
    std::vector<int32_t>               m_contactUserIdsNoChat;

    // Used to remember stuff during asynchronous communication when adding contact
    std::vector<ContactRequest>        m_addContactRequests;

    std::vector<std::unique_ptr<PendingRequest>> m_requests;
    std::vector<SendMessageInfo>       m_sentMessages;
    std::map<int64_t, TdMessagePtr>    m_messages;

    std::unique_ptr<PendingRequest>    getPendingRequestImpl(uint64_t requestId);
};

#endif
