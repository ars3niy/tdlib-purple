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
    protocol->max_layer_     = 92;
    return protocol;
}

bool initiateCall(int32_t userId, TdAccountData &account, TdTransceiver &transceiver)
{
#ifndef NoVoip
    if (!account.hasActiveCall()) {
        td::td_api::object_ptr<td::td_api::createCall> callRequest = td::td_api::make_object<td::td_api::createCall>();
        callRequest->user_id_  = userId;
        callRequest->protocol_ = getCallProtocol();
        transceiver.sendQuery(std::move(callRequest), nullptr);
    } else
        purple_notify_warning(account.purpleAccount, _("Voice call"),
                              _("Cannot start new call, already in another call"), NULL);
#endif

    return false;
}

static void discardCall(int32_t callId, TdTransceiver &transceiver)
{
    td::td_api::object_ptr<td::td_api::discardCall> discardReq = td::td_api::make_object<td::td_api::discardCall>();
    discardReq->call_id_ = callId;
    discardReq->is_disconnected_ = true;
    discardReq->duration_ = 0;
    discardReq->connection_id_ = 0;
    transceiver.sendQuery(std::move(discardReq), nullptr);
}

struct CallRequestData {
    int            callId;
    TdTransceiver *transceiver;
    TdAccountData *account;
};

static void acceptCallCb(CallRequestData *data, int action)
{
    std::unique_ptr<CallRequestData> request(data);

    td::td_api::object_ptr<td::td_api::acceptCall> acceptReq = td::td_api::make_object<td::td_api::acceptCall>();
    acceptReq->call_id_ = request->callId;
    acceptReq->protocol_ = getCallProtocol();
    request->transceiver->sendQuery(std::move(acceptReq), nullptr);
}

static void discardCallCb(CallRequestData *data, int action)
{
    std::unique_ptr<CallRequestData> request(data);
    discardCall(request->callId, *request->transceiver);
    request->account->removeActiveCall();
}

static std::string getPurpleUserName(int32_t userId, TdAccountData &account)
{
    const td::td_api::user *user = account.getUser(userId);
    if (user) {
        const td::td_api::chat *privateChat = account.getPrivateChatByUserId(userId);
        if (privateChat && isChatInContactList(*privateChat, user))
            return getPurpleBuddyName(*user);
        else
            return account.getDisplayName(*user);
    } else
        return std::string();
}

static bool activateCall(const td::td_api::call &call, const std::string &buddyName,
                         TdAccountData &account, TdTransceiver &transceiver)
{
#ifndef NoVoip
    if (call.state_->get_id() != td::td_api::callStateReady::ID)
        return false;
    const td::td_api::callStateReady &state = static_cast<const td::td_api::callStateReady &>(*call.state_);

    if (state.protocol_->max_layer_ < 74) {
        // libtgvoip crashes with openssl assertion failure if it goes into if(!useMTProto2) branch
        // of VoIPController::ProcessIncomingPacket
        // Unlikely error message not worth translating
        if (!buddyName.empty())
            showMessageTextIm(account, buddyName.c_str(), NULL,
                              "Discarding call due to low protocol layer",
                              time(NULL), PURPLE_MESSAGE_SYSTEM);

        return false;
    }

    tgvoip::VoIPController *voip = account.getCallData();
    if (!voip)
        return false;

    static tgvoip::VoIPController::Config config = {
        .init_timeout      = 30.0,
        .recv_timeout      = 30.0,
        .data_saving       = tgvoip::DATA_SAVING_NEVER,
        .enableAEC         = true,
        .enableNS          = true,
        .enableAGC         = true,
        .enableCallUpgrade = false,
    };
    voip->SetConfig(config);

    std::vector<tgvoip::Endpoint> endpoints;
    for (const auto &pConnection: state.connections_) {
        std::vector<char *> tag(pConnection->peer_tag_.length());
        memmove(tag.data(), pConnection->peer_tag_.c_str(), tag.size());
        endpoints.push_back(tgvoip::Endpoint(pConnection->id_, pConnection->port_,
                                             tgvoip::IPv4Address(pConnection->ip_),
                                             tgvoip::IPv6Address(pConnection->ipv6_),
                                             tgvoip::Endpoint::UDP_RELAY,
                                             reinterpret_cast<unsigned char *>(tag.data())));
    }
    voip->SetRemoteEndpoints(endpoints, state.allow_p2p_ && state.protocol_->udp_p2p_,
                             state.protocol_->max_layer_);

    std::vector<char> key(state.encryption_key_.length()+1);
    memmove(key.data(), state.encryption_key_.c_str(), key.size());
    voip->SetEncryptionKey(key.data(), call.is_outgoing_);
    voip->Start();
    voip->Connect();

    if (!buddyName.empty()) {
        // For an outgoing call, "type /hangup to terminate" has already been shown when the call
        // was initiated
        const char *message = call.is_outgoing_ ? _("Call active") :
                                                  _("Call active, type /hangup to terminate");
        showMessageTextIm(account, buddyName.c_str(), NULL, message,
                          time(NULL), PURPLE_MESSAGE_SYSTEM);
    }

#endif
    return true;
}

static void deactivateCall(TdAccountData &account)
{
#ifndef NoVoip
    tgvoip::VoIPController *voip = account.getCallData();
    if (voip)
        voip->Stop();
#endif
}

static void notifyCallError(const td::td_api::callStateError &error, const std::string &buddyName,
                            TdAccountData &account)
{
    std::string message;
    if (error.error_)
        message = formatMessage(errorCodeMessage(), {std::to_string(error.error_->code_),
                                error.error_->message_});
    else
        // Unlikely message not worth translating
        message = "unknown error";
    message = formatMessage(_("Call failed: {}"), message);
    if (!buddyName.empty())
        showMessageTextIm(account, buddyName.c_str(), NULL, message.c_str(),
                          time(NULL), PURPLE_MESSAGE_SYSTEM);
}

void updateCall(const td::td_api::call &call, TdAccountData &account, TdTransceiver &transceiver)
{
    std::string buddyName = getPurpleUserName(call.user_id_, account);

#ifndef NoVoip
    PurpleMediaManager *mediaManager = purple_media_manager_get();
    PurpleMediaCaps capabilities = purple_media_manager_get_ui_caps(mediaManager);

    GHashTable *ui_info = purple_core_get_ui_info();
    const char *name = static_cast<char *>(g_hash_table_lookup(ui_info, "name"));
    bool isPidgin = name && !strcmp(name, "Pidgin");

    if (!(capabilities & PURPLE_MEDIA_CAPS_AUDIO) && !(capabilities & PURPLE_MEDIA_CAPS_AUDIO_SINGLE_DIRECTION) &&
        !isPidgin) {
#else
    if (true) {
#endif
        purple_debug_misc(config::pluginId, "Ignoring incoming call: no audio capability\n");
        if (call.state_ && (call.state_->get_id() == td::td_api::callStatePending::ID)) {
            if (!buddyName.empty())
                showMessageTextIm(account, buddyName.c_str(), NULL,
                                  _("Received incoming call, but calls are not supported"),
                                  time(NULL), PURPLE_MESSAGE_SYSTEM);

            discardCall(call.id_, transceiver);
        }
        return;
    }

    if (!call.state_) return; // just in case

    if (!call.is_outgoing_ && (call.state_->get_id() == td::td_api::callStatePending::ID)) {
        if (!account.hasActiveCall()) {
            account.setActiveCall(call.id_);
            std::string message = formatMessage(_("{} wishes to start a call with you."),
                                                account.getDisplayName(call.user_id_));
            CallRequestData *request = new CallRequestData;
            request->callId = call.id_;
            request->transceiver = &transceiver;
            request->account = &account;
            purple_request_action(purple_account_get_connection(account.purpleAccount),
                                  _("Voice call"), message.c_str(), NULL,
                                  PURPLE_DEFAULT_ACTION_NONE,
                                  account.purpleAccount, !buddyName.empty() ? buddyName.c_str() : NULL, NULL,
                                  request, 2, _("_OK"), acceptCallCb, _("_Cancel"), discardCallCb);
        } else if (call.id_ != account.getActiveCallId()) {
            if (!buddyName.empty())
                showMessageTextIm(account, buddyName.c_str(), NULL,
                                _("Received incoming call while already in another call"),
                                time(NULL), PURPLE_MESSAGE_SYSTEM);

            discardCall(call.id_, transceiver);
        }
    } else if (call.is_outgoing_ && (call.state_->get_id() == td::td_api::callStatePending::ID)) {
        if (!account.hasActiveCall()) {
            account.setActiveCall(call.id_);
            if (!buddyName.empty())
                showMessageTextIm(account, buddyName.c_str(), NULL,
                                _("Call pending, type /hangup to terminate"),
                                time(NULL), PURPLE_MESSAGE_SYSTEM);
        } else if (call.id_ != account.getActiveCallId()) {
            // This would happen if there was no active call when sending createCall, but there is one
            // a millisecond later when asynchronous response is received. Possible if two calls are
            // started at the same time, or one is started an another received at the same time.
            discardCall(call.id_, transceiver);
        }
    } else if (call.state_->get_id() == td::td_api::callStateReady::ID) {
        if (! activateCall(call, buddyName, account, transceiver)) {
            discardCall(call.id_, transceiver);
            account.removeActiveCall();
        }
    }
    else if ( ((call.state_->get_id() == td::td_api::callStateHangingUp::ID) ||
               (call.state_->get_id() == td::td_api::callStateDiscarded::ID) ||
               (call.state_->get_id() == td::td_api::callStateError::ID)) &&
              account.hasActiveCall() && account.getActiveCallId() == call.id_)
    {
        if (call.state_->get_id() == td::td_api::callStateError::ID) {
            const td::td_api::callStateError &error = static_cast<const td::td_api::callStateError &>(*call.state_);
            notifyCallError(error, buddyName, account);
        }
        deactivateCall(account);
        account.removeActiveCall();
    }
}

void discardCurrentCall(TdAccountData &account, TdTransceiver &transceiver)
{
    if (account.hasActiveCall()) {
        deactivateCall(account);
        discardCall(account.getActiveCallId(), transceiver);
    }
}

void showCallMessage(const td::td_api::chat &chat, const TgMessageInfo &message,
                     const td::td_api::messageCall &callEnded, TdAccountData &account)
{
    std::string notification;
    if (callEnded.discard_reason_)
        switch (callEnded.discard_reason_->get_id()) {
            case td::td_api::callDiscardReasonMissed::ID:
                notification = _("call missed");
                break;
            case td::td_api::callDiscardReasonDeclined::ID:
                notification = _("declined by peer");
                break;
            case td::td_api::callDiscardReasonDisconnected::ID:
                notification = _("users disconnected");
                break;
            case td::td_api::callDiscardReasonHungUp::ID:
                notification = _("hung up");
                break;
        }
    if (notification.empty())
        notification = _("reason unknown");

    notification = formatMessage(_("Call ended ({} seconds): {}"), {std::to_string(callEnded.duration_), notification});
    showMessageText(account, chat, message, NULL, notification.c_str());
}
