#include "transceiver.h"
#include "config.h"
#include <purple.h>

TdTransceiver::TdTransceiver(PurpleTdClient *owner, UpdateCb updateCb)
:   m_owner(owner),
    m_stopThread(false),
    m_updateCb(updateCb),
    m_lastQueryId(0)
{
#if !GLIB_CHECK_VERSION(2, 32, 0)
    // GLib threading system is automaticaly initialized since 2.32.
    // For earlier versions, it have to be initialized before calling any
    // Glib or GTK+ functions.
    if (!g_thread_supported())
        g_thread_init(NULL);
#endif

    m_client = std::make_unique<td::Client>();
    m_pollThread = std::thread([this]() { pollThreadLoop(); });
}

TdTransceiver::~TdTransceiver()
{
    m_stopThread = true;
    m_client->send({UINT64_MAX, td::td_api::make_object<td::td_api::close>()});
    m_pollThread.join();
}

void TdTransceiver::pollThreadLoop()
{
    while (!m_stopThread) {
        td::Client::Response response = m_client->receive(1);

        if (response.object) {
            {
                std::unique_lock<std::mutex> lock(m_rxQueueMutex);
                m_rxQueue.push_back(std::move(response));
            }
            g_idle_add(rxCallback, this);
        }
    }
}

int TdTransceiver::rxCallback(gpointer user_data)
{
    TdTransceiver *self = static_cast<TdTransceiver *>(user_data);

    while (1) {
        td::Client::Response response;
        {
            std::unique_lock<std::mutex> lock(self->m_rxQueueMutex);
            if (self->m_rxQueue.empty())
                break;
            response = std::move(self->m_rxQueue.front());
            self->m_rxQueue.erase(self->m_rxQueue.begin());
        }
        if (response.id == 0)
            ((self->m_owner)->*(self->m_updateCb))(std::move(response.object));
        else {
            ResponseCb callback = nullptr;
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

    return FALSE; // This idle handler will not be called again
}

uint64_t TdTransceiver::sendQuery(td::td_api::object_ptr<td::td_api::Function> f, ResponseCb handler)
{
    uint64_t queryId = ++m_lastQueryId;
    purple_debug_misc(config::pluginId, "Sending query id %lu\n", (unsigned long)queryId);
    if (handler)
        m_responseHandlers.emplace(queryId, std::move(handler));
    m_client->send({queryId, std::move(f)});
    return queryId;
}
