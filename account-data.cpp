#include "account-data.h"
#include "config.h"
#include <purple.h>
#include <algorithm>

void TdAccountData::updateUser(TdUserPtr user)
{
    if (!user) {
        purple_debug_warning(config::pluginId, "updateUser with null user info\n");
        return;
    }
    purple_debug_misc(config::pluginId, "Update user: %s '%s' '%s'\n", user->phone_number_.c_str(),
                      user->first_name_.c_str(), user->last_name_.c_str());

    addUserUpdate(user->id_);
    m_userInfo[user->id_] = std::move(user);
}

void TdAccountData::addNewChat(TdChatPtr chat)
{
    if (!chat) {
        purple_debug_warning(config::pluginId, "addNewChat with null chat info\n");
        return;
    }
    purple_debug_misc(config::pluginId, "Add new chat: %s\n", chat->title_.c_str());

    m_chatInfo[chat->id_] = std::move(chat);
}

void TdAccountData::setActiveChats(std::vector<std::int64_t> &&chats)
{
    m_activeChats = std::move(chats);
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

static bool isPhoneEqual(const std::string &n1, const std::string &n2)
{
    const char *s1 = n1.c_str();
    const char *s2 = n2.c_str();
    if (*s1 == '+') s1++;
    if (*s2 == '+') s2++;
    return !strcmp(s1, s2);
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

UserUpdate &TdAccountData::addUserUpdate(int32_t userId)
{
    auto pExisting = std::find_if(m_updatedUsers.begin(), m_updatedUsers.end(),
                                  [userId](const UserUpdate &upd) { return (upd.userId == userId); });
    if (pExisting == m_updatedUsers.end()) {
        m_updatedUsers.emplace_back();
        UserUpdate &updateInfo = m_updatedUsers.back();
        updateInfo.userId = userId;
        return updateInfo;
    } else
        return *pExisting;
}

void TdAccountData::updateUserStatus(int32_t userId, td::td_api::object_ptr<td::td_api::UserStatus> status)
{
    auto pUser = m_userInfo.find(userId);
    if (pUser == m_userInfo.end()) {
        purple_debug_warning(config::pluginId, "Received updateUserStatus with unknown user_id\n");
        return;
    }

    pUser->second->status_ = std::move(status);
    UserUpdate &updateInfo = addUserUpdate(userId);
    updateInfo.updates.status = true;
}

void TdAccountData::getPrivateChats(std::vector<PrivateChat> &chats) const
{
    chats.clear();
    for (int64_t chatId: m_activeChats) {
        const td::td_api::chat *chat = getChat(chatId);
        if (!chat) {
            purple_debug_warning(config::pluginId, "Received unknown chat id %lld\n", (long long)chatId);
            continue;
        }

        if (chat->type_->get_id() == td::td_api::chatTypePrivate::ID) {
            const td::td_api::chatTypePrivate &privType = static_cast<const td::td_api::chatTypePrivate &>(*chat->type_);
            const td::td_api::user *user = getUser(privType.user_id_);
            if (user)
                chats.emplace_back(*chat, *user);
            else
                purple_debug_warning(config::pluginId, "Received private chat with unknown user id %d\n", (int)privType.user_id_);
        }
    }
}

void TdAccountData::getUnreadChatMessages(std::vector<UnreadChat> &chats)
{
    chats.clear();
    for (auto &pMessage: m_newMessages) {
        int64_t chatId = pMessage->chat_id_;
        auto pUnreadChat = std::find_if(chats.begin(), chats.end(),
                                        [chatId](const UnreadChat &unread) {
                                            return (unread.chatId == chatId);
                                        });
        if (pUnreadChat == chats.end()) {
            chats.emplace_back();
            chats.back().chatId = chatId;
            chats.back().messages.push_back(std::move(pMessage));
        } else
            pUnreadChat->messages.push_back(std::move(pMessage));
    }

    m_newMessages.clear();
}

void TdAccountData::getUpdatedUsers(std::vector<UserUpdate> updates)
{
    updates = std::move(m_updatedUsers);
}

void TdAccountData::addNewMessage(td::td_api::object_ptr<td::td_api::message> message)
{
    m_newMessages.push_back(std::move(message));
}
