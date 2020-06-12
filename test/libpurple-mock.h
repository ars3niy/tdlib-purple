#ifndef _LIBPURPLE_MOCK_H
#define _LIBPURPLE_MOCK_H

#include <glib.h>
#include <vector>
#include <string>
#include <purple.h>

extern "C" {

void setFakeFileSize(const char *path, size_t size);
void clearFakeFiles();
int  getLastImgstoreId();
guint8 *arrayDup(gpointer data, size_t size);

};

using RoomlistData = std::vector<std::pair<PurpleRoomlistFieldType, std::string>>;

#endif

