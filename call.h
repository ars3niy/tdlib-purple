#ifndef _CALL_H
#define _CALL_H

#include "account-data.h"

bool initiateCall(int32_t userId, TdAccountData &account, TdTransceiver &transceiver);
void updateCall(const td::td_api::call &call, TdAccountData &account, TdTransceiver &transceiver);
void discardCurrentCall(TdAccountData &account, TdTransceiver &transceiver);
void showCallMessage(const td::td_api::chat &chat, const TgMessageInfo &message,
                     const td::td_api::messageCall &callEnded, TdAccountData &account);

#endif
