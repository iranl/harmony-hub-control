#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

#include "cJSON.h"

#define RFSPI_DEV          "/dev/rfspi"
#define RFFW_DEV           "/dev/rffw"
#define CC2544_FW          "/lib/firmware/cc2544.bin"
#define DAEMON_PORT        8089
#define WEBUI_PORT         8080
#define RF_API_PORT        8092
#define ELITE_MAP_FILE     "/data/codex/elite_remote_map.json"
#define CURRENT_ACT_FILE   "/data/codex/current_activity"

static volatile bool g_running = true;
static int g_rf_fd = -1;
static pthread_mutex_t g_rf_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint8_t g_paired_addr[4] = {0};
static bool g_has_paired_device = false;
static uint8_t g_fw_version = 0;
static bool g_pairing_mode = false;

static void sig_handler(int sig) {
    (void)sig;
    g_running = false;
}

/* Flash CC2544 firmware if needed */
static int flash_cc2544(void) {
    int fin = open(CC2544_FW, O_RDONLY);
    if (fin < 0) return -1;
    int fout = open(RFFW_DEV, O_WRONLY);
    if (fout < 0) { close(fin); return -1; }

    char buf[1024];
    ssize_t n, total = 0;
    while ((n = read(fin, buf, sizeof(buf))) > 0) {
        if (write(fout, buf, n) != n) {
            close(fin);
            close(fout);
            return -1;
        }
        total += n;
    }
    close(fin);
    close(fout);
    printf("[+] Programmed %zd bytes into CC2544 firmware\n", total);
    usleep(100000);
    return 0;
}

/* Send raw HID++ command with mutex */
static void rf_write_cmd(const uint8_t *cmd, size_t len) {
    if (g_rf_fd < 0) return;
    pthread_mutex_lock(&g_rf_mutex);
    write(g_rf_fd, cmd, len);
    pthread_mutex_unlock(&g_rf_mutex);
}

/* Trigger HTTP POST request to local service */
static void http_post_local(int port, const char *endpoint, const char *json_body) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return;

    struct timeval tv = { .tv_sec = 2, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (connect(sock, (struct sockaddr *)&sin, sizeof(sin)) == 0) {
        char req[1024];
        size_t blen = json_body ? strlen(json_body) : 0;
        int req_len = snprintf(req, sizeof(req),
            "POST %s HTTP/1.1\r\n"
            "Host: 127.0.0.1:%d\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n\r\n%s",
            endpoint, port, blen, json_body ? json_body : "");
        write(sock, req, req_len);
        char resp[256];
        read(sock, resp, sizeof(resp));
    }
    close(sock);
}

/* Read current running activity ID from Hub */
static void get_current_activity(char *out, size_t outlen) {
    FILE *f = fopen(CURRENT_ACT_FILE, "r");
    if (f) {
        if (fgets(out, outlen, f)) {
            fclose(f);
            char *p = out + strlen(out) - 1;
            while (p >= out && (*p == '\r' || *p == '\n' || *p == ' ')) *p-- = '\0';
            if (out[0]) return;
        } else {
            fclose(f);
        }
    }
    snprintf(out, outlen, "-1");
}

/* In-memory cache for elite_remote_map.json */
static cJSON *g_elite_map_cache = NULL;
static time_t g_elite_map_mtime = 0;
static pthread_mutex_t g_map_lock = PTHREAD_MUTEX_INITIALIZER;

static cJSON *get_elite_map(void) {
    struct stat st;
    if (stat(ELITE_MAP_FILE, &st) != 0) return NULL;
    pthread_mutex_lock(&g_map_lock);
    if (st.st_mtime != g_elite_map_mtime || !g_elite_map_cache) {
        if (g_elite_map_cache) {
            cJSON_Delete(g_elite_map_cache);
            g_elite_map_cache = NULL;
        }
        FILE *mf = fopen(ELITE_MAP_FILE, "r");
        if (mf) {
            fseek(mf, 0, SEEK_END);
            long sz = ftell(mf);
            fseek(mf, 0, SEEK_SET);
            if (sz > 0 && sz < 500000) {
                char *buf = malloc(sz + 1);
                if (buf) {
                    size_t n = fread(buf, 1, sz, mf);
                    buf[n] = '\0';
                    g_elite_map_cache = cJSON_Parse(buf);
                    free(buf);
                }
            }
            fclose(mf);
        }
        g_elite_map_mtime = st.st_mtime;
        printf("[+] (Re)loaded elite remote map cache (size=%ld bytes)\n", (long)st.st_size);
    }
    pthread_mutex_unlock(&g_map_lock);
    return g_elite_map_cache;
}

/* Dispatch button to configured action via cached elite_remote_map.json */
static void dispatch_elite_button(const char *button_name, const char *action_state) {
    int is_press = (strcmp(action_state, "release") != 0);

    /* Forward raw button event to daemon for MQTT (fire and forget) */
    char mqtt_body[256];
    snprintf(mqtt_body, sizeof(mqtt_body),
             "{\"command\":\"%s\",\"action\":\"%s\",\"source\":\"elite_rf\"}",
             button_name, action_state);
    http_post_local(DAEMON_PORT, "/api/button-press", mqtt_body);

    if (!is_press) return;

    /* Check in-memory cached map */
    cJSON *map = get_elite_map();
    if (!map) return;

    char act_id[32] = "-1";
    get_current_activity(act_id, sizeof(act_id));

    char act_type_buf[32] = {0};
    char dev_id_buf[64] = {0};
    char cmd_buf[64] = {0};
    char target_act_id_buf[64] = {0};

    pthread_mutex_lock(&g_map_lock);
    cJSON *acts = cJSON_GetObjectItemCaseSensitive(map, "activities");
    if (!acts || !cJSON_IsObject(acts)) acts = map;

    cJSON *btn_cfg = NULL;
    if (act_id[0] && strcmp(act_id, "-1") != 0) {
        cJSON *act_obj = cJSON_GetObjectItemCaseSensitive(acts, act_id);
        if (act_obj && cJSON_IsObject(act_obj)) {
            btn_cfg = cJSON_GetObjectItemCaseSensitive(act_obj, button_name);
        }
    }
    if (!btn_cfg) {
        cJSON *def_obj = cJSON_GetObjectItemCaseSensitive(acts, "-1");
        if (!def_obj) def_obj = cJSON_GetObjectItemCaseSensitive(acts, "default");
        if (def_obj && cJSON_IsObject(def_obj)) {
            btn_cfg = cJSON_GetObjectItemCaseSensitive(def_obj, button_name);
        }
    }

    if (btn_cfg && cJSON_IsObject(btn_cfg)) {
        cJSON *j_act = cJSON_GetObjectItemCaseSensitive(btn_cfg, "action");
        if (j_act && cJSON_IsString(j_act)) strncpy(act_type_buf, j_act->valuestring, sizeof(act_type_buf) - 1);

        cJSON *j_dev = cJSON_GetObjectItemCaseSensitive(btn_cfg, "targetDevice");
        if (!j_dev) j_dev = cJSON_GetObjectItemCaseSensitive(btn_cfg, "deviceId");
        if (j_dev && cJSON_IsString(j_dev)) strncpy(dev_id_buf, j_dev->valuestring, sizeof(dev_id_buf) - 1);

        cJSON *j_cmd = cJSON_GetObjectItemCaseSensitive(btn_cfg, "command");
        if (j_cmd && cJSON_IsString(j_cmd)) strncpy(cmd_buf, j_cmd->valuestring, sizeof(cmd_buf) - 1);

        cJSON *j_target_act = cJSON_GetObjectItemCaseSensitive(btn_cfg, "activityId");
        if (j_target_act && cJSON_IsString(j_target_act)) {
            strncpy(target_act_id_buf, j_target_act->valuestring, sizeof(target_act_id_buf) - 1);
        } else if (dev_id_buf[0]) {
            strncpy(target_act_id_buf, dev_id_buf, sizeof(target_act_id_buf) - 1);
        }
    }
    pthread_mutex_unlock(&g_map_lock);

    if (strcmp(act_type_buf, "device_cmd") == 0 && dev_id_buf[0] && cmd_buf[0]) {
        char body[256];
        snprintf(body, sizeof(body), "{\"deviceId\":\"%s\",\"command\":\"%s\"}", dev_id_buf, cmd_buf);
        printf("[*] RF Dispatch: IR/BT send -> dev=%s cmd=%s\n", dev_id_buf, cmd_buf);
        http_post_local(WEBUI_PORT, "/api/ir-send", body);
    } else if (strcmp(act_type_buf, "activity_start") == 0 && target_act_id_buf[0]) {
        char body[64];
        snprintf(body, sizeof(body), "{\"id\":\"%s\"}", target_act_id_buf);
        printf("[*] RF Dispatch: Start activity -> id=%s\n", target_act_id_buf);
        http_post_local(WEBUI_PORT, "/api/activity-start", body);
    } else if (strcmp(act_type_buf, "activity_stop") == 0) {
        printf("[*] RF Dispatch: Stop activity (PowerOff)\n");
        http_post_local(WEBUI_PORT, "/api/activity-stop", "{}");
    }
}

/* Asynchronous button dispatch queue to keep rx_worker completely non-blocking */
typedef struct {
    char button_name[64];
    char action_state[16];
} rf_event_t;

#define RF_QUEUE_SIZE 64
static rf_event_t g_rf_queue[RF_QUEUE_SIZE];
static int g_rf_q_head = 0;
static int g_rf_q_tail = 0;
static pthread_mutex_t g_rf_q_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_rf_q_cond = PTHREAD_COND_INITIALIZER;

static void enqueue_elite_button(const char *name, const char *action) {
    pthread_mutex_lock(&g_rf_q_mutex);
    int next = (g_rf_q_head + 1) % RF_QUEUE_SIZE;
    if (next != g_rf_q_tail) {
        strncpy(g_rf_queue[g_rf_q_head].button_name, name, sizeof(g_rf_queue[0].button_name) - 1);
        strncpy(g_rf_queue[g_rf_q_head].action_state, action, sizeof(g_rf_queue[0].action_state) - 1);
        g_rf_q_head = next;
        pthread_cond_signal(&g_rf_q_cond);
    } else {
        printf("[-] Warning: RF dispatch queue overflow!\n");
    }
    pthread_mutex_unlock(&g_rf_q_mutex);
}

static void *dispatch_worker(void *arg) {
    (void)arg;
    while (g_running) {
        rf_event_t ev;
        pthread_mutex_lock(&g_rf_q_mutex);
        while (g_rf_q_head == g_rf_q_tail && g_running) {
            pthread_cond_wait(&g_rf_q_cond, &g_rf_q_mutex);
        }
        if (!g_running) {
            pthread_mutex_unlock(&g_rf_q_mutex);
            break;
        }
        ev = g_rf_queue[g_rf_q_tail];
        g_rf_q_tail = (g_rf_q_tail + 1) % RF_QUEUE_SIZE;
        pthread_mutex_unlock(&g_rf_q_mutex);

        dispatch_elite_button(ev.button_name, ev.action_state);
    }
    return NULL;
}

/* Dispatch activity start to webui_server on 127.0.0.1:8080 */
static void dispatch_activity_to_daemon(const char *act_id) {
    if (strcmp(act_id, "-1") == 0) {
        printf("[*] RF Dispatching Activity Stop (PowerOff) to Hub\n");
        http_post_local(WEBUI_PORT, "/api/activity-stop", "{}");
    } else {
        printf("[*] RF Dispatching Activity Start: %s to Hub\n", act_id);
        char body[64];
        snprintf(body, sizeof(body), "{\"id\":\"%s\"}", act_id);
        http_post_local(WEBUI_PORT, "/api/activity-start", body);
    }
}

/* Map RF button raw key code to human-readable Harmony name */
static const char *map_button_code(uint8_t k0, uint8_t k1) {
    uint16_t key = ((uint16_t)k0 << 8) | k1;
    switch (key) {
        /* Real Harmony RF keycodes */
        case 0x01EC: return "PowerOffActivity";
        case 0x0052: return "DirectionUp";
        case 0x0051: return "DirectionDown";
        case 0x0050: return "DirectionLeft";
        case 0x004F: return "DirectionRight";
        case 0x0058: return "Select";
        case 0x0225: return "Back";
        case 0x0065: return "Menu";
        case 0x0094: return "Exit";
        case 0x01FF: return "Info";
        case 0x008D: return "Guide";
        case 0x0089: return "Live";
        case 0x009A: return "Dvr";
        case 0x00E9: return "VolumeUp";
        case 0x00EA: return "VolumeDown";
        case 0x00E2: return "VolumeMute";
        case 0x009C: return "ChannelUp";
        case 0x009D: return "ChannelDown";
        case 0x0224: return "PrevChannel";
        case 0x00B0: return "Play";
        case 0x00B1: return "Pause";
        case 0x00B2: return "Record";
        case 0x00B3: return "FastForward";
        case 0x00B4: return "Rewind";
        case 0x00B7: return "Stop";
        case 0x01F7: return "Red";
        case 0x01F6: return "Green";
        case 0x01F5: return "Yellow";
        case 0x01F4: return "Blue";
        case 0x0FF2: return "Ha1";
        case 0x0FF3: return "Ha2";
        case 0x0FF4: return "Ha3";
        case 0x0FF5: return "Ha4";
        case 0x0FF0: return "RockerUp";
        case 0x0FF1: return "RockerDown";

        /* Legacy mapping compatibility */
        case 0x0101: return "WatchTVActivity";
        case 0x0102: return "MusicActivity";
        case 0x0103: return "MovieActivity";
        case 0x0104: return "PCTVActivity";
        case 0x0105: return "PowerOffActivity";
        case 0x0201: return "DirectionUp";
        case 0x0202: return "DirectionDown";
        case 0x0203: return "DirectionLeft";
        case 0x0204: return "DirectionRight";
        case 0x0205: return "Select";
        case 0x0206: return "Back";
        case 0x0207: return "Menu";
        case 0x0208: return "Exit";
        case 0x0209: return "Info";
        case 0x020A: return "Guide";
        case 0x020B: return "Live";
        case 0x020C: return "Dvr";
        case 0x0301: return "VolumeUp";
        case 0x0302: return "VolumeDown";
        case 0x0303: return "VolumeMute";
        case 0x0304: return "ChannelUp";
        case 0x0305: return "ChannelDown";
        case 0x0306: return "PrevChannel";
        case 0x0401: return "Play";
        case 0x0402: return "Pause";
        case 0x0403: return "Record";
        case 0x0404: return "FastForward";
        case 0x0405: return "Rewind";
        case 0x0501: return "Red";
        case 0x0502: return "Green";
        case 0x0503: return "Yellow";
        case 0x0504: return "Blue";
        case 0x0601: return "Ha1";
        case 0x0602: return "Ha2";
        case 0x0603: return "Ha3";
        case 0x0604: return "Ha4";
        case 0x0605: return "RockerUp";
        case 0x0606: return "RockerDown";

        default: return NULL;
    }
}

/* Parse incoming HID++ packet from CC2544 */
static void handle_rf_packet(const uint8_t *pkt, size_t len) {
    if (len < 7) return;

    printf("[*] RX (len=%zd): ", len);
    for (size_t i = 0; i < len; i++) printf("%02X ", pkt[i]);
    printf("\n");

    uint8_t report_id = pkt[0];

    /* Short Report: 7 bytes */
    if (report_id == 0x10) {
        uint8_t sub_id = pkt[2];

        /* Firmware Version Reply */
        if (sub_id == 0x81 && pkt[3] == 0xF1) {
            g_fw_version = pkt[5];
            printf("[+] CC2544 FW Version: v%d.00 (0x%02X)\n", g_fw_version, g_fw_version);
            return;
        }

        /* 0x41 / 0x40 / 0x01 / 0x03: Button / Status Notification */
        if (sub_id == 0x41 || sub_id == 0x40 || sub_id == 0x01 || sub_id == 0x03) {
            uint8_t k0, k1, state;
            if (sub_id == 0x03) {
                k0 = pkt[4]; k1 = pkt[3]; state = pkt[5];
            } else {
                k0 = pkt[3]; k1 = pkt[4]; state = pkt[5];
            }
            const char *action = (state == 1) ? "press" : ((state == 2) ? "hold" : "release");
            const char *name = map_button_code(k0, k1);
            if (!name && k0 == 0) name = map_button_code(0, k1);

            printf("[*] Remote Button (0x10/0x%02X): raw=[0x%02X, 0x%02X] action=%s name=%s\n",
                   sub_id, k0, k1, action, name ? name : "Unknown");

            if (name) {
                enqueue_elite_button(name, action);
            }
            return;
        }

        /* 0x80 / 0xFD: HOT RF Key packet from Remote */
        if (sub_id == 0x80 && pkt[3] == 0xFD && pkt[4] == 0x01 && len >= 7) {
            uint8_t k0 = pkt[5];
            uint8_t k1 = pkt[6];
            const char *name = map_button_code(k0, k1);
            printf("[*] Remote Button (HOT 0x10): raw=[0x%02X, 0x%02X] name=%s\n",
                   k0, k1, name ? name : "Unknown");
            if (name) {
                enqueue_elite_button(name, "press");
                enqueue_elite_button(name, "release");
            }
            return;
        }

        /* 0x4A: Wireless Link / Activity heartbeat */
        if (sub_id == 0x4A) {
            /* Quietly accept Remote link packets */
            return;
        }

        /* Wireless Link / Connection notification from Remote */
        if (sub_id == 0x80 && pkt[3] == 0xB2) {
            printf("[+] Wireless Link / Connection packet received from Remote\n");
            return;
        }

        /* 0x8F: Error / NAK */
        if (sub_id == 0x8F) {
            printf("[-] CC2544 Error/NAK: for SubID=0x%02X err=0x%02X\n", pkt[3], pkt[5]);
            return;
        }
    }

    /* Long Report: 20 bytes */
    if (report_id == 0x11) {
        uint8_t sub_id = pkt[2];

        /* Paired Device List reply (0x83 0xB5 0x03) */
        if (sub_id == 0x83 && pkt[3] == 0xB5 && pkt[4] == 0x03) {
            memcpy(g_paired_addr, &pkt[5], 4);
            g_has_paired_device = (g_paired_addr[0] != 0 || g_paired_addr[1] != 0);
            printf("[+] Paired Remote RF Address: %02X %02X %02X %02X (paired=%d)\n",
                   g_paired_addr[0], g_paired_addr[1], g_paired_addr[2], g_paired_addr[3],
                   g_has_paired_device ? 1 : 0);
            return;
        }

        /* 0xFD: HOT (Harmony Object Transfer) Data Frame */
        if (sub_id == 0xFD) {
            uint8_t hot_cmd = pkt[3];
            if (hot_cmd == 0x01) {
                /* Start Activity command from Remote */
                char act_id[32] = {0};
                memcpy(act_id, &pkt[4], (len - 4 < 31) ? len - 4 : 31);
                printf("[*] Remote requested Activity Start: %s\n", act_id);
                dispatch_activity_to_daemon(act_id);
            } else {
                printf("[*] HOT Frame (len=%zd): ", len);
                for (size_t i = 0; i < len; i++) printf("%02X ", pkt[i]);
                printf("\n");
            }
            return;
        }
    }

    /* Wireless / Very Long Report: 0x20 or 0x12 (up to 32 bytes) */
    if (report_id == 0x20 || report_id == 0x12) {
        uint8_t sub_id = pkt[2];
        if (sub_id == 0x01 || sub_id == 0x03 || sub_id == 0x41 || sub_id == 0x40 || sub_id == 0x42) {
            uint8_t k0, k1, state;
            if (sub_id == 0x03) {
                k0 = pkt[4]; k1 = pkt[3]; state = pkt[5];
            } else {
                k0 = pkt[3]; k1 = pkt[4]; state = pkt[5];
            }
            const char *action = (state == 1) ? "press" : ((state == 2) ? "hold" : "release");
            const char *name = map_button_code(k0, k1);
            if (!name && k0 == 0) name = map_button_code(0, k1);

            printf("[*] Remote Button (0x%02X/0x%02X): raw=[0x%02X, 0x%02X] action=%s name=%s\n",
                   report_id, sub_id, k0, k1, action, name ? name : "Unknown");

            if (name) {
                enqueue_elite_button(name, action);
            }
            return;
        }
        if (sub_id == 0xFD) {
            uint8_t hot_cmd = pkt[3];
            if (hot_cmd == 0x01) {
                char act_id[32] = {0};
                memcpy(act_id, &pkt[4], (len - 4 < 31) ? len - 4 : 31);
                printf("[*] Remote requested Activity Start (0x%02X): %s\n", report_id, act_id);
                dispatch_activity_to_daemon(act_id);
            }
            return;
        }
    }

    /* Generic packet log */
    printf("[*] RX (%zd bytes): ", len);
    for (size_t i = 0; i < len; i++) printf("%02X ", pkt[i]);
    printf("\n");
}

/* Background receiver thread (blocking read on /dev/rfspi) */
static void *rx_worker(void *arg) {
    (void)arg;
    uint8_t buf[64];

    while (g_running) {
        memset(buf, 0, sizeof(buf));
        ssize_t n = read(g_rf_fd, buf, sizeof(buf));
        if (n > 0) {
            handle_rf_packet(buf, (size_t)n);
        } else if (n < 0) {
            if (errno == EINTR) continue;
            perror("read /dev/rfspi");
            usleep(100000);
        }
    }
    return NULL;
}

/* Pairing control commands */
static void start_pairing_mode(void) {
    uint8_t open_lock[7] = {0x10, 0xFF, 0x80, 0xB2, 0x01, 0x00, 0x00};
    printf("[*] Opening pairing lock (10 FF 80 B2 01 00 00)...\n");
    rf_write_cmd(open_lock, sizeof(open_lock));
    g_pairing_mode = true;
}

static void stop_pairing_mode(void) {
    uint8_t close_lock[7] = {0x10, 0xFF, 0x80, 0xB2, 0x02, 0x00, 0x00};
    printf("[*] Closing pairing lock (10 FF 80 B2 02 00 00)...\n");
    rf_write_cmd(close_lock, sizeof(close_lock));
    g_pairing_mode = false;
}

static void unpair_device(void) {
    uint8_t unpair[7] = {0x10, 0xFF, 0x80, 0xB2, 0x03, 0x00, 0x00};
    printf("[*] Unpairing remote (10 FF 80 B2 03 00 00)...\n");
    rf_write_cmd(unpair, sizeof(unpair));
    g_has_paired_device = false;
    memset(g_paired_addr, 0, sizeof(g_paired_addr));
}

/* Minimal HTTP server on 8092 for WebUI / CLI control */
static void *api_server_worker(void *arg) {
    (void)arg;
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) return NULL;

    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(RF_API_PORT);
    sin.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(srv, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        close(srv);
        return NULL;
    }
    listen(srv, 4);

    while (g_running) {
        int cli = accept(srv, NULL, NULL);
        if (cli < 0) {
            if (errno == EINTR) continue;
            break;
        }

        char buf[512];
        ssize_t n = read(cli, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            if (strstr(buf, "GET /api/rf/pair") == buf || strstr(buf, "POST /api/rf/pair") == buf) {
                start_pairing_mode();
                const char *res = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n{\"status\":\"pairing_started\"}";
                write(cli, res, strlen(res));
            } else if (strstr(buf, "GET /api/rf/stop-pair") == buf) {
                stop_pairing_mode();
                const char *res = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n{\"status\":\"pairing_stopped\"}";
                write(cli, res, strlen(res));
            } else if (strstr(buf, "GET /api/rf/unpair") == buf) {
                unpair_device();
                const char *res = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n{\"status\":\"unpaired\"}";
                write(cli, res, strlen(res));
            } else {
                /* Status endpoint */
                char body[256];
                int blen = snprintf(body, sizeof(body),
                    "{\"fw_version\":\"v%d.00\",\"paired\":%s,\"rf_address\":\"%02X%02X%02X%02X\",\"pairing_active\":%s}",
                    g_fw_version, g_has_paired_device ? "true" : "false",
                    g_paired_addr[0], g_paired_addr[1], g_paired_addr[2], g_paired_addr[3],
                    g_pairing_mode ? "true" : "false");

                char header[256];
                int hlen = snprintf(header, sizeof(header),
                    "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n", blen);
                write(cli, header, hlen);
                write(cli, body, blen);
            }
        }
        close(cli);
    }
    close(srv);
    return NULL;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    printf("[+] Starting Harmony Hub RF Daemon (codex_rf)...\n");

    /* Open SPI driver */
    g_rf_fd = open(RFSPI_DEV, O_RDWR);
    if (g_rf_fd < 0) {
        fprintf(stderr, "[-] Warning: cannot open %s, attempting CC2544 flash...\n", RFSPI_DEV);
        flash_cc2544();
        g_rf_fd = open(RFSPI_DEV, O_RDWR);
        if (g_rf_fd < 0) {
            fprintf(stderr, "[-] Fatal: cannot open %s: %s\n", RFSPI_DEV, strerror(errno));
            return 1;
        }
    }
    printf("[+] Opened %s (fd=%d)\n", RFSPI_DEV, g_rf_fd);

    /* 1. Initialize CC2544 message engine */
    uint8_t pkt_init[7] = {0x10, 0xFF, 0x80, 0x00, 0x00, 0x01, 0x00};
    rf_write_cmd(pkt_init, sizeof(pkt_init));
    usleep(50000);

    /* 2. Start background RX and Dispatch threads */
    pthread_t rx_th;
    if (pthread_create(&rx_th, NULL, rx_worker, NULL) != 0) {
        perror("pthread_create rx_worker");
        close(g_rf_fd);
        return 1;
    }

    pthread_t disp_th;
    pthread_create(&disp_th, NULL, dispatch_worker, NULL);

    /* 3. Query initial state */
    uint8_t pkt_ver[7] = {0x10, 0xFF, 0x81, 0xF1, 0x01, 0x00, 0x00};
    rf_write_cmd(pkt_ver, sizeof(pkt_ver));
    usleep(50000);

    uint8_t pkt_paired_list[7] = {0x10, 0xFF, 0x83, 0xB5, 0x03, 0x00, 0x00};
    rf_write_cmd(pkt_paired_list, sizeof(pkt_paired_list));
    usleep(50000);

    /* 4. Start local REST API thread */
    pthread_t api_th;
    pthread_create(&api_th, NULL, api_server_worker, NULL);
    printf("[+] RF API listening on http://127.0.0.1:%d/api/rf/...\n", RF_API_PORT);

    /* Optional CLI flags */
    if (argc > 1) {
        if (strcmp(argv[1], "--pair") == 0) start_pairing_mode();
        else if (strcmp(argv[1], "--unpair") == 0) unpair_device();
    } else {
        stop_pairing_mode();
    }

    printf("[+] codex_rf ready and operational.\n");

    /* Main loop wait */
    while (g_running) {
        sleep(1);
    }

    printf("[+] Shutting down codex_rf...\n");
    close(g_rf_fd);
    g_rf_fd = -1;
    pthread_join(rx_th, NULL);
    return 0;
}
