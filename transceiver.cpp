#include "transceiver.h"
#include "config.h"
#include <purple.h>

// This class is used to share ownership of its instances between TdTransceiver and glib idle
// function queue. This way, those idle functions can be called safely after TdTransceiver is
// destroyed.
class TdTransceiverImpl {
public:
    TdTransceiverImpl(PurpleTdClient *owner, TdTransceiver::UpdateCb updateCb, ITransceiverBackend *testBackend);
    ~TdTransceiverImpl();
    static int rxCallback(void *user_data);

    PurpleTdClient                     *m_owner;
    std::unique_ptr<td::Client>         m_client;

    // The mutex protects m_rxQueue and reference counters of all shared pointers to this object
    std::mutex                          m_rxMutex;
    std::vector<td::Client::Response>   m_rxQueue;

    TdTransceiver::UpdateCb             m_updateCb;
    // Data structures for transmission are not thread-safe for sendQuery is only called from glib main thread
    uint64_t                                           m_lastQueryId;
    std::map<std::uint64_t, TdTransceiver::ResponseCb> m_responseHandlers;
};

TdTransceiverImpl::TdTransceiverImpl(PurpleTdClient *owner, TdTransceiver::UpdateCb updateCb,
                                     ITransceiverBackend *testBackend
)
:   m_owner(owner),
    m_updateCb(updateCb),
    m_lastQueryId(0)
{
    if (!testBackend)
        m_client = std::make_unique<td::Client>();
}

TdTransceiverImpl::~TdTransceiverImpl()
{
    purple_debug_misc(config::pluginId, "Destroyed TdTransceiverImpl\n");
}

TdTransceiver::TdTransceiver(PurpleTdClient *owner, UpdateCb updateCb, ITransceiverBackend *testBackend)
:   m_stopThread(false)
{
    m_impl = std::make_shared<TdTransceiverImpl>(owner, updateCb, testBackend);
    if (testBackend) {
        m_testBackend = testBackend;
        m_testBackend->setOwner(this);
        return;
    }
    m_testBackend = nullptr;

#if !GLIB_CHECK_VERSION(2, 32, 0)
    // GLib threading system is automaticaly initialized since 2.32.
    // For earlier versions, it have to be initialized before calling any
    // Glib or GTK+ functions.
    if (!g_thread_supported())
        g_thread_init(NULL);
#endif

    m_pollThread = std::thread([this]() { pollThreadLoop(); });
}

TdTransceiver::~TdTransceiver()
{
    m_stopThread = true;
    if (!m_testBackend) {
        m_impl->m_client->send({UINT64_MAX, td::td_api::make_object<td::td_api::close>()});
        m_pollThread.join();
    }

    // Orphan m_impl - if the background thread generated idle callbacks while we were waiting for
    // it to quit, those callbacks will be called after this destructor return (doing nothing, as
    // m_impl->m_owner gets set to NULL), and only then with TdTransceiverImpl instance be destroyed
    m_impl->m_owner = nullptr;

    // Since poll thread is no longer running, there is no need to lock the mutex before decrementing
    // shared pointer reference count
    m_impl.reset();
    purple_debug_misc(config::pluginId, "Destroyed TdTransceiver\n");
}

void *TdTransceiver::queueResponse(td::Client::Response &&response)
{
    m_impl->m_rxQueue.push_back(std::move(response));
    return new std::shared_ptr<TdTransceiverImpl>(m_impl);
}

void TdTransceiver::pollThreadLoop()
{
    // TODO: what happens if some other update is received at the same time as the destructor is
    // called? Is there something important that could get missed?
    while (!m_stopThread) {
        td::Client::Response response = m_impl->m_client->receive(1);

        if (response.object) {
            // Passing shared pointer through glib event queue using pointer to pointer seems funky,
            // but it works
            void *implRef;
            {
                std::unique_lock<std::mutex> lock(m_impl->m_rxMutex);
                implRef = queueResponse(std::move(response));
            }
            g_idle_add(TdTransceiverImpl::rxCallback, implRef);
        }
    }
}

int TdTransceiverImpl::rxCallback(gpointer user_data)
{
    std::shared_ptr<TdTransceiverImpl> *ppSelf =
        static_cast<std::shared_ptr<TdTransceiverImpl> *>(user_data);
    std::shared_ptr<TdTransceiverImpl> &self = *ppSelf;

    while (1) {
        td::Client::Response response;
        {
            std::unique_lock<std::mutex> lock(self->m_rxMutex);
            if (self->m_rxQueue.empty())
                break;
            response = std::move(self->m_rxQueue.front());
            self->m_rxQueue.erase(self->m_rxQueue.begin());
        }
        // m_owner will be NULL if this callback is invoked after TdTransceiver destructor
        if (!self->m_owner)
            purple_debug_misc(config::pluginId,
                              "Ignoring response (object id %d) as transceiver is already destroyed\n",
                              (int)response.object->get_id());
        else if (response.id == 0)
            ((self->m_owner)->*(self->m_updateCb))(std::move(response.object));
        else {
            TdTransceiver::ResponseCb callback = nullptr;
            auto it = self->m_responseHandlers.find(response.id);
            if (it != self->m_responseHandlers.end()) {
                callback = it->second;
                self->m_responseHandlers.erase(it);
            } else
                purple_debug_misc(config::pluginId, "Ignoring response to request %llu\n",
                                  (unsigned long long)response.id);
            if (callback)
                ((self->m_owner)->*callback)(response.id, std::move(response.object));
        }
    }

    std::unique_lock<std::mutex> lock(self->m_rxMutex, std::defer_lock);
    // owner=NULL means TdTransceiver has been destroyed, so the poll thread is no longer running
    // and no mutex lock is needed - in fact, it must be avoided because otherwise unlocking the
    // mutex after clearing the pointer will be use after free.
    if (self->m_owner)
        lock.lock();
    self.reset();
    delete ppSelf;

    return FALSE; // This idle handler will not be called again
}

uint64_t TdTransceiver::sendQuery(td::td_api::object_ptr<td::td_api::Function> f, ResponseCb handler)
{
    uint64_t queryId = ++m_impl->m_lastQueryId;
    purple_debug_misc(config::pluginId, "Sending query id %lu\n", (unsigned long)queryId);
    if (handler)
        m_impl->m_responseHandlers.emplace(queryId, std::move(handler));
    if (m_testBackend)
        m_testBackend->send({queryId, std::move(f)});
    else
        m_impl->m_client->send({queryId, std::move(f)});
    return queryId;
}

void ITransceiverBackend::receive(td::Client::Response response)
{
    TdTransceiverImpl::rxCallback(m_owner->queueResponse(std::move(response)));
}
