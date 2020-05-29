#include "account-data.h"
#include "config.h"
#include <purple.h>
#include <algorithm>

static bool isCanonicalPhoneNumber(const char *s)
{
    if (*s == '\0')
        return false;

    for (const char *c = s; *c; c++)
        if (!isdigit(*c))
            return false;

    return true;
}

bool isPhoneNumber(const char *s)
{
    if (*s == '+') s++;
    return isCanonicalPhoneNumber(s);
}

const char *getCanonicalPhoneNumber(const char *s)
{
    if (*s == '+')
        return s+1;
    else
        return s;
}

int32_t stringToUserId(const char *s)
{
    if (strncmp(s, "id", 2))
        return 0;
    s += 2;

    const char *c = s;
    if (*c == '-')
        c++;
    if ((*c == '\0') || (*c == '0'))
        return 0;
    for (; *c; c++)
        if ((c >= s+12) || !isdigit(*c))
            return 0;

    long long x;
    static_assert(sizeof(x) > 4, "need more than int32 here");
    x = atoll(s);
    if ((x < INT32_MIN) || (x > INT32_MAX))
        return 0;

    return x;
}

static bool isPhoneEqual(const std::string &n1, const std::string &n2)
{
    const char *s1 = n1.c_str();
    const char *s2 = n2.c_str();
    if (*s1 == '+') s1++;
    if (*s2 == '+') s2++;
    return !strcmp(s1, s2);
}

bool isPrivateChat(const td::td_api::chat &chat)
{
    return (getUserIdByPrivateChat(chat) != 0);
}

int32_t getUserIdByPrivateChat(const td::td_api::chat &chat)
{
    if (chat.type_ && (chat.type_->get_id() == td::td_api::chatTypePrivate::ID)) {
        const td::td_api::chatTypePrivate &privType = static_cast<const td::td_api::chatTypePrivate &>(*chat.type_);
        return privType.user_id_;
    }
    return 0;
}

int32_t getBasicGroupId(const td::td_api::chat &chat)
{
    if (chat.type_ && (chat.type_->get_id() == td::td_api::chatTypeBasicGroup::ID))
        return static_cast<const td::td_api::chatTypeBasicGroup &>(*chat.type_).basic_group_id_;

    return 0;
}

int32_t getSupergroupId(const td::td_api::chat &chat)
{
    if (chat.type_ && (chat.type_->get_id() == td::td_api::chatTypeSupergroup::ID))
        return static_cast<const td::td_api::chatTypeSupergroup &>(*chat.type_).supergroup_id_;

    return 0;
}

bool isGroupMember(const td::td_api::object_ptr<td::td_api::ChatMemberStatus> &status)
{
    if (!status)
        return false;
    else if ((status->get_id() == td::td_api::chatMemberStatusLeft::ID) ||
             (status->get_id() == td::td_api::chatMemberStatusBanned::ID))
        return false;
    else if (status->get_id() == td::td_api::chatMemberStatusRestricted::ID)
        return static_cast<const td::td_api::chatMemberStatusRestricted &>(*status).is_member_;
    else if (status->get_id() == td::td_api::chatMemberStatusCreator::ID)
        return static_cast<const td::td_api::chatMemberStatusCreator &>(*status).is_member_;
    else
        return true;
}

static std::string makeDisplayName(const td::td_api::user &user)
{
    std::string result = user.first_name_;
    if (!result.empty() && !user.last_name_.empty())
        result += ' ';
    result += user.last_name_;

    // If some sneaky user sets their name equal to someone else's libpurple username, or to our
    // phone number which is libpurple account name, make sure display name is different, because
    // of how it is used for group chat members
    if ((stringToUserId(result.c_str()) != 0) || isPhoneNumber(result.c_str()))
        result += ' ';

    return result;
}

void TdAccountData::updateUser(TdUserPtr user)
{
    if (user) {
        int32_t userId = user->id_;
        auto    it     = m_userInfo.find(userId);
        if (it != m_userInfo.end())
            it->second.user  = std::move(user);
        else {
            std::string displayName = makeDisplayName(*user);
            UserInfo entry;
            entry.user = std::move(user);
            entry.displayName = std::move(displayName);
            m_userInfo[userId] = std::move(entry);
        }
    }
}

void TdAccountData::setUserStatus(int32_t userId, td::td_api::object_ptr<td::td_api::UserStatus> status)
{
    auto it = m_userInfo.find(userId);
    if (it != m_userInfo.end())
        it->second.user->status_ = std::move(status);
}

void TdAccountData::updateBasicGroup(TdGroupPtr group)
{
    if (group)
        m_groups[group->id_].group = std::move(group);
}

void TdAccountData::setBasicGroupInfoRequested(int32_t groupId)
{
    auto it = m_groups.find(groupId);
    if (it != m_groups.end())
        it->second.fullInfoRequested = true;
}

bool TdAccountData::isBasicGroupInfoRequested(int32_t groupId)
{
    auto it = m_groups.find(groupId);
    if (it != m_groups.end())
        return it->second.fullInfoRequested;
    return false;
}

void TdAccountData::updateBasicGroupInfo(int32_t groupId, TdGroupInfoPtr groupInfo)
{
    if (groupInfo)
        m_groups[groupId].fullInfo = std::move(groupInfo);
}

void TdAccountData::updateSupergroup(TdSupergroupPtr group)
{
    if (group)
        m_supergroups[group->id_] = std::move(group);
}

void TdAccountData::addChat(TdChatPtr chat)
{
    if (!chat)
        return;

    if (chat->type_->get_id() == td::td_api::chatTypePrivate::ID) {
        const td::td_api::chatTypePrivate &privType = static_cast<const td::td_api::chatTypePrivate &>(*chat->type_);
        auto pContact = std::find(m_contactUserIdsNoChat.begin(), m_contactUserIdsNoChat.end(),
                                  privType.user_id_);
        if (pContact != m_contactUserIdsNoChat.end()) {
            purple_debug_misc(config::pluginId, "Private chat (id %" G_GINT64_FORMAT ") now known for user %d\n",
                              chat->id_, (int)privType.user_id_);
            m_contactUserIdsNoChat.erase(pContact);
        }
    }

    auto it = m_chatInfo.find(chat->id_);
    if (it != m_chatInfo.end())
        it->second.chat = std::move(chat);
    else {
        auto entry = m_chatInfo.emplace(chat->id_, ChatInfo());
        entry.first->second.chat     = std::move(chat);
        entry.first->second.purpleId = ++m_lastChatPurpleId;
    }
}

void TdAccountData::updateChatChatList(int64_t chatId, td::td_api::object_ptr<td::td_api::ChatList> list)
{
    auto it = m_chatInfo.find(chatId);
    if (it != m_chatInfo.end())
        it->second.chat->chat_list_ = std::move(list);
}

void TdAccountData::updateChatTitle(int64_t chatId, const std::string &title)
{
    auto it = m_chatInfo.find(chatId);
    if (it != m_chatInfo.end())
        it->second.chat->title_ = title;
}

void TdAccountData::setContacts(const std::vector<std::int32_t> &userIds)
{
    for (int32_t userId: userIds)
        if (getPrivateChatByUserId(userId) == nullptr) {
            purple_debug_misc(config::pluginId, "Private chat not yet known for user %d\n", (int)userId);
            m_contactUserIdsNoChat.push_back(userId);
        }
}

void TdAccountData::getContactsWithNoChat(std::vector<std::int32_t> &userIds)
{
    userIds = m_contactUserIdsNoChat;
}

const td::td_api::chat *TdAccountData::getChat(int64_t chatId) const
{
    auto pChatInfo = m_chatInfo.find(chatId);
    if (pChatInfo == m_chatInfo.end())
        return nullptr;
    else
        return pChatInfo->second.chat.get();
}

int TdAccountData::getPurpleChatId(int64_t tdChatId)
{
    auto pChatInfo = m_chatInfo.find(tdChatId);
    if (pChatInfo == m_chatInfo.end())
        return 0;
    else
        return pChatInfo->second.purpleId;
}

const td::td_api::chat *TdAccountData::getChatByPurpleId(int32_t purpleChatId) const
{
    auto pChatInfo = std::find_if(m_chatInfo.begin(), m_chatInfo.end(),
                                  [purpleChatId](const ChatMap::value_type &entry) {
                                      return (entry.second.purpleId == purpleChatId);
                                  });

    if (pChatInfo != m_chatInfo.end())
        return pChatInfo->second.chat.get();
    else
        return nullptr;
}

static bool isPrivateChat(const td::td_api::chat &chat, int32_t userId)
{
    if (chat.type_->get_id() == td::td_api::chatTypePrivate::ID) {
        const td::td_api::chatTypePrivate &privType = static_cast<const td::td_api::chatTypePrivate &>(*chat.type_);
        return (privType.user_id_ == userId);
    }
    return false;
}

const td::td_api::chat *TdAccountData::getPrivateChatByUserId(int32_t userId) const
{
    auto pChatInfo = std::find_if(m_chatInfo.begin(), m_chatInfo.end(),
                                  [userId](const ChatMap::value_type &entry) {
                                      return isPrivateChat(*entry.second.chat, userId);
                                  });
    if (pChatInfo == m_chatInfo.end())
        return nullptr;
    else
        return pChatInfo->second.chat.get();
}

const td::td_api::user *TdAccountData::getUser(int32_t userId) const
{
    auto pUser = m_userInfo.find(userId);
    if (pUser == m_userInfo.end())
        return nullptr;
    else
        return pUser->second.user.get();
}

const td::td_api::user *TdAccountData::getUserByPhone(const char *phoneNumber) const
{
    auto pUser = std::find_if(m_userInfo.begin(), m_userInfo.end(),
                              [phoneNumber](const UserMap::value_type &entry) {
                                  return isPhoneEqual(entry.second.user->phone_number_, phoneNumber);
                              });
    if (pUser == m_userInfo.end())
        return nullptr;
    else
        return pUser->second.user.get();
}

const td::td_api::user *TdAccountData::getUserByPrivateChat(const td::td_api::chat &chat)
{
    int32_t userId = getUserIdByPrivateChat(chat);
    if (userId != 0)
        return getUser(userId);
    return nullptr;
}

std::string TdAccountData::getDisplayName(const td::td_api::user &user) const
{
    return getDisplayName(user.id_);
}

std::string TdAccountData::getDisplayName(int32_t userId) const
{
    auto it = m_userInfo.find(userId);
    if (it != m_userInfo.end())
        return it->second.displayName;
    else
        return std::string();
}

void TdAccountData::getUsersByDisplayName(const char *displayName,
                                         std::vector<const td::td_api::user*> &users)
{
    users.clear();
    if (!displayName || (*displayName == '\0'))
        return;

    for (const UserMap::value_type &entry: m_userInfo)
        if (entry.second.displayName == displayName)
            users.push_back(entry.second.user.get());
}

const td::td_api::basicGroup *TdAccountData::getBasicGroup(int32_t groupId) const
{
    auto it = m_groups.find(groupId);
    if (it != m_groups.end())
        return it->second.group.get();
    else
        return nullptr;
}

const td::td_api::basicGroupFullInfo *TdAccountData::getBasicGroupInfo(int32_t groupId) const
{
    auto it = m_groups.find(groupId);
    if (it != m_groups.end())
        return it->second.fullInfo.get();
    else
        return nullptr;
}

const td::td_api::supergroup *TdAccountData::getSupergroup(int32_t groupId) const
{
    auto it = m_supergroups.find(groupId);
    if (it != m_supergroups.end())
        return it->second.get();
    else
        return nullptr;
}

const td::td_api::chat *TdAccountData::getBasicGroupChatByGroup(int32_t groupId) const
{
    if (groupId == 0)
        return nullptr;

    auto it = std::find_if(m_chatInfo.begin(), m_chatInfo.end(),
                           [groupId](const ChatMap::value_type &entry) {
                               return (getBasicGroupId(*entry.second.chat) == groupId);
                           });

    if (it != m_chatInfo.end())
        return it->second.chat.get();
    else
        return nullptr;
}

const td::td_api::chat *TdAccountData::getSupergroupChatByGroup(int32_t groupId) const
{
    if (groupId == 0)
        return nullptr;

    auto it = std::find_if(m_chatInfo.begin(), m_chatInfo.end(),
                           [groupId](const ChatMap::value_type &entry) {
                               return (getSupergroupId(*entry.second.chat) == groupId);
                           });

    if (it != m_chatInfo.end())
        return it->second.chat.get();
    else
        return nullptr;
}

bool TdAccountData::isGroupChatWithMembership(const td::td_api::chat &chat)
{
    int groupId = getBasicGroupId(chat);
    if (groupId) {
        const td::td_api::basicGroup *group = getBasicGroup(groupId);
        return (group && isGroupMember(group->status_));
    }
    groupId = getSupergroupId(chat);
    if (groupId) {
        const td::td_api::supergroup *group = getSupergroup(groupId);
        return (group && isGroupMember(group->status_));
    }
    return false;
}

void TdAccountData::getChats(std::vector<const td::td_api::chat *> &chats) const
{
    chats.clear();
    for (const ChatMap::value_type &item: m_chatInfo)
        chats.push_back(item.second.chat.get());
}

void TdAccountData::deleteChat(int64_t id)
{
    m_chatInfo.erase(id);
}

std::unique_ptr<PendingRequest> TdAccountData::getPendingRequestImpl(uint64_t requestId)
{
    auto it = std::find_if(m_requests.begin(), m_requests.end(),
                           [requestId](const std::unique_ptr<PendingRequest> &req) {
                               return (req->requestId == requestId);
                           });

    if (it != m_requests.end()) {
        auto result = std::move(*it);
        m_requests.erase(it);
        return result;
    }

    return nullptr;
}

const ContactRequest *TdAccountData::findContactRequest(int32_t userId)
{
    auto it = std::find_if(m_requests.begin(), m_requests.end(),
                           [userId](const std::unique_ptr<PendingRequest> &req) {
                               const ContactRequest *contactReq = dynamic_cast<const ContactRequest *>(req.get());
                               return (contactReq && (contactReq->userId == userId));
                           });

    if (it != m_requests.end())
        return static_cast<const ContactRequest *>(it->get());
    return nullptr;
}

void TdAccountData::addTempFileUpload(int64_t messageId, const std::string &path)
{
    m_sentMessages.emplace_back();
    m_sentMessages.back().messageId = messageId;
    m_sentMessages.back().tempFile = path;
}

std::string TdAccountData::extractTempFileUpload(int64_t messageId)
{
    auto it = std::find_if(m_sentMessages.begin(), m_sentMessages.end(),
                           [messageId](const SendMessageInfo &item) {
                               return (item.messageId == messageId);
                           });

    std::string result;
    if (it != m_sentMessages.end()) {
        result = it->tempFile;
        m_sentMessages.erase(it);
    }

    return result;
}

void TdAccountData::saveMessage(TdMessagePtr message)
{
    if (message)
        m_messages[message->id_] = std::move(message);
}

td::td_api::message *TdAccountData::findMessage(int64_t messageId)
{
    auto it = m_messages.find(messageId);
    return (it != m_messages.end()) ? it->second.get() : nullptr;
}

std::unique_ptr<UploadRequest> TdAccountData::getUploadRequest(PurpleXfer *xfer)
{
    auto it = std::find_if(m_requests.begin(), m_requests.end(),
                           [xfer](const std::unique_ptr<PendingRequest> &req) {
                               const UploadRequest *uploadReq = dynamic_cast<const UploadRequest *>(req.get());
                               return (uploadReq && (uploadReq->xfer == xfer));
                           });

    if (it != m_requests.end()) {
        UploadRequest *result = static_cast<UploadRequest *>(it->release());
        m_requests.erase(it);
        return std::unique_ptr<UploadRequest>(result);
    }
    return nullptr;
}

void TdAccountData::addUpload(int32_t fileId, PurpleXfer *xfer, int64_t chatId)
{
    if (std::find_if(m_uploads.begin(), m_uploads.end(),
                    [fileId](const UploadInfo &upload) {
                        return (upload.fileId == fileId);
                    }) == m_uploads.end()) {
        m_uploads.emplace_back();
        m_uploads.back().fileId = fileId;
        m_uploads.back().xfer = xfer;
        m_uploads.back().chatId = chatId;
    }
}

bool TdAccountData::getUpload(int32_t fileId, PurpleXfer *&xfer, int64_t &chatId)
{
    auto it = std::find_if(m_uploads.begin(), m_uploads.end(),
                           [fileId](const UploadInfo &upload) { return (upload.fileId == fileId); });
    if (it != m_uploads.end()) {
        xfer = it->xfer;
        chatId = it->chatId;
        return true;
    }

    return false;
}

bool TdAccountData::getFileIdForUpload(PurpleXfer *xfer, int &fileId)
{
    auto it = std::find_if(m_uploads.begin(), m_uploads.end(),
                           [xfer](const UploadInfo &upload) { return (upload.xfer == xfer); });
    if (it != m_uploads.end()) {
        fileId = it->fileId;
        return true;
    } else
        return false;
}

void TdAccountData::removeUpload(int32_t fileId)
{
    auto it = std::find_if(m_uploads.begin(), m_uploads.end(),
                           [fileId](const UploadInfo &upload) { return (upload.fileId == fileId); });
    if (it != m_uploads.end())
        m_uploads.erase(it);
}
