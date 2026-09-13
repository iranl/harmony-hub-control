#include "orchestrator.h"
#include "hw_action.h"
#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define CURRENT_ACT_FILE "/data/codex/current_activity"
#define ACTIVITY_LOCK_FILE "/tmp/codex_activity_transition"

static orch_state_t g_state = ORCH_STATE_IDLE;
static char g_current_act[32] = ORCH_POWEROFF_ID;
static char g_target_act[32] = ORCH_POWEROFF_ID;

static orch_step_t g_queue[ORCH_MAX_STEPS];
static size_t g_queue_len = 0;
static size_t g_current_step = 0;
static uint64_t g_next_deadline_ms = 0;

static orch_progress_cb g_progress_cb = NULL;
static void *g_cb_user_data = NULL;

static uint64_t get_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

static void save_current_activity(const char *act_id) {
    FILE *f = fopen(CURRENT_ACT_FILE, "w");
    if (f) {
        fprintf(f, "%s\n", act_id);
        fclose(f);
    }
}

static void load_current_activity(void) {
    FILE *f = fopen(CURRENT_ACT_FILE, "r");
    if (f) {
        if (fscanf(f, "%31s", g_current_act) != 1) {
            strncpy(g_current_act, ORCH_POWEROFF_ID, sizeof(g_current_act) - 1);
        }
        fclose(f);
    } else {
        strncpy(g_current_act, ORCH_POWEROFF_ID, sizeof(g_current_act) - 1);
    }
}

int orch_init(void) {
    load_current_activity();
    g_state = (strcmp(g_current_act, ORCH_POWEROFF_ID) == 0) ? ORCH_STATE_IDLE : ORCH_STATE_RUNNING;
    g_queue_len = 0;
    g_current_step = 0;
    g_next_deadline_ms = 0;
    return 0;
}

void orch_set_progress_callback(orch_progress_cb cb, void *user_data) {
    g_progress_cb = cb;
    g_cb_user_data = user_data;
}

int orch_is_busy(void) {
    if (access(ACTIVITY_LOCK_FILE, F_OK) == 0) return 1;
    return (g_state == ORCH_STATE_STARTING || g_state == ORCH_STATE_STOPPING);
}

const char *orch_get_current_activity(void) {
    if (!orch_is_busy()) {
        load_current_activity();
    }
    return g_current_act;
}

orch_state_t orch_get_state(void) {
    if (orch_is_busy()) {
        return (strcmp(g_target_act, ORCH_POWEROFF_ID) == 0) ? ORCH_STATE_STOPPING : ORCH_STATE_STARTING;
    }
    load_current_activity();
    return (strcmp(g_current_act, ORCH_POWEROFF_ID) == 0) ? ORCH_STATE_IDLE : ORCH_STATE_RUNNING;
}

void orch_cancel(void) {
    hw_ir_cancel(IR_PORT_ALL);
    g_queue_len = 0;
    g_current_step = 0;
    load_current_activity();
    g_state = (strcmp(g_current_act, ORCH_POWEROFF_ID) == 0) ? ORCH_STATE_IDLE : ORCH_STATE_RUNNING;
    if (g_progress_cb) {
        g_progress_cb(g_current_act, g_state, 0, 0, "Sequence cancelled", g_cb_user_data);
    }
}

int orch_enqueue_steps(const orch_step_t *steps, size_t count) {
    if (count > ORCH_MAX_STEPS) return -1;
    memcpy(g_queue, steps, count * sizeof(orch_step_t));
    g_queue_len = count;
    g_current_step = 0;
    g_next_deadline_ms = get_now_ms();
    return 0;
}

static void execute_step(const orch_step_t *step) {
    switch (step->type) {
    case STEP_ACT_IR_PRONTO:
        if (step->pronto_hex[0]) {
            hw_ir_send_pronto(step->pronto_hex, step->ir_ports, step->ir_repeats);
        }
        break;
    case STEP_ACT_DEVICE_CMD:
        if (step->device_id[0] && step->command_name[0]) {
            hw_device_command_send(step->device_id, step->command_name);
        }
        break;
    case STEP_ACT_BTHID:
        break;
    case STEP_ACT_DELAY:
    default:
        break;
    }
}

int orch_tick(void) {
    if (!orch_is_busy() || g_queue_len == 0) {
        if (access(ACTIVITY_LOCK_FILE, F_OK) != 0 &&
            (g_state == ORCH_STATE_STARTING || g_state == ORCH_STATE_STOPPING)) {
            load_current_activity();
            g_state = (strcmp(g_current_act, ORCH_POWEROFF_ID) == 0) ? ORCH_STATE_IDLE : ORCH_STATE_RUNNING;
            if (g_progress_cb) {
                g_progress_cb(g_current_act, g_state, 0, 0, "Transition completed", g_cb_user_data);
            }
        }
        return 0;
    }

    uint64_t now = get_now_ms();
    if (now < g_next_deadline_ms) {
        return 1;
    }

    if (g_current_step < g_queue_len) {
        const orch_step_t *step = &g_queue[g_current_step];
        execute_step(step);

        uint32_t delay = step->delay_ms ? step->delay_ms : 50;
        g_next_deadline_ms = now + delay;

        if (g_progress_cb) {
            g_progress_cb(g_target_act, g_state, (int)g_current_step + 1,
                          (int)g_queue_len, step->description, g_cb_user_data);
        }
        g_current_step++;
        return 1;
    }

    strncpy(g_current_act, g_target_act, sizeof(g_current_act) - 1);
    save_current_activity(g_current_act);

    g_state = (strcmp(g_current_act, ORCH_POWEROFF_ID) == 0) ? ORCH_STATE_IDLE : ORCH_STATE_RUNNING;
    g_queue_len = 0;
    g_current_step = 0;

    if (g_progress_cb) {
        g_progress_cb(g_current_act, g_state, (int)g_queue_len, (int)g_queue_len,
                      "Transition completed", g_cb_user_data);
    }
    return 0;
}

static int trigger_webui_activity(const char *target_id) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_in addr;
    struct timeval tv = { .tv_sec = 3, .tv_usec = 0 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        return -1;
    }
    const char *endpoint = (strcmp(target_id, ORCH_POWEROFF_ID) == 0) ? "/api/activity-stop" : "/api/activity-start";
    cJSON *jb = cJSON_CreateObject();
    if (strcmp(target_id, ORCH_POWEROFF_ID) != 0) {
        cJSON_AddStringToObject(jb, "id", target_id);
    }
    char *body = cJSON_PrintUnformatted(jb);
    cJSON_Delete(jb);

    char req[512];
    snprintf(req, sizeof(req),
             "POST %s HTTP/1.1\r\n"
             "Host: 127.0.0.1:8080\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n\r\n%s",
             endpoint, body ? strlen(body) : 0, body ? body : "");
    if (body) free(body);
    send(fd, req, strlen(req), MSG_NOSIGNAL);
    char resp[512] = {0};
    ssize_t r = recv(fd, resp, sizeof(resp) - 1, 0);
    close(fd);
    if (r > 0 && strstr(resp, "409 Conflict")) return -2;
    if (r > 0 && strstr(resp, "200 OK")) return 0;
    return -1;
}

int orch_switch_activity(const char *target_activity_id, int dry_run,
                         char *preview_json, size_t preview_len) {
    if (!target_activity_id || !target_activity_id[0]) return -1;
    if (orch_is_busy()) return -2;

    if (strcmp(target_activity_id, ORCH_POWEROFF_ID) == 0) {
        return orch_power_off(dry_run, preview_json, preview_len);
    }

    if (dry_run) {
        if (preview_json && preview_len > 0) {
            cJSON *pj = cJSON_CreateObject();
            cJSON_AddStringToObject(pj, "target", target_activity_id);
            cJSON_AddArrayToObject(pj, "steps");
            char *pstr = cJSON_PrintUnformatted(pj);
            if (pstr) {
                snprintf(preview_json, preview_len, "%s", pstr);
                free(pstr);
            }
            cJSON_Delete(pj);
        }
        return 0;
    }

    strncpy(g_target_act, target_activity_id, sizeof(g_target_act) - 1);
    g_state = ORCH_STATE_STARTING;
    int rc = trigger_webui_activity(target_activity_id);
    if (rc == 0) {
        return 0;
    }
    if (rc == -2) {
        g_state = (strcmp(g_current_act, ORCH_POWEROFF_ID) == 0) ? ORCH_STATE_IDLE : ORCH_STATE_RUNNING;
        return -2;
    }
    save_current_activity(target_activity_id);
    strncpy(g_current_act, target_activity_id, sizeof(g_current_act) - 1);
    g_state = ORCH_STATE_RUNNING;
    return 0;
}

int orch_power_off(int dry_run, char *preview_json, size_t preview_len) {
    if (orch_is_busy()) return -2;

    if (dry_run) {
        if (preview_json && preview_len > 0) {
            cJSON *pj = cJSON_CreateObject();
            cJSON_AddStringToObject(pj, "target", ORCH_POWEROFF_ID);
            cJSON_AddArrayToObject(pj, "steps");
            char *pstr = cJSON_PrintUnformatted(pj);
            if (pstr) {
                snprintf(preview_json, preview_len, "%s", pstr);
                free(pstr);
            }
            cJSON_Delete(pj);
        }
        return 0;
    }

    strncpy(g_target_act, ORCH_POWEROFF_ID, sizeof(g_target_act) - 1);
    g_state = ORCH_STATE_STOPPING;
    int rc = trigger_webui_activity(ORCH_POWEROFF_ID);
    if (rc == 0) {
        return 0;
    }
    if (rc == -2) {
        g_state = (strcmp(g_current_act, ORCH_POWEROFF_ID) == 0) ? ORCH_STATE_IDLE : ORCH_STATE_RUNNING;
        return -2;
    }
    save_current_activity(ORCH_POWEROFF_ID);
    strncpy(g_current_act, ORCH_POWEROFF_ID, sizeof(g_current_act) - 1);
    g_state = ORCH_STATE_IDLE;
    return 0;
}
