#ifndef _TRANSCEIVER_H
#define _TRANSCEIVER_H

#include <td/telegram/Client.h>
#include <td/telegram/td_api.hpp>
#include <thread>
#include <mutex>
#include <map>
#include <atomic>
#include <purple.h>

class PurpleTdClient;
class TdTransceiverImpl;
class TdTransceiver;

class ITransceiverBackend {
public:
    virtual ~ITransceiverBackend() {}

    void         setOwner(TdTransceiver *owner) { m_owner = owner; }
    virtual void send(td::Client::Request &&request) = 0;
    virtual void addTimeout(guint interval, GSourceFunc function, gpointer data) = 0;
    void         receive(td::Client::Response response);
private:
    TdTransceiver *m_owner = nullptr;
};

// A wrapper around td::Client which processes incoming events (updates and responses to requests)
// in glib main thread using idle function, and also provides request-id-to-callback mapping
class TdTransceiver {
    friend class ITransceiverBackend;
private:
    using TdObjectPtr = td::td_api::object_ptr<td::td_api::Object>;
public:
    using ResponseCb = void (PurpleTdClient::*)(uint64_t requestId, TdObjectPtr object);
    using UpdateCb   = void (PurpleTdClient::*)(td::td_api::Object &object);
    using TimerCb    = void (PurpleTdClient::*)(uint64_t requestId);

    TdTransceiver(PurpleTdClient *owner, PurpleAccount *account, UpdateCb updateCb,
                  ITransceiverBackend *testBackend);
    ~TdTransceiver();
    uint64_t sendQuery(td::td_api::object_ptr<td::td_api::Function> f, ResponseCb handler);

    // WARNING: receiving response does not cancel timeout callback, nor does timeout prevent
    // response callback if response is received later
    uint64_t sendQueryWithTimeout(td::td_api::object_ptr<td::td_api::Function> f,
                                  ResponseCb handler, unsigned timeoutSeconds);
    void     setQueryTimer(uint64_t queryId, TimerCb handler, unsigned timeoutSeconds);
private:
    void  pollThreadLoop();
    void *queueResponse(td::Client::Response &&response);

    std::shared_ptr<TdTransceiverImpl>  m_impl;
    PurpleAccount                      *m_account;
    std::thread                         m_pollThread;
    std::atomic_bool                    m_stopThread;
    ITransceiverBackend                *m_testBackend;
};

#endif
