#include "call.h"
#include "client-utils.h"
#include "config.h"
#include "buildopt.h"
#include "format.h"

static td::td_api::object_ptr<td::td_api::callProtocol> getCallProtocol()
{
    auto protocol = td::td_api::make_object<td::td_api::callProtocol>();
    protocol->udp_p2p_       = true;
    protocol->udp_reflector_ = true;
    protocol->min_layer_     = 65;
    protocol->max_layer_     = 65;
    return protocol;
}

bool initiateCall(int32_t userId, TdAccountData &account, TdTransceiver &transceiver)
{
    if (account.hasActiveCall())
        return false;

    td::td_api::object_ptr<td::td_api::createCall> callRequest = td::td_api::make_object<td::td_api::createCall>();
    callRequest->user_id_  = userId;
    callRequest->protocol_ = getCallProtocol();
    return false;
}

static void discardCall(const td::td_api::call &call, TdTransceiver &transceiver)
{
    td::td_api::object_ptr<td::td_api::discardCall> discardReq = td::td_api::make_object<td::td_api::discardCall>();
    discardReq->call_id_ = call.id_;
    discardReq->is_disconnected_ = true;
    discardReq->duration_ = 0;
    discardReq->connection_id_ = 0;
    transceiver.sendQuery(std::move(discardReq), nullptr);
}

void updateCall(const td::td_api::call &call, TdAccountData &account, TdTransceiver &transceiver)
{
#ifndef NoVoip
    PurpleMediaManager *mediaManager = purple_media_manager_get();
    PurpleMediaCaps capabilities = purple_media_manager_get_ui_caps(mediaManager);

    if (!(capabilities & PURPLE_MEDIA_CAPS_AUDIO) && !(capabilities & PURPLE_MEDIA_CAPS_AUDIO_SINGLE_DIRECTION)) {
#else
    if (true) {
#endif
        purple_debug_misc(config::pluginId, "Ignoring incoming call: no audio capability\n");
        if (call.state_ && (call.state_->get_id() == td::td_api::callStatePending::ID)) {
            const td::td_api::user *user = account.getUser(call.user_id_);
            if (user) {
                std::string buddyName = getPurpleBuddyName(*user);
                showMessageTextIm(account, buddyName.c_str(), NULL,
                                _("Received incoming call, but calls are not supported"),
                                time(NULL), PURPLE_MESSAGE_SYSTEM);
            }

            discardCall(call, transceiver);
        }
        return;
    }

    if (!call.is_outgoing_) {
        /*if (not already in this call && call.state_ && (call.state_->get_id() == td::td_api::callStatePending::ID)) {
            const td::td_api::user *user = account.getUser(call.user_id_);
            if (!user) return;
            request call
            if accepted:
            td::td_api::object_ptr<td::td_api::acceptCall> acceptReq = td::td_api::make_object<td::td_api::acceptCall>();
            acceptReq->call_id_ = GPOINTER_TO_INT(purple_media_get_prpl_data(media));
            acceptReq->protocol_ = getCallProtocol();
            transceiver->sendQuery(std::move(acceptReq), nullptr);
        }*/
    }
}
