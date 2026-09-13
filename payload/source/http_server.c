#include "http_server.h"
#include "ws_server.h"
#include "orchestrator.h"
#include "hw_action.h"
#include "mqtt_client.h"
#include "cJSON.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>

#define HTTP_REQ_BUF_SIZE 4096

#define MAX_PENDING_PULSES 8
struct pending_pulse {
    char topic[256];
    uint64_t off_deadline_ms;
    int active;
};
static struct pending_pulse g_pulses[MAX_PENDING_PULSES];

void pulse_tick(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t now = (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
    for (int i = 0; i < MAX_PENDING_PULSES; i++) {
        if (g_pulses[i].active && now >= g_pulses[i].off_deadline_ms) {
            mqtt_publish(g_pulses[i].topic, "0", 0);
            g_pulses[i].active = 0;
        }
    }
}

int http_server_init(int port) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return -1;

    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port > 0 ? port : HTTP_SERVER_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(s);
        return -1;
    }

    if (listen(s, HTTP_MAX_CLIENTS) != 0) {
        close(s);
        return -1;
    }

    return s;
}

void http_server_close(int server_fd) {
    if (server_fd >= 0) close(server_fd);
}

static void send_cjson_resp(int fd, int code, cJSON *obj) {
    const char *status_str = (code == 200) ? "OK" : (code == 400 ? "Bad Request" : (code == 404 ? "Not Found" : (code == 409 ? "Conflict" : (code == 503 ? "Service Unavailable" : "Internal Server Error"))));
    char *json = obj ? cJSON_PrintUnformatted(obj) : NULL;
    size_t body_len = json ? strlen(json) : 0;
    char header[256];
    int hlen = snprintf(header, sizeof(header),
                       "HTTP/1.1 %d %s\r\n"
                       "Content-Type: application/json; charset=utf-8\r\n"
                       "Content-Length: %zu\r\n"
                       "Access-Control-Allow-Origin: *\r\n"
                       "Connection: close\r\n\r\n",
                       code, status_str, body_len);
    send(fd, header, (size_t)hlen, 0);
    if (body_len && json) send(fd, json, body_len, 0);
    if (json) free(json);
}

int mqtt_dispatch_button_pulse(const char *dev_id, const char *dev_name_in, const char *cmd_name,
                               int req_pulse_ms, char *out_topic, size_t out_topic_len, int *out_pulse_ms) {
    if (!cmd_name || !cmd_name[0]) return -1;

    char dev_name[128] = {0};
    if (dev_name_in && dev_name_in[0]) {
        strncpy(dev_name, dev_name_in, sizeof(dev_name) - 1);
    } else if (dev_id && dev_id[0]) {
        strncpy(dev_name, dev_id, sizeof(dev_name) - 1);
    } else {
        strcpy(dev_name, "device");
    }

    char base_topic[128] = "harmony/hub";
    char topic_pattern[256] = "{root}/button/{device}/{command}";
    int pulse_ms = 1000;

    FILE *f = fopen("/data/codexmqtt/config.json", "r");
    if (f) {
        fseek(f, 0, SEEK_END);
        long fsz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (fsz > 0 && fsz < 65536) {
            char *mcfg = malloc(fsz + 1);
            if (mcfg) {
                size_t nr = fread(mcfg, 1, fsz, f);
                mcfg[nr] = '\0';
                cJSON *cfg_j = cJSON_Parse(mcfg);
                free(mcfg);
                if (cfg_j) {
                    cJSON *bt = cJSON_GetObjectItemCaseSensitive(cfg_j, "baseTopic");
                    if (bt && cJSON_IsString(bt) && bt->valuestring && bt->valuestring[0]) {
                        strncpy(base_topic, bt->valuestring, sizeof(base_topic) - 1);
                    }
                    cJSON *def_p = cJSON_GetObjectItemCaseSensitive(cfg_j, "defaultPulseMs");
                    if (!def_p) def_p = cJSON_GetObjectItemCaseSensitive(cfg_j, "pulseMs");
                    if (def_p && cJSON_IsNumber(def_p) && def_p->valueint > 0) {
                        pulse_ms = def_p->valueint;
                    }
                    cJSON *cdevs = cJSON_GetObjectItemCaseSensitive(cfg_j, "mqttDevices");
                    if (!cdevs) cdevs = cJSON_GetObjectItemCaseSensitive(cfg_j, "devices");
                    if (cdevs && cJSON_IsObject(cdevs)) {
                        cJSON *elem = cJSON_GetObjectItemCaseSensitive(cdevs, (dev_id && dev_id[0]) ? dev_id : dev_name);
                        if (!elem && dev_name[0]) elem = cJSON_GetObjectItemCaseSensitive(cdevs, dev_name);
                        if (elem && cJSON_IsObject(elem)) {
                            cJSON *top = cJSON_GetObjectItemCaseSensitive(elem, "topic");
                            if (top && cJSON_IsString(top) && top->valuestring && top->valuestring[0]) {
                                strncpy(topic_pattern, top->valuestring, sizeof(topic_pattern) - 1);
                            }
                            cJSON *p_item = cJSON_GetObjectItemCaseSensitive(elem, "pulseMs");
                            if (!p_item) p_item = cJSON_GetObjectItemCaseSensitive(elem, "pulse_ms");
                            if (p_item && cJSON_IsNumber(p_item) && p_item->valueint > 0) {
                                pulse_ms = p_item->valueint;
                            }
                        }
                    }
                    cJSON_Delete(cfg_j);
                }
            }
        }
        fclose(f);
    }

    if (req_pulse_ms > 0) {
        pulse_ms = req_pulse_ms;
    }

    char final_topic[256];
    char *t = topic_pattern;
    char temp[256];
    char *r_pos = strstr(t, "{root}");
    if (!r_pos) r_pos = strstr(t, "{base}");
    if (r_pos) {
        snprintf(temp, sizeof(temp), "%s%s", base_topic, r_pos + 6);
        t = temp;
    }
    char *d_pos = strstr(t, "{device}");
    if (d_pos) {
        char temp2[256];
        *d_pos = '\0';
        snprintf(temp2, sizeof(temp2), "%s%s%s", t, dev_name, d_pos + 8);
        strcpy(temp, temp2);
        t = temp;
    }
    char *c_pos = strstr(t, "{command}");
    int c_len = 9;
    if (!c_pos) {
        c_pos = strstr(t, "{button}");
        c_len = 8;
    }
    if (c_pos) {
        char temp2[256];
        *c_pos = '\0';
        snprintf(temp2, sizeof(temp2), "%s%s%s", t, cmd_name, c_pos + c_len);
        strcpy(final_topic, temp2);
    } else {
        strncpy(final_topic, t, sizeof(final_topic) - 1);
    }

    if (out_topic && out_topic_len > 0) {
        strncpy(out_topic, final_topic, out_topic_len - 1);
        out_topic[out_topic_len - 1] = '\0';
    }
    if (out_pulse_ms) {
        *out_pulse_ms = pulse_ms;
    }

    if (!mqtt_is_connected()) {
        return -1;
    }

    mqtt_publish(final_topic, "1", 0);
    int queued = 0;
    for (int i = 0; i < MAX_PENDING_PULSES; i++) {
        if (!g_pulses[i].active) {
            strncpy(g_pulses[i].topic, final_topic, sizeof(g_pulses[i].topic) - 1);
            g_pulses[i].topic[sizeof(g_pulses[i].topic) - 1] = '\0';
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            g_pulses[i].off_deadline_ms = (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000 + (pulse_ms > 0 ? pulse_ms : 50);
            g_pulses[i].active = 1;
            queued = 1;
            break;
        }
    }
    if (!queued) {
        mqtt_publish(final_topic, "0", 0);
    }
    return 0;
}

void http_handle_client(int client_fd) {
    if (client_fd < 0) return;

    char req[HTTP_REQ_BUF_SIZE] = {0};
    ssize_t n = recv(client_fd, req, sizeof(req) - 1, 0);
    if (n <= 0) {
        close(client_fd);
        return;
    }
    req[n] = '\0';

    /* Check if WebSocket upgrade */
    if (ws_is_upgrade_req(req)) {
        if (ws_handle_upgrade(client_fd, req) != 0) {
            close(client_fd);
        }
        return; /* Keep client_fd open for WebSocket frames */
    }

    char method[16] = {0}, path[128] = {0};
    sscanf(req, "%15s %127s", method, path);

    const char *body = strstr(req, "\r\n\r\n");
    if (body) body += 4;

    /* Route: GET /api/activity */
    if ((strcmp(path, "/api/activity") == 0 || strcmp(path, "/api/current_activity") == 0) &&
        strcmp(method, "GET") == 0) {
        cJSON *aj = cJSON_CreateObject();
        cJSON_AddStringToObject(aj, "activity", orch_get_current_activity());
        cJSON_AddNumberToObject(aj, "state", (int)orch_get_state());
        cJSON_AddBoolToObject(aj, "busy", orch_is_busy() ? 1 : 0);
        send_cjson_resp(client_fd, 200, aj);
        cJSON_Delete(aj);
        close(client_fd);
        return;
    }

    /* Route: POST /api/activity-progress */
    if (strcmp(path, "/api/activity-progress") == 0 && strcmp(method, "POST") == 0) {
        if (body && body[0]) {
            ws_broadcast_text(body);
            mqtt_publish("harmony/hub/activity/status", body, 1);
            extern void daemon_publish_mqtt_state(void);
            daemon_publish_mqtt_state();
        }
        cJSON *ok = cJSON_CreateObject();
        cJSON_AddBoolToObject(ok, "ok", 1);
        send_cjson_resp(client_fd, 200, ok);
        cJSON_Delete(ok);
        close(client_fd);
        return;
    }

    /* Route: POST /api/sync or POST /api/reload */
    if ((strcmp(path, "/api/sync") == 0 || strcmp(path, "/api/reload") == 0) && strcmp(method, "POST") == 0) {
        extern void daemon_sync_mqtt(void);
        daemon_sync_mqtt();
        cJSON *ok = cJSON_CreateObject();
        cJSON_AddBoolToObject(ok, "ok", 1);
        send_cjson_resp(client_fd, 200, ok);
        cJSON_Delete(ok);
        close(client_fd);
        return;
    }

    /* Route: POST /api/activity */
    if (strcmp(path, "/api/activity") == 0 && strcmp(method, "POST") == 0) {
        char act_id[32] = {0};
        int dry_run = 0;
        cJSON *req_j = body ? cJSON_Parse(body) : NULL;
        if (req_j) {
            cJSON *ja = cJSON_GetObjectItemCaseSensitive(req_j, "activity");
            if (ja && cJSON_IsString(ja) && ja->valuestring) strncpy(act_id, ja->valuestring, sizeof(act_id) - 1);
            else if (ja && cJSON_IsNumber(ja)) snprintf(act_id, sizeof(act_id), "%ld", (long)ja->valuedouble);

            cJSON *jd = cJSON_GetObjectItemCaseSensitive(req_j, "dry_run");
            if (jd) {
                if (cJSON_IsTrue(jd)) dry_run = 1;
                else if (cJSON_IsNumber(jd) && jd->valuedouble > 0) dry_run = 1;
                else if (cJSON_IsString(jd) && (strcmp(jd->valuestring, "true") == 0 || strcmp(jd->valuestring, "1") == 0)) dry_run = 1;
            }
            cJSON_Delete(req_j);
        }

        if (!act_id[0]) {
            cJSON *err = cJSON_CreateObject();
            cJSON_AddStringToObject(err, "error", "Missing activity parameter");
            send_cjson_resp(client_fd, 400, err);
            cJSON_Delete(err);
            close(client_fd);
            return;
        }

        if (dry_run) {
            char preview[1024] = {0};
            int rc = orch_switch_activity(act_id, 1, preview, sizeof(preview));
            if (rc == 0) {
                cJSON *pj = preview[0] ? cJSON_Parse(preview) : NULL;
                if (!pj) {
                    pj = cJSON_CreateObject();
                    cJSON_AddStringToObject(pj, "target", act_id);
                    cJSON_AddArrayToObject(pj, "steps");
                }
                send_cjson_resp(client_fd, 200, pj);
                cJSON_Delete(pj);
            } else {
                cJSON *err = cJSON_CreateObject();
                cJSON_AddStringToObject(err, "error", "Interlock busy");
                send_cjson_resp(client_fd, 409, err);
                cJSON_Delete(err);
            }
        } else {
            int rc = orch_switch_activity(act_id, 0, NULL, 0);
            if (rc == 0) {
                cJSON *suc = cJSON_CreateObject();
                cJSON_AddBoolToObject(suc, "success", 1);
                cJSON_AddStringToObject(suc, "state", "starting");
                send_cjson_resp(client_fd, 200, suc);
                cJSON_Delete(suc);
            } else {
                cJSON *err = cJSON_CreateObject();
                cJSON_AddStringToObject(err, "error", "Interlock busy");
                send_cjson_resp(client_fd, 409, err);
                cJSON_Delete(err);
            }
        }
        close(client_fd);
        return;
    }

    /* Route: POST /api/poweroff */
    if (strcmp(path, "/api/poweroff") == 0 && strcmp(method, "POST") == 0) {
        int dry_run = 0;
        cJSON *req_j = body ? cJSON_Parse(body) : NULL;
        if (req_j) {
            cJSON *jd = cJSON_GetObjectItemCaseSensitive(req_j, "dry_run");
            if (jd) {
                if (cJSON_IsTrue(jd)) dry_run = 1;
                else if (cJSON_IsNumber(jd) && jd->valuedouble > 0) dry_run = 1;
                else if (cJSON_IsString(jd) && (strcmp(jd->valuestring, "true") == 0 || strcmp(jd->valuestring, "1") == 0)) dry_run = 1;
            }
            cJSON_Delete(req_j);
        }

        if (dry_run) {
            char preview[1024] = {0};
            int rc = orch_power_off(1, preview, sizeof(preview));
            if (rc == 0) {
                cJSON *pj = preview[0] ? cJSON_Parse(preview) : NULL;
                if (!pj) {
                    pj = cJSON_CreateObject();
                    cJSON_AddStringToObject(pj, "target", ORCH_POWEROFF_ID);
                    cJSON_AddArrayToObject(pj, "steps");
                }
                send_cjson_resp(client_fd, 200, pj);
                cJSON_Delete(pj);
            } else {
                cJSON *err = cJSON_CreateObject();
                cJSON_AddStringToObject(err, "error", "busy");
                send_cjson_resp(client_fd, 409, err);
                cJSON_Delete(err);
            }
        } else {
            int rc = orch_power_off(0, NULL, 0);
            if (rc == 0) {
                cJSON *suc = cJSON_CreateObject();
                cJSON_AddBoolToObject(suc, "success", 1);
                cJSON_AddStringToObject(suc, "state", "stopping");
                send_cjson_resp(client_fd, 200, suc);
                cJSON_Delete(suc);
            } else {
                cJSON *err = cJSON_CreateObject();
                cJSON_AddStringToObject(err, "error", "busy");
                send_cjson_resp(client_fd, 409, err);
                cJSON_Delete(err);
            }
        }
        close(client_fd);
        return;
    }

    /* Route: POST /api/pronto-blast */
    if (strcmp(path, "/api/pronto-blast") == 0 && strcmp(method, "POST") == 0) {
        char pronto[512] = {0};
        uint8_t ports = IR_PORT_ALL;
        uint8_t repeats = 3;

        cJSON *req_j = body ? cJSON_Parse(body) : NULL;
        if (req_j) {
            cJSON *jp = cJSON_GetObjectItemCaseSensitive(req_j, "pronto");
            if (jp && cJSON_IsString(jp) && jp->valuestring) strncpy(pronto, jp->valuestring, sizeof(pronto) - 1);

            cJSON *jport = cJSON_GetObjectItemCaseSensitive(req_j, "ports");
            if (jport && cJSON_IsNumber(jport)) ports = (uint8_t)jport->valuedouble;
            else if (jport && cJSON_IsString(jport) && jport->valuestring) ports = (uint8_t)atoi(jport->valuestring);

            cJSON *jrep = cJSON_GetObjectItemCaseSensitive(req_j, "repeats");
            if (jrep && cJSON_IsNumber(jrep)) repeats = (uint8_t)jrep->valuedouble;
            else if (jrep && cJSON_IsString(jrep) && jrep->valuestring) repeats = (uint8_t)atoi(jrep->valuestring);

            cJSON_Delete(req_j);
        }

        if (!pronto[0]) {
            cJSON *err = cJSON_CreateObject();
            cJSON_AddStringToObject(err, "error", "Missing pronto parameter");
            send_cjson_resp(client_fd, 400, err);
            cJSON_Delete(err);
            close(client_fd);
            return;
        }

        int rc = hw_ir_send_pronto(pronto, ports, repeats);
        if (rc == 0) {
            cJSON *suc = cJSON_CreateObject();
            cJSON_AddBoolToObject(suc, "success", 1);
            send_cjson_resp(client_fd, 200, suc);
            cJSON_Delete(suc);
        } else {
            cJSON *err = cJSON_CreateObject();
            cJSON_AddStringToObject(err, "error", "IR send failed");
            send_cjson_resp(client_fd, 500, err);
            cJSON_Delete(err);
        }
        close(client_fd);
        return;
    }

    /* Route: GET /api/remote-status */
    if (strcmp(path, "/api/remote-status") == 0 || strcmp(path, "/api/remote-pair-status") == 0) {
        cJSON *st = NULL;
        FILE *f = fopen("/tmp/codex_btstack_status.json", "r");
        if (f) {
            char stat_buf[1024] = {0};
            fread(stat_buf, 1, sizeof(stat_buf) - 1, f);
            fclose(f);
            if (stat_buf[0]) st = cJSON_Parse(stat_buf);
        }
        if (!st) {
            st = cJSON_CreateObject();
            cJSON_AddStringToObject(st, "state", "idle");
        }
        send_cjson_resp(client_fd, 200, st);
        cJSON_Delete(st);
        close(client_fd);
        return;
    }

    /* Route: POST /api/button-press or POST /api/mqtt-publish */
    if ((strcmp(path, "/api/button-press") == 0 || strcmp(path, "/api/mqtt-publish") == 0) &&
        strcmp(method, "POST") == 0) {
        char dev_id[64] = {0};
        char dev_name[128] = {0};
        char cmd_name[128] = {0};
        int req_pulse_ms = 0;

        cJSON *req_j = body ? cJSON_Parse(body) : NULL;
        if (req_j) {
            cJSON *jid = cJSON_GetObjectItemCaseSensitive(req_j, "deviceId");
            if (jid && cJSON_IsString(jid) && jid->valuestring) strncpy(dev_id, jid->valuestring, sizeof(dev_id) - 1);
            cJSON *jdev = cJSON_GetObjectItemCaseSensitive(req_j, "device");
            if (jdev && cJSON_IsString(jdev) && jdev->valuestring) strncpy(dev_name, jdev->valuestring, sizeof(dev_name) - 1);
            cJSON *jcmd = cJSON_GetObjectItemCaseSensitive(req_j, "command");
            if (jcmd && cJSON_IsString(jcmd) && jcmd->valuestring) strncpy(cmd_name, jcmd->valuestring, sizeof(cmd_name) - 1);
            cJSON *jp = cJSON_GetObjectItemCaseSensitive(req_j, "pulseMs");
            if (!jp) jp = cJSON_GetObjectItemCaseSensitive(req_j, "pulse_ms");
            if (jp && cJSON_IsNumber(jp) && jp->valueint > 0) req_pulse_ms = jp->valueint;
            cJSON_Delete(req_j);
        }

        if (!cmd_name[0]) {
            cJSON *err = cJSON_CreateObject();
            cJSON_AddStringToObject(err, "error", "Missing command parameter");
            send_cjson_resp(client_fd, 400, err);
            cJSON_Delete(err);
            close(client_fd);
            return;
        }

        if (!dev_name[0]) {
            if (dev_id[0]) strcpy(dev_name, dev_id);
            else strcpy(dev_name, "device");
        }

        char final_topic[256] = {0};
        int final_pulse_ms = 0;
        int mrc = mqtt_dispatch_button_pulse(dev_id, dev_name, cmd_name, req_pulse_ms,
                                            final_topic, sizeof(final_topic), &final_pulse_ms);
        if (mrc == 0) {
            cJSON *resp_j = cJSON_CreateObject();
            cJSON_AddBoolToObject(resp_j, "ok", 1);
            cJSON_AddNumberToObject(resp_j, "pulseMs", final_pulse_ms);
            cJSON_AddStringToObject(resp_j, "topic", final_topic);
            cJSON_AddStringToObject(resp_j, "reply", "ok");
            send_cjson_resp(client_fd, 200, resp_j);
            cJSON_Delete(resp_j);
        } else {
            cJSON *resp_j = cJSON_CreateObject();
            cJSON_AddBoolToObject(resp_j, "ok", 0);
            cJSON_AddStringToObject(resp_j, "error", "MQTT broker not connected");
            send_cjson_resp(client_fd, 503, resp_j);
            cJSON_Delete(resp_j);
        }
        close(client_fd);
        return;
    }

    /* Default root page / health */
    if (strcmp(path, "/") == 0 || strcmp(path, "/health") == 0) {
        cJSON *hj = cJSON_CreateObject();
        cJSON_AddStringToObject(hj, "status", "ok");
        cJSON_AddStringToObject(hj, "service", "codex_daemon");
        send_cjson_resp(client_fd, 200, hj);
        cJSON_Delete(hj);
        close(client_fd);
        return;
    }

    /* 404 */
    cJSON *nf = cJSON_CreateObject();
    cJSON_AddStringToObject(nf, "error", "Not Found");
    send_cjson_resp(client_fd, 404, nf);
    cJSON_Delete(nf);
    close(client_fd);
}
