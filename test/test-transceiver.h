#ifndef _TEST_TRANSCEIVER_H
#define _TEST_TRANSCEIVER_H

#include "transceiver.h"
#include <queue>
#include <vector>

class TestTransceiver: public ITransceiverBackend {
public:
    void send(td::Client::Request &&request) override;
    void verifyRequest(const td::td_api::Function &request);
    void verifyRequests(const std::vector<td::td_api::Function> &requests);
    void verifyNoRequests();
    void update(td::td_api::object_ptr<td::td_api::Object> object);
    void reply(td::td_api::object_ptr<td::td_api::Object> object);
private:
    std::queue<td::Client::Request> m_requests;
    std::vector<uint64_t>           m_lastRequestIds;

    void verifyRequestImpl(const td::td_api::Function &request);
};

#endif
