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

const td::td_api::user *TdAccountData::getUser(int32_t userId) const
{
    auto pUser = m_userInfo.find(userId);
    if (pUser == m_userInfo.end())
        return nullptr;
    else
        return pUser->second.get();
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

void TdAccountData::addUnreadHistoryRequest(uint64_t queryId, int64_t chatId, int64_t lastReadInId,
                                            int64_t lastReadOutId)
{
    m_chatHistoryRequests.emplace_back();
    RequestHistoryState &state = m_chatHistoryRequests.back();
    state.chatId          = chatId;
    state.lastReadInId    = lastReadInId;
    state.lastReadOutId   = lastReadOutId;
    state.oldestSeenInId  = 0;
    state.oldestSeenOutId = 0;
    state.inboxFinished   = false;
    state.outboxFinished  = false;
    state.queryId         = queryId;
}

void TdAccountData::addFollowUpHistoryRequest(uint64_t lastQueryId, uint64_t newQueryId)
{
    auto it = std::find_if(m_chatHistoryRequests.begin(), m_chatHistoryRequests.end(),
                           [lastQueryId](const RequestHistoryState &state) {
                               return (state.queryId == lastQueryId);
                           });
    if (it != m_chatHistoryRequests.end())
        it->queryId = newQueryId;
}

ChatHistoryResult TdAccountData::handleHistoryResponse(uint64_t requestId,
                                                       td::td_api::object_ptr<td::td_api::Object> object,
                                                       int64_t &chatId,
                                                       int64_t &oldestMessageId)
{
    ChatHistoryResult result = ChatHistoryResult::Unfinished;
    chatId = 0;
    oldestMessageId = 0;

    auto it = std::find_if(m_chatHistoryRequests.begin(), m_chatHistoryRequests.end(),
                           [requestId](const RequestHistoryState &state) {
                               return (state.queryId == requestId);
                           });
    if (it == m_chatHistoryRequests.end()) {
        purple_debug_warning(config::pluginId, "Received history response id %llu doesn't match any RequestHistoryState\n",
            (unsigned long long)requestId);
    } else {
        RequestHistoryState &state = *it;
        int64_t lastMessageId = 0;
        if (object->get_id() != td::td_api::messages::ID) {
            purple_debug_misc(config::pluginId, "Error retreiving unread messages for chat %lld (object id %d) - aborting\n",
                            (long long)state.chatId, (int)object->get_id());
            state.inboxFinished = true;
            state.outboxFinished = true;
        } else {
            td::td_api::messages &messages = static_cast<td::td_api::messages &>(*object);
            purple_debug_misc(config::pluginId, "Received %zu messages for chat %lld\n",
                              messages.messages_.size(), (long long)state.chatId);
            for (auto it = messages.messages_.begin(); it != messages.messages_.end(); ++it)
                if (*it) {
                    td::td_api::object_ptr<td::td_api::message> message = std::move(*it);
                    lastMessageId = message->id_;
                    if (message->is_outgoing_) {
                        if (!state.outboxFinished) {
                            purple_debug_misc(config::pluginId, "Retreived outgoing message %lld for chat %lld\n",
                                              (long long)message->id_, (long long)state.chatId);
                            state.oldestSeenOutId = message->id_;
                            state.outboxFinished = (message->id_ == state.lastReadOutId);

                            // An oddity that happens
                            if (message->id_ == state.lastReadInId) {
                                purple_debug_misc(config::pluginId, "last_read_inbox_message_id_ is actually outgoing message - finishing history retrieval\n");
                                state.inboxFinished = true;
                                state.outboxFinished = true;
                            }

                            if (state.outboxFinished)
                                purple_debug_misc(config::pluginId, "All unread outgoing messages retreived for chat %lld\n",
                                                  (long long)state.chatId);
                            else
                                state.messages.push_back(std::move(message));
                        }
                    } else {
                        if (!state.inboxFinished) {
                            purple_debug_misc(config::pluginId, "Retreived incoming message %lld for chat %lld\n",
                                              (long long)message->id_, (long long)state.chatId);
                            state.oldestSeenInId = message->id_;
                            state.inboxFinished = (message->id_ == state.lastReadInId);

                            // An oddity that happens
                            if (message->id_ == state.lastReadOutId) {
                                purple_debug_misc(config::pluginId, "last_read_outbox_message_id_ is actually incoming message - finishing history retrieval\n");
                                state.inboxFinished = true;
                                state.outboxFinished = true;
                            }

                            if (state.inboxFinished)
                                purple_debug_misc(config::pluginId, "All unread incoming messages retreived for chat %lld\n",
                                                  (long long)state.chatId);
                            else
                                state.messages.push_back(std::move(message));
                        }
                    }

                    if (state.messages.size() >= CHAT_HISTORY_RETRIEVE_LIMIT) {
                        purple_debug_misc(config::pluginId, "Reached unread message limit for chat id %lld\n",
                                          (long long)state.chatId);
                        state.outboxFinished = true;
                        state.inboxFinished = true;
                    }
                }
        }
        if (lastMessageId == 0) {
            purple_debug_misc(config::pluginId, "No messages in the batch - aborting\n");
            state.outboxFinished = true;
            state.inboxFinished = true;
        }
        if (state.inboxFinished && state.outboxFinished) {
            purple_debug_misc(config::pluginId, "Retreived %zu unread messages for chat %lld\n",
                state.messages.size(), (long long)state.chatId);
            result = ChatHistoryResult::Finished;
        } else {
            chatId = state.chatId;
            oldestMessageId = lastMessageId;
        }
    }

    return result;
}

void TdAccountData::getUnreadChatMessages(std::vector<UnreadChat> &chats)
{
    chats.clear();
    for (size_t i = 0; i < m_chatHistoryRequests.size(); ) {
        if (m_chatHistoryRequests[i].inboxFinished && m_chatHistoryRequests[i].outboxFinished) {
            purple_debug_misc(config::pluginId, "Need to show %zu unread messages for chat %lld\n",
                              m_chatHistoryRequests[i].messages.size(),
                              (long long)m_chatHistoryRequests[i].chatId);
            chats.emplace_back();
            chats.back().chatId = m_chatHistoryRequests[i].chatId;
            chats.back().messages = std::move(m_chatHistoryRequests[i].messages);
            m_chatHistoryRequests.erase(m_chatHistoryRequests.begin()+i);
        } else
            i++;
    }
}
