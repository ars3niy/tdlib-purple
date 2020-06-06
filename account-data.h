#ifndef _ACCOUNT_DATA_H
#define _ACCOUNT_DATA_H

#include <td/telegram/td_api.h>
#include <map>
#include <mutex>
#include <set>
#include <purple.h>

bool        isPhoneNumber(const char *s);
const char *getCanonicalPhoneNumber(const char *s);
int32_t     stringToUserId(const char *s);
bool        isPrivateChat(const td::td_api::chat &chat);
int32_t     getUserIdByPrivateChat(const td::td_api::chat &chat); // return 0 if not private chat
bool        isChatInContactList(const td::td_api::chat &chat, const td::td_api::user *privateChatUser);
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
    virtual ~PendingRequest() {}
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
    int64_t     chatId;
    std::string tempFile;

    SendMessageRequest(uint64_t requestId, int64_t chatId, const std::string &tempFile = "")
    : PendingRequest(requestId), chatId(chatId), tempFile(tempFile) {}
};

class PendingMessage: public PendingRequest {
public:
    td::td_api::object_ptr<td::td_api::message> message;

    PendingMessage(uint64_t requestId, td::td_api::message *message)
    : PendingRequest(requestId), message(message) {}
};

class UploadRequest: public PendingRequest {
public:
    PurpleXfer *xfer;
    int64_t     chatId;

    UploadRequest(uint64_t requestId, PurpleXfer *xfer, int64_t chatId)
    : PendingRequest(requestId), xfer(xfer), chatId(chatId) {}
};

struct TgMessageInfo {
    enum class Type {
        Sticker,
        Other
    };
    Type        type;
    std::string sender;
    time_t      timestamp;
    bool        outgoing;
    int64_t     repliedMessageId = 0;
    td::td_api::object_ptr<td::td_api::message> repliedMessage;
    std::string forwardedFrom;
};

class PurpleTdClient;
using FileDownloadCb = void (PurpleTdClient::*)(int64_t chatId, TgMessageInfo &message,
                                                const std::string &filePath, const char *caption,
                                                const std::string &fileDescription,
                                                td::td_api::object_ptr<td::td_api::file> thumbnail);

// Used for matching completed downloads to chats they belong to, and for starting PurpleXfer for
// time-consuming downloads
class DownloadRequest: public PendingRequest {
public:
    int64_t        chatId;
    TgMessageInfo  message;
    int32_t        fileId;
    int32_t        fileSize;
    int32_t        downloadedSize;
    std::string    fileDescription;
    int            tempFd = -1;
    std::string    tempFileName;
    td::td_api::object_ptr<td::td_api::file> thumbnail;
    FileDownloadCb callback;

    // Could not pass object_ptr through variadic funciton :(
    DownloadRequest(uint64_t requestId, int64_t chatId, TgMessageInfo &message,
                    int32_t fileId, int32_t fileSize, const std::string &fileDescription,
                    td::td_api::file *thumbnail, FileDownloadCb callback)
    : PendingRequest(requestId), chatId(chatId), message(std::move(message)), fileId(fileId),
      fileSize(fileSize), downloadedSize(0), fileDescription(fileDescription),
      thumbnail(thumbnail), callback(callback) {}
};

class AvatarDownloadRequest: public PendingRequest {
public:
    int32_t userId;
    int64_t chatId;

    AvatarDownloadRequest(uint64_t requestId, const td::td_api::user *user)
    : PendingRequest(requestId), userId(user->id_), chatId(0) {}
    AvatarDownloadRequest(uint64_t requestId, const td::td_api::chat *chat)
    : PendingRequest(requestId), userId(0), chatId(chat->id_) {}
};

class TdAccountData {
public:
    using TdUserPtr           = td::td_api::object_ptr<td::td_api::user>;
    using TdChatPtr           = td::td_api::object_ptr<td::td_api::chat>;
    using TdGroupPtr          = td::td_api::object_ptr<td::td_api::basicGroup>;
    using TdGroupInfoPtr      = td::td_api::object_ptr<td::td_api::basicGroupFullInfo>;
    using TdSupergroupPtr     = td::td_api::object_ptr<td::td_api::supergroup>;
    using TdSupergroupInfoPtr = td::td_api::object_ptr<td::td_api::supergroupFullInfo>;

    struct {
        unsigned maxCaptionLength = 0;
        unsigned maxMessageLength = 0;
    } options;

    PurpleAccount *const purpleAccount;
    TdAccountData(PurpleAccount *purpleAccount) : purpleAccount(purpleAccount) {}

    void updateUser(TdUserPtr user);
    void setUserStatus(int32_t userId, td::td_api::object_ptr<td::td_api::UserStatus> status);
    void updateSmallProfilePhoto(int32_t userId, td::td_api::object_ptr<td::td_api::file> photo);
    void updateBasicGroup(TdGroupPtr group);
    void setBasicGroupInfoRequested(int32_t groupId);
    bool isBasicGroupInfoRequested(int32_t groupId);
    void updateBasicGroupInfo(int32_t groupId, TdGroupInfoPtr groupInfo);
    void updateSupergroup(TdSupergroupPtr group);
    void setSupergroupInfoRequested(int32_t groupId);
    bool isSupergroupInfoRequested(int32_t groupId);
    void updateSupergroupInfo(int32_t groupId, TdSupergroupInfoPtr groupInfo);

    void addChat(TdChatPtr chat); // Updates existing chat if any
    void updateChatChatList(int64_t chatId, td::td_api::object_ptr<td::td_api::ChatList> list);
    void updateChatTitle(int64_t chatId, const std::string &title);
    void updateSmallChatPhoto(int64_t chatId, td::td_api::object_ptr<td::td_api::file> photo);
    void updateChatOrder(int64_t chatId, int64_t order);
    void setContacts(const std::vector<std::int32_t> &userIds);
    void getContactsWithNoChat(std::vector<std::int32_t> &userIds);
    void getChats(std::vector<const td::td_api::chat *> &chats) const;
    void deleteChat(int64_t id);
    void getSmallestOrderChat(const td::td_api::ChatList &list, int64_t &chatId, int64_t &order);

    const td::td_api::chat       *getChat(int64_t chatId) const;
    int                           getPurpleChatId(int64_t tdChatId);
    const td::td_api::chat       *getChatByPurpleId(int32_t purpleChatId) const;
    const td::td_api::chat       *getPrivateChatByUserId(int32_t userId) const;
    const td::td_api::user       *getUser(int32_t userId) const;
    const td::td_api::user       *getUserByPhone(const char *phoneNumber) const;
    const td::td_api::user       *getUserByPrivateChat(const td::td_api::chat &chat);
    std::string                   getDisplayName(const td::td_api::user &user) const;
    std::string                   getDisplayName(int32_t userId) const;
    void                          getUsersByDisplayName(const char *displayName,
                                                        std::vector<const td::td_api::user*> &users);
    const td::td_api::basicGroup *getBasicGroup(int32_t groupId) const;
    const td::td_api::basicGroupFullInfo *getBasicGroupInfo(int32_t groupId) const;
    const td::td_api::supergroup *getSupergroup(int32_t groupId) const;
    const td::td_api::supergroupFullInfo *getSupergroupInfo(int32_t groupId) const;
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
    template<typename ReqType>
    ReqType *findPendingRequest(uint64_t requestId)
    {
        return dynamic_cast<ReqType *>(findPendingRequestImpl(requestId));
    }

    const ContactRequest *     findContactRequest(int32_t userId);
    void                       addTempFileUpload(int64_t messageId, const std::string &path);
    std::string                extractTempFileUpload(int64_t messageId);
    DownloadRequest *          findDownloadRequest(int32_t fileId);

    void                       addFileTransfer(int32_t fileId, PurpleXfer *xfer, int64_t chatId);
    void                       addPurpleFileTransfer(int32_t fileId, PurpleXfer *xfer);
    bool                       getFileTransfer(int32_t fileId, PurpleXfer *&xfer, int64_t &chatId);
    bool                       getFileIdForTransfer(PurpleXfer *xfer, int &fileId);
    void                       removeFileTransfer(int32_t fileId);

    void                       addSecretChat(td::td_api::object_ptr<td::td_api::secretChat> secretChat);
    bool                       getSecretChat(int32_t id);
private:
    TdAccountData(const TdAccountData &other) = delete;
    TdAccountData &operator=(const TdAccountData &other) = delete;

    struct UserInfo {
        TdUserPtr   user;
        std::string displayName;
    };

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

    struct SupergroupInfo {
        TdSupergroupPtr     group;
        TdSupergroupInfoPtr fullInfo;
        bool                fullInfoRequested = false;
    };

    struct SendMessageInfo {
        int64_t     messageId;
        std::string tempFile;
    };

    struct FileTransferInfo {
        int32_t     fileId;
        int64_t     chatId;
        PurpleXfer *xfer;
    };

    using ChatMap = std::map<int64_t, ChatInfo>;
    using UserMap = std::map<int32_t, UserInfo>;
    UserMap                            m_userInfo;
    ChatMap                            m_chatInfo;
    std::map<int32_t, GroupInfo>       m_groups;
    std::map<int32_t, SupergroupInfo>  m_supergroups;
    std::set<int32_t>                  m_secretChats;
    int                                m_lastChatPurpleId = 0;

    // List of contacts for which private chat is not known yet.
    std::vector<int32_t>               m_contactUserIdsNoChat;

    // Used to remember stuff during asynchronous communication when adding contact
    std::vector<ContactRequest>        m_addContactRequests;

    std::vector<std::unique_ptr<PendingRequest>> m_requests;

    // Newly sent messages containing inline images, for which a temporary file must be removed when
    // transfer is completed
    std::vector<SendMessageInfo>       m_sentMessages;

    // Currently active file transfers for which PurpleXfer is used
    std::vector<FileTransferInfo>      m_fileTransfers;

    std::unique_ptr<PendingRequest> getPendingRequestImpl(uint64_t requestId);
    PendingRequest *                findPendingRequestImpl(uint64_t requestId);
    void                            setDisplayNameWithoutSuffix(UserInfo &entry);
};

#endif
