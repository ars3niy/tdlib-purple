#ifndef _IDENTIFIERS_H
#define _IDENTIFIERS_H

#include <stdint.h>
#include <string>
#include <td/telegram/td_api.h>

template<typename IntType>
class Identifier {
public:
    explicit Identifier(IntType value) : m_value(value) {}
    bool    valid() const { return (m_value != 0); }
    IntType value() const { return m_value; }
protected:
    using IdType = IntType;
private:
    IntType m_value;
    
    template<typename Object>
    friend void setId(Object &object, Identifier<IntType> id);
};

template<typename Object, typename IntType>
void setId(Object &object, Identifier<IntType> id)
{
    object.id_ = id.value;
}

#define DEFINE_ID_CLASS(classname, inttype) \
class classname: public Identifier<inttype> { \
private: \
    explicit classname(IdType id) : Identifier(id) {} \
public: \
    classname() : classname(classname::invalid) {} \
    bool operator==(const classname &other) const { return (value() == other.value()); } \
    bool operator!=(const classname &other) const { return (value() != other.value()); } \
    bool operator<(const classname &other) const  { return (value() < other.value()); } \
    static const classname invalid;

DEFINE_ID_CLASS(UserId, int32_t)
    friend UserId getId(const td::td_api::user &user);
    friend UserId getUserId(const td::td_api::chatTypePrivate &privType);
    friend UserId getUserId(const td::td_api::chatMember &member);
    friend UserId getUserId(const td::td_api::call &call);
    friend UserId getSenderUserId(const td::td_api::message &message);
    friend UserId getSenderUserId(const td::td_api::messageForwardOriginUser &forwardOrigin);
    friend UserId getUserId(const td::td_api::secretChat &secretChat);
    friend UserId getUserId(const td::td_api::updateUserStatus &update);
    friend UserId getUserId(const td::td_api::updateUserChatAction &update);
    friend UserId getUserId(const td::td_api::importedContacts &contacts, unsigned index);
    friend UserId getUserId(const td::td_api::users &users, unsigned index);
    friend UserId stringToUserId(const char *s);
};

DEFINE_ID_CLASS(ChatId, int64_t)
    friend ChatId getId(const td::td_api::chat &chat);
    friend ChatId getChatId(const td::td_api::updateChatChatList &update);
    friend ChatId getChatId(const td::td_api::updateChatOrder &update);
    friend ChatId getChatId(const td::td_api::updateChatTitle &update);
    friend ChatId getChatId(const td::td_api::messageForwardOriginChannel &forwardOrigin);
    friend ChatId getChatId(const td::td_api::message &message);
    friend ChatId getChatId(const td::td_api::updateUserChatAction &update);
    friend ChatId stringToChatId(const char *s);
};

DEFINE_ID_CLASS(BasicGroupId, int32_t)
    friend BasicGroupId getId(const td::td_api::basicGroup &group);
    friend BasicGroupId getBasicGroupId(const td::td_api::updateBasicGroupFullInfo &update);
    friend BasicGroupId getBasicGroupId(const td::td_api::chatTypeBasicGroup &update);
};

DEFINE_ID_CLASS(SupergroupId, int32_t)
    friend SupergroupId getId(const td::td_api::supergroup &group);
    friend SupergroupId getSupergroupId(const td::td_api::updateSupergroupFullInfo &update);
    friend SupergroupId getSupergroupId(const td::td_api::chatTypeSupergroup &update);
};

DEFINE_ID_CLASS(SecretChatId, int32_t)
    friend SecretChatId getId(const td::td_api::secretChat &secretChat);
    friend SecretChatId getSecretChatId(const td::td_api::chatTypeSecret &update);
};

#undef DEFINE_ID_CLASS

UserId       getId(const td::td_api::user &user);
ChatId       getId(const td::td_api::chat &chat);
BasicGroupId getId(const td::td_api::basicGroup &group);
SupergroupId getId(const td::td_api::supergroup &group);
SecretChatId getId(const td::td_api::secretChat &secretChat);

UserId       getUserId(const td::td_api::chatTypePrivate &privType);
UserId       getUserId(const td::td_api::chatMember &member);
UserId       getUserId(const td::td_api::call &call);
UserId       getSenderUserId(const td::td_api::message &message);
UserId       getSenderUserId(const td::td_api::messageForwardOriginUser &forwardOrigin);
UserId       getUserId(const td::td_api::secretChat &secretChat);
UserId       getUserId(const td::td_api::updateUserStatus &update);
UserId       getUserId(const td::td_api::updateUserChatAction &update);
UserId       getUserId(const td::td_api::importedContacts &contacts, unsigned index);
UserId       getUserId(const td::td_api::users &users, unsigned index);
UserId       stringToUserId(const char *s);

ChatId       getChatId(const td::td_api::updateChatChatList &update);
ChatId       getChatId(const td::td_api::updateChatOrder &update);
ChatId       getChatId(const td::td_api::updateChatTitle &update);
ChatId       getChatId(const td::td_api::messageForwardOriginChannel &forwardOrigin);
ChatId       getChatId(const td::td_api::message &message);
ChatId       getChatId(const td::td_api::updateUserChatAction &update);
ChatId       stringToChatId(const char *s);

BasicGroupId getBasicGroupId(const td::td_api::updateBasicGroupFullInfo &update);
BasicGroupId getBasicGroupId(const td::td_api::chatTypeBasicGroup &chatType);

SupergroupId getSupergroupId(const td::td_api::updateSupergroupFullInfo &update);
SupergroupId getSupergroupId(const td::td_api::chatTypeSupergroup &chatType);

SecretChatId getSecretChatId(const td::td_api::chatTypeSecret &update);

namespace std {
    static inline std::string to_string(UserId id) { return to_string(id.value()); }
}

#endif
