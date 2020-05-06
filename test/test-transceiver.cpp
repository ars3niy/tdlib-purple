#include "test-transceiver.h"
#include "printout.h"
#include <gtest/gtest.h>

using namespace td::td_api;

void TestTransceiver::send(td::Client::Request &&request)
{
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

void TestTransceiver::verifyRequests(const std::vector<Function> &requests)
{
    m_lastRequestIds.clear();
    for (const Function &req: requests) {
        verifyRequestImpl(req);
        m_lastRequestIds.push_back(m_requests.front().id);
        m_requests.pop();
    }
    verifyNoRequests();
}

static void compare(const Function &actual, const Function &expected)
{
}

static void compare(const setTdlibParameters &actual, const setTdlibParameters &expected)
{
    EXPECT_EQ(expected.parameters_->database_directory_, actual.parameters_->database_directory_);
}

static void compare(const checkDatabaseEncryptionKey &actual, const checkDatabaseEncryptionKey &expected)
{
    EXPECT_EQ(expected.encryption_key_, actual.encryption_key_);
}

static void compare(const setAuthenticationPhoneNumber &actual, const setAuthenticationPhoneNumber &expected)
{
    EXPECT_EQ(expected.phone_number_, actual.phone_number_);
    EXPECT_TRUE((expected.settings_ != nullptr) == (actual.settings_ != nullptr));
}

static void compareRequests(const Function &actual, const Function &expected)
{
    EXPECT_EQ(expected.get_id(), actual.get_id()) << "Wrong request type: expected " << requestToString(expected);

#define C(class) case class::ID: \
    compare(static_cast<const class &>(actual), static_cast<const class &>(expected)); \
    break;

    switch (actual.get_id()) {
        C(setTdlibParameters)
        C(checkDatabaseEncryptionKey)
        C(setAuthenticationPhoneNumber)
        default:
            compare(actual, expected);
            break;
    }
}

void TestTransceiver::verifyRequestImpl(const Function &request)
{
    EXPECT_FALSE(m_requests.empty()) << "Missing request: expected " << requestToString(request);

    std::cout << "Received request " << m_requests.front().id << ": " << requestToString(*m_requests.front().function) << "\n";
    compareRequests(*m_requests.front().function, request);
}

void TestTransceiver::verifyNoRequests()
{
    EXPECT_TRUE(m_requests.empty()) << "Unexpected request: " << requestToString(*m_requests.front().function);
}

void TestTransceiver::update(object_ptr<Object> object)
{
    std::cout << "Sending update: " << responseToString(*object) << "\n";
    receive({0, std::move(object)});
}

void TestTransceiver::reply(object_ptr<Object> object)
{
    EXPECT_GE(1u, m_lastRequestIds.size()) << "No requests to reply to";
    std::cout << "Replying to request " << m_lastRequestIds.front() << ": " << responseToString(*object) << "\n";
    receive({m_lastRequestIds.front(), std::move(object)});
    m_lastRequestIds.erase(m_lastRequestIds.begin());
}
