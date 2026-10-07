#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <time.h>
#include "cJSON.h"
#include "codex_webui_types.h"
#include "resource_cache.h"
#include "webui_utils.h"
#include "webui_ir.h"
#include "webui_config.h"
#include "webui_html.h"

void load_mqtt(struct mqtt_config *cfg) {
    char raw[8192];
    memset(cfg, 0, sizeof(*cfg));
    cfg->enabled = 0;
    cfg->ha_discovery = 1;
    cfg->port = 1883;
    cfg->poll_seconds = 10;
    cfg->keep_alive = 60;
    strcpy(cfg->base_topic, "harmony/hub");
    strcpy(cfg->discovery_prefix, "homeassistant");
    strcpy(cfg->client_id, "harmony-local-mqtt");
    strcpy(cfg->name, "Harmony Hub");
    if (read_text(MQTT_CONFIG, raw, sizeof(raw)) <= 0) return;

    cJSON *root = cJSON_Parse(raw);
    if (!root) return;

    cJSON *item;
    if ((item = cJSON_GetObjectItemCaseSensitive(root, "enabled")) && cJSON_IsBool(item))
        cfg->enabled = cJSON_IsTrue(item) ? 1 : 0;
    if ((item = cJSON_GetObjectItemCaseSensitive(root, "haDiscovery")) && cJSON_IsBool(item))
        cfg->ha_discovery = cJSON_IsTrue(item) ? 1 : 0;
    if ((item = cJSON_GetObjectItemCaseSensitive(root, "btRemoteEvents")) && cJSON_IsBool(item))
        cfg->bt_remote_events = cJSON_IsTrue(item) ? 1 : 0;
    if ((item = cJSON_GetObjectItemCaseSensitive(root, "port")) && cJSON_IsNumber(item))
        cfg->port = item->valueint;
    if ((item = cJSON_GetObjectItemCaseSensitive(root, "pollSeconds")) && cJSON_IsNumber(item))
        cfg->poll_seconds = item->valueint;
    if ((item = cJSON_GetObjectItemCaseSensitive(root, "keepAlive")) && cJSON_IsNumber(item))
        cfg->keep_alive = item->valueint;

    cJSON *broker = cJSON_GetObjectItemCaseSensitive(root, "broker");

    item = cJSON_GetObjectItemCaseSensitive(root, "host");
    if (!item && broker) item = cJSON_GetObjectItemCaseSensitive(broker, "host");
    if (item && cJSON_IsString(item) && item->valuestring)
        strncpy(cfg->host, item->valuestring, sizeof(cfg->host) - 1);

    item = cJSON_GetObjectItemCaseSensitive(root, "username");
    if (!item && broker) item = cJSON_GetObjectItemCaseSensitive(broker, "username");
    if (item && cJSON_IsString(item) && item->valuestring)
        strncpy(cfg->username, item->valuestring, sizeof(cfg->username) - 1);

    item = cJSON_GetObjectItemCaseSensitive(root, "password");
    if (!item && broker) item = cJSON_GetObjectItemCaseSensitive(broker, "password");
    if (item && cJSON_IsString(item) && item->valuestring)
        strncpy(cfg->password, item->valuestring, sizeof(cfg->password) - 1);

    if (broker) {
        cJSON *bport = cJSON_GetObjectItemCaseSensitive(broker, "port");
        if (bport && cJSON_IsNumber(bport)) cfg->port = bport->valueint;
    }

    if ((item = cJSON_GetObjectItemCaseSensitive(root, "baseTopic")) && cJSON_IsString(item) && item->valuestring)
        strncpy(cfg->base_topic, item->valuestring, sizeof(cfg->base_topic) - 1);
    if ((item = cJSON_GetObjectItemCaseSensitive(root, "discoveryPrefix")) && cJSON_IsString(item) && item->valuestring)
        strncpy(cfg->discovery_prefix, item->valuestring, sizeof(cfg->discovery_prefix) - 1);
    if ((item = cJSON_GetObjectItemCaseSensitive(root, "clientId")) && cJSON_IsString(item) && item->valuestring)
        strncpy(cfg->client_id, item->valuestring, sizeof(cfg->client_id) - 1);
    if ((item = cJSON_GetObjectItemCaseSensitive(root, "name")) && cJSON_IsString(item) && item->valuestring)
        strncpy(cfg->name, item->valuestring, sizeof(cfg->name) - 1);

    cJSON_Delete(root);
}

/* json_write_string removed - replaced by cJSON */

int save_mqtt(const struct mqtt_config *cfg) {
    cJSON *root = NULL;
    char raw[8192];
    if (read_text(MQTT_CONFIG, raw, sizeof(raw)) > 0) {
        root = cJSON_Parse(raw);
    }
    if (!root) root = cJSON_CreateObject();
    if (!root) return -1;

    cJSON_ReplaceItemInObject(root, "baseTopic", cJSON_CreateString(cfg->base_topic));
    cJSON_ReplaceItemInObject(root, "clientId", cJSON_CreateString(cfg->client_id));
    cJSON_ReplaceItemInObject(root, "discoveryPrefix", cJSON_CreateString(cfg->discovery_prefix));
    cJSON_ReplaceItemInObject(root, "name", cJSON_CreateString(cfg->name));
    cJSON_ReplaceItemInObject(root, "enabled", cJSON_CreateBool(cfg->enabled));
    cJSON_ReplaceItemInObject(root, "haDiscovery", cJSON_CreateBool(cfg->ha_discovery));
    cJSON_ReplaceItemInObject(root, "btRemoteEvents", cJSON_CreateBool(cfg->bt_remote_events));
    cJSON_ReplaceItemInObject(root, "keepAlive", cJSON_CreateNumber(cfg->keep_alive));
    cJSON_ReplaceItemInObject(root, "pollSeconds", cJSON_CreateNumber(cfg->poll_seconds));

    cJSON *broker = cJSON_GetObjectItemCaseSensitive(root, "broker");
    if (!broker) {
        broker = cJSON_CreateObject();
        cJSON_AddItemToObject(root, "broker", broker);
    }
    cJSON_ReplaceItemInObject(broker, "host", cJSON_CreateString(cfg->host));
    cJSON_ReplaceItemInObject(broker, "port", cJSON_CreateNumber(cfg->port));
    cJSON_ReplaceItemInObject(broker, "username", cJSON_CreateString(cfg->username));
    cJSON_ReplaceItemInObject(broker, "password", cJSON_CreateString(cfg->password));

    char *out = cJSON_Print(root);
    cJSON_Delete(root);
    if (!out) return -1;

    FILE *f = fopen(MQTT_CONFIG ".new", "w");
    if (!f) { free(out); return -1; }
    fputs(out, f);
    fputc('\n', f);
    fclose(f);
    free(out);

    chmod(MQTT_CONFIG ".new", 0600);
    if (rename(MQTT_CONFIG ".new", MQTT_CONFIG) != 0) return -1;
    chmod(MQTT_CONFIG, 0600);
    sync();
    return 0;
}

void parse_wpa_quoted(const char *raw, const char *key, char *out, size_t outlen) {
    char needle[32];
    const char *p;
    char *w = out;
    snprintf(needle, sizeof(needle), "%s=", key);
    p = strstr(raw, needle);
    out[0] = 0;
    if (!p) return;
    p += strlen(needle);
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '"') return;
    p++;
    while (*p && *p != '"' && (size_t)(w - out) + 1 < outlen) {
        if (*p == '\\' && p[1]) p++;
        *w++ = *p++;
    }
    *w = 0;
}

void load_wifi(struct wifi_config *cfg) {
    char raw[4096];
    memset(cfg, 0, sizeof(*cfg));
    if (read_text(WPA_CONFIG, raw, sizeof(raw)) <= 0) return;
    parse_wpa_quoted(raw, "ssid", cfg->ssid, sizeof(cfg->ssid));
    parse_wpa_quoted(raw, "psk", cfg->psk, sizeof(cfg->psk));
    cfg->hidden = strstr(raw, "scan_ssid=1") != NULL;
    cfg->open = strstr(raw, "key_mgmt=NONE") != NULL;
}

void wpa_write_quoted(FILE *f, const char *s) {
    fputc('"', f);
    while (*s) {
        if (*s == '"' || *s == '\\') fputc('\\', f);
        fputc(*s, f);
        s++;
    }
    fputc('"', f);
}

int save_wifi(const struct wifi_config *cfg) {
    FILE *f = fopen(WPA_CONFIG ".new", "w");
    if (!f) return -1;
    fprintf(f, "ctrl_interface=/var/run/wpa_supplicant\n");
    fprintf(f, "ap_scan=1\n\n");
    fprintf(f, "network={\n\tssid=");
    wpa_write_quoted(f, cfg->ssid);
    fprintf(f, "\n");
    if (cfg->hidden) fprintf(f, "\tscan_ssid=1\n");
    if (cfg->open) {
        fprintf(f, "\tkey_mgmt=NONE\n");
    } else {
        fprintf(f, "\tkey_mgmt=WPA-PSK\n\tpsk=");
        wpa_write_quoted(f, cfg->psk);
        fprintf(f, "\n");
    }
    fprintf(f, "}\n");
    fclose(f);
    chmod(WPA_CONFIG ".new", 0600);
    if (rename(WPA_CONFIG ".new", WPA_CONFIG) != 0) return -1;
    chmod(WPA_CONFIG, 0600);
    sync();
    return 0;
}

void load_ethernet(struct ethernet_config *cfg) {
    char raw[4096];
    memset(cfg, 0, sizeof(*cfg));
    cfg->enabled = 0;
    cfg->fallback_wifi = 1;
    cfg->is_static = 0;
    cfg->usb_serial_console = 1;
    if (read_text(ETHERNET_CONFIG, raw, sizeof(raw)) <= 0) return;

    char *saveptr = NULL;
    char *line = strtok_r(raw, "\r\n", &saveptr);
    while (line) {
        while (*line == ' ' || *line == '\t') line++;
        if (*line != '#' && *line != '\0') {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = line;
                char *v = eq + 1;
                while (*v == ' ' || *v == '\t' || *v == '"') v++;
                char *end = v + strlen(v);
                while (end > v && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '"' || end[-1] == '\r' || end[-1] == '\n')) {
                    *(--end) = '\0';
                }
                if (strcmp(k, "ETH_ENABLED") == 0) {
                    cfg->enabled = (atoi(v) == 1);
                } else if (strcmp(k, "ETH_FALLBACK_WIFI") == 0) {
                    cfg->fallback_wifi = (atoi(v) == 1);
                } else if (strcmp(k, "ETH_MODE") == 0) {
                    cfg->is_static = (strcmp(v, "static") == 0);
                } else if (strcmp(k, "ETH_USB_SERIAL") == 0 || strcmp(k, "USB_SERIAL_CONSOLE") == 0) {
                    cfg->usb_serial_console = 1;
                } else if (strcmp(k, "ETH_IP") == 0) {
                    strncpy(cfg->ip, v, sizeof(cfg->ip) - 1);
                } else if (strcmp(k, "ETH_NETMASK") == 0) {
                    strncpy(cfg->netmask, v, sizeof(cfg->netmask) - 1);
                } else if (strcmp(k, "ETH_GATEWAY") == 0) {
                    strncpy(cfg->gateway, v, sizeof(cfg->gateway) - 1);
                } else if (strcmp(k, "ETH_DNS") == 0) {
                    strncpy(cfg->dns, v, sizeof(cfg->dns) - 1);
                }
            }
        }
        line = strtok_r(NULL, "\r\n", &saveptr);
    }
}

int save_ethernet(const struct ethernet_config *cfg) {
    FILE *f = fopen(ETHERNET_CONFIG ".new", "w");
    if (!f) return -1;
    fprintf(f, "# Ethernet and USB Host configuration\n");
    fprintf(f, "ETH_ENABLED=%d\n", cfg->enabled ? 1 : 0);
    fprintf(f, "ETH_FALLBACK_WIFI=%d\n", cfg->fallback_wifi ? 1 : 0);
    fprintf(f, "ETH_USB_SERIAL=1\n");
    fprintf(f, "ETH_MODE=%s\n", cfg->is_static ? "static" : "dhcp");
    fprintf(f, "ETH_IP=\"%s\"\n", cfg->ip);
    fprintf(f, "ETH_NETMASK=\"%s\"\n", cfg->netmask);
    fprintf(f, "ETH_GATEWAY=\"%s\"\n", cfg->gateway);
    fprintf(f, "ETH_DNS=\"%s\"\n", cfg->dns);
    fclose(f);
    chmod(ETHERNET_CONFIG ".new", 0644);
    if (rename(ETHERNET_CONFIG ".new", ETHERNET_CONFIG) != 0) return -1;
    chmod(ETHERNET_CONFIG, 0644);
    sync();
    return 0;
}

void get_network_status(struct network_status *st) {
    memset(st, 0, sizeof(*st));
    strcpy(st->active_interface, "none");
    strcpy(st->connection_type, "Disconnected");

    /* Check modules to determine USB mode */
    char mods[4096];
    if (read_text("/proc/modules", mods, sizeof(mods)) > 0) {
        if (strstr(mods, "ehci_hcd") != NULL) {
            st->usb_host_mode = 1;
        } else if (strstr(mods, "ath_udc") != NULL) {
            st->usb_host_mode = 0;
            if (strstr(mods, "g_serial") != NULL) {
                st->usb_serial_active = 1;
            }
        }
    }

    /* Check if PC is connected in gadget mode */
    if (!st->usb_host_mode) {
        char sbuf[64];
        if ((read_text("/sys/devices/platform/ath_udc.0/state", sbuf, sizeof(sbuf)) > 0 ||
             read_text("/sys/devices/platform/ath_udc/state", sbuf, sizeof(sbuf)) > 0) &&
            (strstr(sbuf, "configured") != NULL || strstr(sbuf, "addressed") != NULL)) {
            st->usb_pc_connected = 1;
        }
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    /* Check Ethernet interfaces ONLY when USB host mode is active.
     * Ignore internal SoC eth0 (ag71xx platform device without physical port). */
    if (st->usb_host_mode) {
        DIR *dir = opendir("/sys/class/net");
        if (dir) {
            struct dirent *de;
            while ((de = readdir(dir)) != NULL) {
                if (de->d_name[0] == '.') continue;
                if (strcmp(de->d_name, "lo") == 0 ||
                    strncmp(de->d_name, "ath", 3) == 0 ||
                    strncmp(de->d_name, "wifi", 4) == 0) continue;

                /* Must be backed by a USB device */
                char devpath[256], target[512];
                snprintf(devpath, sizeof(devpath), "/sys/class/net/%s/device", de->d_name);
                ssize_t len = readlink(devpath, target, sizeof(target) - 1);
                if (len <= 0) continue;
                target[len] = '\0';
                if (strstr(target, "usb") == NULL && strstr(target, "ehci") == NULL) {
                    continue; /* Internal platform device, not USB ethernet */
                }

                st->eth_present = 1;
                strncpy(st->eth_ifname, de->d_name, sizeof(st->eth_ifname) - 1);

                st->eth_carrier = 0;
                char cpath[256], cbuf[16];
                snprintf(cpath, sizeof(cpath), "/sys/class/net/%s/carrier", de->d_name);
                if (read_text(cpath, cbuf, sizeof(cbuf)) > 0 && cbuf[0] == '1') {
                    st->eth_carrier = 1;
                }

                if (sock >= 0) {
                    struct ifreq ifr;
                    memset(&ifr, 0, sizeof(ifr));
                    strncpy(ifr.ifr_name, de->d_name, IFNAMSIZ - 1);
                    if (ioctl(sock, SIOCGIFADDR, &ifr) == 0) {
                        struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_addr;
                        strncpy(st->eth_ip, inet_ntoa(sin->sin_addr), sizeof(st->eth_ip) - 1);
                    }
                }
                break;
            }
            closedir(dir);
        }
    }

    /* Check Wi-Fi interface (ath0) */
    if (access("/sys/class/net/ath0", F_OK) == 0) {
        struct wifi_config wcfg;
        load_wifi(&wcfg);
        strncpy(st->wifi_ssid, wcfg.ssid, sizeof(st->wifi_ssid) - 1);
        if (sock >= 0) {
            struct ifreq ifr;
            memset(&ifr, 0, sizeof(ifr));
            strncpy(ifr.ifr_name, "ath0", IFNAMSIZ - 1);
            if (ioctl(sock, SIOCGIFADDR, &ifr) == 0) {
                struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_addr;
                strncpy(st->wifi_ip, inet_ntoa(sin->sin_addr), sizeof(st->wifi_ip) - 1);
                st->wifi_connected = (st->wifi_ip[0] != '\0');
            }
        }
    }

    /* Read default route from /proc/net/route */
    FILE *rf = fopen("/proc/net/route", "r");
    if (rf) {
        char line[256];
        if (fgets(line, sizeof(line), rf)) {
            while (fgets(line, sizeof(line), rf)) {
                char iface[32];
                unsigned long dest = 0, gw = 0;
                if (sscanf(line, "%31s %lx %lx", iface, &dest, &gw) >= 3) {
                    if (dest == 0) {
                        strncpy(st->active_interface, iface, sizeof(st->active_interface) - 1);
                        struct in_addr gw_addr;
                        gw_addr.s_addr = (in_addr_t)gw;
                        strncpy(st->gateway, inet_ntoa(gw_addr), sizeof(st->gateway) - 1);
                        break;
                    }
                }
            }
        }
        fclose(rf);
    }

    if (strcmp(st->active_interface, "none") == 0) {
        if (st->eth_present && st->eth_ip[0]) {
            strncpy(st->active_interface, st->eth_ifname, sizeof(st->active_interface) - 1);
        } else if (st->wifi_ip[0]) {
            strcpy(st->active_interface, "ath0");
        }
    }

    if (sock >= 0 && strcmp(st->active_interface, "none") != 0) {
        struct ifreq ifr;
        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, st->active_interface, IFNAMSIZ - 1);
        if (ioctl(sock, SIOCGIFADDR, &ifr) == 0) {
            struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_addr;
            strncpy(st->ip, inet_ntoa(sin->sin_addr), sizeof(st->ip) - 1);
        }
        if (ioctl(sock, SIOCGIFNETMASK, &ifr) == 0) {
            struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_addr;
            strncpy(st->netmask, inet_ntoa(sin->sin_addr), sizeof(st->netmask) - 1);
        }
        if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
            unsigned char *mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;
            snprintf(st->mac, sizeof(st->mac), "%02x:%02x:%02x:%02x:%02x:%02x",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        }
    }
    if (sock >= 0) close(sock);

    if (st->eth_present && (strncmp(st->active_interface, "eth", 3) == 0 || strncmp(st->active_interface, "usb", 3) == 0)) {
        strcpy(st->connection_type, "Ethernet");
    } else if (strcmp(st->active_interface, "ath0") == 0) {
        char state[128];
        if (read_text("/tmp/codex_active_net_state", state, sizeof(state)) > 0 && strstr(state, "wifi-fallback")) {
            strcpy(st->connection_type, "Wi-Fi (Fallback)");
        } else {
            strcpy(st->connection_type, "Wi-Fi");
        }
    }
}

int load_hub_id(char *hub_id, size_t hub_id_len) {
    size_t i, n;
    if (!hub_id || hub_id_len == 0) return 0;
    hub_id[0] = '\0';
    read_text(HUB_ID_FILE, hub_id, hub_id_len);
    chomp(hub_id);
    n = strlen(hub_id);
    if (n < 4) return 0;
    for (i = 0; i < n; i++) {
        if (!isdigit((unsigned char)hub_id[i])) return 0;
    }
    return 1;
}

void trigger_mqtt_discover(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return;
    struct sockaddr_in addr;
    struct timeval tv = { .tv_sec = 0, .tv_usec = 100000 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8089);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
        const char *req = "POST /api/sync HTTP/1.1\r\nHost: 127.0.0.1:8089\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        send(fd, req, strlen(req), MSG_NOSIGNAL);
        char dummy[64];
        recv(fd, dummy, sizeof(dummy) - 1, 0);
    }
    close(fd);
}

void request_resource_reload(void) {
    invalidate_device_list_cache();
    mark_protocol_repair_needed();
    FILE *f = fopen(RESOURCE_RELOAD_FLAG, "w");
    if (f) {
        fputs("DeviceList\nFunctionList\nProtocolList\nActivityList\n", f);
        fclose(f);
    }
    sync();
    trigger_mqtt_discover();
}

static void backup_copy_list(const char *dir, const char *const *files, int count) {
    int i;
    char dst[512];
    mkdir(RESOURCE_BACKUP_DIR, 0755);
    mkdir(dir, 0755);
    for (i = 0; i < count; i++) {
        const char *base = strrchr(files[i], '/');
        base = base ? base + 1 : files[i];
        snprintf(dst, sizeof(dst), "%s/%s", dir, base);
        copy_file_raw(files[i], dst);
    }
}

void backup_resources(void) {
    char dir[256];
    time_t now = time(NULL);
    struct tm tm;
    static const char *const files[] = { DEVICE_LIST, FUNCTION_LIST, PROTOCOL_LIST, ACTIVITY_LIST };
    localtime_r(&now, &tm);
    strftime(dir, sizeof(dir), RESOURCE_BACKUP_DIR "/%Y%m%d_%H%M%S", &tm);
    backup_copy_list(dir, files, 4);
    prune_resource_backups(3);
}

void backup_settings(void) {
    char dir[256];
    time_t now = time(NULL);
    struct tm tm;
    static const char *const files[] = {
        MQTT_CONFIG, WPA_CONFIG, BT_DEVICE_STORE, BT_REMOTE_MAP_FILE,
        HUB_ID_FILE, WEBUI_AUTH_CONFIG, DEBUG_LOG_CONFIG, BT_TARGET_FILE,
        BT_REMOTE_TARGET, BT_KEYS_TLV
    };
    localtime_r(&now, &tm);
    strftime(dir, sizeof(dir), RESOURCE_BACKUP_DIR "/settings_%Y%m%d_%H%M%S", &tm);
    backup_copy_list(dir, files, 10);
    prune_resource_backups(3);
}

int tcp_established(const char *host, int port) {
    FILE *f;
    char line[256];
    struct in_addr addr;
    unsigned long want_addr;
    if (!host[0] || inet_aton(host, &addr) == 0) return 0;
    want_addr = ntohl(addr.s_addr);
    f = fopen("/proc/net/tcp", "r");
    if (!f) return 0;
    while (fgets(line, sizeof(line), f)) {
        unsigned long rem_addr, rem_port, state;
        if (sscanf(line, " %*d: %*X:%*X %lX:%lX %lX", &rem_addr, &rem_port, &state) == 3) {
            if (rem_addr == want_addr && rem_port == (unsigned long)port && state == 1) {
                fclose(f);
                return 1;
            }
        }
    }
    fclose(f);
    return 0;
}

static const char *bundle_get_string(cJSON *root, const char *key) {
    if (!root || !key) return NULL;
    cJSON *item = NULL;
    cJSON *files = cJSON_GetObjectItemCaseSensitive(root, "files");
    if (!files) files = cJSON_GetObjectItem(root, "files");
    if (files && cJSON_IsObject(files)) {
        item = cJSON_GetObjectItemCaseSensitive(files, key);
        if (!item) item = cJSON_GetObjectItem(files, key);
    }
    if (!item) {
        item = cJSON_GetObjectItemCaseSensitive(root, key);
        if (!item) item = cJSON_GetObjectItem(root, key);
    }
    if (item && cJSON_IsString(item) && item->valuestring) {
        return item->valuestring;
    }
    return NULL;
}

static const char *BUNDLE_CHECKSUM_KEYS[] = {
    "DeviceList.json", "FunctionList.json", "ProtocolList.json",
    "ActivityList.json", "mqtt-config.json", "wpa_supplicant.conf",
    "ethernet.conf", "bt-devices.json", "bt_remote_map.json", "hub_id",
    "webui_auth.conf", "debug_logging.conf", "bthid_target",
    "bt_remote_target", "btstack_keys.b64"
};

static uint32_t crc32_feed(uint32_t crc, const unsigned char *buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        crc ^= buf[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return crc;
}

static uint32_t compute_bundle_crc32(cJSON *root) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < sizeof(BUNDLE_CHECKSUM_KEYS) / sizeof(BUNDLE_CHECKSUM_KEYS[0]); i++) {
        const char *k = BUNDLE_CHECKSUM_KEYS[i];
        const char *v = bundle_get_string(root, k);
        if (v && v[0]) {
            crc = crc32_feed(crc, (const unsigned char *)k, strlen(k));
            crc = crc32_feed(crc, (const unsigned char *)v, strlen(v));
        }
    }
    return ~crc;
}

void send_bundle_download(int fd) {
    cJSON *root = cJSON_CreateObject();
    if (!root) return;
    cJSON_AddStringToObject(root, "format", "harmony-owner-bundle-v1");
    cJSON *files = cJSON_CreateObject();

    const char *paths[][2] = {
        {"DeviceList.json", DEVICE_LIST},
        {"FunctionList.json", FUNCTION_LIST},
        {"ProtocolList.json", PROTOCOL_LIST},
        {"ActivityList.json", ACTIVITY_LIST},
        {"mqtt-config.json", MQTT_CONFIG},
        {"wpa_supplicant.conf", WPA_CONFIG},
        {"ethernet.conf", ETHERNET_CONFIG},
        {"bt-devices.json", BT_DEVICE_STORE},
        {"bt_remote_map.json", BT_REMOTE_MAP_FILE},
        {"hub_id", HUB_ID_FILE},
        {"webui_auth.conf", WEBUI_AUTH_CONFIG},
        {"debug_logging.conf", DEBUG_LOG_CONFIG},
        {"bthid_target", BT_TARGET_FILE},
        {"bt_remote_target", BT_REMOTE_TARGET}
    };
    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        char *data = read_file_alloc(paths[i][1], MAX_RESOURCE_FILE, NULL);
        cJSON_AddStringToObject(files, paths[i][0], data ? data : "");
        free(data);
    }

    size_t klen = 0;
    char *kdata = read_file_alloc(BT_KEYS_TLV, 65536, &klen);
    if (kdata && klen > 0) {
        char *b64 = base64_encode((const unsigned char *)kdata, klen);
        if (b64) {
            cJSON_AddStringToObject(files, "btstack_keys.b64", b64);
            free(b64);
        }
    }
    free(kdata);

    cJSON_AddItemToObject(root, "files", files);
    uint32_t crc = compute_bundle_crc32(root);
    char crc_str[16];
    snprintf(crc_str, sizeof(crc_str), "%08X", crc);
    cJSON_AddStringToObject(root, "crc32", crc_str);

    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!out) return;

    char hdr[512];
    size_t len = strlen(out);
    snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
        "Content-Length: %lu\r\n"
        "Cache-Control: no-store\r\n"
        "Content-Disposition: attachment; filename=\"harmony-owner-bundle.json\"\r\n"
        "Connection: close\r\n\r\n", (unsigned long)len);
    send_all(fd, hdr, strlen(hdr));
    send_all(fd, out, len);
    free(out);
}




void handle_mqtt(int fd, const struct request *req) {
    struct mqtt_config old, cfg;
    char tmp[256];
    load_mqtt(&old);
    cfg = old;
    cfg.enabled = form_checked(req->body, "enabled");
    cfg.ha_discovery = form_checked(req->body, "haDiscovery");
    cfg.bt_remote_events = form_checked(req->body, "btRemoteEvents");
    form_value(req->body, "host", cfg.host, sizeof(cfg.host));
    form_value(req->body, "username", cfg.username, sizeof(cfg.username));
    form_value(req->body, "baseTopic", cfg.base_topic, sizeof(cfg.base_topic));
    form_value(req->body, "discoveryPrefix", cfg.discovery_prefix, sizeof(cfg.discovery_prefix));
    form_value(req->body, "clientId", cfg.client_id, sizeof(cfg.client_id));
    form_value(req->body, "name", cfg.name, sizeof(cfg.name));
    form_value(req->body, "port", tmp, sizeof(tmp));
    cfg.port = atoi(tmp);
    if (cfg.port <= 0) cfg.port = 1883;
    form_value(req->body, "pollSeconds", tmp, sizeof(tmp));
    cfg.poll_seconds = atoi(tmp);
    if (cfg.poll_seconds <= 0) cfg.poll_seconds = 10;
    form_value(req->body, "keepAlive", tmp, sizeof(tmp));
    cfg.keep_alive = atoi(tmp);
    if (cfg.keep_alive <= 0) cfg.keep_alive = 60;
    form_value(req->body, "password", tmp, sizeof(tmp));
    if (tmp[0] || !form_checked(req->body, "keep_password")) {
        strncpy(cfg.password, tmp, sizeof(cfg.password) - 1);
        cfg.password[sizeof(cfg.password) - 1] = 0;
    }
    if (save_mqtt(&cfg) == 0) {
        if (cfg.bt_remote_events) {
            FILE *bf = fopen("/data/codex/bt_remote_mqtt", "w");
            if (bf) fclose(bf);
        } else {
            unlink("/data/codex/bt_remote_mqtt");
        }
        trigger_mqtt_discover();
        render_page(fd, req, "MQTT settings saved. The bridge will reconnect when it notices the config change.");
    } else {
        render_page(fd, req, "Failed to save MQTT settings.");
    }
}

void handle_wifi(int fd, const struct request *req) {
    struct wifi_config old, cfg;
    char tmp[256], apply[32];
    load_wifi(&old);
    cfg = old;
    form_value(req->body, "ssid", cfg.ssid, sizeof(cfg.ssid));
    form_value(req->body, "password", tmp, sizeof(tmp));
    cfg.hidden = form_checked(req->body, "hidden");
    cfg.open = form_checked(req->body, "open");
    if (tmp[0] || !form_checked(req->body, "keep_password")) {
        strncpy(cfg.psk, tmp, sizeof(cfg.psk) - 1);
        cfg.psk[sizeof(cfg.psk) - 1] = 0;
    }
    if (!cfg.ssid[0]) {
        render_page(fd, req, "Wi-Fi SSID is required.");
        return;
    }
    if (!cfg.open && !cfg.psk[0]) {
        render_page(fd, req, "Wi-Fi password is required unless Open network is checked.");
        return;
    }
    if (save_wifi(&cfg) != 0) {
        render_page(fd, req, "Failed to save Wi-Fi settings.");
        return;
    }
    form_value(req->body, "apply", apply, sizeof(apply));
    if (strcmp(apply, "reboot") == 0) {
        render_page(fd, req, "Wi-Fi settings saved. Rebooting now.");
        sync();
        system("/sbin/reboot >/dev/null 2>&1 &");
    } else {
        render_page(fd, req, "Wi-Fi settings saved. Reboot when ready to use them.");
    }
}

void handle_ethernet(int fd, const struct request *req) {
    struct ethernet_config old, cfg;
    char apply[32], mode[32];
    load_ethernet(&old);
    cfg = old;
    cfg.enabled = form_checked(req->body, "eth_enabled");
    cfg.fallback_wifi = form_checked(req->body, "eth_fallback_wifi");
    cfg.usb_serial_console = 1;
    form_value(req->body, "eth_mode", mode, sizeof(mode));
    cfg.is_static = (strcmp(mode, "static") == 0);
    form_value(req->body, "eth_ip", cfg.ip, sizeof(cfg.ip));
    form_value(req->body, "eth_netmask", cfg.netmask, sizeof(cfg.netmask));
    form_value(req->body, "eth_gateway", cfg.gateway, sizeof(cfg.gateway));
    form_value(req->body, "eth_dns", cfg.dns, sizeof(cfg.dns));

    if (cfg.is_static && !cfg.ip[0]) {
        render_page(fd, req, "Static IP address is required when Static mode is selected.");
        return;
    }
    if (save_ethernet(&cfg) != 0) {
        render_page(fd, req, "Failed to save Ethernet settings.");
        return;
    }

    form_value(req->body, "apply", apply, sizeof(apply));
    if (strcmp(apply, "reboot") == 0) {
        render_page(fd, req, "Ethernet settings saved. Rebooting hub now...");
        sync();
        system("/sbin/reboot >/dev/null 2>&1 &");
    } else if (strcmp(apply, "apply") == 0 || strcmp(apply, "reconfigure") == 0) {
        render_page(fd, req, "Ethernet settings saved and applied.");
        system("/data/codex/network_manager.sh apply >/dev/null 2>&1 &");
    } else {
        render_page(fd, req, "Ethernet settings saved. Reboot when ready to use them.");
    }
}

void handle_system(int fd, const struct request *req) {
    char action[64];
    form_value(req->body, "action", action, sizeof(action));
    if (strcmp(action, "reboot") == 0) {
        unlink(REBOOT_COUNTER_FILE);
        render_page(fd, req, "Rebooting now.");
        system("sync 2>/dev/null || /data/codex/bin/codex_sync 2>/dev/null || true");
        system("/sbin/reboot >/dev/null 2>&1 &");
    } else if (strcmp(action, "auth") == 0) {
        struct webui_auth_config old, cfg;
        char username[64], password[128];
        load_webui_auth(&old);
        cfg = old;
        cfg.enabled = form_checked(req->body, "authEnabled");
        form_value(req->body, "authUsername", username, sizeof(username));
        form_value(req->body, "authPassword", password, sizeof(password));
        if (username[0]) snprintf(cfg.username, sizeof(cfg.username), "%s", username);
        if (password[0]) snprintf(cfg.password, sizeof(cfg.password), "%s", password);
        if (!safe_auth_field(cfg.username, 0)) {
            render_page(fd, req, "Username is required and cannot contain a colon.");
            return;
        }
        if (cfg.enabled && !cfg.password[0]) {
            render_page(fd, req, "Enter a password before enabling web UI sign-in.");
            return;
        }
        if (cfg.password[0] && !safe_auth_field(cfg.password, 1)) {
            render_page(fd, req, "Password cannot contain control characters.");
            return;
        }
        if (save_webui_auth(&cfg) != 0) {
            render_page(fd, req, "Failed to save web UI sign-in setting.");
            return;
        }
        render_page(fd, req, cfg.enabled ? "Web UI sign-in enabled. Your browser may ask you to sign in again on the next page load." : "Web UI sign-in disabled.");
    } else if (strcmp(action, "rediscover") == 0) {
        trigger_mqtt_discover();
        render_page(fd, req, "MQTT discovery reload requested.");
    } else if (strcmp(action, "debug_logging") == 0) {
        int enabled = form_checked(req->body, "debugLogging");
        if (enabled) {
            FILE *f = fopen(DEBUG_LOG_CONFIG, "w");
            if (f) { fprintf(f, "1\n"); fclose(f); }
            FILE *f2 = fopen(BT_DEBUG_FLAG, "w");
            if (f2) fclose(f2);
            { int pfd = open("/proc/sys/kernel/printk", O_WRONLY); if (pfd >= 0) { write(pfd, "7 4 1 7\n", 8); close(pfd); } }
            render_page(fd, req, "Verbose debug logging enabled.");
        } else {
            unlink(DEBUG_LOG_CONFIG);
            unlink(BT_DEBUG_FLAG);
            { int pfd = open("/proc/sys/kernel/printk", O_WRONLY); if (pfd >= 0) { write(pfd, "3 4 1 7\n", 8); close(pfd); } }
            render_page(fd, req, "Verbose debug logging disabled.");
        }
    } else if (strcmp(action, "radio_toggles") == 0) {
        int bt_on = form_checked(req->body, "btEnabled");
        int rf_on = form_checked(req->body, "rfEnabled");
        if (!bt_on) {
            FILE *f = fopen(BT_DISABLED_CONFIG, "w");
            if (f) { fprintf(f, "1\n"); fclose(f); }
            system("killall -9 codex_btstack 2>/dev/null; hciconfig hci0 down 2>/dev/null || true");
        } else {
            unlink(BT_DISABLED_CONFIG);
            system("hciconfig hci0 up 2>/dev/null || true");
        }
        if (!rf_on) {
            FILE *f = fopen(RF_DISABLED_CONFIG, "w");
            if (f) { fprintf(f, "1\n"); fclose(f); }
            system("killall -9 codex_rf 2>/dev/null || true");
        } else {
            unlink(RF_DISABLED_CONFIG);
        }
        render_page(fd, req, "Radio and hardware settings saved.");
    } else if (strcmp(action, "enable_radio") == 0) {
        char radio[32];
        form_value(req->body, "radio", radio, sizeof(radio));
        if (strcmp(radio, "bluetooth") == 0 || strcmp(radio, "bt") == 0) {
            unlink(BT_DISABLED_CONFIG);
            system("hciconfig hci0 up 2>/dev/null || true");
            render_page(fd, req, "Bluetooth re-enabled. BTstack backend starting.");
        } else if (strcmp(radio, "rf") == 0) {
            unlink(RF_DISABLED_CONFIG);
            render_page(fd, req, "Harmony Elite RF re-enabled. Codex RF daemon starting.");
        } else {
            render_page(fd, req, "Unknown radio target.");
        }
    } else if (strcmp(action, "reset_reboot_counter") == 0) {
        unlink(REBOOT_COUNTER_FILE);
        render_page(fd, req, "Reboot panic counter reset to 0.");
    } else {
        render_page(fd, req, "Unknown system action.");
    }
}

static char *trim_payload(char *s) {
    char *end;
    while (*s && isspace((unsigned char)*s)) s++;
    end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) *--end = 0;
    return s;
}

static int looks_like_json_object(const char *s) {
    const char *end;
    while (*s && isspace((unsigned char)*s)) s++;
    if (*s != '{') return 0;
    end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) end--;
    return end > s && end[-1] == '}';
}

static const char *import_path_for_target(const char *target) {
    if (strcmp(target, "devices") == 0) return DEVICE_LIST;
    if (strcmp(target, "functions") == 0) return FUNCTION_LIST;
    if (strcmp(target, "protocols") == 0) return PROTOCOL_LIST;
    if (strcmp(target, "activities") == 0) return ACTIVITY_LIST;
    if (strcmp(target, "mqtt") == 0) return MQTT_CONFIG;
    if (strcmp(target, "wifi") == 0) return WPA_CONFIG;
    if (strcmp(target, "ethernet") == 0 || strcmp(target, "network") == 0) return ETHERNET_CONFIG;
    if (strcmp(target, "bluetooth") == 0) return BT_DEVICE_STORE;
    if (strcmp(target, "remote-mapping") == 0 || strcmp(target, "remotemap") == 0) return BT_REMOTE_MAP_FILE;
    if (strcmp(target, "auth") == 0) return WEBUI_AUTH_CONFIG;
    if (strcmp(target, "hub_id") == 0) return HUB_ID_FILE;
    return NULL;
}

static const char *import_label_for_target(const char *target) {
    if (strcmp(target, "bundle") == 0) return "backup bundle";
    if (strcmp(target, "devices") == 0) return "DeviceList.json";
    if (strcmp(target, "functions") == 0) return "FunctionList.json";
    if (strcmp(target, "protocols") == 0) return "ProtocolList.json";
    if (strcmp(target, "activities") == 0) return "ActivityList.json";
    if (strcmp(target, "mqtt") == 0) return "MQTT config";
    if (strcmp(target, "wifi") == 0) return "Wi-Fi config";
    if (strcmp(target, "ethernet") == 0 || strcmp(target, "network") == 0) return "Ethernet config";
    if (strcmp(target, "bluetooth") == 0) return "Bluetooth devices";
    if (strcmp(target, "remote-mapping") == 0 || strcmp(target, "remotemap") == 0) return "bt_remote_map.json";
    if (strcmp(target, "auth") == 0) return "WebUI auth config";
    if (strcmp(target, "hub_id") == 0) return "hub_id";
    return "import";
}

static int validate_import_payload(const char *target, const char *payload, char *msg, size_t msglen) {
    if (!payload[0]) {
        snprintf(msg, msglen, "Import payload is empty.");
        return -1;
    }
    if (strcmp(target, "bundle") == 0) {
        if (looks_like_json_object(payload) && strstr(payload, "harmony-owner-bundle-v1") && strstr(payload, "\"DeviceList.json\"")) return 0;
        snprintf(msg, msglen, "Bundle import must be a harmony-owner-bundle-v1 JSON export.");
        return -1;
    }
    if (strcmp(target, "wifi") == 0) {
        if (strstr(payload, "network={") && strstr(payload, "ssid=")) return 0;
        snprintf(msg, msglen, "Wi-Fi import must look like a wpa_supplicant config with a network block and ssid.");
        return -1;
    }
    if (strcmp(target, "ethernet") == 0 || strcmp(target, "network") == 0) {
        if (strstr(payload, "ETH_ENABLED") || strstr(payload, "ETH_MODE")) return 0;
        snprintf(msg, msglen, "Ethernet import must contain ETH_ENABLED or ETH_MODE.");
        return -1;
    }
    if (strcmp(target, "auth") == 0) {
        if (strchr(payload, ':')) return 0;
        snprintf(msg, msglen, "Auth import must be in username:hash format.");
        return -1;
    }
    if (strcmp(target, "hub_id") == 0) {
        if (strlen(payload) >= 1 && strlen(payload) < 64) return 0;
        snprintf(msg, msglen, "hub_id must be a valid hub identifier.");
        return -1;
    }
    if (strcmp(target, "bluetooth") == 0) {
        if (looks_like_json_object(payload) && strstr(payload, "\"devices\"")) return 0;
        snprintf(msg, msglen, "Bluetooth devices import must be a JSON object with a devices list.");
        return -1;
    }
    if (strcmp(target, "remote-mapping") == 0 || strcmp(target, "remotemap") == 0) {
        if (looks_like_json_object(payload) && (strstr(payload, "\"remote\"") || strstr(payload, "\"activities\""))) return 0;
        snprintf(msg, msglen, "BT remote mapping import must be a JSON object containing remote or activities.");
        return -1;
    }
    if (!looks_like_json_object(payload)) {
        snprintf(msg, msglen, "%s import must be a JSON object.", import_label_for_target(target));
        return -1;
    }
    if (strcmp(target, "devices") == 0 && !strstr(payload, "\"DevicesWithFeatures\"")) {
        snprintf(msg, msglen, "DeviceList import must contain DevicesWithFeatures.");
        return -1;
    }
    if (strcmp(target, "functions") == 0 && !strstr(payload, "\"FunctionMaps\"")) {
        snprintf(msg, msglen, "FunctionList import must contain FunctionMaps.");
        return -1;
    }
    if (strcmp(target, "protocols") == 0 && !strstr(payload, "\"Protocols\"")) {
        snprintf(msg, msglen, "ProtocolList import must contain Protocols.");
        return -1;
    }
    if (strcmp(target, "activities") == 0 && !strstr(payload, "\"Activities\"")) {
        snprintf(msg, msglen, "ActivityList import must contain Activities.");
        return -1;
    }
    if (strcmp(target, "mqtt") == 0 && (!strstr(payload, "\"broker\"") || !strstr(payload, "\"baseTopic\""))) {
        snprintf(msg, msglen, "MQTT import must contain broker and baseTopic.");
        return -1;
    }
    return 0;
}

static void handle_import_bundle(int fd, const struct request *req, const char *payload) {
    char msg[256];
    cJSON *root = cJSON_Parse(payload);
    if (!root) {
        render_page(fd, req, "Failed to parse backup bundle JSON.");
        return;
    }

    cJSON *jcrc = cJSON_GetObjectItemCaseSensitive(root, "crc32");
    if (!jcrc) jcrc = cJSON_GetObjectItem(root, "crc32");
    if (jcrc && cJSON_IsString(jcrc) && jcrc->valuestring && jcrc->valuestring[0]) {
        uint32_t actual = compute_bundle_crc32(root);
        char actual_str[16];
        snprintf(actual_str, sizeof(actual_str), "%08X", actual);
        if (strcasecmp(jcrc->valuestring, actual_str) != 0) {
            cJSON_Delete(root);
            render_page(fd, req, "Bundle integrity check failed: CRC32 checksum mismatch.");
            return;
        }
    }

    const char *devices = bundle_get_string(root, "DeviceList.json");
    const char *functions = bundle_get_string(root, "FunctionList.json");
    const char *protocols = bundle_get_string(root, "ProtocolList.json");

    if (!devices || !devices[0] || !functions || !functions[0] || !protocols || !protocols[0]) {
        cJSON_Delete(root);
        render_page(fd, req, "Bundle is missing core resource files (DeviceList, FunctionList, or ProtocolList).");
        return;
    }

    if (validate_import_payload("devices", devices, msg, sizeof(msg)) != 0 ||
        validate_import_payload("functions", functions, msg, sizeof(msg)) != 0 ||
        validate_import_payload("protocols", protocols, msg, sizeof(msg)) != 0) {
        cJSON_Delete(root);
        render_page(fd, req, msg);
        return;
    }

    const char *activities = bundle_get_string(root, "ActivityList.json");
    if (activities && activities[0] && validate_import_payload("activities", activities, msg, sizeof(msg)) != 0) {
        cJSON_Delete(root);
        render_page(fd, req, msg);
        return;
    }

    const char *mqtt = bundle_get_string(root, "mqtt-config.json");
    if (mqtt && mqtt[0] && validate_import_payload("mqtt", mqtt, msg, sizeof(msg)) != 0) {
        cJSON_Delete(root);
        render_page(fd, req, msg);
        return;
    }

    const char *wifi = bundle_get_string(root, "wpa_supplicant.conf");
    if (wifi && wifi[0] && validate_import_payload("wifi", wifi, msg, sizeof(msg)) != 0) {
        cJSON_Delete(root);
        render_page(fd, req, msg);
        return;
    }

    const char *bluetooth = bundle_get_string(root, "bt-devices.json");
    if (bluetooth && bluetooth[0] && validate_import_payload("bluetooth", bluetooth, msg, sizeof(msg)) != 0) {
        cJSON_Delete(root);
        render_page(fd, req, msg);
        return;
    }

    const char *remotemap = bundle_get_string(root, "bt_remote_map.json");
    if (remotemap && remotemap[0] && validate_import_payload("remote-mapping", remotemap, msg, sizeof(msg)) != 0) {
        cJSON_Delete(root);
        render_page(fd, req, msg);
        return;
    }

    backup_resources();
    backup_settings();

    if (write_file_atomic(DEVICE_LIST, devices, strlen(devices)) != 0 ||
        write_file_atomic(FUNCTION_LIST, functions, strlen(functions)) != 0 ||
        write_file_atomic(PROTOCOL_LIST, protocols, strlen(protocols)) != 0) {
        cJSON_Delete(root);
        render_page(fd, req, "Failed to save restored core resources.");
        return;
    }

    if (activities && activities[0]) {
        write_file_atomic(ACTIVITY_LIST, activities, strlen(activities));
        chmod(ACTIVITY_LIST, 0644);
    }
    if (mqtt && mqtt[0]) {
        write_file_atomic(MQTT_CONFIG, mqtt, strlen(mqtt));
        chmod(MQTT_CONFIG, 0600);
        trigger_mqtt_discover();
    }
    if (wifi && wifi[0]) {
        write_file_atomic(WPA_CONFIG, wifi, strlen(wifi));
        chmod(WPA_CONFIG, 0600);
    }
    const char *ethernet = bundle_get_string(root, "ethernet.conf");
    if (ethernet && ethernet[0]) {
        write_file_atomic(ETHERNET_CONFIG, ethernet, strlen(ethernet));
        chmod(ETHERNET_CONFIG, 0644);
    }
    if (bluetooth && bluetooth[0]) {
        write_file_atomic(BT_DEVICE_STORE, bluetooth, strlen(bluetooth));
        chmod(BT_DEVICE_STORE, 0644);
    }
    if (remotemap && remotemap[0]) {
        write_file_atomic(BT_REMOTE_MAP_FILE, remotemap, strlen(remotemap));
        chmod(BT_REMOTE_MAP_FILE, 0644);
    }

    const char *hub_id = bundle_get_string(root, "hub_id");
    if (hub_id && hub_id[0]) {
        write_file_atomic(HUB_ID_FILE, hub_id, strlen(hub_id));
        chmod(HUB_ID_FILE, 0644);
    }
    const char *auth = bundle_get_string(root, "webui_auth.conf");
    if (auth && auth[0]) {
        write_file_atomic(WEBUI_AUTH_CONFIG, auth, strlen(auth));
        chmod(WEBUI_AUTH_CONFIG, 0600);
    }
    const char *dbg = bundle_get_string(root, "debug_logging.conf");
    if (dbg && dbg[0]) {
        write_file_atomic(DEBUG_LOG_CONFIG, dbg, strlen(dbg));
        chmod(DEBUG_LOG_CONFIG, 0644);
    }
    const char *bthid_tgt = bundle_get_string(root, "bthid_target");
    if (bthid_tgt && bthid_tgt[0]) {
        write_file_atomic(BT_TARGET_FILE, bthid_tgt, strlen(bthid_tgt));
        chmod(BT_TARGET_FILE, 0644);
    }
    const char *bt_rem_tgt = bundle_get_string(root, "bt_remote_target");
    if (bt_rem_tgt && bt_rem_tgt[0]) {
        write_file_atomic(BT_REMOTE_TARGET, bt_rem_tgt, strlen(bt_rem_tgt));
        chmod(BT_REMOTE_TARGET, 0644);
    }
    const char *b64_keys = bundle_get_string(root, "btstack_keys.b64");
    if (b64_keys && b64_keys[0]) {
        size_t klen = 0;
        unsigned char *kbin = base64_decode(b64_keys, &klen);
        if (kbin && klen > 0) {
            write_file_atomic(BT_KEYS_TLV, (const char *)kbin, klen);
            chmod(BT_KEYS_TLV, 0600);
            free(kbin);
        }
    }

    cJSON_Delete(root);
    request_resource_reload();
    render_page(fd, req, "Complete backup bundle imported successfully. Reboot when ready if Wi-Fi settings changed.");
}

void handle_import(int fd, const struct request *req) {
    char target[64], msg[256];
    char *payload_buf, *payload;
    const char *path;
    size_t len;
    if (req->body_truncated) {
        render_page(fd, req, "Import payload was too large for this device-side form. Use a smaller file or import one resource at a time.");
        return;
    }
    form_value(req->body, "target", target, sizeof(target));
    path = import_path_for_target(target);
    if (!path && strcmp(target, "bundle") != 0) {
        render_page(fd, req, "Unknown import target.");
        return;
    }
    payload_buf = (char *)malloc(MAX_REQUEST_BODY);
    if (!payload_buf) {
        render_page(fd, req, "Not enough memory to receive import payload.");
        return;
    }
    form_value(req->body, "payload", payload_buf, MAX_REQUEST_BODY);
    payload = trim_payload(payload_buf);
    if (validate_import_payload(target, payload, msg, sizeof(msg)) != 0) {
        render_page(fd, req, msg);
        free(payload_buf);
        return;
    }
    if (strcmp(target, "bundle") == 0) {
        handle_import_bundle(fd, req, payload);
        free(payload_buf);
        return;
    }
    len = strlen(payload);
    if (strcmp(target, "devices") == 0 || strcmp(target, "functions") == 0 || strcmp(target, "protocols") == 0 || strcmp(target, "activities") == 0) {
        backup_resources();
    } else {
        backup_settings();
    }
    if (write_file_atomic(path, payload, len) != 0) {
        snprintf(msg, sizeof(msg), "Failed to import %s.", import_label_for_target(target));
        render_page(fd, req, msg);
        free(payload_buf);
        return;
    }
    if (strcmp(target, "activities") == 0) {
        request_resource_reload();
        render_page(fd, req, "Activity settings imported.");
    } else if (strcmp(target, "mqtt") == 0) {
        chmod(MQTT_CONFIG, 0600);
        trigger_mqtt_discover();
        render_page(fd, req, "MQTT settings imported. The bridge will reconnect when it notices the config change.");
    } else if (strcmp(target, "wifi") == 0) {
        chmod(WPA_CONFIG, 0600);
        render_page(fd, req, "Wi-Fi settings imported. Reboot when ready to use them.");
    } else if (strcmp(target, "bluetooth") == 0) {
        chmod(BT_DEVICE_STORE, 0644);
        render_page(fd, req, "Bluetooth devices imported.");
    } else if (strcmp(target, "remote-mapping") == 0 || strcmp(target, "remotemap") == 0) {
        chmod(BT_REMOTE_MAP_FILE, 0644);
        render_page(fd, req, "BT remote mapping imported.");
    } else if (strcmp(target, "auth") == 0) {
        chmod(WEBUI_AUTH_CONFIG, 0600);
        render_page(fd, req, "WebUI sign-in credentials imported.");
    } else if (strcmp(target, "hub_id") == 0) {
        chmod(HUB_ID_FILE, 0644);
        render_page(fd, req, "Hub ID imported.");
    } else {
        request_resource_reload();
        snprintf(msg, sizeof(msg), "Imported %s and requested a Harmony resource reload.", import_label_for_target(target));
        render_page(fd, req, msg);
    }
    free(payload_buf);
}

void render_web_remote_layout_json(int fd) {
    char *raw = read_file_alloc(WEB_REMOTE_LAYOUT_FILE, 1048576, NULL);
    cJSON *resp = cJSON_CreateObject();
    if (raw) {
        cJSON *layouts = cJSON_Parse(raw);
        free(raw);
        if (layouts) {
            cJSON_AddBoolToObject(resp, "ok", 1);
            cJSON_AddItemToObject(resp, "layouts", layouts);
        } else {
            cJSON_AddBoolToObject(resp, "ok", 0);
            cJSON_AddStringToObject(resp, "message", "Corrupt layout file on hub");
        }
    } else {
        cJSON_AddBoolToObject(resp, "ok", 0);
        cJSON_AddStringToObject(resp, "message", "No layout saved on hub");
    }
    send_cjson_resp(fd, "200 OK", resp);
    cJSON_Delete(resp);
}

void render_web_remote_layout_save_json(int fd, const struct request *req) {
    const char *payload = NULL;
    char *alloc_buf = NULL;
    if (req->body && req->body[0] == '{') {
        payload = req->body;
    } else if (req->body) {
        alloc_buf = (char *)malloc(MAX_REQUEST_BODY);
        if (alloc_buf) {
            form_value(req->body, "json", alloc_buf, MAX_REQUEST_BODY);
            if (alloc_buf[0] == '{') payload = alloc_buf;
        }
    }
    cJSON *parsed = payload ? cJSON_Parse(payload) : NULL;
    if (!parsed) {
        if (alloc_buf) free(alloc_buf);
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "Invalid JSON payload");
        send_cjson_resp(fd, "400 Bad Request", err);
        cJSON_Delete(err);
        return;
    }
    char *out = cJSON_Print(parsed);
    cJSON_Delete(parsed);
    if (alloc_buf) free(alloc_buf);
    if (!out) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "Formatting failed");
        send_cjson_resp(fd, "500 Internal Server Error", err);
        cJSON_Delete(err);
        return;
    }
    FILE *f = fopen(WEB_REMOTE_LAYOUT_FILE, "w");
    if (!f) {
        free(out);
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "Failed to write layout file");
        send_cjson_resp(fd, "500 Internal Server Error", err);
        cJSON_Delete(err);
        return;
    }
    fputs(out, f);
    fclose(f);
    free(out);
    chmod(WEB_REMOTE_LAYOUT_FILE, 0644);
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", 1);
    cJSON_AddStringToObject(resp, "message", "Remote layout saved to hub");
    send_cjson_resp(fd, "200 OK", resp);
    cJSON_Delete(resp);
}


