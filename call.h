#ifndef _CALL_H
#define _CALL_H

#include "transceiver.h"
#include "account-data.h"

bool initiateCall(int32_t userId, TdAccountData &account, TdTransceiver &transceiver);
void updateCall(const td::td_api::call &call, TdAccountData &account, TdTransceiver &transceiver);
void discardCurrentCall(TdAccountData &account, TdTransceiver &transceiver);

#endif
