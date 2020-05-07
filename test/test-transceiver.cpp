#include "test-transceiver.h"
#include "printout.h"
#include <gtest/gtest.h>

using namespace td::td_api;

void TestTransceiver::send(td::Client::Request &&request)
{
    ASSERT_EQ(expectedRequestId, request.id);
    expectedRequestId++;
    m_requests.push(std::move(request));
}

void TestTransceiver::verifyRequest(const Function &request)
{
    m_lastRequestIds.clear();
    verifyRequestImpl(request);
    m_lastRequestIds.push_back(m_requests.front().id);
    m_requests.pop();
    verifyNoRequests();
}

void TestTransceiver::verifyRequests(std::initializer_list<td::td_api::object_ptr<td::td_api::Function>> requests)
{
    m_lastRequestIds.clear();
    for (auto &pReq: requests) {
        verifyRequestImpl(*pReq);
        m_lastRequestIds.push_back(m_requests.front().id);
        m_requests.pop();
    }
    verifyNoRequests();
}

static void compare(const setTdlibParameters &actual, const setTdlibParameters &expected)
{
    ASSERT_EQ(expected.parameters_->database_directory_, actual.parameters_->database_directory_);
}

static void compare(const checkDatabaseEncryptionKey &actual, const checkDatabaseEncryptionKey &expected)
{
    ASSERT_EQ(expected.encryption_key_, actual.encryption_key_);
}

static void compare(const setAuthenticationPhoneNumber &actual, const setAuthenticationPhoneNumber &expected)
{
    ASSERT_EQ(expected.phone_number_, actual.phone_number_);
    ASSERT_TRUE((expected.settings_ != nullptr) == (actual.settings_ != nullptr));
}

static void compare(const viewMessages &actual, const viewMessages &expected)
{
    ASSERT_EQ(expected.chat_id_, actual.chat_id_);
    ASSERT_EQ(expected.message_ids_.size(), actual.message_ids_.size());
    for (size_t i = 0; i < actual.message_ids_.size(); i++)
        ASSERT_EQ(expected.message_ids_[i], actual.message_ids_[i]);
    ASSERT_EQ(expected.force_read_, actual.force_read_);
}

static void compareRequests(const Function &actual, const Function &expected)
{
    ASSERT_EQ(expected.get_id(), actual.get_id()) << "Wrong request type: expected " << requestToString(expected);

#define C(class) case class::ID: \
    compare(static_cast<const class &>(actual), static_cast<const class &>(expected)); \
    break;

    switch (actual.get_id()) {
        C(setTdlibParameters)
        C(checkDatabaseEncryptionKey)
        C(setAuthenticationPhoneNumber)
        case getContacts::ID: break;
        case getChats::ID: break;
        C(viewMessages)
        default: ASSERT_TRUE(false) << "Unsupported request " << requestToString(actual);
    }
}

void TestTransceiver::verifyRequestImpl(const Function &request)
{
    ASSERT_FALSE(m_requests.empty()) << "Missing request: expected " << requestToString(request);

    std::cout << "Received request " << m_requests.front().id << ": " << requestToString(*m_requests.front().function) << "\n";
    compareRequests(*m_requests.front().function, request);
}

void TestTransceiver::verifyNoRequests()
{
    ASSERT_TRUE(m_requests.empty()) << "Unexpected request: " << requestToString(*m_requests.front().function);
}

void TestTransceiver::update(object_ptr<Object> object)
{
    std::cout << "Sending update: " << responseToString(*object) << "\n";
    receive({0, std::move(object)});
}

void TestTransceiver::reply(object_ptr<Object> object)
{
    ASSERT_FALSE(m_lastRequestIds.empty()) << "No requests to reply to";
    std::cout << "Replying to request " << m_lastRequestIds.front() << ": " << responseToString(*object) << "\n";
    receive({m_lastRequestIds.front(), std::move(object)});
    m_lastRequestIds.erase(m_lastRequestIds.begin());
}

namespace td {
namespace td_api {

object_ptr<user> makeUser(std::int32_t id_, std::string const &first_name_,
                          std::string const &last_name_,
                          std::string const &phone_number_,
                          object_ptr<UserStatus> &&status_)
{
    return make_object<user>(
        id_, first_name_, last_name_, "", phone_number_, std::move(status_),
        nullptr,
        false,
        false,
        false,
        false,
        "",
        false,
        true,
        make_object<userTypeRegular>(),
        ""
    );
}

object_ptr<chat> makeChat(std::int64_t id_,
                          object_ptr<ChatType> &&type_,
                          std::string const &title_,
                          object_ptr<message> &&last_message_,
                          std::int32_t unread_count_,
                          std::int64_t last_read_inbox_message_id_,
                          std::int64_t last_read_outbox_message_id_)
{
    return make_object<chat>(
        id_,
        std::move(type_),
        nullptr,
        title_,
        nullptr,
        make_object<chatPermissions>(true, true, true, true, true, false, false, false),
        std::move(last_message_),
        0,
        false,
        unread_count_ > 0,
        false,
        false,
        true,
        true,
        false,
        false,
        unread_count_,
        last_read_inbox_message_id_,
        last_read_outbox_message_id_,
        0,
        make_object<chatNotificationSettings>(true, 0, true, "default", true, false, true, false, true, false),
        nullptr,
        0,
        0,
        nullptr,
        ""
    );
}

object_ptr<message> makeMessage(std::int64_t id_, std::int32_t sender_user_id_, std::int64_t chat_id_,
                                bool is_outgoing_, std::int32_t date_, object_ptr<MessageContent> &&content_)
{
    return make_object<message>(
        id_,
        sender_user_id_,
        chat_id_,
        nullptr,
        nullptr,
        is_outgoing_,
        false,
        true,
        true,
        true,
        false,
        false,
        date_,
        0,
        nullptr,
        0,
        0,
        0,
        0,
        "",
        0,
        0,
        "",
        std::move(content_),
        nullptr
    );
}

object_ptr<messageText> makeTextMessage(const std::string &text)
{
    return make_object<messageText>(
        make_object<formattedText>(text, std::vector<object_ptr<textEntity>>()),
        nullptr
    );
}

}
}
