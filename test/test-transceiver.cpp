#include "test-transceiver.h"
#include <gtest/gtest.h>

static std::string requestToString(const td::td_api::Function &req)
{
    return "whatever";
}

void TestTransceiver::send(td::Client::Request &&request)
{
    m_requests.push(std::move(request));
}

void TestTransceiver::verifyRequest(const td::td_api::Function &request)
{
    m_lastRequestIds.clear();
    verifyRequestImpl(request);
    m_lastRequestIds.push_back(m_requests.front().id);
    m_requests.pop();
    verifyNoRequests();
}

void TestTransceiver::verifyRequests(const std::vector<td::td_api::Function> &requests)
{
    m_lastRequestIds.clear();
    for (const td::td_api::Function &req: requests) {
        verifyRequestImpl(req);
        m_lastRequestIds.push_back(m_requests.front().id);
        m_requests.pop();
    }
    verifyNoRequests();
}

void TestTransceiver::verifyRequestImpl(const td::td_api::Function &request)
{
    EXPECT_FALSE(m_requests.empty()) << "Missing request: expected " << requestToString(request);
}

void TestTransceiver::verifyNoRequests()
{
    EXPECT_TRUE(m_requests.empty()) << "Unexpected request: " << requestToString(*m_requests.front().function);
}
