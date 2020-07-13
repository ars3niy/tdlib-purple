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

#if !GLIB_CHECK_VERSION(2, 34, 0)
static void g_list_free_full (GList *list, GDestroyNotify free_func)
{
    if (free_func)
        for (GList *item = list; item; item = item->next)
            free_func(item->data);
    g_list_free(list);
}
#endif

#endif
