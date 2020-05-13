#ifndef _TG_UTILS_H
#define _TG_UTILS_H

#include "transceiver.h"

void transmitMessage(int64_t chatId, const char *message, TdTransceiver &transceiver);

#endif
