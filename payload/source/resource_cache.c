#include "resource_cache.h"
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define DEVICE_LIST_FILE "/data/resources/DeviceList.json"

static cJSON *g_device_list_cache = NULL;
static time_t g_device_list_mtime = 0;

cJSON *get_device_list_cached(void) {
    struct stat st;
    if (stat(DEVICE_LIST_FILE, &st) != 0) {
        if (g_device_list_cache) {
            cJSON_Delete(g_device_list_cache);
            g_device_list_cache = NULL;
        }
        g_device_list_mtime = 0;
        return NULL;
    }

    if (g_device_list_cache && st.st_mtime == g_device_list_mtime) {
        return g_device_list_cache;
    }

    if (g_device_list_cache) {
        cJSON_Delete(g_device_list_cache);
        g_device_list_cache = NULL;
    }

    FILE *f = fopen(DEVICE_LIST_FILE, "r");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0 || sz > 4000000) {
        fclose(f);
        return NULL;
    }

    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';

    g_device_list_cache = cJSON_Parse(buf);
    free(buf);

    if (g_device_list_cache) {
        g_device_list_mtime = st.st_mtime;
    } else {
        g_device_list_mtime = 0;
    }

    return g_device_list_cache;
}

void invalidate_device_list_cache(void) {
    if (g_device_list_cache) {
        cJSON_Delete(g_device_list_cache);
        g_device_list_cache = NULL;
    }
    g_device_list_mtime = 0;
}
