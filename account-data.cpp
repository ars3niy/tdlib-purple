#include "account-data.h"
#include "client-utils.h"
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

UserId purpleBuddyNameToUserId(const char *s)
{
    if (strncmp(s, "id", 2))
        return UserId::invalid;
    return stringToUserId(s+2);
}

SecretChatId purpleBuddyNameToSecretChatId(const char *s)
{
    if (strncmp(s, "secret", 2))
        return SecretChatId::invalid;
    return stringToSecretChatId(s+6);
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
    return getUserIdByPrivateChat(chat).valid();
}

UserId getUserIdByPrivateChat(const td::td_api::chat &chat)
{
    if (chat.type_ && (chat.type_->get_id() == td::td_api::chatTypePrivate::ID)) {
        const td::td_api::chatTypePrivate &privType = static_cast<const td::td_api::chatTypePrivate &>(*chat.type_);
        return getUserId(privType);
    }
    return UserId::invalid;
}

bool isChatInContactList(const td::td_api::chat &chat, const td::td_api::user *privateChatUser)
{
    return (chat.chat_list_ != nullptr) || (privateChatUser && privateChatUser->is_contact_);
}

BasicGroupId getBasicGroupId(const td::td_api::chat &chat)
{
    if (chat.type_ && (chat.type_->get_id() == td::td_api::chatTypeBasicGroup::ID))
        return getBasicGroupId(static_cast<const td::td_api::chatTypeBasicGroup &>(*chat.type_));

    return BasicGroupId::invalid;
}

SupergroupId getSupergroupId(const td::td_api::chat &chat)
{
    if (chat.type_ && (chat.type_->get_id() == td::td_api::chatTypeSupergroup::ID))
        return getSupergroupId(static_cast<const td::td_api::chatTypeSupergroup &>(*chat.type_));

    return SupergroupId::invalid;
}

SecretChatId getSecretChatId(const td::td_api::chat &chat)
{
    if (chat.type_ && (chat.type_->get_id() == td::td_api::chatTypeSecret::ID))
        return getSecretChatId(static_cast<const td::td_api::chatTypeSecret&>(*chat.type_));

    return SecretChatId::invalid;
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
    std::string result = makeBasicDisplayName(user);

    // If some sneaky user sets their name equal to someone else's libpurple username, or to our
    // phone number which is libpurple account name, make sure display name is different, because
    // of how it is used for group chat members
    if ((purpleBuddyNameToUserId(result.c_str()).valid()) || isPhoneNumber(result.c_str()))
        result += ' ';

    return result;
}

auto PendingMessageQueue::getChatQueue(ChatId chatId) -> std::vector<ChatQueue>::iterator 
{
    return std::find_if(m_queues.begin(), m_queues.end(), [chatId](const ChatQueue &queue) {
        return (queue.chatId == chatId);
    });
}

PendingMessageQueue::Message &PendingMessageQueue::addMessage(ChatQueue &queue, MessageAction action)
{
    if (action == MessageAction::Append) {
        queue.messages.emplace_back();
        return queue.messages.back();
    } else {
        queue.messages.emplace_front();
        return queue.messages.front();
    }
}

IncomingMessage &PendingMessageQueue::addPendingMessage(IncomingMessage &&message,
    MessageAction action)
{
    if (!message.message) return message;

    ChatId     chatId  = getChatId(*message.message);
    auto       queueIt = getChatQueue(chatId);
    ChatQueue *queue;
    purple_debug_misc(config::pluginId,"MessageQueue: chat %" G_GINT64_FORMAT ": "
                      "adding pending message %" G_GINT64_FORMAT " (not ready)\n",
                      chatId.value(), message.message->id_);

    if (queueIt != m_queues.end())
        queue = &*queueIt;
    else {
        m_queues.emplace_back();
        m_queues.back().chatId = chatId;
        queue = &m_queues.back();
    }

    Message &newEntry = addMessage(*queue, action);
    newEntry.ready = false;
    newEntry.message = std::move(message);
    return newEntry.message;
}

void PendingMessageQueue::extractReadyMessages(
    std::vector<ChatQueue>::iterator pQueue,
    std::vector<IncomingMessage> &readyMessages)
{
    std::list<Message>::iterator pReady;
    for (pReady = pQueue->messages.begin(); pReady != pQueue->messages.end(); ++pReady) {
        if (!pReady->ready) break;
        purple_debug_misc(config::pluginId,"MessageQueue: chat %" G_GINT64_FORMAT ": "
                            "showing message %" G_GINT64_FORMAT "\n",
                            pQueue->chatId.value(), getId(*pReady->message.message).value());
        readyMessages.push_back(std::move(pReady->message));
    }

    pQueue->messages.erase(pQueue->messages.begin(), pReady);
    if (pQueue->messages.empty()) {
        m_queues.erase(pQueue);
        pQueue = m_queues.end();
    }
}

void PendingMessageQueue::setMessageReady(ChatId chatId, MessageId messageId,
                                          std::vector<IncomingMessage> &readyMessages)
{
    readyMessages.clear();

    auto pQueue = getChatQueue(chatId);
    if (pQueue == m_queues.end()) return;

    purple_debug_misc(config::pluginId,"MessageQueue: chat %" G_GINT64_FORMAT ": "
                      "message %" G_GINT64_FORMAT " now ready\n",
                      chatId.value(), messageId.value());

    auto it = std::find_if(pQueue->messages.begin(), pQueue->messages.end(), [messageId](const Message &m) {
        return (getId(*m.message.message) == messageId);
    });
    if (it == pQueue->messages.end()) return;

    it->ready = true;
    if (pQueue->ready && (it == pQueue->messages.begin()))
        extractReadyMessages(pQueue, readyMessages);
}

IncomingMessage PendingMessageQueue::addReadyMessage(IncomingMessage &&message,
    MessageAction action)
{
    if (!message.message) return IncomingMessage();

    ChatId chatId  = getChatId(*message.message);
    auto   queueIt = getChatQueue(chatId);
    if (queueIt == m_queues.end())
        return std::move(message);

    purple_debug_misc(config::pluginId,"MessageQueue: chat %" G_GINT64_FORMAT ": "
                      "adding pending message %" G_GINT64_FORMAT " (ready)\n",
                      chatId.value(), message.message->id_);

    Message &newEntry = addMessage(*queueIt, action);
    newEntry.ready = true;
    newEntry.message = std::move(message);

    return IncomingMessage();
}

IncomingMessage *PendingMessageQueue::findPendingMessage(ChatId chatId, MessageId messageId)
{
    auto queueIt = getChatQueue(chatId);
    if (queueIt == m_queues.end()) return nullptr;
    ChatQueue *queue = &*queueIt;

    auto it = std::find_if(queue->messages.begin(), queue->messages.end(), [messageId](const Message &m) {
        return (getId(*m.message.message) == messageId);
    });
    return (it != queue->messages.end()) ? &it->message : nullptr;
}

void PendingMessageQueue::flush(std::vector<IncomingMessage> &messages)
{
    messages.clear();
    for (ChatQueue &queue: m_queues)
        for (Message &message: queue.messages)
            messages.push_back(std::move(message.message));
    m_queues.clear();
}

void PendingMessageQueue::setChatNotReady(ChatId chatId)
{
    auto pQueue = getChatQueue(chatId);
    if (pQueue != m_queues.end())
        pQueue->ready = false;
    else {
        m_queues.emplace_back();
        m_queues.back().chatId = chatId;
        m_queues.back().ready = false;
    }
}

void PendingMessageQueue::setChatReady(ChatId chatId, std::vector<IncomingMessage>& readyMessages)
{
    readyMessages.clear();
    auto pQueue = getChatQueue(chatId);
    if (pQueue == m_queues.end()) return;

    pQueue->ready = true;
    extractReadyMessages(pQueue, readyMessages);
}

bool PendingMessageQueue::isChatReady(ChatId chatId)
{
    auto pQueue = getChatQueue(chatId);
    if (pQueue != m_queues.end())
        return pQueue->ready;
    else
        return true;
}

void TdAccountData::updateUser(TdUserPtr userPtr)
{
    const td::td_api::user *user = userPtr.get();
    if (user) {
        UserId   userId = getId(*user);
        auto     it     = m_userInfo.find(userId);

        if (it == m_userInfo.end()) {
            auto ret = m_userInfo.emplace(std::make_pair(userId, UserInfo()));
            it = ret.first;
        }

        UserInfo &entry = it->second;
        entry.user = std::move(userPtr);
        entry.displayName = makeDisplayName(*user);
        for (unsigned n = 0; n != UINT32_MAX; n++) {
            std::string displayName = entry.displayName;
            if (n != 0) {
                displayName += " #";
                displayName += std::to_string(n);
            }

            std::vector<const td::td_api::user *> existingUsers;
            getUsersByDisplayName(displayName.c_str(), existingUsers);
            if (std::none_of(existingUsers.begin(), existingUsers.end(),
                                [user](const td::td_api::user *otherUser) {
                                    return (otherUser != user);
                                }))
            {
                entry.displayName = std::move(displayName);
                break;
            }
        }
    }
}

void TdAccountData::setUserStatus(UserId userId, td::td_api::object_ptr<td::td_api::UserStatus> status)
{
    auto it = m_userInfo.find(userId);
    if (it != m_userInfo.end())
        it->second.user->status_ = std::move(status);
}

void TdAccountData::updateSmallProfilePhoto(UserId userId, td::td_api::object_ptr<td::td_api::file> photo)
{
    auto it = m_userInfo.find(userId);
    if (it != m_userInfo.end()) {
        td::td_api::user &user = *it->second.user;
        if (user.profile_photo_)
            user.profile_photo_->small_ = std::move(photo);
    }
}

void TdAccountData::updateBasicGroup(TdGroupPtr group)
{
    if (group)
        m_groups[getId(*group)].group = std::move(group);
}

void TdAccountData::setBasicGroupInfoRequested(BasicGroupId groupId)
{
    auto it = m_groups.find(groupId);
    if (it != m_groups.end())
        it->second.fullInfoRequested = true;
}

bool TdAccountData::isBasicGroupInfoRequested(BasicGroupId groupId)
{
    auto it = m_groups.find(groupId);
    if (it != m_groups.end())
        return it->second.fullInfoRequested;
    return false;
}

void TdAccountData::updateBasicGroupInfo(BasicGroupId groupId, TdGroupInfoPtr groupInfo)
{
    if (groupInfo)
        m_groups[groupId].fullInfo = std::move(groupInfo);
}

void TdAccountData::updateSupergroup(TdSupergroupPtr group)
{
    if (group)
        m_supergroups[getId(*group)].group = std::move(group);
}

void TdAccountData::setSupergroupInfoRequested(SupergroupId groupId)
{
    auto it = m_supergroups.find(groupId);
    if (it != m_supergroups.end())
        it->second.fullInfoRequested = true;
}

bool TdAccountData::isSupergroupInfoRequested(SupergroupId groupId)
{
    auto it = m_supergroups.find(groupId);
    if (it != m_supergroups.end())
        return it->second.fullInfoRequested;
    return false;
}

void TdAccountData::updateSupergroupInfo(SupergroupId groupId, TdSupergroupInfoPtr groupInfo)
{
    if (groupInfo)
        m_supergroups[groupId].fullInfo = std::move(groupInfo);
}

void TdAccountData::updateSupergroupMembers(SupergroupId groupId, TdChatMembersPtr members)
{
    if (members)
        m_supergroups[groupId].members = std::move(members);
}

void TdAccountData::addChat(TdChatPtr chat)
{
    if (!chat)
        return;

    if (chat->type_->get_id() == td::td_api::chatTypePrivate::ID) {
        const td::td_api::chatTypePrivate &privType = static_cast<const td::td_api::chatTypePrivate &>(*chat->type_);
        auto pContact = std::find(m_contactUserIdsNoChat.begin(), m_contactUserIdsNoChat.end(),
                                  getUserId(privType));
        if (pContact != m_contactUserIdsNoChat.end()) {
            purple_debug_misc(config::pluginId, "Private chat (id %" G_GINT64_FORMAT ") now known for user %d\n",
                              chat->id_, privType.user_id_);
            m_contactUserIdsNoChat.erase(pContact);
        }
    }

    auto it = m_chatInfo.find(getId(*chat));
    if (it != m_chatInfo.end())
        it->second.chat = std::move(chat);
    else {
        auto entry = m_chatInfo.emplace(getId(*chat), ChatInfo());
        entry.first->second.chat     = std::move(chat);
        entry.first->second.purpleId = ++m_lastChatPurpleId;
    }
}

void TdAccountData::updateChatChatList(ChatId chatId, td::td_api::object_ptr<td::td_api::ChatList> list)
{
    auto it = m_chatInfo.find(chatId);
    if (it != m_chatInfo.end())
        it->second.chat->chat_list_ = std::move(list);
}

void TdAccountData::updateChatTitle(ChatId chatId, const std::string &title)
{
    auto it = m_chatInfo.find(chatId);
    if (it != m_chatInfo.end())
        it->second.chat->title_ = title;
}

void TdAccountData::updateSmallChatPhoto(ChatId chatId, td::td_api::object_ptr<td::td_api::file> photo)
{
    auto it = m_chatInfo.find(chatId);
    if (it != m_chatInfo.end()) {
        td::td_api::chat &chat = *it->second.chat;
        if (chat.photo_)
            chat.photo_->small_ = std::move(photo);
    }
}

void TdAccountData::updateChatOrder(ChatId chatId, int64_t order)
{
    auto it = m_chatInfo.find(chatId);
    if (it != m_chatInfo.end())
        it->second.chat->order_ = order;
}

void TdAccountData::setContacts(const td::td_api::users &users)
{
    for (unsigned i = 0; i < users.user_ids_.size(); i++) {
        UserId userId = getUserId(users, i);
        if (getPrivateChatByUserId(userId) == nullptr) {
            purple_debug_misc(config::pluginId, "Private chat not yet known for user %d\n", userId.value());
            m_contactUserIdsNoChat.push_back(userId);
        }
    }
}

void TdAccountData::getContactsWithNoChat(std::vector<UserId> &userIds)
{
    userIds = m_contactUserIdsNoChat;
}

const td::td_api::chat *TdAccountData::getChat(ChatId chatId) const
{
    auto pChatInfo = m_chatInfo.find(chatId);
    if (pChatInfo == m_chatInfo.end())
        return nullptr;
    else
        return pChatInfo->second.chat.get();
}

int TdAccountData::getPurpleChatId(ChatId tdChatId)
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

static bool isPrivateChat(const td::td_api::chat &chat, UserId userId)
{
    if (chat.type_->get_id() == td::td_api::chatTypePrivate::ID) {
        const td::td_api::chatTypePrivate &privType = static_cast<const td::td_api::chatTypePrivate &>(*chat.type_);
        return (getUserId(privType) == userId);
    }
    return false;
}

const td::td_api::chat *TdAccountData::getPrivateChatByUserId(UserId userId) const
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

const td::td_api::user *TdAccountData::getUser(UserId userId) const
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
    UserId userId = getUserIdByPrivateChat(chat);
    if (userId.valid())
        return getUser(userId);
    return nullptr;
}

std::string TdAccountData::getDisplayName(const td::td_api::user &user) const
{
    return getDisplayName(getId(user));
}

std::string TdAccountData::getDisplayName(UserId userId) const
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

const td::td_api::basicGroup *TdAccountData::getBasicGroup(BasicGroupId groupId) const
{
    auto it = m_groups.find(groupId);
    if (it != m_groups.end())
        return it->second.group.get();
    else
        return nullptr;
}

const td::td_api::basicGroupFullInfo *TdAccountData::getBasicGroupInfo(BasicGroupId groupId) const
{
    auto it = m_groups.find(groupId);
    if (it != m_groups.end())
        return it->second.fullInfo.get();
    else
        return nullptr;
}

const td::td_api::supergroup *TdAccountData::getSupergroup(SupergroupId groupId) const
{
    auto it = m_supergroups.find(groupId);
    if (it != m_supergroups.end())
        return it->second.group.get();
    else
        return nullptr;
}

const td::td_api::supergroupFullInfo *TdAccountData::getSupergroupInfo(SupergroupId groupId) const
{
    auto it = m_supergroups.find(groupId);
    if (it != m_supergroups.end())
        return it->second.fullInfo.get();
    else
        return nullptr;
}

const td::td_api::chatMembers *TdAccountData::getSupergroupMembers(SupergroupId groupId) const
{
    auto it = m_supergroups.find(groupId);
    if (it != m_supergroups.end())
        return it->second.members.get();
    else
        return nullptr;
}

const td::td_api::chat *TdAccountData::getBasicGroupChatByGroup(BasicGroupId groupId) const
{
    if (!groupId.valid())
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

const td::td_api::chat *TdAccountData::getSupergroupChatByGroup(SupergroupId groupId) const
{
    if (!groupId.valid())
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

bool TdAccountData::isGroupChatWithMembership(const td::td_api::chat &chat) const
{
    BasicGroupId groupId = getBasicGroupId(chat);
    if (groupId.valid()) {
        const td::td_api::basicGroup *group = getBasicGroup(groupId);
        return (group && isGroupMember(group->status_));
    }
    SupergroupId supergroupId = getSupergroupId(chat);
    if (supergroupId.valid()) {
        const td::td_api::supergroup *group = getSupergroup(supergroupId);
        return (group && isGroupMember(group->status_));
    }
    return false;
}

const td::td_api::chat *TdAccountData::getChatBySecretChat(SecretChatId secretChatId)
{
    auto it = std::find_if(m_chatInfo.begin(), m_chatInfo.end(),
                           [secretChatId](const ChatMap::value_type &entry) {
                               return (getSecretChatId(*entry.second.chat) == secretChatId);
                           });

    if (it != m_chatInfo.end())
        return it->second.chat.get();
    else
        return nullptr;
}

void TdAccountData::getChats(std::vector<const td::td_api::chat *> &chats) const
{
    chats.clear();
    for (const ChatMap::value_type &item: m_chatInfo)
        chats.push_back(item.second.chat.get());
}

void TdAccountData::deleteChat(ChatId id)
{
    m_chatInfo.erase(id);
}

void TdAccountData::getSmallestOrderChat(const td::td_api::ChatList &list, ChatId &chatId, int64_t &order)
{
    int64_t minOrder = INT64_MAX;
    ChatId  id       = ChatId::invalid;
    for (const ChatMap::value_type &entry: m_chatInfo) {
        int64_t order = entry.second.chat->order_;
        if (entry.second.chat->chat_list_ && (entry.second.chat->chat_list_->get_id() == list.get_id()) &&
            (order < minOrder))
        {
            minOrder = order;
            id = entry.first;
        }
    }

    chatId = id;
    order  = minOrder;
}

void TdAccountData::addExpectedChat(ChatId id)
{
    if (!isExpectedChat(id))
        m_expectedChats.push_back(id);
}

bool TdAccountData::isExpectedChat(ChatId chatId)
{
    return (std::find(m_expectedChats.begin(), m_expectedChats.end(), chatId) != m_expectedChats.end());
}

void TdAccountData::removeExpectedChat(ChatId id)
{
    auto expIt = std::find(m_expectedChats.begin(), m_expectedChats.end(), id);
    if (expIt != m_expectedChats.end())
        m_expectedChats.erase(expIt);

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

PendingRequest *TdAccountData::findPendingRequestImpl(uint64_t requestId)
{
    auto it = std::find_if(m_requests.begin(), m_requests.end(),
                           [requestId](const std::unique_ptr<PendingRequest> &req) {
                               return (req->requestId == requestId);
                           });

    if (it != m_requests.end())
        return it->get();

    return nullptr;
}

const ContactRequest *TdAccountData::findContactRequest(UserId userId)
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

DownloadRequest* TdAccountData::findDownloadRequest(int32_t fileId)
{
    auto it = std::find_if(m_requests.begin(), m_requests.end(),
                           [fileId](const std::unique_ptr<PendingRequest> &req) {
                               DownloadRequest *downloadReq = dynamic_cast<DownloadRequest *>(req.get());
                               return (downloadReq && (downloadReq->fileId == fileId));
                           });

    if (it != m_requests.end())
        return static_cast<DownloadRequest *>(it->get());
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

void TdAccountData::addFileTransfer(int32_t fileId, PurpleXfer *xfer, ChatId chatId)
{
    if (std::find_if(m_fileTransfers.begin(), m_fileTransfers.end(),
                    [fileId](const FileTransferInfo &upload) {
                        return (upload.fileId == fileId);
                    }) == m_fileTransfers.end()) {
        m_fileTransfers.emplace_back();
        m_fileTransfers.back().fileId = fileId;
        m_fileTransfers.back().xfer = xfer;
        m_fileTransfers.back().chatId = chatId;
    }
}

bool TdAccountData::getFileTransfer(int32_t fileId, PurpleXfer *&xfer, ChatId &chatId)
{
    auto it = std::find_if(m_fileTransfers.begin(), m_fileTransfers.end(),
                           [fileId](const FileTransferInfo &upload) { return (upload.fileId == fileId); });
    if (it != m_fileTransfers.end()) {
        xfer = it->xfer;
        chatId = it->chatId;
        return true;
    }

    return false;
}

bool TdAccountData::getFileIdForTransfer(PurpleXfer *xfer, int &fileId)
{
    auto it = std::find_if(m_fileTransfers.begin(), m_fileTransfers.end(),
                           [xfer](const FileTransferInfo &upload) { return (upload.xfer == xfer); });
    if (it != m_fileTransfers.end()) {
        fileId = it->fileId;
        return true;
    } else
        return false;
}

void TdAccountData::removeFileTransfer(int32_t fileId)
{
    auto it = std::find_if(m_fileTransfers.begin(), m_fileTransfers.end(),
                           [fileId](const FileTransferInfo &upload) { return (upload.fileId == fileId); });
    if (it != m_fileTransfers.end())
        m_fileTransfers.erase(it);
}

void TdAccountData::addSecretChat(td::td_api::object_ptr<td::td_api::secretChat> secretChat)
{
    if (secretChat)
        m_secretChats[getId(*secretChat)] = std::move(secretChat);
}

const td::td_api::secretChat *TdAccountData::getSecretChat(SecretChatId id)
{
    auto it = m_secretChats.find(id);
    return (it != m_secretChats.end()) ? it->second.get() : nullptr;
}

void TdAccountData::deleteSecretChat(SecretChatId id)
{
    m_secretChats.erase(id);
}

std::vector<std::pair<BasicGroupId, const td::td_api::basicGroupFullInfo *>>
TdAccountData::getBasicGroupsWithMember(UserId userId)
{
    std::vector<std::pair<BasicGroupId, const td::td_api::basicGroupFullInfo *>> result;

    for (const auto &item: m_groups)
        if (item.second.fullInfo) {
            auto &members = item.second.fullInfo->members_;
            if (std::any_of(members.begin(), members.end(),
                            [userId](const td::td_api::object_ptr<td::td_api::chatMember> &member) {
                                return (member && (getUserId(*member) == userId));
                            }))
            {
                result.push_back(std::make_pair(getId(*item.second.group), item.second.fullInfo.get()));
            }
        }

    return result;
}

bool TdAccountData::hasActiveCall()
{
    return (m_callData != nullptr);
}

void TdAccountData::setActiveCall(int32_t callId)
{
    if (!m_callData) {
        m_callData = std::make_unique<tgvoip::VoIPController>();
        m_callId = callId;
    }
}

tgvoip::VoIPController *TdAccountData::getCallData()
{
    return m_callData.get();
}

void TdAccountData::removeActiveCall()
{
    m_callData.reset();
    m_callId = 0;
}

void TdAccountData::addPendingReadReceipt(ChatId chatId, MessageId messageId)
{
    auto pChatReceipts = std::find_if(m_pendingReadReceipts.begin(), m_pendingReadReceipts.end(),
                                      [chatId](const std::vector<ReadReceipt> &receipts) {
                                          return (!receipts.empty() && (receipts[0].chatId == chatId));
                                      });
    if (pChatReceipts != m_pendingReadReceipts.end())
        pChatReceipts->push_back(ReadReceipt{chatId, messageId});
    else {
        m_pendingReadReceipts.emplace_back();
        m_pendingReadReceipts.back().push_back(ReadReceipt{chatId, messageId});
    }
}

void TdAccountData::extractPendingReadReceipts(ChatId chatId, std::vector<ReadReceipt>& receipts)
{
    auto pChatReceipts = std::find_if(m_pendingReadReceipts.begin(), m_pendingReadReceipts.end(),
                                      [chatId](const std::vector<ReadReceipt> &receipts) {
                                          return (!receipts.empty() && (receipts[0].chatId == chatId));
                                      });
    if (pChatReceipts != m_pendingReadReceipts.end()) {
        receipts = std::move(*pChatReceipts);
        m_pendingReadReceipts.erase(pChatReceipts);
    } else
        receipts.clear();
}
