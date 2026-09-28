#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/file.h>
#include <time.h>
#include "cJSON.h"
#include "codex_webui_types.h"
#include "resource_cache.h"
#include "webui_utils.h"
#include "webui_ir.h"
#include "webui_bt.h"
#include "webui_config.h"
#include "webui_activity.h"
#include "webui_html.h"

static const char *DEFAULT_ACTIVITY_LIST =
    "{\"Activities\":[{\"Activity\":{\"Id-\":-1,\"Name\":\"PowerOff\",\"ActivityOrder\":0,"
    "\"ActivityTypeDisplayName\":\"PowerOff\",\"SuggestedDisplay\":\"PowerOff\",\"Icon\":\"PowerOff\","
    "\"IsTuningDefault\":false,\"Type\":\"PowerOff\"},\"ControlGroup\":[],\"Fixit\":[],\"Sequences\":[],"
    "\"StartSequence\":[],\"StopSequence\":[]}]}";

static char *read_or_init_activity_list(size_t *outlen) {
    char *raw = read_file_alloc(ACTIVITY_LIST, MAX_RESOURCE_FILE, outlen);
    if (!raw || !strstr(raw, "\"Activities\"")) {
        free(raw);
        write_file_atomic(ACTIVITY_LIST, DEFAULT_ACTIVITY_LIST, strlen(DEFAULT_ACTIVITY_LIST));
        raw = read_file_alloc(ACTIVITY_LIST, MAX_RESOURCE_FILE, outlen);
    }
    return raw;
}

static time_t s_last_act_query = 0;
static char s_cached_act_id[32] = "";
static char s_cached_act_raw[1024] = "";

void activity_cache_invalidate(void) {
    s_last_act_query = 0;
    s_cached_act_id[0] = '\0';
    s_cached_act_raw[0] = '\0';
}

static int s_activity_lock_fd = -1;

int activity_is_transitioning(char *target_out, size_t target_len) {
    if (s_activity_lock_fd >= 0) {
        if (target_out && target_len) target_out[0] = '\0';
        return 1;
    }
    int fd = open(ACTIVITY_LOCK_FILE, O_RDWR);
    if (fd < 0) return 0;
    if (flock(fd, LOCK_EX | LOCK_NB) == 0) {
        /* Lock successfully acquired -> stale or unused file */
        flock(fd, LOCK_UN);
        close(fd);
        unlink(ACTIVITY_LOCK_FILE);
        return 0;
    }
    /* Lock is held by another process -> transitioning */
    if (target_out && target_len > 0) {
        target_out[0] = '\0';
        char buf[128] = {0};
        ssize_t r = pread(fd, buf, sizeof(buf) - 1, 0);
        if (r > 0) {
            int pid = 0;
            char target[32] = "";
            if (sscanf(buf, "%d %31s", &pid, target) >= 2) {
                snprintf(target_out, target_len, "%s", target);
            }
        }
    }
    close(fd);
    return 1;
}

#define ACTIVITY_STEP_FILE "/tmp/codex_activity_step"

void broadcast_activity_progress(const char *act_id, int state, int step, int total, const char *desc) {
    FILE *sf = fopen(ACTIVITY_STEP_FILE, "w");
    if (sf) {
        fprintf(sf, "%d %d %s\n", step, total, desc ? desc : "");
        fclose(sf);
    }

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
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        return;
    }
    cJSON *ap_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(ap_obj, "type", "activity_progress");
    cJSON_AddStringToObject(ap_obj, "activity", act_id ? act_id : "");
    cJSON_AddNumberToObject(ap_obj, "state", state);
    cJSON_AddNumberToObject(ap_obj, "step", step);
    cJSON_AddNumberToObject(ap_obj, "total", total);
    cJSON_AddStringToObject(ap_obj, "desc", desc ? desc : "");
    char *body = cJSON_PrintUnformatted(ap_obj);
    cJSON_Delete(ap_obj);
    if (!body) { close(fd); return; }
    char req[768];
    snprintf(req, sizeof(req),
             "POST /api/activity-progress HTTP/1.1\r\n"
             "Host: 127.0.0.1:8089\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n\r\n%s",
             strlen(body), body);
    send(fd, req, strlen(req), MSG_NOSIGNAL);
    free(body);
    char dummy[128];
    recv(fd, dummy, sizeof(dummy) - 1, 0);
    close(fd);
}

void activity_get_transition_step(int *step, int *total, char *desc, size_t desc_len) {
    if (step) *step = 0;
    if (total) *total = 0;
    if (desc && desc_len) desc[0] = '\0';
    FILE *f = fopen(ACTIVITY_STEP_FILE, "r");
    if (!f) return;
    int s = 0, t = 0;
    char d[128] = "";
    if (fscanf(f, "%d %d", &s, &t) == 2) {
        if (fgets(d, sizeof(d), f)) {
            trim_in_place(d);
        }
        if (step) *step = s;
        if (total) *total = t;
        if (desc && desc_len) snprintf(desc, desc_len, "%s", d);
    }
    fclose(f);
}

void activity_set_transitioning(pid_t pid, const char *target_id) {
    if (s_activity_lock_fd < 0) {
        int fd = open(ACTIVITY_LOCK_FILE, O_RDWR | O_CREAT, 0666);
        if (fd >= 0) {
            if (flock(fd, LOCK_EX | LOCK_NB) == 0) {
                s_activity_lock_fd = fd;
            } else {
                close(fd);
                return;
            }
        }
    }
    if (s_activity_lock_fd >= 0) {
        if (ftruncate(s_activity_lock_fd, 0) == 0) {
            lseek(s_activity_lock_fd, 0, SEEK_SET);
            char buf[128];
            int len = snprintf(buf, sizeof(buf), "%d %s %ld\n",
                               (int)pid, (target_id && target_id[0]) ? target_id : "-1", (long)time(NULL));
            if (len > 0) write(s_activity_lock_fd, buf, len);
            fsync(s_activity_lock_fd);
        }
    }
}

void activity_clear_transitioning(void) {
    if (s_activity_lock_fd >= 0) {
        flock(s_activity_lock_fd, LOCK_UN);
        close(s_activity_lock_fd);
        s_activity_lock_fd = -1;
    }
    unlink(ACTIVITY_LOCK_FILE);
    unlink(ACTIVITY_STEP_FILE);
}

void activity_startup_cleanup(void) {
    int fd = open(ACTIVITY_LOCK_FILE, O_RDWR);
    if (fd < 0) return;
    if (flock(fd, LOCK_EX | LOCK_NB) == 0) {
        flock(fd, LOCK_UN);
        unlink(ACTIVITY_LOCK_FILE);
        unlink(ACTIVITY_STEP_FILE);
    }
    close(fd);
}

int activity_run_start(const char *activity_id, char *out, size_t outlen);
int activity_run_stop(char *out, size_t outlen);

void activity_query_current(char *cur_id, size_t cur_id_len, char *raw_out, size_t raw_len) {
    if (cur_id && cur_id_len) cur_id[0] = '\0';
    if (raw_out && raw_len) raw_out[0] = '\0';

    FILE *f = fopen(CURRENT_ACTIVITY_FILE, "r");
    if (f) {
        if (fgets(cur_id, (int)cur_id_len, f)) {
            trim_in_place(cur_id);
        }
        fclose(f);
    }
    if (!cur_id || !cur_id[0]) {
        if (cur_id && cur_id_len) snprintf(cur_id, cur_id_len, "-1");
    }
    if (raw_out && raw_len) {
        cJSON *qobj = cJSON_CreateObject();
        cJSON_AddStringToObject(qobj, "activityId", cur_id);
        char *qstr = cJSON_PrintUnformatted(qobj);
        cJSON_Delete(qobj);
        if (qstr) {
            snprintf(raw_out, raw_len, "%s", qstr);
            free(qstr);
        }
    }
}

void parse_activity_sequence_steps_cjson(const cJSON *seq_arr, struct activity_step *steps, int *step_count, int max_steps) {
    *step_count = 0;
    if (!seq_arr || !cJSON_IsArray(seq_arr)) return;
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, seq_arr) {
        if (*step_count >= max_steps) break;
        struct activity_step *st = &steps[*step_count];
        memset(st, 0, sizeof(*st));

        cJSON *jtype = cJSON_GetObjectItemCaseSensitive(item, "__type");
        if (!jtype) jtype = cJSON_GetObjectItemCaseSensitive(item, "Type");
        const char *tstr = (jtype && cJSON_IsString(jtype) && jtype->valuestring) ? jtype->valuestring : "";
        if (strstr(tstr, "Delay") || strcasecmp(tstr, "Delay") == 0) {
            strcpy(st->type, "Delay");
        } else if (strstr(tstr, "BT") || strcasecmp(tstr, "BTCommand") == 0) {
            strcpy(st->type, "BTCommand");
        } else {
            strcpy(st->type, "IRCommand");
        }

        cJSON *jdid = cJSON_GetObjectItemCaseSensitive(item, "DeviceId-");
        if (!jdid) jdid = cJSON_GetObjectItemCaseSensitive(item, "DeviceId");
        if (jdid && cJSON_IsNumber(jdid)) {
            snprintf(st->device_id, sizeof(st->device_id), "%.0f", jdid->valuedouble);
        } else if (jdid && cJSON_IsString(jdid) && jdid->valuestring) {
            strncpy(st->device_id, jdid->valuestring, sizeof(st->device_id) - 1);
        }

        cJSON *jcmd = cJSON_GetObjectItemCaseSensitive(item, "CommandName");
        if (!jcmd) jcmd = cJSON_GetObjectItemCaseSensitive(item, "Command");
        if (jcmd && cJSON_IsString(jcmd) && jcmd->valuestring) {
            strncpy(st->command, jcmd->valuestring, sizeof(st->command) - 1);
        }

        cJSON *jdel = cJSON_GetObjectItemCaseSensitive(item, "Duration");
        if (!jdel) jdel = cJSON_GetObjectItemCaseSensitive(item, "Delay");
        if (jdel && cJSON_IsNumber(jdel)) st->delay_ms = (int)jdel->valuedouble;
        if (st->delay_ms < 0) st->delay_ms = 0;

        cJSON *jord = cJSON_GetObjectItemCaseSensitive(item, "ActionOrder");
        if (!jord) jord = cJSON_GetObjectItemCaseSensitive(item, "Order");
        if (jord && cJSON_IsNumber(jord)) st->order = (int)jord->valuedouble;
        else st->order = *step_count;

        (*step_count)++;
    }
}

int load_activity_inventory(struct activity_inventory *inv) {
    char *raw;
    size_t len = 0;
    memset(inv, 0, sizeof(*inv));
    raw = read_or_init_activity_list(&len);
    if (!raw) return -1;
    activity_query_current(inv->current_id, sizeof(inv->current_id), NULL, 0);

    cJSON *root = cJSON_Parse(raw);
    free(raw);
    if (!root) return -1;

    cJSON *acts_arr = cJSON_GetObjectItemCaseSensitive(root, "Activities");
    if (!acts_arr || !cJSON_IsArray(acts_arr)) {
        cJSON_Delete(root);
        return -1;
    }

    const cJSON *elem = NULL;
    cJSON_ArrayForEach(elem, acts_arr) {
        if (inv->count >= MAX_ACTIVITIES) break;
        const cJSON *act_obj = elem;
        cJSON *inner = cJSON_GetObjectItemCaseSensitive(elem, "Activity");
        if (inner && cJSON_IsObject(inner)) act_obj = inner;

        struct activity_item *act = &inv->items[inv->count];
        memset(act, 0, sizeof(*act));

        cJSON *jid = cJSON_GetObjectItemCaseSensitive(act_obj, "Id-");
        if (!jid) jid = cJSON_GetObjectItemCaseSensitive(act_obj, "Id");
        long id = 0;
        if (jid && cJSON_IsNumber(jid)) id = (long)jid->valuedouble;
        else if (jid && cJSON_IsString(jid) && jid->valuestring) id = atol(jid->valuestring);
        snprintf(act->id, sizeof(act->id), "%ld", id);
        if (id > inv->max_id) inv->max_id = id;

        cJSON *jname = cJSON_GetObjectItemCaseSensitive(act_obj, "Name");
        if (jname && cJSON_IsString(jname) && jname->valuestring) {
            strncpy(act->name, jname->valuestring, sizeof(act->name) - 1);
        }

        cJSON *jtype = cJSON_GetObjectItemCaseSensitive(act_obj, "ActivityTypeDisplayName");
        if (!jtype) jtype = cJSON_GetObjectItemCaseSensitive(act_obj, "SuggestedDisplay");
        if (!jtype) jtype = cJSON_GetObjectItemCaseSensitive(act_obj, "Type");
        if (jtype && cJSON_IsString(jtype) && jtype->valuestring) {
            strncpy(act->type, jtype->valuestring, sizeof(act->type) - 1);
        }

        cJSON *jord = cJSON_GetObjectItemCaseSensitive(act_obj, "ActivityOrder");
        if (!jord) jord = cJSON_GetObjectItemCaseSensitive(act_obj, "Order");
        if (jord && cJSON_IsNumber(jord)) act->order = (int)jord->valuedouble;
        else act->order = inv->count;

        cJSON *roles = cJSON_GetObjectItemCaseSensitive(elem, "Roles");
        if (!roles) roles = cJSON_GetObjectItemCaseSensitive(act_obj, "Roles");
        if (roles && cJSON_IsArray(roles)) {
            const cJSON *role = NULL;
            cJSON_ArrayForEach(role, roles) {
                cJSON *rdid = cJSON_GetObjectItemCaseSensitive(role, "DeviceId-");
                if (!rdid) rdid = cJSON_GetObjectItemCaseSensitive(role, "DeviceId");
                long did = 0;
                if (rdid && cJSON_IsNumber(rdid)) did = (long)rdid->valuedouble;
                else if (rdid && cJSON_IsString(rdid) && rdid->valuestring) did = atol(rdid->valuestring);
                if (did > 0 && act->device_count < 16) {
                    char did_str[32];
                    snprintf(did_str, sizeof(did_str), "%ld", did);
                    int found = 0;
                    for (int k = 0; k < act->device_count; k++) {
                        if (strcmp(act->device_ids[k], did_str) == 0) { found = 1; break; }
                    }
                    if (!found) {
                        snprintf(act->device_ids[act->device_count++], sizeof(act->device_ids[0]), "%s", did_str);
                    }
                }
            }
        }

        cJSON *start_seq = cJSON_GetObjectItemCaseSensitive(elem, "EnterActions");
        if (!start_seq) start_seq = cJSON_GetObjectItemCaseSensitive(elem, "StartSequence");
        if (!start_seq) start_seq = cJSON_GetObjectItemCaseSensitive(act_obj, "EnterActions");
        if (!start_seq) start_seq = cJSON_GetObjectItemCaseSensitive(act_obj, "StartSequence");
        parse_activity_sequence_steps_cjson(start_seq, act->start_steps, &act->start_count, MAX_ACTIVITY_STEPS);

        cJSON *stop_seq = cJSON_GetObjectItemCaseSensitive(elem, "LeaveActions");
        if (!stop_seq) stop_seq = cJSON_GetObjectItemCaseSensitive(elem, "StopSequence");
        if (!stop_seq) stop_seq = cJSON_GetObjectItemCaseSensitive(act_obj, "LeaveActions");
        if (!stop_seq) stop_seq = cJSON_GetObjectItemCaseSensitive(act_obj, "StopSequence");
        parse_activity_sequence_steps_cjson(stop_seq, act->stop_steps, &act->stop_count, MAX_ACTIVITY_STEPS);

        if (strcmp(inv->current_id, act->id) == 0) {
            snprintf(inv->current_name, sizeof(inv->current_name), "%s", act->name);
        }

        inv->count++;
    }
    cJSON_Delete(root);

    int has_poweroff = 0;
    for (int i = 0; i < inv->count; i++) {
        if (strcmp(inv->items[i].id, "-1") == 0) {
            has_poweroff = 1;
            break;
        }
    }
    if (!has_poweroff && inv->count < MAX_ACTIVITIES) {
        struct activity_item *po = &inv->items[inv->count++];
        memset(po, 0, sizeof(*po));
        strcpy(po->id, "-1");
        strcpy(po->name, "PowerOff");
        strcpy(po->type, "PowerOff");
        po->order = 0;
    }
    if (!inv->current_name[0]) {
        if (strcmp(inv->current_id, "-1") == 0 || inv->current_id[0] == '\0') {
            snprintf(inv->current_name, sizeof(inv->current_name), "PowerOff");
        } else {
            snprintf(inv->current_name, sizeof(inv->current_name), "Activity %s", inv->current_id);
        }
    }
    return 0;
}

static cJSON *build_steps_cjson_from_text(const char *seq_text) {
    cJSON *arr = cJSON_CreateArray();
    if (!arr) return NULL;
    if (!seq_text || !seq_text[0]) return arr;

    const char *p = seq_text;
    char line[512];
    int order = 0;
    long base_action_id = 24000000 + (long)(time(NULL) % 900000);

    while (*p) {
        size_t l = 0;
        char type[32], dev_id[32], cmd[80], del_str[32];
        char *f[4];
        int count, delay_ms = 0;
        while (*p && *p != '\n' && *p != '\r' && l < sizeof(line) - 1) {
            line[l++] = *p++;
        }
        line[l] = '\0';
        while (*p == '\n' || *p == '\r') p++;
        trim_in_place(line);
        if (!line[0] || line[0] == '#') continue;

        count = split_fields(line, '|', f, 4);
        type[0] = dev_id[0] = cmd[0] = del_str[0] = '\0';
        if (count >= 1) snprintf(type, sizeof(type), "%s", trim_in_place(f[0]));
        if (count >= 2) snprintf(dev_id, sizeof(dev_id), "%s", trim_in_place(f[1]));
        if (count >= 3) snprintf(cmd, sizeof(cmd), "%s", trim_in_place(f[2]));
        if (count >= 4) snprintf(del_str, sizeof(del_str), "%s", trim_in_place(f[3]));
        delay_ms = atoi(del_str);
        if (delay_ms < 0) delay_ms = 0;
        if (!type[0]) strcpy(type, "IRCommand");

        cJSON *step = cJSON_CreateObject();
        if (strcasecmp(type, "Delay") == 0) {
            cJSON_AddStringToObject(step, "__type", "DelayActivityAction");
            cJSON_AddNumberToObject(step, "Duration", delay_ms);
            cJSON_AddNumberToObject(step, "ActionOrder", order);
            cJSON_AddNumberToObject(step, "Id", base_action_id + order);
            cJSON_AddStringToObject(step, "Type", "Delay");
            cJSON_AddNumberToObject(step, "DeviceId", 0);
            cJSON_AddStringToObject(step, "Command", "");
            cJSON_AddNumberToObject(step, "Delay", delay_ms);
            cJSON_AddNumberToObject(step, "Order", order);
        } else if (strcasecmp(type, "BTCommand") == 0) {
            cJSON_AddNumberToObject(step, "DeviceId-", 0);
            cJSON_AddStringToObject(step, "__type", "BTCommandActivityAction");
            cJSON_AddNullToObject(step, "TargetLevel");
            cJSON_AddNumberToObject(step, "ActionOrder", order);
            cJSON_AddNumberToObject(step, "Id", base_action_id + order);
            cJSON_AddStringToObject(step, "CommandName", cmd);
            cJSON_AddStringToObject(step, "Type", "BTCommand");
            cJSON_AddNumberToObject(step, "DeviceId", 0);
            cJSON_AddStringToObject(step, "Command", cmd);
            cJSON_AddNumberToObject(step, "Order", order);
            cJSON_AddNumberToObject(step, "Delay", delay_ms);
        } else {
            long did = atol(dev_id);
            cJSON_AddNumberToObject(step, "DeviceId-", did);
            cJSON_AddStringToObject(step, "__type", "CommandActivityAction");
            cJSON_AddNullToObject(step, "TargetLevel");
            cJSON_AddNumberToObject(step, "ActionOrder", order);
            cJSON_AddNumberToObject(step, "Id", base_action_id + order);
            cJSON_AddStringToObject(step, "CommandName", cmd);
            cJSON_AddStringToObject(step, "Type", "IRCommand");
            cJSON_AddNumberToObject(step, "DeviceId", did);
            cJSON_AddStringToObject(step, "Command", cmd);
            cJSON_AddNumberToObject(step, "Order", order);
            cJSON_AddNumberToObject(step, "Delay", delay_ms);
        }
        cJSON_AddItemToArray(arr, step);
        order++;
    }
    return arr;
}

static cJSON *build_roles_cjson(const cJSON *old_roles, const char *device_ids) {
    cJSON *arr = cJSON_CreateArray();
    if (!arr) return NULL;
    if (!device_ids || !device_ids[0]) return arr;

    char dev_buf[256];
    char *dev_list[32];
    int dev_count = 0;
    strncpy(dev_buf, device_ids, sizeof(dev_buf) - 1);
    dev_buf[sizeof(dev_buf) - 1] = '\0';
    dev_count = split_fields(dev_buf, ',', dev_list, 32);

    for (int k = 0; k < dev_count; k++) {
        long did = atol(dev_list[k]);
        if (did <= 0) continue;

        cJSON *found_old = NULL;
        if (old_roles && cJSON_IsArray(old_roles)) {
            cJSON *r = NULL;
            cJSON_ArrayForEach(r, old_roles) {
                cJSON *jdid = cJSON_GetObjectItemCaseSensitive(r, "DeviceId-");
                if (!jdid) jdid = cJSON_GetObjectItemCaseSensitive(r, "DeviceId");
                if (jdid && cJSON_IsNumber(jdid) && (long)jdid->valuedouble == did) {
                    found_old = r;
                    break;
                }
            }
        }

        if (found_old) {
            cJSON *role = cJSON_Duplicate(found_old, 1);
            if (role) {
                cjson_set_or_replace(role, "PowerOnOrder", cJSON_CreateNumber(k + 1));
                cjson_set_or_replace(role, "PowerOffOrder", cJSON_CreateNumber(dev_count - k));
                cJSON_AddItemToArray(arr, role);
            }
        } else {
            static long role_seq = 0;
            role_seq++;
            long role_id = 300000000 + (long)(time(NULL) % 80000000) + role_seq * 37 + (k * 7);

            cJSON *role = cJSON_CreateObject();
            cJSON_AddNumberToObject(role, "DeviceId-", did);
            cJSON_AddStringToObject(role, "__type", "PassThroughActivityRole");
            cJSON_AddNumberToObject(role, "Id-", role_id);
            cJSON_AddNullToObject(role, "SelectedInput");
            cJSON_AddNumberToObject(role, "PowerOnOrder", k + 1);
            cJSON_AddNullToObject(role, "NextDevicePowerOnDelay");
            cJSON_AddNumberToObject(role, "PowerOffOrder", dev_count - k);
            cJSON_AddItemToArray(arr, role);
        }
    }
    return arr;
}

int activity_save(const char *id_in, const char *name, const char *type, int order,
                         const char *device_ids,
                         const char *start_seq, const char *stop_seq,
                         char *msg, size_t msglen, char *saved_id, size_t saved_id_len) {
    long id = 0;
    int is_new = 0;

    if (!safe_label(name)) {
        snprintf(msg, msglen, "Activity name cannot be empty or contain control characters.");
        return -1;
    }
    struct activity_inventory *inv = (struct activity_inventory *)calloc(1, sizeof(*inv));
    if (!inv) {
        snprintf(msg, msglen, "Out of memory.");
        return -1;
    }
    if (load_activity_inventory(inv) != 0) {
        free(inv);
        snprintf(msg, msglen, "Failed to load activities.");
        return -1;
    }
    if (id_in && id_in[0] && strcmp(id_in, "0") != 0 && strcmp(id_in, "-1") != 0) {
        id = atol(id_in);
    }
    if (id <= 0 && (!id_in || strcmp(id_in, "-1") != 0)) {
        is_new = 1;
        id = inv->max_id + 1;
        if (id < 90000000) id = 90000000 + (long)(time(NULL) % 9000000);
    } else if (id_in && strcmp(id_in, "-1") == 0) {
        id = -1;
    }
    free(inv);

    size_t raw_len = 0;
    char *raw = read_or_init_activity_list(&raw_len);
    if (!raw) {
        snprintf(msg, msglen, "Failed to read ActivityList.");
        return -1;
    }

    cJSON *root = cJSON_Parse(raw);
    free(raw);
    if (!root) {
        snprintf(msg, msglen, "Failed to parse ActivityList JSON.");
        return -1;
    }

    cJSON *acts_arr = cJSON_GetObjectItemCaseSensitive(root, "Activities");
    if (!acts_arr || !cJSON_IsArray(acts_arr)) {
        cJSON_Delete(root);
        snprintf(msg, msglen, "Malformed ActivityList: missing Activities array.");
        return -1;
    }

    cJSON *target_act = NULL;
    cJSON *entry = NULL;

    if (!is_new) {
        cJSON_ArrayForEach(entry, acts_arr) {
            cJSON *a = entry;
            cJSON *inner = cJSON_GetObjectItemCaseSensitive(entry, "Activity");
            if (inner && cJSON_IsObject(inner)) a = inner;

            cJSON *jid = cJSON_GetObjectItemCaseSensitive(a, "Id-");
            if (!jid) jid = cJSON_GetObjectItemCaseSensitive(a, "Id");
            if (!jid) jid = cJSON_GetObjectItemCaseSensitive(a, "id");
            long aid = 0;
            if (jid && cJSON_IsNumber(jid)) aid = (long)jid->valuedouble;
            else if (jid && cJSON_IsString(jid) && jid->valuestring) aid = atol(jid->valuestring);

            if (aid == id) {
                target_act = a;
                break;
            }
        }
        if (!target_act) {
            is_new = 1;
        }
    }

    char val_err[256];
    if (start_seq && start_seq[0] && validate_sequence_text(start_seq, val_err, sizeof(val_err)) != 0) {
        cJSON_Delete(root);
        snprintf(msg, msglen, "Start sequence: %s", val_err);
        return -1;
    }
    if (stop_seq && stop_seq[0] && validate_sequence_text(stop_seq, val_err, sizeof(val_err)) != 0) {
        cJSON_Delete(root);
        snprintf(msg, msglen, "Stop sequence: %s", val_err);
        return -1;
    }

    cJSON *old_roles = target_act ? cJSON_GetObjectItemCaseSensitive(target_act, "Roles") : NULL;
    cJSON *new_roles = build_roles_cjson(old_roles, device_ids);
    cJSON *enter_actions = build_steps_cjson_from_text(start_seq);
    cJSON *leave_actions = build_steps_cjson_from_text(stop_seq);

    if (is_new || !target_act) {
        int act_type = 2;
        long act_group = 2;
        if (type && strstr(type, "Game")) { act_type = 15; act_group = 0; }
        else if (type && strstr(type, "Music")) { act_type = 1; act_group = 1; }

        target_act = cJSON_CreateObject();
        cJSON_AddNullToObject(target_act, "DefaultStationName");
        cJSON_AddBoolToObject(target_act, "IsTuningDefault", 0);
        cJSON_AddNumberToObject(target_act, "AccountId-", 16228465);
        cJSON_AddNullToObject(target_act, "Alternatives");
        cJSON_AddNullToObject(target_act, "Zones");
        cJSON_AddItemToObject(target_act, "EnterActions", enter_actions);
        cJSON_AddBoolToObject(target_act, "IsDefault", 0);
        cJSON_AddNumberToObject(target_act, "Type", act_type);
        cJSON_AddStringToObject(target_act, "BaseImageUri", "https://rcbu-prod-ssl-amr.myharmony.com/");
        cJSON_AddStringToObject(target_act, "SuggestedDisplay", (type && type[0]) ? type : "VirtualGeneric");
        cJSON_AddNumberToObject(target_act, "ActivityGroup", act_group);
        cJSON_AddBoolToObject(target_act, "IsMultiZone", 0);
        cJSON_AddNullToObject(target_act, "DefaultStation");
        cJSON_AddStringToObject(target_act, "Name", name);
        cJSON_AddNullToObject(target_act, "ActivityDisplayName");
        cJSON_AddStringToObject(target_act, "StartScreen", "Commands");
        cJSON_AddNumberToObject(target_act, "State", 0);
        cJSON_AddItemToObject(target_act, "LeaveActions", leave_actions);
        cJSON_AddNumberToObject(target_act, "ActivityOrder", order);
        cJSON_AddNullToObject(target_act, "DefaultChannel");
        cJSON_AddItemToObject(target_act, "Roles", new_roles);
        cJSON_AddNumberToObject(target_act, "Id-", id);
        cJSON_AddNullToObject(target_act, "Icon");
        cJSON_AddItemToObject(target_act, "StartSequence", cJSON_Duplicate(enter_actions, 1));
        cJSON_AddItemToObject(target_act, "StopSequence", cJSON_Duplicate(leave_actions, 1));

        cJSON_AddItemToArray(acts_arr, target_act);
    } else {
        /* Update existing in place */
        cjson_set_or_replace(target_act, "Name", cJSON_CreateString(name));
        cjson_set_or_replace(target_act, "ActivityOrder", cJSON_CreateNumber(order));
        if (type && type[0]) {
            cjson_set_or_replace(target_act, "SuggestedDisplay", cJSON_CreateString(type));
        }
        cjson_set_or_replace(target_act, "Roles", new_roles);
        cjson_set_or_replace(target_act, "EnterActions", enter_actions);
        cjson_set_or_replace(target_act, "LeaveActions", leave_actions);
        cjson_set_or_replace(target_act, "StartSequence", cJSON_Duplicate(enter_actions, 1));
        cjson_set_or_replace(target_act, "StopSequence", cJSON_Duplicate(leave_actions, 1));
    }

    backup_resources();

    char *out_json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!out_json) {
        snprintf(msg, msglen, "Failed to serialize ActivityList.");
        return -1;
    }

    if (write_file_atomic(ACTIVITY_LIST, out_json, strlen(out_json)) != 0) {
        free(out_json);
        snprintf(msg, msglen, "Failed to write ActivityList to disk.");
        return -1;
    }
    free(out_json);

    request_resource_reload();
    activity_cache_invalidate();

    if (saved_id && saved_id_len) snprintf(saved_id, saved_id_len, "%ld", id);
    snprintf(msg, msglen, "%s activity %s (%ld).", is_new ? "Created" : "Saved", name, id);
    return 0;
}

int activity_delete(const char *activity_id, char *msg, size_t msglen) {
    if (!activity_id || !activity_id[0] || strcmp(activity_id, "-1") == 0 || strcasecmp(activity_id, "PowerOff") == 0) {
        snprintf(msg, msglen, "Cannot delete PowerOff.");
        return -1;
    }

    size_t len = 0;
    char *raw = read_file_alloc(ACTIVITY_LIST, MAX_RESOURCE_FILE, &len);
    if (!raw) {
        snprintf(msg, msglen, "Failed to read ActivityList.");
        return -1;
    }

    cJSON *root = cJSON_Parse(raw);
    free(raw);
    if (!root) {
        snprintf(msg, msglen, "Failed to parse ActivityList.");
        return -1;
    }

    cJSON *acts_arr = cJSON_GetObjectItemCaseSensitive(root, "Activities");
    if (!acts_arr || !cJSON_IsArray(acts_arr)) {
        cJSON_Delete(root);
        snprintf(msg, msglen, "Malformed ActivityList: missing Activities array.");
        return -1;
    }

    long target_id = atol(activity_id);
    int found_idx = -1;
    int idx = 0;
    cJSON *entry = NULL;

    cJSON_ArrayForEach(entry, acts_arr) {
        cJSON *a = entry;
        cJSON *inner = cJSON_GetObjectItemCaseSensitive(entry, "Activity");
        if (inner && cJSON_IsObject(inner)) a = inner;

        cJSON *jid = cJSON_GetObjectItemCaseSensitive(a, "Id-");
        if (!jid) jid = cJSON_GetObjectItemCaseSensitive(a, "Id");
        if (!jid) jid = cJSON_GetObjectItemCaseSensitive(a, "id");
        long aid = 0;
        if (jid && cJSON_IsNumber(jid)) aid = (long)jid->valuedouble;
        else if (jid && cJSON_IsString(jid) && jid->valuestring) aid = atol(jid->valuestring);

        if (aid == target_id) {
            found_idx = idx;
            break;
        }
        idx++;
    }

    if (found_idx >= 0) {
        backup_resources();
        cJSON_DeleteItemFromArray(acts_arr, found_idx);
        char *out_json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        if (!out_json || write_file_atomic(ACTIVITY_LIST, out_json, strlen(out_json)) != 0) {
            if (out_json) free(out_json);
            snprintf(msg, msglen, "Failed to write ActivityList after deletion.");
            return -1;
        }
        free(out_json);
        request_resource_reload();
        activity_cache_invalidate();
        snprintf(msg, msglen, "Deleted activity %s.", activity_id);
        return 0;
    }

    cJSON_Delete(root);
    snprintf(msg, msglen, "Activity %s not found.", activity_id);
    return -1;
}

int validate_sequence_text(const char *seq_text, char *err, size_t errlen) {
    if (!seq_text || !seq_text[0]) return 0;
    struct ir_inventory *ir_inv = (struct ir_inventory *)calloc(1, sizeof(*ir_inv));
    int has_ir = ir_inv && (load_ir_inventory(ir_inv) == 0);
    const char *p = seq_text;
    char line[512];
    int line_num = 0;
    int ret = 0;

    while (*p) {
        size_t l = 0;
        char type[32], dev_id[32], cmd[80], del_str[32];
        char *f[4];
        int count;
        while (*p && *p != '\n' && *p != '\r' && l < sizeof(line) - 1) {
            line[l++] = *p++;
        }
        line[l] = '\0';
        while (*p == '\n' || *p == '\r') p++;
        trim_in_place(line);
        line_num++;
        if (!line[0] || line[0] == '#') continue;

        count = split_fields(line, '|', f, 4);
        type[0] = dev_id[0] = cmd[0] = del_str[0] = '\0';
        if (count >= 1) snprintf(type, sizeof(type), "%s", trim_in_place(f[0]));
        if (count >= 2) snprintf(dev_id, sizeof(dev_id), "%s", trim_in_place(f[1]));
        if (count >= 3) snprintf(cmd, sizeof(cmd), "%s", trim_in_place(f[2]));

        if (strcasecmp(type, "Delay") == 0) continue;
        if (strcasecmp(type, "BTCommand") == 0) {
            if (!cmd[0]) {
                if (err && errlen) snprintf(err, errlen, "Line %d: BT command cannot be empty", line_num);
                ret = -1;
                break;
            }
            continue;
        }

        /* IR Command */
        if (!dev_id[0]) {
            if (err && errlen) snprintf(err, errlen, "Line %d: missing device ID", line_num);
            ret = -1;
            break;
        }
        if (!cmd[0]) {
            if (err && errlen) snprintf(err, errlen, "Line %d: missing command name", line_num);
            ret = -1;
            break;
        }
        if (has_ir) {
            int dev_idx = -1;
            for (int j = 0; j < ir_inv->device_count; j++) {
                if (strcmp(ir_inv->devices[j].id, dev_id) == 0) {
                    dev_idx = j;
                    break;
                }
            }
            if (dev_idx < 0) {
                if (err && errlen) snprintf(err, errlen, "Line %d: device %s not found in DeviceList", line_num, dev_id);
                ret = -1;
                break;
            }
            int cmd_found = 0;
            for (int k = 0; k < ir_inv->devices[dev_idx].command_count; k++) {
                if (strcasecmp(ir_inv->devices[dev_idx].commands[k].name, cmd) == 0) {
                    cmd_found = 1;
                    break;
                }
            }
            if (!cmd_found) {
                if (err && errlen) snprintf(err, errlen, "Line %d: command '%s' not found on device %s", line_num, cmd, dev_id);
                ret = -1;
                break;
            }
        }
    }
    if (has_ir) free_ir_inventory(ir_inv);
    free(ir_inv);
    return ret;
}

int activity_test_sequence(const char *seq_text, char *out, size_t outlen) {
    char val_err[256];
    if (validate_sequence_text(seq_text, val_err, sizeof(val_err)) != 0) {
        snprintf(out, outlen, "validation error: %s", val_err);
        return -1;
    }
    const char *p = seq_text;
    char line[512], last_reply[256];
    int sent = 0;
    last_reply[0] = '\0';
    if (!seq_text || !seq_text[0]) {
        snprintf(out, outlen, "no steps to execute");
        return 0;
    }
    while (*p) {
        size_t l = 0;
        char type[32], dev_id[32], cmd[80], del_str[32];
        char *f[4];
        int count, delay_ms = 0;
        while (*p && *p != '\n' && *p != '\r' && l < sizeof(line) - 1) {
            line[l++] = *p++;
        }
        line[l] = '\0';
        while (*p == '\n' || *p == '\r') p++;
        trim_in_place(line);
        if (!line[0] || line[0] == '#') continue;
        count = split_fields(line, '|', f, 4);
        type[0] = dev_id[0] = cmd[0] = del_str[0] = '\0';
        if (count >= 1) snprintf(type, sizeof(type), "%s", trim_in_place(f[0]));
        if (count >= 2) snprintf(dev_id, sizeof(dev_id), "%s", trim_in_place(f[1]));
        if (count >= 3) snprintf(cmd, sizeof(cmd), "%s", trim_in_place(f[2]));
        if (count >= 4) snprintf(del_str, sizeof(del_str), "%s", trim_in_place(f[3]));
        delay_ms = atoi(del_str);
        if (delay_ms < 0) delay_ms = 0;
        if (delay_ms > 10000) delay_ms = 10000;

        if (strcasecmp(type, "Delay") == 0) {
            if (delay_ms > 0) usleep((useconds_t)delay_ms * 1000);
            sent++;
            continue;
        }
        if (strcasecmp(type, "BTCommand") == 0) {
            char bt_reply[256];
            run_bt_saved_script("btkeyboard", "", cmd, 50, bt_reply, sizeof(bt_reply));
            snprintf(last_reply, sizeof(last_reply), "%s", bt_reply);
            sent++;
        } else {
            char ir_reply[256];
            send_ir_command_action(dev_id, cmd, ir_reply, sizeof(ir_reply));
            snprintf(last_reply, sizeof(last_reply), "%s", ir_reply);
            sent++;
        }
        if (delay_ms > 0) {
            usleep((useconds_t)delay_ms * 1000);
        } else {
            usleep(250000);
        }
    }
    snprintf(out, outlen, "executed %d step(s); last: %s", sent, last_reply[0] ? last_reply : "ok");
    return sent;
}

void execute_activity_step_list(const struct activity_step *steps, int count) {
    int i;
    for (i = 0; i < count; i++) {
        const struct activity_step *s = &steps[i];
        if (strcasecmp(s->type, "Delay") == 0) {
            if (s->delay_ms > 0) usleep((useconds_t)s->delay_ms * 1000);
            continue;
        }
        if (strcasecmp(s->type, "BTCommand") == 0) {
            char bt_reply[256];
            run_bt_saved_script("btkeyboard", "", s->command, 50, bt_reply, sizeof(bt_reply));
        } else {
            char ir_reply[256];
            send_ir_command_action(s->device_id, s->command, ir_reply, sizeof(ir_reply));
        }
        if (s->delay_ms > 0) {
            usleep((useconds_t)s->delay_ms * 1000);
        } else {
            usleep(250000);
        }
    }
}

void execute_device_power_action(struct ir_device *dev, int is_power_on) {
    if (!dev) return;
    if (dev->is_power_always_on) return; /* Always powered on devices should never execute power on/off commands */

    int count = is_power_on ? dev->power_on_count : dev->power_off_count;
    struct activity_step *steps = is_power_on ? dev->power_on_steps : dev->power_off_steps;

    if (count > 0 && steps) {
        execute_activity_step_list(steps, count);
    } else {
        /* Fallback if no explicit sequence defined: look for PowerOn/PowerOff, or PowerToggle */
        char reply[256];
        int found = 0, k;
        const char *preferred = is_power_on ? "PowerOn" : "PowerOff";
        for (k = 0; k < dev->command_count; k++) {
            if (strcasecmp(dev->commands[k].name, preferred) == 0) {
                send_ir_command_action(dev->id, dev->commands[k].name, reply, sizeof(reply));
                found = 1;
                break;
            }
        }
        if (!found) {
            for (k = 0; k < dev->command_count; k++) {
                if (strcasecmp(dev->commands[k].name, "PowerToggle") == 0 || strcasecmp(dev->commands[k].name, "Power") == 0) {
                    send_ir_command_action(dev->id, dev->commands[k].name, reply, sizeof(reply));
                    break;
                }
            }
        }
    }
    if (dev->inter_device_delay > 0) {
        usleep((useconds_t)dev->inter_device_delay * 1000);
    } else {
        usleep(250000);
    }
}

int activity_run_start(const char *activity_id, char *out, size_t outlen) {
    char trans_target[32] = "";

    if (out && outlen) out[0] = '\0';
    if (!activity_id || !activity_id[0]) activity_id = "-1";

    /* Atomically test and acquire transition lock */
    int fd = open(ACTIVITY_LOCK_FILE, O_RDWR | O_CREAT, 0666);
    if (fd < 0 || flock(fd, LOCK_EX | LOCK_NB) != 0) {
        if (fd >= 0) close(fd);
        activity_is_transitioning(trans_target, sizeof(trans_target));
        if (out && outlen) {
            snprintf(out, outlen, "Activity switch already in progress (switching to %s)",
                     trans_target[0] ? trans_target : "another activity");
        }
        return -2;
    }

    if (s_activity_lock_fd >= 0) {
        flock(s_activity_lock_fd, LOCK_UN);
        close(s_activity_lock_fd);
    }
    s_activity_lock_fd = fd;
    activity_set_transitioning(getpid(), activity_id);

    if (out && outlen) snprintf(out, outlen, "Activity transition started");

    activity_cache_invalidate();
    return 0;
}

/* Run the activity transition inline (no fork).
 * Called AFTER the HTTP response has been sent and the socket closed,
 * so the HTTP handler child can block here for the full transition duration.
 * This avoids the double-fork that was causing OOM kills on 64MB devices. */
void activity_run_transition(const char *activity_id) {
    char cur_id[32] = "";
    struct activity_inventory *act_inv = (struct activity_inventory *)calloc(1, sizeof(*act_inv));
    struct ir_inventory *ir_inv = (struct ir_inventory *)calloc(1, sizeof(*ir_inv));
    int cur_idx = -1, target_idx = -1;
    int has_ir = 0, total_steps = 0, cur_step = 0;
    char step_desc[128] = "";
    int i;

    if (!act_inv || !ir_inv) {
        free(act_inv);
        free(ir_inv);
        activity_clear_transitioning();
        return;
    }

    if (!activity_id || !activity_id[0]) activity_id = "-1";

    activity_query_current(cur_id, sizeof(cur_id), NULL, 0);

    if (load_activity_inventory(act_inv) != 0) {
        activity_clear_transitioning();
        free(act_inv);
        free(ir_inv);
        return;
    }
    for (i = 0; i < act_inv->count; i++) {
        if (cur_id[0] && strcmp(act_inv->items[i].id, cur_id) == 0) cur_idx = i;
        if (strcmp(act_inv->items[i].id, activity_id) == 0) target_idx = i;
    }

    has_ir = (load_ir_inventory(ir_inv) == 0);

    /* Count total steps */
    if (cur_idx >= 0 && strcmp(cur_id, activity_id) != 0) {
        total_steps += act_inv->items[cur_idx].stop_count;
    }
    if (has_ir && cur_idx >= 0 && strcmp(cur_id, activity_id) != 0) {
        struct activity_item *old_act = &act_inv->items[cur_idx];
        struct activity_item *new_act = (target_idx >= 0) ? &act_inv->items[target_idx] : NULL;
        for (i = 0; i < old_act->device_count; i++) {
            const char *dev_id = old_act->device_ids[i];
            int in_new = 0, j;
            if (new_act) {
                for (j = 0; j < new_act->device_count; j++) {
                    if (strcmp(new_act->device_ids[j], dev_id) == 0) { in_new = 1; break; }
                }
            }
            if (!in_new) {
                if (strcmp(activity_id, "-1") == 0 && target_idx >= 0 && act_inv->items[target_idx].start_count > 0) {
                    int in_poweroff_seq = 0;
                    for (j = 0; j < act_inv->items[target_idx].start_count; j++) {
                        if (strcmp(act_inv->items[target_idx].start_steps[j].device_id, dev_id) == 0) {
                            in_poweroff_seq = 1;
                            break;
                        }
                    }
                    if (in_poweroff_seq) continue;
                }
                for (j = 0; j < ir_inv->device_count; j++) {
                    if (strcmp(ir_inv->devices[j].id, dev_id) == 0) {
                        if (!ir_inv->devices[j].is_power_always_on) total_steps++;
                        break;
                    }
                }
            }
        }
    }
    if (has_ir && target_idx >= 0 && strcmp(activity_id, "-1") != 0) {
        struct activity_item *new_act = &act_inv->items[target_idx];
        struct activity_item *old_act = (cur_idx >= 0 && strcmp(cur_id, "-1") != 0) ? &act_inv->items[cur_idx] : NULL;
        for (i = 0; i < new_act->device_count; i++) {
            const char *dev_id = new_act->device_ids[i];
            int in_old = 0, j;
            if (old_act) {
                for (j = 0; j < old_act->device_count; j++) {
                    if (strcmp(old_act->device_ids[j], dev_id) == 0) { in_old = 1; break; }
                }
            }
            if (!in_old) {
                for (j = 0; j < ir_inv->device_count; j++) {
                    if (strcmp(ir_inv->devices[j].id, dev_id) == 0) {
                        if (!ir_inv->devices[j].is_power_always_on) total_steps++;
                        break;
                    }
                }
            }
        }
    }
    if (target_idx >= 0) {
        total_steps += act_inv->items[target_idx].start_count;
    }
    if (total_steps <= 0) total_steps = 1;

    broadcast_activity_progress(activity_id, 1, 0, total_steps, "Starting switch...");

    /* 1. Stop sequence of departing activity */
    if (cur_idx >= 0 && strcmp(cur_id, activity_id) != 0) {
        int k;
        for (k = 0; k < act_inv->items[cur_idx].stop_count; k++) {
            cur_step++;
            const struct activity_step *st = &act_inv->items[cur_idx].stop_steps[k];
            if (strcasecmp(st->type, "Delay") == 0) {
                snprintf(step_desc, sizeof(step_desc), "Stop: Delay %dms", st->delay_ms);
            } else {
                snprintf(step_desc, sizeof(step_desc), "Stop: %s", st->command[0] ? st->command : st->type);
            }
            broadcast_activity_progress(activity_id, 1, cur_step, total_steps, step_desc);
            execute_activity_step_list(st, 1);
        }
    }

    /* 2. Power OFF departing devices */
    if (has_ir && cur_idx >= 0 && strcmp(cur_id, activity_id) != 0) {
        struct activity_item *old_act = &act_inv->items[cur_idx];
        struct activity_item *new_act = (target_idx >= 0) ? &act_inv->items[target_idx] : NULL;

        for (i = old_act->device_count - 1; i >= 0; i--) {
            const char *dev_id = old_act->device_ids[i];
            int in_new = 0, j;
            if (new_act) {
                for (j = 0; j < new_act->device_count; j++) {
                    if (strcmp(new_act->device_ids[j], dev_id) == 0) {
                        in_new = 1;
                        break;
                    }
                }
            }
            if (!in_new) {
                if (strcmp(activity_id, "-1") == 0 && target_idx >= 0 && act_inv->items[target_idx].start_count > 0) {
                    int in_poweroff_seq = 0;
                    for (j = 0; j < act_inv->items[target_idx].start_count; j++) {
                        if (strcmp(act_inv->items[target_idx].start_steps[j].device_id, dev_id) == 0) {
                            in_poweroff_seq = 1;
                            break;
                        }
                    }
                    if (in_poweroff_seq) continue;
                }
                for (j = 0; j < ir_inv->device_count; j++) {
                    if (strcmp(ir_inv->devices[j].id, dev_id) == 0) {
                        if (!ir_inv->devices[j].is_power_always_on) {
                            cur_step++;
                            snprintf(step_desc, sizeof(step_desc), "Power off %s", ir_inv->devices[j].name);
                            broadcast_activity_progress(activity_id, 1, cur_step, total_steps, step_desc);
                            execute_device_power_action(&ir_inv->devices[j], 0);
                        }
                        break;
                    }
                }
            }
        }
    }

    /* 3. Power ON arriving devices */
    if (has_ir && target_idx >= 0 && strcmp(activity_id, "-1") != 0) {
        struct activity_item *new_act = &act_inv->items[target_idx];
        struct activity_item *old_act = (cur_idx >= 0 && strcmp(cur_id, "-1") != 0) ? &act_inv->items[cur_idx] : NULL;
        int max_warmup_delay = 0;

        for (i = 0; i < new_act->device_count; i++) {
            const char *dev_id = new_act->device_ids[i];
            int in_old = 0, j;
            if (old_act) {
                for (j = 0; j < old_act->device_count; j++) {
                    if (strcmp(old_act->device_ids[j], dev_id) == 0) {
                        in_old = 1;
                        break;
                    }
                }
            }
            if (!in_old) {
                for (j = 0; j < ir_inv->device_count; j++) {
                    if (strcmp(ir_inv->devices[j].id, dev_id) == 0) {
                        if (!ir_inv->devices[j].is_power_always_on) {
                            cur_step++;
                            snprintf(step_desc, sizeof(step_desc), "Power on %s", ir_inv->devices[j].name);
                            broadcast_activity_progress(activity_id, 1, cur_step, total_steps, step_desc);
                            execute_device_power_action(&ir_inv->devices[j], 1);
                            if (ir_inv->devices[j].power_on_delay > max_warmup_delay) {
                                max_warmup_delay = ir_inv->devices[j].power_on_delay;
                            }
                        }
                        break;
                    }
                }
            }
        }

        if (max_warmup_delay > 0) {
            usleep((useconds_t)max_warmup_delay * 1000);
        }
    }

    /* 4. Start sequence of arriving activity */
    if (target_idx >= 0) {
        int k;
        for (k = 0; k < act_inv->items[target_idx].start_count; k++) {
            cur_step++;
            const struct activity_step *st = &act_inv->items[target_idx].start_steps[k];
            if (strcasecmp(st->type, "Delay") == 0) {
                snprintf(step_desc, sizeof(step_desc), "%s: Delay %dms",
                         strcmp(activity_id, "-1") == 0 ? "PowerOff" : "Setup", st->delay_ms);
            } else {
                snprintf(step_desc, sizeof(step_desc), "%s: %s",
                         strcmp(activity_id, "-1") == 0 ? "PowerOff" : "Setup",
                         st->command[0] ? st->command : st->type);
            }
            broadcast_activity_progress(activity_id, 1, cur_step, total_steps, step_desc);
            execute_activity_step_list(st, 1);
        }
    }

    broadcast_activity_progress(activity_id, strcmp(activity_id, "-1") == 0 ? 0 : 2, total_steps, total_steps, "Complete");

    /* 5. Update persistent state file */
    {
        FILE *act_fp = fopen(CURRENT_ACTIVITY_FILE, "w");
        if (act_fp) {
            fprintf(act_fp, "%s\n", activity_id);
            fclose(act_fp);
        }
    }

    /* 6. Release lock */
    activity_clear_transitioning();

    if (has_ir) free_ir_inventory(ir_inv);
    free(act_inv);
    free(ir_inv);
}

int activity_run_stop(char *out, size_t outlen) {
    return activity_run_start("-1", out, outlen);
}




void render_activities_json(int fd) {
    struct activity_inventory *inv = (struct activity_inventory *)calloc(1, sizeof(*inv));
    if (!inv) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "out of memory");
        send_cjson_resp(fd, "500 Internal Server Error", err);
        cJSON_Delete(err);
        return;
    }
    char trans_target[32] = "";
    int is_trans = activity_is_transitioning(trans_target, sizeof(trans_target));
    if (load_activity_inventory(inv) != 0) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "unable to read activities");
        send_cjson_resp(fd, "500 Internal Server Error", err);
        cJSON_Delete(err);
        free(inv);
        return;
    }
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "ok", 1);
    cJSON_AddStringToObject(root, "currentId", inv->current_id[0] ? inv->current_id : "-1");
    cJSON_AddStringToObject(root, "currentName", inv->current_name[0] ? inv->current_name : "PowerOff");
    int trans_step = 0, trans_total = 0;
    char trans_desc[128] = "";
    if (is_trans) {
        activity_get_transition_step(&trans_step, &trans_total, trans_desc, sizeof(trans_desc));
    }
    cJSON_AddBoolToObject(root, "isTransitioning", is_trans);
    cJSON_AddStringToObject(root, "transitionTarget", trans_target);
    cJSON_AddNumberToObject(root, "transitionStep", trans_step);
    cJSON_AddNumberToObject(root, "transitionTotal", trans_total);
    cJSON_AddStringToObject(root, "transitionDesc", trans_desc);

    cJSON *acts = cJSON_CreateArray();
    for (int i = 0; i < inv->count; i++) {
        struct activity_item *act = &inv->items[i];
        cJSON *a = cJSON_CreateObject();
        cJSON_AddStringToObject(a, "id", act->id);
        cJSON_AddStringToObject(a, "name", act->name);
        cJSON_AddStringToObject(a, "type", act->type);
        cJSON_AddNumberToObject(a, "order", act->order);

        cJSON *dids = cJSON_CreateArray();
        for (int j = 0; j < act->device_count; j++) {
            cJSON_AddItemToArray(dids, cJSON_CreateString(act->device_ids[j]));
        }
        cJSON_AddItemToObject(a, "deviceIds", dids);

        cJSON *starts = cJSON_CreateArray();
        for (int j = 0; j < act->start_count; j++) {
            struct activity_step *st = &act->start_steps[j];
            cJSON *s = cJSON_CreateObject();
            cJSON_AddStringToObject(s, "type", st->type);
            cJSON_AddStringToObject(s, "deviceId", st->device_id);
            cJSON_AddStringToObject(s, "command", st->command);
            cJSON_AddNumberToObject(s, "delay", st->delay_ms);
            cJSON_AddNumberToObject(s, "order", st->order);
            cJSON_AddItemToArray(starts, s);
        }
        cJSON_AddItemToObject(a, "startSteps", starts);

        cJSON *stops = cJSON_CreateArray();
        for (int j = 0; j < act->stop_count; j++) {
            struct activity_step *st = &act->stop_steps[j];
            cJSON *s = cJSON_CreateObject();
            cJSON_AddStringToObject(s, "type", st->type);
            cJSON_AddStringToObject(s, "deviceId", st->device_id);
            cJSON_AddStringToObject(s, "command", st->command);
            cJSON_AddNumberToObject(s, "delay", st->delay_ms);
            cJSON_AddNumberToObject(s, "order", st->order);
            cJSON_AddItemToArray(stops, s);
        }
        cJSON_AddItemToObject(a, "stopSteps", stops);

        cJSON_AddItemToArray(acts, a);
    }
    cJSON_AddItemToObject(root, "activities", acts);

    send_cjson_resp(fd, "200 OK", root);
    cJSON_Delete(root);
    free(inv);
}

void render_activity_start_json(int fd, const struct request *req) {
    char id[64], reply[1024];
    form_value(req->body, "id", id, sizeof(id));
    if (!id[0]) json_string(req->body, "id", id, sizeof(id));
    if (!id[0]) {
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddBoolToObject(resp, "ok", 0);
        cJSON_AddStringToObject(resp, "error", "missing activity id");
        send_cjson_resp(fd, "400 Bad Request", resp);
        cJSON_Delete(resp);
        return;
    }
    int rc = activity_run_start(id, reply, sizeof(reply));
    cJSON *resp = cJSON_CreateObject();
    if (rc == -2) {
        cJSON_AddBoolToObject(resp, "ok", 0);
        cJSON_AddStringToObject(resp, "error", "Activity transition already in progress");
        cJSON_AddStringToObject(resp, "reply", reply[0] ? reply : "Activity switch in progress");
        send_cjson_resp(fd, "409 Conflict", resp);
    } else {
        cJSON_AddBoolToObject(resp, "ok", rc == 0);
        cJSON_AddStringToObject(resp, "id", id);
        cJSON_AddStringToObject(resp, "reply", reply[0] ? reply : "ok");
        send_cjson_resp(fd, rc == 0 ? "200 OK" : "400 Bad Request", resp);
    }
    cJSON_Delete(resp);
    if (rc == 0) {
        shutdown(fd, SHUT_WR);
        activity_run_transition(id);
    }
}

void render_activity_stop_json(int fd) {
    char reply[1024];
    int rc = activity_run_stop(reply, sizeof(reply));
    cJSON *resp = cJSON_CreateObject();
    if (rc == -2) {
        cJSON_AddBoolToObject(resp, "ok", 0);
        cJSON_AddStringToObject(resp, "error", "Activity transition already in progress");
        cJSON_AddStringToObject(resp, "reply", reply[0] ? reply : "Activity switch in progress");
        send_cjson_resp(fd, "409 Conflict", resp);
    } else {
        cJSON_AddBoolToObject(resp, "ok", rc == 0);
        cJSON_AddStringToObject(resp, "reply", reply[0] ? reply : "ok");
        send_cjson_resp(fd, rc == 0 ? "200 OK" : "400 Bad Request", resp);
    }
    cJSON_Delete(resp);
    if (rc == 0) {
        shutdown(fd, SHUT_WR);
        activity_run_transition("-1");
    }
}

void render_activity_save_json(int fd, const struct request *req) {
    char id[32], name[128], type[64], ord_str[32], device_ids[1024], reply[1024], saved_id[32];
    char *start_steps = (char *)calloc(1, 16384);
    char *stop_steps = (char *)calloc(1, 16384);
    int order = 1, rc;

    if (!start_steps || !stop_steps) {
        free(start_steps); free(stop_steps);
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddBoolToObject(resp, "ok", 0);
        cJSON_AddStringToObject(resp, "error", "out of memory");
        send_cjson_resp(fd, "500 Internal Server Error", resp);
        cJSON_Delete(resp);
        return;
    }

    id[0] = name[0] = type[0] = ord_str[0] = device_ids[0] = '\0';
    if (req->body && req->body[0] == '{') {
        json_string(req->body, "id", id, sizeof(id));
        json_string(req->body, "name", name, sizeof(name));
        json_string(req->body, "type", type, sizeof(type));
        json_string(req->body, "order", ord_str, sizeof(ord_str));
        json_string(req->body, "deviceIds", device_ids, sizeof(device_ids));
        json_string(req->body, "startSteps", start_steps, 16384);
        json_string(req->body, "stopSteps", stop_steps, 16384);
    } else {
        form_value(req->body, "id", id, sizeof(id));
        form_value(req->body, "name", name, sizeof(name));
        form_value(req->body, "type", type, sizeof(type));
        form_value(req->body, "order", ord_str, sizeof(ord_str));
        form_value(req->body, "deviceIds", device_ids, sizeof(device_ids));
        form_value(req->body, "startSteps", start_steps, 16384);
        form_value(req->body, "stopSteps", stop_steps, 16384);
    }

    if (ord_str[0]) order = atoi(ord_str);
    if (!type[0]) strcpy(type, "VirtualTelevision");

    reply[0] = saved_id[0] = '\0';
    rc = activity_save(id, name, type, order, device_ids, start_steps, stop_steps, reply, sizeof(reply), saved_id, sizeof(saved_id));
    free(start_steps);
    free(stop_steps);

    cJSON *resp = cJSON_CreateObject();
    if (rc != 0) {
        cJSON_AddBoolToObject(resp, "ok", 0);
        cJSON_AddStringToObject(resp, "error", reply[0] ? reply : "failed to save activity");
        send_cjson_resp(fd, "400 Bad Request", resp);
    } else {
        cJSON_AddBoolToObject(resp, "ok", 1);
        cJSON_AddStringToObject(resp, "id", saved_id[0] ? saved_id : id);
        cJSON_AddStringToObject(resp, "message", reply[0] ? reply : "saved");
        send_cjson_resp(fd, "200 OK", resp);
    }
    cJSON_Delete(resp);
}

void render_activity_delete_json(int fd, const struct request *req) {
    char id[32], reply[1024];
    int rc;

    if (req->body && req->body[0] == '{') {
        json_string(req->body, "id", id, sizeof(id));
    } else {
        form_value(req->body, "id", id, sizeof(id));
    }

    if (!id[0] || strcmp(id, "-1") == 0) {
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddBoolToObject(resp, "ok", 0);
        cJSON_AddStringToObject(resp, "error", "invalid or protected activity id");
        send_cjson_resp(fd, "400 Bad Request", resp);
        cJSON_Delete(resp);
        return;
    }

    rc = activity_delete(id, reply, sizeof(reply));
    cJSON *resp = cJSON_CreateObject();
    if (rc != 0) {
        cJSON_AddBoolToObject(resp, "ok", 0);
        cJSON_AddStringToObject(resp, "error", reply[0] ? reply : "failed to delete activity");
        send_cjson_resp(fd, "400 Bad Request", resp);
    } else {
        cJSON_AddBoolToObject(resp, "ok", 1);
        cJSON_AddStringToObject(resp, "message", reply[0] ? reply : "deleted");
        send_cjson_resp(fd, "200 OK", resp);
    }
    cJSON_Delete(resp);
}

void render_activity_test_sequence_json(int fd, const struct request *req) {
    char *steps = (char *)calloc(1, 16384);
    char reply[1024];

    if (!steps) {
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddBoolToObject(resp, "ok", 0);
        cJSON_AddStringToObject(resp, "error", "out of memory");
        send_cjson_resp(fd, "500 Internal Server Error", resp);
        cJSON_Delete(resp);
        return;
    }

    if (req->body && req->body[0] == '{') {
        json_string(req->body, "steps", steps, 16384);
    } else {
        form_value(req->body, "steps", steps, 16384);
    }

    activity_test_sequence(steps, reply, sizeof(reply));
    free(steps);

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", 1);
    cJSON_AddStringToObject(resp, "message", reply[0] ? reply : "tested");
    send_cjson_resp(fd, "200 OK", resp);
    cJSON_Delete(resp);
}

void handle_activity_start(int fd, const struct request *req) {
    char id[32], reply[1024], msg[1152];
    form_value(req->body, "id", id, sizeof(id));
    int rc = activity_run_start(id, reply, sizeof(reply));
    snprintf(msg, sizeof(msg), "Started activity %s: %s", id[0] ? id : "PowerOff", reply[0] ? reply : "ok");
    render_page(fd, req, msg);
    if (rc == 0) {
        shutdown(fd, SHUT_WR);
        activity_run_transition(id);
    }
}

void handle_activity_stop(int fd, const struct request *req) {
    char reply[1024], msg[1152];
    int rc = activity_run_stop(reply, sizeof(reply));
    snprintf(msg, sizeof(msg), "Stopped activity (PowerOff): %s", reply[0] ? reply : "ok");
    render_page(fd, req, msg);
    if (rc == 0) {
        shutdown(fd, SHUT_WR);
        activity_run_transition("-1");
    }
}

void handle_activity_save(int fd, const struct request *req) {
    char id[32], name[128], type[64], ord_str[32], device_ids[1024], reply[1024], saved_id[32], msg[1200];
    char *start_steps = (char *)calloc(1, 16384);
    char *stop_steps = (char *)calloc(1, 16384);
    int order = 1, rc;

    if (!start_steps || !stop_steps) {
        free(start_steps); free(stop_steps);
        render_page(fd, req, "Out of memory.");
        return;
    }

    device_ids[0] = '\0';
    form_value(req->body, "id", id, sizeof(id));
    form_value(req->body, "name", name, sizeof(name));
    form_value(req->body, "type", type, sizeof(type));
    form_value(req->body, "order", ord_str, sizeof(ord_str));
    form_value(req->body, "deviceIds", device_ids, sizeof(device_ids));
    form_value(req->body, "startSteps", start_steps, 16384);
    form_value(req->body, "stopSteps", stop_steps, 16384);
    if (ord_str[0]) order = atoi(ord_str);

    rc = activity_save(id, name, type, order, device_ids, start_steps, stop_steps, reply, sizeof(reply), saved_id, sizeof(saved_id));
    free(start_steps);
    free(stop_steps);

    snprintf(msg, sizeof(msg), "Activity %s: %s", rc == 0 ? "saved" : "save failed", reply[0] ? reply : "");
    render_page(fd, req, msg);
}

void handle_activity_delete(int fd, const struct request *req) {
    char id[32], reply[1024], msg[1152];
    form_value(req->body, "id", id, sizeof(id));
    activity_delete(id, reply, sizeof(reply));
    snprintf(msg, sizeof(msg), "Activity deletion: %s", reply[0] ? reply : "");
    render_page(fd, req, msg);
}

