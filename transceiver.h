#ifndef _TRANSCEIVER_H
#define _TRANSCEIVER_H

#include <td/telegram/Client.h>
#include <td/telegram/td_api.hpp>
#include <thread>
#include <mutex>
#include <map>
#include <atomic>

class PurpleTdClient;

// A wrapper around td::Client which processes incoming events (updates and responses to requests)
// in glib main thread using idle function, and also provides request-id-to-callback mapping
class TdTransceiver {
private:
    using TdObjectPtr = td::td_api::object_ptr<td::td_api::Object>;
public:
    using ResponseCb = void (PurpleTdClient::*)(uint64_t requestId, TdObjectPtr object);
    using UpdateCb   = void (PurpleTdClient::*)(TdObjectPtr object);

    TdTransceiver(PurpleTdClient *owner, UpdateCb updateCb);
    ~TdTransceiver();
    uint64_t sendQuery(td::td_api::object_ptr<td::td_api::Function> f, ResponseCb handler);
private:
    void       pollThreadLoop();
    static int rxCallback(void *user_data);

    PurpleTdClient                     *m_owner;
    std::unique_ptr<td::Client>         m_client;
    std::thread                         m_pollThread;
    std::atomic_bool                    m_stopThread;

    std::vector<td::Client::Response>   m_rxQueue;
    std::mutex                          m_rxQueueMutex;

    UpdateCb                            m_updateCb;
    // Data structures for transmission are not thread-safe for sendQuery is only called from glib main thread
    uint64_t                            m_lastQueryId;
    std::map<std::uint64_t, ResponseCb> m_responseHandlers;
};

#endif
