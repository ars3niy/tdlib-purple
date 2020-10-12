#ifndef _ACCOUNT_DATA_H
#define _ACCOUNT_DATA_H

#include "buildopt.h"
#include "identifiers.h"
#include <td/telegram/td_api.h>

#include <map>
#include <mutex>
#include <set>
#include <purple.h>

#ifndef NoVoip
#include <VoIPController.h>
#else

namespace tgvoip {
    struct VoIPController {};
}
#endif

bool        isPhoneNumber(const char *s);
const char *getCanonicalPhoneNumber(const char *s);
UserId      purpleBuddyNameToUserId(const char *s);
SecretChatId purpleBuddyNameToSecretChatId(const char *s);
bool        isPrivateChat(const td::td_api::chat &chat);
UserId      getUserIdByPrivateChat(const td::td_api::chat &chat);
bool        isChatInContactList(const td::td_api::chat &chat, const td::td_api::user *privateChatUser);
BasicGroupId getBasicGroupId(const td::td_api::chat &chat);
SupergroupId getSupergroupId(const td::td_api::chat &chat);
SecretChatId getSecretChatId(const td::td_api::chat &chat);
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
    BasicGroupId groupId;

    GroupInfoRequest(uint64_t requestId, BasicGroupId groupId)
    : PendingRequest(requestId), groupId(groupId) {}
};

class SupergroupInfoRequest: public PendingRequest {
public:
    SupergroupId groupId;

    SupergroupInfoRequest(uint64_t requestId, SupergroupId groupId)
    : PendingRequest(requestId), groupId(groupId) {}
};

class GroupMembersRequestCont: public PendingRequest {
public:
    SupergroupId groupId;
    td::td_api::object_ptr<td::td_api::chatMembers> members;

    GroupMembersRequestCont(uint64_t requestId, SupergroupId groupId, td::td_api::chatMembers *members)
    : PendingRequest(requestId), groupId(groupId), members(std::move(members)) {}
};

class ContactRequest: public PendingRequest {
public:
    std::string phoneNumber;
    std::string alias;
    std::string groupName;
    UserId      userId;

    ContactRequest(uint64_t requestId, const std::string &phoneNumber, const std::string &alias,
                   const std::string &groupName, UserId userId)
    : PendingRequest(requestId), phoneNumber(phoneNumber), alias(alias), groupName(groupName),
      userId(userId) {}
};

class GroupJoinRequest: public PendingRequest {
public:
    enum class Type {
        InviteLink,
        Username,
    };
    std::string joinString;
    Type        type;
    ChatId      chatId;

    GroupJoinRequest(uint64_t requestId, const std::string &joinString, Type type,
                     ChatId chatId = ChatId::invalid)
    : PendingRequest(requestId), joinString(joinString), type(type), chatId(chatId) {}
};

class SendMessageRequest: public PendingRequest {
public:
    ChatId      chatId;
    std::string tempFile;

    SendMessageRequest(uint64_t requestId, ChatId chatId, const char *tempFile)
    : PendingRequest(requestId), chatId(chatId), tempFile(tempFile ? tempFile : "") {}
};

class UploadRequest: public PendingRequest {
public:
    PurpleXfer *xfer;
    ChatId      chatId;

    UploadRequest(uint64_t requestId, PurpleXfer *xfer, ChatId chatId)
    : PendingRequest(requestId), xfer(xfer), chatId(chatId) {}
};

struct TgMessageInfo {
    enum class Type {
        Photo,
        Sticker,
        Other
    };
    MessageId   id;
    Type        type;
    std::string incomingGroupchatSender;
    time_t      timestamp;
    bool        outgoing;
    bool        sentLocally = false; // For outgoing messages, whether sent by this very client
    MessageId   repliedMessageId;
    td::td_api::object_ptr<td::td_api::message> repliedMessage;
    std::string forwardedFrom;

    void assign(const TgMessageInfo &other)
    {
        id = other.id;
        type = other.type;
        incomingGroupchatSender = other.incomingGroupchatSender;
        timestamp = other.timestamp;
        outgoing = other.outgoing;
        sentLocally = other.sentLocally;
        repliedMessageId = other.repliedMessageId;
        repliedMessage = nullptr;
        forwardedFrom = other.forwardedFrom;
    }
};

class PurpleTdClient;

// Used for matching completed downloads to chats they belong to, and for starting PurpleXfer for
// time-consuming downloads
class DownloadRequest: public PendingRequest {
public:
    ChatId         chatId;

    // For inline downloads this is a copy of original TgMessageInfo from IncomingMessage.
    TgMessageInfo  message;

    int32_t        fileId;
    int32_t        fileSize;
    int32_t        downloadedSize;
    std::string    fileDescription;
    int            tempFd = -1;
    std::string    tempFileName;
    td::td_api::object_ptr<td::td_api::file> thumbnail;

    // Could not pass object_ptr through variadic funciton :(
    DownloadRequest(uint64_t requestId, ChatId chatId, TgMessageInfo &message,
                    int32_t fileId, int32_t fileSize, const std::string &fileDescription,
                    td::td_api::file *thumbnail)
    : PendingRequest(requestId), chatId(chatId), fileId(fileId),
      fileSize(fileSize), downloadedSize(0), fileDescription(fileDescription),
      thumbnail(thumbnail)
    {
        // If download is started while the message is in PendingMessageQueue, repliedMessage will
        // be on IncomingMessage, and one here in TgMessageInfo will be NULL. In this case,
        // repliedMessage will be moved onto DownloadRequest if message leaves PendingMessageQueue
        // before download is complete (meaning it took more than 1 second to download).
        this->message.assign(message);
        if (message.repliedMessage)
            this->message.repliedMessage = std::move(message.repliedMessage);
    }
};

class AvatarDownloadRequest: public PendingRequest {
public:
    UserId userId;
    ChatId chatId;

    AvatarDownloadRequest(uint64_t requestId, const td::td_api::user *user)
    : PendingRequest(requestId), userId(getId(*user)), chatId(ChatId::invalid) {}
    AvatarDownloadRequest(uint64_t requestId, const td::td_api::chat *chat)
    : PendingRequest(requestId), userId(UserId::invalid), chatId(getId(*chat)) {}
};

class NewPrivateChatForMessage: public PendingRequest {
public:
    std::string  username;
    std::string  message;
    PurpleXfer  *fileUpload;

    NewPrivateChatForMessage(uint64_t requestId, const char *username, const char *message)
    : PendingRequest(requestId), username(username), message(message ? message : nullptr),
      fileUpload(nullptr) {}

    NewPrivateChatForMessage(uint64_t requestId, const char *username, PurpleXfer *upload)
    : PendingRequest(requestId), username(username), fileUpload(upload) {}
};

class ChatActionRequest: public PendingRequest {
public:
    enum class Type: uint8_t {
        Kick,
        Invite,
        GenerateInviteLink
    };
    Type   type;
    ChatId chatId;
    ChatActionRequest(uint64_t requestId, Type type, ChatId chatId)
    : PendingRequest(requestId), type(type), chatId(chatId) {}
};

struct IncomingMessage {
    td::td_api::object_ptr<td::td_api::message> message;
    td::td_api::object_ptr<td::td_api::message> repliedMessage;
    std::string inlineDownloadedFilePath;

    // This doesn't have to be a separate struct, it exists for historical reasons.
    // Could be refactored.
    TgMessageInfo messageInfo;

    int32_t  selectedPhotoSizeId;
    unsigned inlineFileSizeLimit;
    bool     standardDownloadConfigured;
    bool     repliedMessageFailed;
    bool     inlineDownloadComplete;
    bool     inlineDownloadTimeout;
};

class PendingMessageQueue {
public:
    using TdMessagePtr = td::td_api::object_ptr<td::td_api::message>;

    void             addPendingMessage(IncomingMessage &&message);
    void             setMessageReady(ChatId chatId, MessageId messageId,
                                     std::vector<IncomingMessage> &readyMessages);
    IncomingMessage  addReadyMessage(IncomingMessage &&message);
    IncomingMessage *findPendingMessage(ChatId chatId, MessageId messageId);
    void             flush(std::vector<IncomingMessage> &messages);
private:
    struct Message {
        IncomingMessage message;
        bool            ready;
    };
    struct ChatQueue {
        ChatId               chatId;
        std::vector<Message> messages;
    };
    std::vector<ChatQueue> m_queues;

    std::vector<ChatQueue>::iterator getChatQueue(ChatId chatId);
};

class TdAccountData {
public:
    using TdUserPtr           = td::td_api::object_ptr<td::td_api::user>;
    using TdChatPtr           = td::td_api::object_ptr<td::td_api::chat>;
    using TdGroupPtr          = td::td_api::object_ptr<td::td_api::basicGroup>;
    using TdGroupInfoPtr      = td::td_api::object_ptr<td::td_api::basicGroupFullInfo>;
    using TdSupergroupPtr     = td::td_api::object_ptr<td::td_api::supergroup>;
    using TdSupergroupInfoPtr = td::td_api::object_ptr<td::td_api::supergroupFullInfo>;
    using TdChatMembersPtr    = td::td_api::object_ptr<td::td_api::chatMembers>;
    using SecretChatPtr       = td::td_api::object_ptr<td::td_api::secretChat>;

    struct {
        unsigned maxCaptionLength = 0;
        unsigned maxMessageLength = 0;
    } options;

    PurpleAccount *const purpleAccount;
    TdAccountData(PurpleAccount *purpleAccount) : purpleAccount(purpleAccount) {}

    void updateUser(TdUserPtr user);
    void setUserStatus(UserId UserId, td::td_api::object_ptr<td::td_api::UserStatus> status);
    void updateSmallProfilePhoto(UserId userId, td::td_api::object_ptr<td::td_api::file> photo);
    void updateBasicGroup(TdGroupPtr group);
    void setBasicGroupInfoRequested(BasicGroupId groupId);
    bool isBasicGroupInfoRequested(BasicGroupId groupId);
    void updateBasicGroupInfo(BasicGroupId groupId, TdGroupInfoPtr groupInfo);
    void updateSupergroup(TdSupergroupPtr group);
    void setSupergroupInfoRequested(SupergroupId groupId);
    bool isSupergroupInfoRequested(SupergroupId groupId);
    void updateSupergroupInfo(SupergroupId groupId, TdSupergroupInfoPtr groupInfo);
    void updateSupergroupMembers(SupergroupId groupId, TdChatMembersPtr members);

    void addChat(TdChatPtr chat); // Updates existing chat if any
    void updateChatChatList(ChatId chatId, td::td_api::object_ptr<td::td_api::ChatList> list);
    void updateChatTitle(ChatId chatId, const std::string &title);
    void updateSmallChatPhoto(ChatId chatId, td::td_api::object_ptr<td::td_api::file> photo);
    void updateChatOrder(ChatId chatId, int64_t order);
    void setContacts(const td::td_api::users &users);
    void getContactsWithNoChat(std::vector<UserId> &userIds);
    void getChats(std::vector<const td::td_api::chat *> &chats) const;
    void deleteChat(ChatId id);
    void getSmallestOrderChat(const td::td_api::ChatList &list, ChatId &chatId, int64_t &order);
    void addExpectedChat(ChatId id);
    bool isExpectedChat(ChatId chatId);
    void removeExpectedChat(ChatId id);

    const td::td_api::chat       *getChat(ChatId chatId) const;
    int                           getPurpleChatId(ChatId tdChatId);
    const td::td_api::chat       *getChatByPurpleId(int32_t purpleChatId) const;
    const td::td_api::chat       *getPrivateChatByUserId(UserId userId) const;
    const td::td_api::user       *getUser(UserId userId) const;
    const td::td_api::user       *getUserByPhone(const char *phoneNumber) const;
    const td::td_api::user       *getUserByPrivateChat(const td::td_api::chat &chat);
    std::string                   getDisplayName(const td::td_api::user &user) const;
    std::string                   getDisplayName(UserId userId) const;
    void                          getUsersByDisplayName(const char *displayName,
                                                        std::vector<const td::td_api::user*> &users);

    const td::td_api::basicGroup *getBasicGroup(BasicGroupId groupId) const;
    const td::td_api::basicGroupFullInfo *getBasicGroupInfo(BasicGroupId groupId) const;
    const td::td_api::supergroup *getSupergroup(SupergroupId groupId) const;
    const td::td_api::supergroupFullInfo *getSupergroupInfo(SupergroupId groupId) const;
    const td::td_api::chatMembers*getSupergroupMembers(SupergroupId groupId) const;
    const td::td_api::chat       *getBasicGroupChatByGroup(BasicGroupId groupId) const;
    const td::td_api::chat       *getSupergroupChatByGroup(SupergroupId groupId) const;
    bool                          isGroupChatWithMembership(const td::td_api::chat &chat) const;

    const td::td_api::chat       *getChatBySecretChat(SecretChatId secretChatId);

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

    const ContactRequest *     findContactRequest(UserId userId);
    void                       addTempFileUpload(int64_t messageId, const std::string &path);
    std::string                extractTempFileUpload(int64_t messageId);
    DownloadRequest *          findDownloadRequest(int32_t fileId);

    void                       addFileTransfer(int32_t fileId, PurpleXfer *xfer, ChatId chatId);
    void                       addPurpleFileTransfer(int32_t fileId, PurpleXfer *xfer);
    bool                       getFileTransfer(int32_t fileId, PurpleXfer *&xfer, ChatId &chatId);
    bool                       getFileIdForTransfer(PurpleXfer *xfer, int &fileId);
    void                       removeFileTransfer(int32_t fileId);

    void                       addSecretChat(td::td_api::object_ptr<td::td_api::secretChat> secretChat);
    const td::td_api::secretChat *getSecretChat(SecretChatId id);
    void                       deleteSecretChat(SecretChatId id);

    auto                       getBasicGroupsWithMember(UserId userId) ->
                               std::vector<std::pair<BasicGroupId, const td::td_api::basicGroupFullInfo *>>;

    bool                       hasActiveCall();
    void                       setActiveCall(int32_t id);
    int32_t                    getActiveCallId() const { return m_callId; }
    tgvoip::VoIPController    *getCallData();
    void                       removeActiveCall();

    PendingMessageQueue        pendingMessages;
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
        TdChatMembersPtr    members;
        bool                fullInfoRequested = false;
    };

    struct SendMessageInfo {
        int64_t     messageId;
        std::string tempFile;
    };

    struct FileTransferInfo {
        int32_t     fileId;
        ChatId      chatId;
        PurpleXfer *xfer;
    };

    using ChatMap = std::map<ChatId, ChatInfo>;
    using UserMap = std::map<UserId, UserInfo>;
    UserMap                            m_userInfo;
    ChatMap                            m_chatInfo;
    std::map<BasicGroupId, GroupInfo>  m_groups;
    std::map<SupergroupId, SupergroupInfo>  m_supergroups;
    std::map<SecretChatId, SecretChatPtr>   m_secretChats;
    int                                m_lastChatPurpleId = 0;

    // List of contacts for which private chat is not known yet.
    std::vector<UserId>                m_contactUserIdsNoChat;

    // Used to remember stuff during asynchronous communication when adding contact
    std::vector<ContactRequest>        m_addContactRequests;

    // Chats we want to libpurple-join when we get an updateNewChat about them
    std::vector<ChatId>                m_expectedChats;

    std::vector<std::unique_ptr<PendingRequest>> m_requests;

    // Newly sent messages containing inline images, for which a temporary file must be removed when
    // transfer is completed
    std::vector<SendMessageInfo>       m_sentMessages;

    // Currently active file transfers for which PurpleXfer is used
    std::vector<FileTransferInfo>      m_fileTransfers;

    // Voice call data
    std::unique_ptr<tgvoip::VoIPController> m_callData;
    int32_t                                 m_callId;

    std::unique_ptr<PendingRequest> getPendingRequestImpl(uint64_t requestId);
    PendingRequest *                findPendingRequestImpl(uint64_t requestId);
};

#endif
