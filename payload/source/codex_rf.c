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
#include "codex_rf_proto.h"
#include <sys/stat.h>
#include <sys/select.h>

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
static uint8_t g_remote_slot = 0x01;
static bool g_has_paired_device = false;
static uint8_t g_fw_version = 0;
static bool g_pairing_mode = false;

static int g_nak_count = 0;
static int g_backoff_ms = 0;
#define NAK_BACKOFF_MAX_MS 5000
#define SYNC_DEDUP_MS      1000
static char g_last_sync_act[32] = "";
static uint64_t g_last_sync_ms = 0;
static int g_debug_logging = 1;

static uint64_t get_monotonic_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

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

/* Send raw HID++ command with mutex; returns 0 on success, -1 on failure */
static int rf_write_cmd(const uint8_t *cmd, size_t len) {
    if (g_rf_fd < 0) return -1;
    pthread_mutex_lock(&g_rf_mutex);
    ssize_t w = write(g_rf_fd, cmd, len);
    pthread_mutex_unlock(&g_rf_mutex);
    if (w < 0) {
        perror("write /dev/rfspi");
        return -1;
    }
    if (w > 0 && (size_t)w != len) {
        if (g_debug_logging)
            printf("[-] TX partial (%zd/%zu bytes)\n", w, len);
        return -1;
    }
    if (g_debug_logging) {
        printf("[*] TX (%zu bytes): ", len);
        for (size_t i = 0; i < len; i++) printf("%02X ", cmd[i]);
        printf("\n");
    }
    return 0;
}

static uint8_t g_tx_seq = 0;
static uint8_t g_pending_ack_seq = 0;
static int g_ack_received = 0;
static int g_ack_status = 0;
static pthread_mutex_t g_ack_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_ack_cond = PTHREAD_COND_INITIALIZER;

/* Send a Codex RF frame over /dev/rfspi (32 bytes) */
static int rf_send_frame(rf_frame_t *frame) {
    frame->report_id = RF_REPORT_ID;
    uint8_t slot = (frame->dev_slot & 0x0F) ? (frame->dev_slot & 0x0F) : (g_remote_slot ? (g_remote_slot & 0x0F) : 0x01);
    frame->dev_slot = slot | 0x30;
    frame->sub_id = 0x12;
    return rf_write_cmd((const uint8_t *)frame, RF_FRAME_SIZE);
}

/* Send frame and wait for ACK (blocking, up to timeout_ms). Returns 0 on ACK, -1 on timeout. */
static int rf_send_frame_acked(rf_frame_t *frame, int timeout_ms) {
    pthread_mutex_lock(&g_ack_mutex);
    g_pending_ack_seq = frame->seq;
    g_ack_received = 0;
    pthread_mutex_unlock(&g_ack_mutex);

    if (rf_send_frame(frame) < 0) return -1;

    pthread_mutex_lock(&g_ack_mutex);
    if (!g_ack_received) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout_ms / 1000;
        ts.tv_nsec += (timeout_ms % 1000) * 1000000L;
        if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
        pthread_cond_timedwait(&g_ack_cond, &g_ack_mutex, &ts);
    }
    int ok = g_ack_received;
    pthread_mutex_unlock(&g_ack_mutex);
    return ok ? 0 : -1;
}

/* Trigger HTTP POST request to local service (with connect timeout) */
static void http_post_local(int port, const char *endpoint, const char *json_body) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return;

    /* Non-blocking connect with 2s timeout */
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    int rc = connect(sock, (struct sockaddr *)&sin, sizeof(sin));
    if (rc < 0 && errno == EINPROGRESS) {
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(sock, &wfds);
        struct timeval ctv = { .tv_sec = 2, .tv_usec = 0 };
        if (select(sock + 1, NULL, &wfds, NULL, &ctv) <= 0) {
            close(sock);
            return;
        }
        int err = 0;
        socklen_t elen = sizeof(err);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &elen);
        if (err) { close(sock); return; }
    } else if (rc < 0) {
        close(sock);
        return;
    }

    /* Restore blocking mode with send/recv timeouts */
    fcntl(sock, F_SETFL, flags);
    struct timeval tv = { .tv_sec = 2, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

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
        if (!def_obj) def_obj = cJSON_GetObjectItemCaseSensitive(acts, "53591842");
        if (!def_obj && acts->child) def_obj = acts->child;
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
        snprintf(body, sizeof(body), "{\"activity\":\"%s\"}", target_act_id_buf);
        printf("[*] RF Dispatch: Start activity -> id=%s\n", target_act_id_buf);
        http_post_local(DAEMON_PORT, "/api/activity", body);
    } else if (strcmp(act_type_buf, "activity_stop") == 0) {
        printf("[*] RF Dispatch: Stop activity (PowerOff)\n");
        http_post_local(DAEMON_PORT, "/api/poweroff", "{}");
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

/* Dispatch activity start to daemon on 127.0.0.1:8089 */
static void dispatch_activity_to_daemon(const char *act_id) {
    if (strcmp(act_id, "-1") == 0) {
        printf("[*] RF Dispatching Activity Stop (PowerOff) to Hub\n");
        http_post_local(DAEMON_PORT, "/api/poweroff", "{}");
    } else {
        printf("[*] RF Dispatching Activity Start: %s to Hub\n", act_id);
        char body[64];
        snprintf(body, sizeof(body), "{\"activity\":\"%s\"}", act_id);
        http_post_local(DAEMON_PORT, "/api/activity", body);
    }
}

/* Dispatch device command from remote (e.g. Devices page on screen) to webui_server */
static void dispatch_device_command_to_webui(const char *payload) {
    char buf[128] = {0};
    strncpy(buf, payload, sizeof(buf) - 1);
    char *colon = strchr(buf, ':');
    if (!colon) return;
    *colon = '\0';
    const char *dev = buf;
    const char *cmd = colon + 1;
    char body[256];
    snprintf(body, sizeof(body), "{\"deviceId\":\"%s\",\"command\":\"%s\"}", dev, cmd);
    printf("[*] RF Dispatch Device Cmd: dev=%s cmd=%s\n", dev, cmd);
    http_post_local(WEBUI_PORT, "/api/ir-send", body);
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

        default: return NULL;
    }
}

/* Look up activity name by ID from /data/resources/ActivityList.json */
static void get_activity_name_by_id(const char *act_id, char *out_name, size_t out_len) {
    if (!act_id || strcmp(act_id, "-1") == 0) {
        snprintf(out_name, out_len, "PowerOff");
        return;
    }
    FILE *f = fopen("/data/resources/ActivityList.json", "r");
    if (!f) f = fopen("/data/resources/ActivityList_from_brain.json", "r");
    if (f) {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz > 0 && sz < 1000000) {
            char *buf = malloc(sz + 1);
            if (buf) {
                size_t n = fread(buf, 1, sz, f);
                buf[n] = '\0';
                cJSON *root = cJSON_Parse(buf);
                free(buf);
                if (root) {
                    cJSON *acts = cJSON_GetObjectItem(root, "Activities");
                    if (acts && cJSON_IsArray(acts)) {
                        cJSON *item = NULL;
                        cJSON_ArrayForEach(item, acts) {
                            cJSON *act_obj = item;
                            cJSON *inner = cJSON_GetObjectItemCaseSensitive(item, "Activity");
                            if (inner && cJSON_IsObject(inner)) act_obj = inner;

                            cJSON *j_name = cJSON_GetObjectItem(act_obj, "Name");
                            cJSON *j_id = cJSON_GetObjectItem(act_obj, "Id-");
                            if (!j_id) j_id = cJSON_GetObjectItem(act_obj, "Id");
                            if (!j_id) j_id = cJSON_GetObjectItem(act_obj, "id");
                            if (j_name && cJSON_IsString(j_name) && j_id) {
                                char id_str[32] = {0};
                                if (cJSON_IsNumber(j_id)) snprintf(id_str, sizeof(id_str), "%d", j_id->valueint);
                                else if (cJSON_IsString(j_id)) strncpy(id_str, j_id->valuestring, sizeof(id_str) - 1);
                                if (strcmp(id_str, act_id) == 0) {
                                    strncpy(out_name, j_name->valuestring, out_len - 1);
                                    out_name[out_len - 1] = '\0';
                                    cJSON_Delete(root);
                                    fclose(f);
                                    return;
                                }
                            }
                        }
                    }
                    cJSON_Delete(root);
                }
            }
        }
        fclose(f);
    }
    strncpy(out_name, act_id, out_len - 1);
    out_name[out_len - 1] = '\0';
}

/* Send Codex Activity Sync frame with explicit sequence number */
static void rf_send_codex_activity_sync_seq(const char *act_id, uint8_t seq) {
    if (g_rf_fd < 0 || !g_has_paired_device) return;

    char act_name[17] = {0};
    get_activity_name_by_id(act_id, act_name, sizeof(act_name));

    rf_frame_t f;
    uint8_t payload[26] = {0};
    strncpy((char *)&payload[0], act_id, 12);
    strncpy((char *)&payload[12], act_name, 14);
    rf_build_frame(&f, (g_remote_slot ? g_remote_slot : 0x01) | 0x30, RF_MSG_ACTIVITY_SYNC, seq, 0, payload, 26);
    rf_send_frame(&f);

    if (g_debug_logging)
        printf("[*] Codex ACTIVITY_SYNC sent (seq=%d): id=%s name=%s\n", seq, act_id, act_name);
}

/* Send Codex Activity Sync frame (RF_MSG_ACTIVITY_SYNC) */
static void rf_send_codex_activity_sync(const char *act_id) {
    if (g_rf_fd < 0 || !g_has_paired_device) return;

    /* Dedup */
    uint64_t now = get_monotonic_ms();
    if (strcmp(act_id, g_last_sync_act) == 0 && (now - g_last_sync_ms) < SYNC_DEDUP_MS) return;
    strncpy(g_last_sync_act, act_id, sizeof(g_last_sync_act) - 1);
    g_last_sync_act[sizeof(g_last_sync_act) - 1] = '\0';
    g_last_sync_ms = now;

    rf_send_codex_activity_sync_seq(act_id, ++g_tx_seq);
}

/* Background worker: pushes full config to Remote */
static void *config_push_worker(void *arg) {
    (void)arg;
    static pthread_mutex_t push_lock = PTHREAD_MUTEX_INITIALIZER;
    if (pthread_mutex_trylock(&push_lock) != 0) {
        printf("[-] Config push already in progress\n");
        return NULL;
    }
    printf("[*] Starting config push to remote...\n");

    static uint8_t config_ver = 0;
    config_ver++;

    /* 1. CONFIG_END (clear) */
    rf_frame_t f;
    uint8_t target_slot = g_remote_slot ? g_remote_slot : 0x01;
    uint8_t payload[26] = {0};
    payload[0] = 0x00; /* clear */
    payload[1] = config_ver;
    rf_build_frame(&f, target_slot, RF_MSG_CONFIG_END, ++g_tx_seq, RF_FLAG_ACK_REQ, payload, 2);
    if (rf_send_frame_acked(&f, 500) < 0) {
        if (rf_send_frame_acked(&f, 500) < 0) {
            printf("[-] Config push: no ACK for CONFIG_END(clear), aborting\n");
            pthread_mutex_unlock(&push_lock);
            return NULL;
        }
    }
    usleep(20000);

    /* 2. Activities */
    struct {
        char id[13];
        char name[15];
    } acts[16];
    int act_count = 0;

    FILE *fa = fopen("/data/resources/ActivityList.json", "r");
    if (!fa) fa = fopen("/data/resources/ActivityList_from_brain.json", "r");
    if (fa) {
        fseek(fa, 0, SEEK_END);
        long sz = ftell(fa);
        fseek(fa, 0, SEEK_SET);
        if (sz > 0 && sz < 1000000) {
            char *buf = malloc(sz + 1);
            if (buf) {
                size_t n = fread(buf, 1, sz, fa);
                buf[n] = '\0';
                cJSON *root = cJSON_Parse(buf);
                free(buf);
                if (root) {
                    cJSON *acts_arr = cJSON_GetObjectItem(root, "Activities");
                    if (acts_arr && cJSON_IsArray(acts_arr)) {
                        cJSON *elem = NULL;
                        cJSON_ArrayForEach(elem, acts_arr) {
                            if (act_count >= 16) break;
                            cJSON *act_obj = elem;
                            cJSON *inner = cJSON_GetObjectItemCaseSensitive(elem, "Activity");
                            if (inner && cJSON_IsObject(inner)) act_obj = inner;

                            cJSON *j_name = cJSON_GetObjectItem(act_obj, "Name");
                            cJSON *j_id = cJSON_GetObjectItem(act_obj, "Id-");
                            if (!j_id) j_id = cJSON_GetObjectItem(act_obj, "Id");
                            if (!j_id) j_id = cJSON_GetObjectItem(act_obj, "id");
                            if (j_name && cJSON_IsString(j_name) && j_id) {
                                char id_str[32] = {0};
                                if (cJSON_IsNumber(j_id)) snprintf(id_str, sizeof(id_str), "%d", j_id->valueint);
                                else if (cJSON_IsString(j_id)) strncpy(id_str, j_id->valuestring, sizeof(id_str) - 1);
                                if (strcmp(id_str, "-1") != 0) {
                                    strncpy(acts[act_count].id, id_str, 12);
                                    acts[act_count].id[12] = '\0';
                                    strncpy(acts[act_count].name, j_name->valuestring, 14);
                                    acts[act_count].name[14] = '\0';
                                    act_count++;
                                }
                            }
                        }
                    }
                    cJSON_Delete(root);
                }
            }
        }
        fclose(fa);
    }

    for (int i = 0; i < act_count; i++) {
        memset(payload, 0, sizeof(payload));
        payload[0] = (uint8_t)i;
        payload[1] = (uint8_t)act_count;
        memcpy(&payload[2], acts[i].id, 10);
        memcpy(&payload[12], acts[i].name, 14);
        rf_build_frame(&f, target_slot, RF_MSG_ACTIVITY_LIST_CHUNK, ++g_tx_seq, RF_FLAG_ACK_REQ, payload, 26);
        if (rf_send_frame_acked(&f, 500) < 0) {
            rf_send_frame_acked(&f, 500);
        }
        usleep(20000);
    }

    /* 3. Devices */
    struct {
        char id[13];
        char name[15];
    } devs[16];
    int dev_count = 0;

    FILE *fd = fopen("/data/resources/DeviceList.json", "r");
    if (fd) {
        fseek(fd, 0, SEEK_END);
        long sz = ftell(fd);
        fseek(fd, 0, SEEK_SET);
        if (sz > 0 && sz < 1000000) {
            char *buf = malloc(sz + 1);
            if (buf) {
                size_t n = fread(buf, 1, sz, fd);
                buf[n] = '\0';
                cJSON *root = cJSON_Parse(buf);
                free(buf);
                if (root) {
                    cJSON *devs_arr = cJSON_GetObjectItem(root, "DevicesWithFeatures");
                    if (!devs_arr) devs_arr = cJSON_GetObjectItem(root, "devices");
                    if (devs_arr && cJSON_IsArray(devs_arr)) {
                        cJSON *elem = NULL;
                        cJSON_ArrayForEach(elem, devs_arr) {
                            if (dev_count >= 16) break;
                            cJSON *d_obj = elem;
                            cJSON *inner = cJSON_GetObjectItemCaseSensitive(elem, "Device");
                            if (inner && cJSON_IsObject(inner)) d_obj = inner;

                            cJSON *jid = cJSON_GetObjectItemCaseSensitive(d_obj, "Id-");
                            if (!jid) jid = cJSON_GetObjectItemCaseSensitive(d_obj, "Id");
                            if (!jid) jid = cJSON_GetObjectItemCaseSensitive(d_obj, "id");
                            if (!jid) jid = cJSON_GetObjectItemCaseSensitive(d_obj, "ContentProfileKey");

                            cJSON *jname = cJSON_GetObjectItemCaseSensitive(d_obj, "ParentDeviceModel");
                            if (!jname) jname = cJSON_GetObjectItemCaseSensitive(d_obj, "Name");
                            if (!jname) jname = cJSON_GetObjectItemCaseSensitive(d_obj, "Model");
                            if (!jname) jname = cJSON_GetObjectItemCaseSensitive(d_obj, "FriendlyName");

                            if (jid && jname && cJSON_IsString(jname)) {
                                char id_str[32] = {0};
                                if (cJSON_IsNumber(jid)) snprintf(id_str, sizeof(id_str), "%d", jid->valueint);
                                else if (cJSON_IsString(jid)) strncpy(id_str, jid->valuestring, sizeof(id_str) - 1);

                                if (id_str[0]) {
                                    strncpy(devs[dev_count].id, id_str, 12);
                                    devs[dev_count].id[12] = '\0';
                                    strncpy(devs[dev_count].name, jname->valuestring, 14);
                                    devs[dev_count].name[14] = '\0';
                                    dev_count++;
                                }
                            }
                        }
                    }
                    cJSON_Delete(root);
                }
            }
        }
        fclose(fd);
    }

    for (int i = 0; i < dev_count; i++) {
        memset(payload, 0, sizeof(payload));
        payload[0] = (uint8_t)i;
        payload[1] = (uint8_t)dev_count;
        memcpy(&payload[2], devs[i].id, 10);
        memcpy(&payload[12], devs[i].name, 14);
        rf_build_frame(&f, target_slot, RF_MSG_DEVICE_LIST_CHUNK, ++g_tx_seq, RF_FLAG_ACK_REQ, payload, 26);
        if (rf_send_frame_acked(&f, 500) < 0) {
            rf_send_frame_acked(&f, 500);
        }
        usleep(20000);
    }

    /* 4. Buttons from elite_remote_map.json */
    cJSON *map = get_elite_map();
    int btn_count = 0;
    if (map) {
        pthread_mutex_lock(&g_map_lock);
        cJSON *acts_map = cJSON_GetObjectItemCaseSensitive(map, "activities");
        if (acts_map && cJSON_IsObject(acts_map)) {
            cJSON *act_entry = NULL;
            cJSON_ArrayForEach(act_entry, acts_map) {
                const char *ctx_id = act_entry->string ? act_entry->string : "-1";
                if (!cJSON_IsObject(act_entry)) continue;

                cJSON *b_entry = NULL;
                cJSON_ArrayForEach(b_entry, act_entry) {
                    if (btn_count >= 48) break;
                    const char *btn_name = b_entry->string;
                    if (!btn_name || !cJSON_IsObject(b_entry)) continue;

                    cJSON *j_act = cJSON_GetObjectItemCaseSensitive(b_entry, "action");
                    uint8_t atype = 0x01;
                    if (j_act && cJSON_IsString(j_act)) {
                        if (strcmp(j_act->valuestring, "device_cmd") == 0) atype = 0x01;
                        else if (strcmp(j_act->valuestring, "activity_start") == 0) atype = 0x02;
                        else if (strcmp(j_act->valuestring, "activity_stop") == 0) atype = 0x03;
                    }

                    memset(payload, 0, sizeof(payload));
                    payload[0] = (uint8_t)btn_count;
                    payload[1] = 48;
                    payload[2] = 0x00; /* per-activity */
                    strncpy((char *)&payload[3], ctx_id, 9);
                    strncpy((char *)&payload[12], btn_name, 12);
                    payload[24] = atype;

                    rf_build_frame(&f, target_slot, RF_MSG_BUTTON_LIST_CHUNK, ++g_tx_seq, RF_FLAG_ACK_REQ, payload, 25);
                    if (rf_send_frame_acked(&f, 500) < 0) {
                        rf_send_frame_acked(&f, 500);
                    }
                    btn_count++;
                    usleep(20000);
                }
            }
        }
        pthread_mutex_unlock(&g_map_lock);
    }

    /* 5. Settings push */
    memset(payload, 0, sizeof(payload));
    payload[0] = 4;
    payload[1] = 1;
    payload[2] = 1;
    payload[3] = 0;
    payload[4] = 15;
    rf_build_frame(&f, target_slot, RF_MSG_SETTINGS_PUSH, ++g_tx_seq, RF_FLAG_ACK_REQ, payload, 5);
    if (rf_send_frame_acked(&f, 500) < 0) {
        rf_send_frame_acked(&f, 500);
    }
    usleep(20000);

    /* 6. CONFIG_END (complete) */
    memset(payload, 0, sizeof(payload));
    payload[0] = 0x01; /* complete */
    payload[1] = config_ver;
    rf_build_frame(&f, target_slot, RF_MSG_CONFIG_END, ++g_tx_seq, RF_FLAG_ACK_REQ, payload, 2);
    rf_send_frame_acked(&f, 500);
    rf_send_frame_acked(&f, 500);

    printf("[+] Config push complete (acts=%d devs=%d btns=%d ver=%d)\n",
           act_count, dev_count, btn_count, config_ver);

    pthread_mutex_unlock(&push_lock);
    return NULL;
}

/* Handle incoming Codex RF protocol frame */
static void handle_codex_frame(const rf_frame_t *f) {
    switch (f->msg_type) {
        case RF_MSG_ACK: {
            pthread_mutex_lock(&g_ack_mutex);
            if (f->payload[0] == g_pending_ack_seq) {
                g_ack_received = 1;
                g_ack_status = f->payload[1];
                pthread_cond_signal(&g_ack_cond);
            }
            pthread_mutex_unlock(&g_ack_mutex);
            break;
        }

        case RF_MSG_BUTTON_PRESS: {
            uint16_t key_code = ((uint16_t)f->payload[0] << 8) | f->payload[1];
            uint8_t state = f->payload[2];
            char btn_name[13] = {0};
            memcpy(btn_name, &f->payload[3], 12);

            const char *action = (state == 0x01) ? "press" : ((state == 0x02) ? "hold" : "release");
            const char *name = map_button_code(key_code >> 8, key_code & 0xFF);
            if (!name) name = btn_name[0] ? btn_name : "Unknown";

            printf("[*] Codex BUTTON_PRESS: code=0x%04X state=%s name=%s\n", key_code, action, name);
            enqueue_elite_button(name, action);

            char cur[32] = "-1";
            get_current_activity(cur, sizeof(cur));
            rf_send_codex_activity_sync(cur);
            break;
        }

        case RF_MSG_ACTIVITY_REQUEST: {
            char act_id[13] = {0};
            memcpy(act_id, f->payload, 12);
            printf("[*] Codex ACTIVITY_REQUEST: %s\n", act_id);
            dispatch_activity_to_daemon(act_id);
            break;
        }

        case RF_MSG_DEVICE_CMD_REQUEST: {
            char dev_id[13] = {0};
            char cmd[17] = {0};
            memcpy(dev_id, f->payload, 12);
            memcpy(cmd, &f->payload[12], 16);
            printf("[*] Codex DEVICE_CMD: dev=%s cmd=%s\n", dev_id, cmd);

            char body[256];
            snprintf(body, sizeof(body), "{\"deviceId\":\"%s\",\"command\":\"%s\"}", dev_id, cmd);
            http_post_local(WEBUI_PORT, "/api/ir-send", body);
            break;
        }

        case RF_MSG_SYNC_REQUEST: {
            uint8_t req_type = f->payload[0];
            printf("[*] Codex SYNC_REQUEST: type=%d seq=%d\n", req_type, f->seq);

            char cur[32] = "-1";
            get_current_activity(cur, sizeof(cur));
            rf_send_codex_activity_sync_seq(cur, f->seq);

            if (req_type == 0x02) {
                usleep(50000);
                pthread_t push_th;
                pthread_create(&push_th, NULL, config_push_worker, NULL);
                pthread_detach(push_th);
            }
            break;
        }

        case RF_MSG_PING: {
            rf_frame_t ack;
            rf_build_ack(&ack, g_remote_slot ? g_remote_slot : 0x01, f->seq, 0x00);
            rf_send_frame(&ack);
            break;
        }

        default:
            printf("[*] Codex RF unknown type 0x%02X\n", f->msg_type);
            break;
    }
}

/* Parse incoming packet from CC2544 */
static void handle_rf_packet(const uint8_t *pkt, size_t len) {
    if (len < 7) return;

    if (pkt[1] > 0 && pkt[1] < 0x10) {
        g_remote_slot = pkt[1];
    }

    if (g_debug_logging) {
        printf("[*] RX (len=%zd): ", len);
        for (size_t i = 0; i < len; i++) printf("%02X ", pkt[i]);
        printf("\n");
    }

    /* Route Codex RF protocol frames */
    if (rf_is_valid_frame(pkt, len)) {
        rf_frame_t f;
        memset(&f, 0, sizeof(f));
        if (pkt[0] == 0x20 && len >= 32) {
            if ((pkt[4] & 0xE0) == 0x40 || (pkt[4] & 0xE0) == 0x60) {
                f.report_id = 0x20;
                f.dev_slot = pkt[1];
                f.sub_id = 0x12;
                f.seq = pkt[3];
                f.hdr_flag = pkt[4];
                f.msg_type = pkt[5];
                f.flags = pkt[6];
                memcpy(f.payload, &pkt[7], sizeof(f.payload));
            } else {
                memcpy(&f, pkt, sizeof(f));
            }
        } else if (pkt[0] == 0x12 && len >= 30) {
            if ((pkt[2] & 0xE0) == 0x40 || (pkt[2] & 0xE0) == 0x60) {
                f.report_id = 0x20;
                f.dev_slot = g_remote_slot ? g_remote_slot : 0x01;
                f.sub_id = 0x12;
                f.seq = pkt[1];
                f.hdr_flag = pkt[2];
                f.msg_type = pkt[3];
                f.flags = pkt[4];
                memcpy(f.payload, &pkt[5], sizeof(f.payload));
            } else {
                f.report_id = 0x20;
                f.dev_slot = g_remote_slot ? g_remote_slot : 0x01;
                f.sub_id = 0x12;
                f.seq = pkt[1];
                f.hdr_flag = 0x40;
                f.msg_type = pkt[2];
                f.flags = pkt[3];
                memcpy(f.payload, &pkt[4], sizeof(f.payload));
            }
        }
        handle_codex_frame(&f);
        return;
    }

    /* 0x42: CC2544 hardware TX completion report: 20 <slot> 42 <status> ... */
    if (len >= 4 && pkt[0] == 0x20 && pkt[2] == 0x42) {
        uint8_t status = pkt[3];
        if (status == 0x00) {
            pthread_mutex_lock(&g_ack_mutex);
            g_ack_received = 1;
            pthread_cond_signal(&g_ack_cond);
            pthread_mutex_unlock(&g_ack_mutex);
            if (g_debug_logging)
                printf("[+] CC2544 Hardware ACK (0x42) for seq=%d\n", g_pending_ack_seq);
        } else if (status != 0x01) {
            if (g_debug_logging)
                printf("[-] CC2544 Hardware TX status=0x%02X\n", status);
        }
        return;
    }

    /* 0x41: Link status notification: 10 <slot> 41 04 <status> ... */
    if (len == 7 && pkt[0] == 0x10 && pkt[2] == 0x41) {
        bool link_lost = (pkt[4] & 0x40) != 0;
        if (g_debug_logging)
            printf("[*] Hub RF Link Status: %s (slot=%d byte4=0x%02X)\n",
                   link_lost ? "LOST" : "ESTABLISHED", pkt[1], pkt[4]);
        return;
    }

    /* Reset NAK backoff on any valid non-error response from CC2544 */
    if (!(pkt[0] == 0x10 && len >= 3 && pkt[2] == 0x8F)) {
        if (g_nak_count > 0 && g_debug_logging)
            printf("[+] NAK backoff cleared (was %d NAKs, %dms)\n", g_nak_count, g_backoff_ms);
        g_nak_count = 0;
        g_backoff_ms = 0;
    }

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

        /* 0x8F: Error / NAK — trigger exponential backoff */
        if (sub_id == 0x8F) {
            g_nak_count++;
            if (g_nak_count > 3) {
                g_backoff_ms = 100 * (1 << (g_nak_count - 3));
                if (g_backoff_ms > NAK_BACKOFF_MAX_MS) g_backoff_ms = NAK_BACKOFF_MAX_MS;
            }
            if (g_debug_logging || g_nak_count <= 5)
                printf("[-] CC2544 NAK: SubID=0x%02X err=0x%02X (count=%d, backoff=%dms)\n",
                       pkt[3], pkt[5], g_nak_count, g_backoff_ms);
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
            printf("[+] Paired Remote RF Address: %02X %02X %02X %02X (paired=%d, slot=%02X)\n",
                   g_paired_addr[0], g_paired_addr[1], g_paired_addr[2], g_paired_addr[3],
                   g_has_paired_device ? 1 : 0, g_remote_slot);
            return;
        }
    }

    /* Generic packet log */
    printf("[*] RX (%zd bytes): ", len);
    for (size_t i = 0; i < len; i++) printf("%02X ", pkt[i]);
    printf("\n");
}

/* Background worker: syncs Hub current_activity to Remote on change */
static void *activity_sync_worker(void *arg) {
    (void)arg;
    char last_act[32] = "";

    /* Wait for pairing response from CC2544 */
    for (int i = 0; i < 20 && g_running && !g_has_paired_device; i++) {
        usleep(100000);
    }

    /* Seed last_act on startup to prevent unsolicited TX into empty air */
    get_current_activity(last_act, sizeof(last_act));

    while (g_running) {
        char cur_act[32] = "-1";
        get_current_activity(cur_act, sizeof(cur_act));
        if (strcmp(cur_act, last_act) != 0) {
            printf("[*] Hub Activity changed: %s -> %s, syncing to Remote\n", last_act, cur_act);
            strncpy(last_act, cur_act, sizeof(last_act) - 1);
            last_act[sizeof(last_act) - 1] = '\0';
            rf_send_codex_activity_sync(cur_act);
        }
        sleep(2);
    }
    return NULL;
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
    if (g_pairing_mode) {
        uint8_t close_lock[7] = {0x10, 0xFF, 0x80, 0xB2, 0x02, 0x00, 0x00};
        printf("[*] Closing pairing lock (10 FF 80 B2 02 00 00)...\n");
        rf_write_cmd(close_lock, sizeof(close_lock));
    }
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
            } else if (strstr(buf, "GET /api/rf/send?hex=") == buf) {
                char *hex = strstr(buf, "hex=") + 4;
                char *end = strchr(hex, ' ');
                if (end) *end = '\0';
                uint8_t raw[64] = {0};
                size_t raw_len = 0;
                while (*hex && *(hex + 1) && raw_len < sizeof(raw)) {
                    unsigned int byte_val = 0;
                    char hex_byte[3] = { hex[0], hex[1], '\0' };
                    if (sscanf(hex_byte, "%02x", &byte_val) == 1 || sscanf(hex_byte, "%02X", &byte_val) == 1) {
                        raw[raw_len++] = (uint8_t)byte_val;
                        hex += 2;
                    } else break;
                }
                if (raw_len > 0) {
                    printf("[*] API RF TX (%zu bytes): ", raw_len);
                    for (size_t i = 0; i < raw_len; i++) printf("%02X ", raw[i]);
                    printf("\n");
                    rf_write_cmd(raw, raw_len);
                }
                const char *res = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n{\"status\":\"sent\"}";
                write(cli, res, strlen(res));
            } else if (strstr(buf, "GET /api/rf/push-config") == buf || strstr(buf, "POST /api/rf/push-config") == buf) {
                pthread_t push_th;
                pthread_create(&push_th, NULL, config_push_worker, NULL);
                pthread_detach(push_th);
                const char *res = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n{\"status\":\"push_started\"}";
                write(cli, res, strlen(res));
            } else if (strstr(buf, "GET /api/rf/sync") == buf) {
                char cur[32] = "-1";
                get_current_activity(cur, sizeof(cur));
                rf_send_codex_activity_sync(cur);
                const char *res = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n{\"status\":\"synced\"}";
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

    /* Load debug logging flag from persistent config */
    if (access("/data/codex/debug_logging.conf", F_OK) == 0) g_debug_logging = 1;

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

    /* Enable extended eQuad frames (matches hal_hub libhal_get_paired_device_info) */
    uint8_t pkt_ext[7] = {0x10, 0xFF, 0x80, 0xFD, 0x01, 0x00, 0x00};
    rf_write_cmd(pkt_ext, sizeof(pkt_ext));
    usleep(30000);

    /* Set CC2544 powermode to 0x30 (active communication) */
    uint8_t pkt_power[7] = {0x20, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00};
    rf_write_cmd(pkt_power, sizeof(pkt_power));
    usleep(30000);

    /* 2. Start background RX and Dispatch threads */
    pthread_t rx_th;
    if (pthread_create(&rx_th, NULL, rx_worker, NULL) != 0) {
        perror("pthread_create rx_worker");
        close(g_rf_fd);
        return 1;
    }

    pthread_t disp_th;
    pthread_create(&disp_th, NULL, dispatch_worker, NULL);

    pthread_t sync_th;
    pthread_create(&sync_th, NULL, activity_sync_worker, NULL);

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
    }

    printf("[+] codex_rf ready and operational.\n");

    /* Main loop wait */
    while (g_running) {
        sleep(1);
    }

    printf("[+] Shutting down codex_rf...\n");
    close(g_rf_fd);
    g_rf_fd = -1;
    pthread_cond_broadcast(&g_rf_q_cond);
    pthread_join(rx_th, NULL);
    pthread_join(disp_th, NULL);
    pthread_join(sync_th, NULL);
    return 0;
}
