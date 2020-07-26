#include "call.h"
#include "client-utils.h"
#include "config.h"
#include "buildopt.h"

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

void updateCall(const td::td_api::call &call, TdAccountData &account, TdTransceiver &transceiver)
{
    PurpleMediaManager *mediaManager = purple_media_manager_get();
    PurpleMediaCaps capabilities = purple_media_manager_get_ui_caps(mediaManager);
    if (!(capabilities & PURPLE_MEDIA_CAPS_AUDIO) && !(capabilities & PURPLE_MEDIA_CAPS_AUDIO_SINGLE_DIRECTION)) {
        purple_debug_misc(config::pluginId, "Ignoring incoming call: no audio capabilities\n");
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
