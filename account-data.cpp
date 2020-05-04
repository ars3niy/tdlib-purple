#include "account-data.h"
#include "config.h"
#include <purple.h>
#include <algorithm>

bool isCanonicalPhoneNumber(const char *s)
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

static bool isPhoneEqual(const std::string &n1, const std::string &n2)
{
    const char *s1 = n1.c_str();
    const char *s2 = n2.c_str();
    if (*s1 == '+') s1++;
    if (*s2 == '+') s2++;
    return !strcmp(s1, s2);
}

void TdAccountData::updateUser(TdUserPtr user)
{
    if (!user) {
        purple_debug_warning(config::pluginId, "updateUser with null user info\n");
        return;
    }
    purple_debug_misc(config::pluginId, "Update user: %s '%s' '%s'\n", user->phone_number_.c_str(),
                      user->first_name_.c_str(), user->last_name_.c_str());

    m_userInfo[user->id_] = std::move(user);
}

void TdAccountData::addChat(TdChatPtr chat)
{
    if (!chat) {
        purple_debug_warning(config::pluginId, "addNewChat with null chat info\n");
        return;
    }
    purple_debug_misc(config::pluginId, "Add new chat: %s\n", chat->title_.c_str());

    if (chat->type_->get_id() == td::td_api::chatTypePrivate::ID) {
        const td::td_api::chatTypePrivate &privType = static_cast<const td::td_api::chatTypePrivate &>(*chat->type_);
        auto pContact = std::find(m_contactUserIdsNoChat.begin(), m_contactUserIdsNoChat.end(),
                                  privType.user_id_);
        if (pContact != m_contactUserIdsNoChat.end()) {
            purple_debug_misc(config::pluginId, "Private chat (id %lld) now known for user %d\n",
                              (long long)chat->id_, (int)privType.user_id_);
            m_contactUserIdsNoChat.erase(pContact);
        }
    }

    m_chatInfo[chat->id_] = std::move(chat);
}

void TdAccountData::setContacts(const std::vector<std::int32_t> &userIds)
{
    for (int32_t userId: userIds)
        if (getPrivateChatByUserId(userId) == nullptr) {
            purple_debug_misc(config::pluginId, "Private chat not yet known for user %d\n", (int)userId);
            m_contactUserIdsNoChat.push_back(userId);
        }
}

void TdAccountData::setActiveChats(std::vector<std::int64_t> &&chats)
{
    m_activeChats = std::move(chats);
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
        return pChatInfo->second.get();
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
                                  [userId](const ChatInfoMap::value_type &entry) {
                                      return isPrivateChat(*entry.second, userId);
                                  });
    if (pChatInfo == m_chatInfo.end())
        return nullptr;
    else
        return pChatInfo->second.get();
}

const td::td_api::user *TdAccountData::getUser(int32_t userId) const
{
    auto pUser = m_userInfo.find(userId);
    if (pUser == m_userInfo.end())
        return nullptr;
    else
        return pUser->second.get();
}

const td::td_api::user *TdAccountData::getUserByPhone(const char *phoneNumber) const
{
    auto pUser = std::find_if(m_userInfo.begin(), m_userInfo.end(),
                              [phoneNumber](const UserInfoMap::value_type &entry) {
                                  return isPhoneEqual(entry.second->phone_number_, phoneNumber);
                              });
    if (pUser == m_userInfo.end())
        return nullptr;
    else
        return pUser->second.get();
}

const td::td_api::user *TdAccountData::getUserByPrivateChat(const td::td_api::chat &chat)
{
    if (chat.type_->get_id() == td::td_api::chatTypePrivate::ID) {
        const td::td_api::chatTypePrivate &privType = static_cast<const td::td_api::chatTypePrivate &>(*chat.type_);
        return getUser(privType.user_id_);
    }
    return nullptr;
}

void TdAccountData::getPrivateChats(std::vector<const td::td_api::chat *> &chats) const
{
    chats.clear();
    for (int64_t chatId: m_activeChats) {
        const td::td_api::chat *chat = getChat(chatId);
        if (!chat) {
            purple_debug_warning(config::pluginId, "Received unknown chat id %lld\n", (long long)chatId);
            continue;
        }

        if (chat->type_->get_id() == td::td_api::chatTypePrivate::ID)
            chats.push_back(&*chat);
    }
}

void TdAccountData::addNewContactRequest(uint64_t requestId, const char *phoneNumber,
                                         const char *alias, int32_t userId)
{
    m_addContactRequests.emplace_back();
    m_addContactRequests.back().requestId = requestId;
    m_addContactRequests.back().phoneNumber = phoneNumber;
    m_addContactRequests.back().alias = alias;
    m_addContactRequests.back().userId = userId;
}

bool TdAccountData::extractContactRequest(uint64_t requestId, std::string &phoneNumber,
                                          std::string &alias, int32_t &userId)
{
    auto pReq = std::find_if(m_addContactRequests.begin(), m_addContactRequests.end(),
                             [requestId](const ContactRequest &req) { return (req.requestId == requestId); });
    if (pReq != m_addContactRequests.end()) {
        phoneNumber = std::move(pReq->phoneNumber);
        alias = std::move(pReq->alias);
        userId = pReq->userId;
        m_addContactRequests.erase(pReq);
        return true;
    }

    return false;
}
