#define _GNU_SOURCE
#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "cJSON.h"
#include "codex_webui_types.h"
#include "resource_cache.h"
#include "hw_action.h"
#include "ir_i2s.h"

#include "webui_utils.h"
#include "webui_ir.h"
#include "webui_activity.h"
#include "webui_bt.h"
#include "webui_update.h"
#include "webui_config.h"
#include "webui_html.h"
#include "webui_server.h"
#include "mqtt_client.h"

#include "codex_webui_html.c"

/* Async IR capture state */
static int s_cap_client_fd = -1;
static int s_cap_i2s_fd = -1;
static int s_cap_lock_fd = -1;
static uint8_t *s_cap_raw_buf = NULL;
static size_t s_cap_raw_len = 0;
static const size_t s_cap_buf_cap = 64 * 1024;
static uint64_t s_cap_start_ms = 0;
static uint64_t s_cap_act_ms = 0;
static int s_cap_active = 0;

static uint64_t cap_get_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

int webui_capture_is_active(void) {
    return (s_cap_client_fd >= 0 && s_cap_i2s_fd >= 0);
}

int webui_capture_fd(void) {
    return s_cap_i2s_fd;
}

static void webui_capture_finish(int timed_out) {
    if (s_cap_i2s_fd >= 0) {
        close(s_cap_i2s_fd);
        s_cap_i2s_fd = -1;
    }
    if (s_cap_lock_fd >= 0) {
        flock(s_cap_lock_fd, LOCK_UN);
        close(s_cap_lock_fd);
        s_cap_lock_fd = -1;
    }

    if (s_cap_client_fd >= 0) {
        char reply[4096] = {0};
        char mode[16] = {0}, keycode[512] = {0}, nec[64] = {0}, summary[160] = {0};
        int protocol_id = 2;

        if (!timed_out && s_cap_raw_buf && s_cap_raw_len > 0) {
            int drc = ir_i2s_decode_capture(s_cap_raw_buf, s_cap_raw_len, reply, sizeof(reply));
            if (drc == 0 && reply[0]) {
                analyze_capture_storage(reply, "", "", "2", mode, sizeof(mode),
                                        keycode, sizeof(keycode), nec, sizeof(nec),
                                        &protocol_id, summary, sizeof(summary));
            }
        }

        cJSON *resp = cJSON_CreateObject();
        if (resp) {
            cJSON_AddBoolToObject(resp, "ok", (reply[0] != '\0') ? 1 : 0);
            cJSON_AddStringToObject(resp, "raw", reply);
            cJSON_AddStringToObject(resp, "mode", mode);
            cJSON_AddNumberToObject(resp, "protocolId", protocol_id);
            cJSON_AddStringToObject(resp, "keycode", keycode);
            cJSON_AddStringToObject(resp, "nec", nec);
            cJSON_AddStringToObject(resp, "analysis", summary);
            if (reply[0] == '\0') {
                cJSON_AddStringToObject(resp, "message", "No IR signal received (timeout)");
            }
            send_cjson_resp(s_cap_client_fd, "200 OK", resp);
            cJSON_Delete(resp);
        }
        close(s_cap_client_fd);
        s_cap_client_fd = -1;
    }

    if (s_cap_raw_buf) {
        free(s_cap_raw_buf);
        s_cap_raw_buf = NULL;
    }
    s_cap_raw_len = 0;
    s_cap_active = 0;

    /* Restore TX descriptor */
    ir_i2s_init();
}

static int webui_capture_start(int client_fd) {
    if (s_cap_client_fd >= 0) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "IR capture already in progress");
        send_cjson_resp(client_fd, "409 Conflict", err);
        cJSON_Delete(err);
        close(client_fd);
        return -1;
    }

    s_cap_lock_fd = open("/tmp/codex_i2s.lock", O_RDWR | O_CREAT, 0666);
    if (s_cap_lock_fd < 0 || flock(s_cap_lock_fd, LOCK_EX | LOCK_NB) != 0) {
        if (s_cap_lock_fd >= 0) close(s_cap_lock_fd);
        s_cap_lock_fd = -1;
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "I2S hardware is busy");
        send_cjson_resp(client_fd, "503 Service Unavailable", err);
        cJSON_Delete(err);
        close(client_fd);
        return -1;
    }

    ir_i2s_shutdown();

    s_cap_i2s_fd = open("/dev/i2s", O_RDONLY | O_NONBLOCK);
    if (s_cap_i2s_fd < 0) {
        flock(s_cap_lock_fd, LOCK_UN);
        close(s_cap_lock_fd);
        s_cap_lock_fd = -1;
        ir_i2s_init();
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "Failed to open /dev/i2s for RX");
        send_cjson_resp(client_fd, "500 Internal Server Error", err);
        cJSON_Delete(err);
        close(client_fd);
        return -1;
    }

    #ifndef I2S_FREQ_CAP
    #define I2S_FREQ_CAP 0x00258000
    #define I2S_VOLUME 0x80044e20
    #define I2S_FREQ   0x80044e21
    #define I2S_DSIZE  0x80044e22
    #define I2S_MODE   0x80044e23
    #define I2S_START  0x80044e30
    #endif

    ioctl(s_cap_i2s_fd, I2S_DSIZE, 16);
    ioctl(s_cap_i2s_fd, I2S_MODE, 2);
    ioctl(s_cap_i2s_fd, I2S_VOLUME, 15);
    ioctl(s_cap_i2s_fd, I2S_FREQ, I2S_FREQ_CAP);
    ioctl(s_cap_i2s_fd, I2S_START, 0);

    s_cap_raw_buf = (uint8_t *)malloc(s_cap_buf_cap);
    if (!s_cap_raw_buf) {
        close(s_cap_i2s_fd);
        s_cap_i2s_fd = -1;
        flock(s_cap_lock_fd, LOCK_UN);
        close(s_cap_lock_fd);
        s_cap_lock_fd = -1;
        ir_i2s_init();
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "Out of memory");
        send_cjson_resp(client_fd, "500 Internal Server Error", err);
        cJSON_Delete(err);
        close(client_fd);
        return -1;
    }

    s_cap_raw_len = 0;
    s_cap_active = 0;
    s_cap_start_ms = cap_get_now_ms();
    s_cap_act_ms = 0;
    s_cap_client_fd = client_fd;
    return 0;
}

void webui_capture_tick(int is_readable) {
    if (!webui_capture_is_active()) return;

    uint64_t now = cap_get_now_ms();

    if (is_readable && s_cap_i2s_fd >= 0) {
        uint8_t chunk[192];
        ssize_t n = read(s_cap_i2s_fd, chunk, sizeof(chunk));
        if (n > 0) {
            for (ssize_t i = 0; i < n; i++) {
                chunk[i] = (uint8_t)~chunk[i];
            }
            int chunk_has_act = 0;
            for (ssize_t i = 0; i < n; i++) {
                if (chunk[i] != 0x00) { chunk_has_act = 1; break; }
            }
            if (!s_cap_active && chunk_has_act) {
                s_cap_active = 1;
                s_cap_act_ms = now;
            }
            if (s_cap_active) {
                if (s_cap_raw_len + (size_t)n <= s_cap_buf_cap) {
                    memcpy(s_cap_raw_buf + s_cap_raw_len, chunk, (size_t)n);
                    s_cap_raw_len += (size_t)n;
                }
            }
        }
    }

    if (!s_cap_active && (now - s_cap_start_ms) >= 5000) {
        webui_capture_finish(1);
    } else if (s_cap_active) {
        if ((now - s_cap_act_ms) >= 300 || s_cap_raw_len >= s_cap_buf_cap - 192) {
            webui_capture_finish(0);
        }
    }
}

static void serve_static_asset(int client, const char *path, const char *mime) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "Asset not found");
        send_cjson_resp(client, "404 Not Found", err);
        cJSON_Delete(err);
        return;
    }
    struct stat st;
    if (fstat(fd, &st) != 0) {
        close(fd);
        return;
    }
    char hdr[256];
    int hlen = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "Cache-Control: public, max-age=86400\r\n"
        "Connection: close\r\n\r\n",
        mime, (long)st.st_size);
    write(client, hdr, hlen);
    char buf[4096];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        write(client, buf, n);
    }
    close(fd);
}

static void handle_client(int client) {
    struct request req;
    if (read_request(client, &req) != 0) {
        free_request(&req);
        return;
    }
    if (strcmp(req.method, "OPTIONS") == 0) {
        const char *resp = "HTTP/1.1 204 No Content\r\n"
                           "Access-Control-Allow-Origin: *\r\n"
                           "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                           "Access-Control-Allow-Headers: Authorization, Content-Type\r\n"
                           "Access-Control-Max-Age: 86400\r\n"
                           "Content-Length: 0\r\n"
                           "Connection: close\r\n\r\n";
        send(client, resp, strlen(resp), 0);
        free_request(&req);
        return;
    }
    if (!webui_auth_ok(&req)) {
        send_auth_required(client);
        free_request(&req);
        return;
    }
    if (req.body_truncated) {
        send_payload_too_large(client, &req);
        free_request(&req);
        return;
    }
    if (strcmp(req.method, "GET") == 0 && (strcmp(req.path, "/") == 0 || strcmp(req.path, "/index.html") == 0)) {
        render_page(client, &req, "");
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/api/inventory") == 0) {
        render_inventory_json(client);
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/api/activities") == 0) {
        render_activities_json(client);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/activity-start") == 0) {
        render_activity_start_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/activity-stop") == 0) {
        render_activity_stop_json(client);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/activity-save") == 0) {
        render_activity_save_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/activity-delete") == 0) {
        render_activity_delete_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/activity-test-sequence") == 0) {
        render_activity_test_sequence_json(client, &req);
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/export/activities") == 0) {
        send_file_download(client, ACTIVITY_LIST, "ActivityList.json", "application/json");
    } else if ((strcmp(req.method, "GET") == 0 || strcmp(req.method, "POST") == 0) &&
        strncmp(req.path, "/api/device-commands", 20) == 0 &&
        (req.path[20] == 0 || req.path[20] == '?')) {
        render_device_commands_json(client, &req);
    } else if ((strcmp(req.method, "GET") == 0 || strcmp(req.method, "POST") == 0) &&
        strncmp(req.path, "/api/remotecentral-fetch", 24) == 0 &&
        (req.path[24] == 0 || req.path[24] == '?')) {
        render_remotecentral_fetch_json(client, &req);
    } else if ((strcmp(req.method, "GET") == 0 || strcmp(req.method, "POST") == 0) && strcmp(req.path, "/api/capture") == 0) {
        if (webui_capture_start(client) == 0) {
            free_request(&req);
            return; /* Asynchronous capture started - connection kept open */
        }
        free_request(&req);
        return;
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/ir-send") == 0) {
        render_ir_send_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/ir-test-learned") == 0) {
        render_ir_test_learned_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/ir-batch-send") == 0) {
        render_ir_batch_send_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/ir-cancel") == 0) {
        render_ir_cancel_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/ir-lab-target") == 0) {
        render_ir_lab_target_json(client);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/ir-lab-clear") == 0) {
        render_ir_lab_clear_json(client, &req);
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/api/bt-status") == 0) {
        render_bt_status_json(client);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/bt-key") == 0) {
        render_bt_key_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/bt-pairing") == 0) {
        render_bt_pairing_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/bt-connect") == 0) {
        render_bt_connect_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/bt-disconnect") == 0) {
        render_bt_disconnect_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/bt-link-device") == 0) {
        render_bt_link_device_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/bt-script") == 0) {
        render_bt_script_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/bt-call") == 0) {
        render_bluetooth_call_json(client, &req);
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/api/bt-text-status") == 0) {
        render_bluetooth_text_status_json(client);
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/api/bt-sent-log") == 0) {
        render_bt_sent_log_json(client);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/bt-text") == 0) {
        render_bluetooth_text_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/bt-saved-command") == 0) {
        render_bt_saved_command_json(client, &req);
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/api/remote-mapping") == 0) {
        render_remote_mapping_json(client);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/remote-mapping-save") == 0) {
        render_remote_mapping_save_json(client, &req);
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/api/elite-mapping") == 0) {
        render_elite_mapping_json(client);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/elite-mapping-save") == 0) {
        render_elite_mapping_save_json(client, &req);
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/assets/remote_elite_skin.jpg") == 0) {
        serve_static_asset(client, "/data/codex/assets/remote_elite_skin.jpg", "image/jpeg");
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/api/ui-remote-layout") == 0) {
        render_web_remote_layout_json(client);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/ui-remote-layout") == 0) {
        render_web_remote_layout_save_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/remote-scan") == 0) {
        render_remote_scan_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/remote-pair") == 0) {
        render_remote_pair_json(client, &req);
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/api/remote-pair-status") == 0) {
        render_remote_pair_status_json(client);
    } else if (strncmp(req.path, "/api/rf", 7) == 0) {
        int rfsock = socket(AF_INET, SOCK_STREAM, 0);
        if (rfsock >= 0) {
            struct sockaddr_in sin;
            memset(&sin, 0, sizeof(sin));
            sin.sin_family = AF_INET;
            sin.sin_port = htons(8092);
            sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

            struct timeval tv = { .tv_sec = 2, .tv_usec = 0 };
            setsockopt(rfsock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(rfsock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

            if (connect(rfsock, (struct sockaddr *)&sin, sizeof(sin)) == 0) {
                char proxy_req[512];
                int prlen = snprintf(proxy_req, sizeof(proxy_req),
                    "%s %s HTTP/1.1\r\nHost: 127.0.0.1:8092\r\nConnection: close\r\n\r\n",
                    req.method, req.path);
                write(rfsock, proxy_req, prlen);

                char resp_buf[1024];
                ssize_t rn;
                while ((rn = read(rfsock, resp_buf, sizeof(resp_buf))) > 0) {
                    write(client, resp_buf, rn);
                }
                close(rfsock);
                close(client);
                return;
            }
            close(rfsock);
        }
        cJSON *err = cJSON_CreateObject();
        cJSON_AddStringToObject(err, "error", "codex_rf daemon unavailable on port 8092");
        send_cjson_resp(client, "502 Bad Gateway", err);
        cJSON_Delete(err);

    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/api/disk-space") == 0) {
        char diskspace[1024] = {0};
        run_cmd("/data/codex/bin/check_space 2>&1 || check_space 2>&1", diskspace, sizeof(diskspace));
        cJSON *res = cJSON_CreateObject();
        cJSON_AddBoolToObject(res, "ok", 1);
        cJSON_AddStringToObject(res, "output", diskspace);
        send_cjson_resp(client, "200 OK", res);
        cJSON_Delete(res);
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/api/update-status") == 0) {
        render_update_status_json(client);
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/api/update-check-state") == 0) {
        render_update_check_state_json(client);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/update-check-state") == 0) {
        render_update_check_state_post_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/update-begin") == 0) {
        render_update_begin_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/update-chunk") == 0) {
        render_update_chunk_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/update-apply") == 0) {
        render_update_apply_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/irdb-import") == 0) {
        render_irdb_import_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/device-mqtt") == 0) {
        render_device_mqtt_save_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/device-mqtt-test") == 0) {
        render_device_mqtt_test_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/device-save") == 0) {
        render_device_save_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/device-power-save") == 0) {
        render_device_power_save_json(client, &req);
    } else if ((strcmp(req.method, "GET") == 0 || strcmp(req.method, "POST") == 0) && strcmp(req.path, "/export/bundle") == 0) {
        send_bundle_download(client);
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/export/devices") == 0) {
        send_file_download(client, DEVICE_LIST, "DeviceList.json", "application/json");
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/export/functions") == 0) {
        send_file_download(client, FUNCTION_LIST, "FunctionList.json", "application/json");
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/export/protocols") == 0) {
        send_file_download(client, PROTOCOL_LIST, "ProtocolList.json", "application/json");
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/export/mqtt") == 0) {
        send_file_download(client, MQTT_CONFIG, "mqtt-config.json", "application/json");
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/export/wifi") == 0) {
        send_file_download(client, WPA_CONFIG, "wpa_supplicant.conf", "text/plain");
    } else if (strcmp(req.method, "GET") == 0 && (strcmp(req.path, "/export/ethernet") == 0 || strcmp(req.path, "/export/network") == 0)) {
        send_file_download(client, ETHERNET_CONFIG, "ethernet.conf", "text/plain");
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/export/bluetooth") == 0) {
        send_bt_devices_download(client);
    } else if (strcmp(req.method, "GET") == 0 && (strcmp(req.path, "/export/remote-mapping") == 0 || strcmp(req.path, "/export/remotemap") == 0)) {
        send_remote_mapping_download(client);
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/export/auth") == 0) {
        send_file_download(client, WEBUI_AUTH_CONFIG, "webui_auth.conf", "text/plain");
    } else if (strcmp(req.method, "GET") == 0 && (strcmp(req.path, "/export/hub-id") == 0 || strcmp(req.path, "/export/hub_id") == 0)) {
        send_file_download(client, HUB_ID_FILE, "hub_id", "text/plain");
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/activity/start") == 0) {
        handle_activity_start(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/activity/stop") == 0) {
        handle_activity_stop(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/activity/save") == 0) {
        handle_activity_save(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/activity/delete") == 0) {
        handle_activity_delete(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/mqtt") == 0) {
        handle_mqtt(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/wifi") == 0) {
        handle_wifi(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && (strcmp(req.path, "/ethernet") == 0 || strcmp(req.path, "/network") == 0)) {
        handle_ethernet(client, &req);
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/api/network-status") == 0) {
        struct network_status st;
        get_network_status(&st);
        cJSON *res = cJSON_CreateObject();
        cJSON_AddBoolToObject(res, "ok", 1);
        cJSON_AddStringToObject(res, "activeInterface", st.active_interface);
        cJSON_AddStringToObject(res, "connectionType", st.connection_type);
        cJSON_AddStringToObject(res, "ip", st.ip);
        cJSON_AddStringToObject(res, "netmask", st.netmask);
        cJSON_AddStringToObject(res, "gateway", st.gateway);
        cJSON_AddStringToObject(res, "mac", st.mac);
        cJSON_AddBoolToObject(res, "ethPresent", st.eth_present);
        cJSON_AddBoolToObject(res, "ethCarrier", st.eth_carrier);
        cJSON_AddStringToObject(res, "ethIfname", st.eth_ifname);
        cJSON_AddStringToObject(res, "ethIp", st.eth_ip);
        cJSON_AddBoolToObject(res, "wifiConnected", st.wifi_connected);
        cJSON_AddStringToObject(res, "wifiSsid", st.wifi_ssid);
        cJSON_AddStringToObject(res, "wifiIp", st.wifi_ip);
        cJSON_AddBoolToObject(res, "usbHostMode", st.usb_host_mode);
        cJSON_AddBoolToObject(res, "usbPcConnected", st.usb_pc_connected);
        send_cjson_resp(client, "200 OK", res);
        cJSON_Delete(res);
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/api/mqtt-status") == 0) {
        struct mqtt_config cfg;
        load_mqtt(&cfg);
        int connected = mqtt_is_connected() || tcp_established(cfg.host, cfg.port);
        cJSON *res = cJSON_CreateObject();
        cJSON_AddBoolToObject(res, "ok", 1);
        cJSON_AddBoolToObject(res, "connected", connected);
        cJSON_AddBoolToObject(res, "enabled", cfg.enabled);
        cJSON_AddStringToObject(res, "host", cfg.host);
        cJSON_AddNumberToObject(res, "port", cfg.port);
        send_cjson_resp(client, "200 OK", res);
        cJSON_Delete(res);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/system") == 0) {
        handle_system(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/import") == 0) {
        handle_import(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/ir/send") == 0) {
        handle_ir_send(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/ir/device") == 0) {
        handle_ir_device(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/ir/device-power") == 0) {
        handle_ir_device_power(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/ir/device-mqtt") == 0) {
        handle_ir_device_mqtt(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/ir/new-device") == 0) {
        handle_ir_new_device(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/ir/command") == 0) {
        handle_ir_command(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/ir/update-command") == 0) {
        handle_ir_update_command(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/ir/irdb-import") == 0) {
        handle_irdb_import(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/ir/capture") == 0) {
        handle_ir_capture(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/ir/delete-device") == 0) {
        handle_ir_delete_device(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/ir/delete-command") == 0) {
        handle_ir_delete_command(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/bt/device") == 0) {
        handle_bt_device(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/bt/delete-device") == 0) {
        handle_bt_delete_device(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/bt/command") == 0) {
        handle_bt_command(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/bt/delete-command") == 0) {
        handle_bt_delete_command(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/bt/send-command") == 0) {
        handle_bt_send_command(client, &req);
    } else {
        send_text(client, "404 Not Found", "not found\n");
    }
    free_request(&req);
}

int webui_server_init(int port) {
    if (port <= 0) port = 8080;
    activity_startup_cleanup();
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("webui socket");
        return -1;
    }
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        perror("webui bind");
        close(fd);
        return -1;
    }
    if (listen(fd, 32) != 0) {
        perror("webui listen");
        close(fd);
        return -1;
    }
    return fd;
}

void webui_server_close(int fd) {
    if (fd >= 0) close(fd);
}

void webui_handle_client(int client) {
    struct timeval tv = { .tv_sec = 3, .tv_usec = 0 };
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    handle_client(client);
    if (client != s_cap_client_fd) {
        close(client);
    }
}
