#ifndef _TDLIB_PURPLE_H
#define _TDLIB_PURPLE_H

#include "transceiver.h"
#include <purple.h>

extern "C" {
    gboolean purple_init_plugin(PurplePlugin *plugin);
};
void tgprpl_set_test_backend(ITransceiverBackend *backend);

#endif
