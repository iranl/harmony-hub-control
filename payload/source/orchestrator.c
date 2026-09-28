#include "orchestrator.h"
#include "hw_action.h"
#include "cJSON.h"
#include "webui_activity.h"
#include "webui_ir.h"
#include "webui_bt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/file.h>

#define CURRENT_ACT_FILE "/data/codex/current_activity"

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
            int rc = hw_device_command_send(step->device_id, step->command_name);
            if (rc != 0 && (strncmp(step->command_name, "0000 ", 5) == 0 ||
                            step->command_name[0] == 'F' || step->command_name[0] == 'f')) {
                hw_ir_send_harmony_keycode(step->command_name, IR_PORT_ALL, 3);
            }
        }
        break;
    case STEP_ACT_BTHID:
        if (step->command_name[0]) {
            char bt_reply[256];
            run_bt_saved_script("btkeyboard", "", step->command_name, 50, bt_reply, sizeof(bt_reply));
        }
        break;
    case STEP_ACT_DELAY:
    default:
        break;
    }
}

int orch_tick(void) {
    if (!orch_is_busy() || g_queue_len == 0) {
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
        g_progress_cb(g_current_act, g_state, 0, 0, "Transition completed", g_cb_user_data);
    }
    return 0;
}

static int orch_compile_transition(const char *target_id) {
    if (!target_id || !target_id[0]) target_id = ORCH_POWEROFF_ID;

    struct activity_inventory *act_inv = (struct activity_inventory *)calloc(1, sizeof(*act_inv));
    struct ir_inventory *ir_inv = (struct ir_inventory *)calloc(1, sizeof(*ir_inv));
    if (!act_inv || !ir_inv) {
        free(act_inv);
        free(ir_inv);
        return -1;
    }

    if (load_activity_inventory(act_inv) != 0) {
        free(act_inv);
        free(ir_inv);
        return -1;
    }

    int has_ir = (load_ir_inventory(ir_inv) == 0);

    int cur_idx = -1, target_idx = -1;
    for (int i = 0; i < act_inv->count; i++) {
        if (g_current_act[0] && strcmp(act_inv->items[i].id, g_current_act) == 0) cur_idx = i;
        if (strcmp(act_inv->items[i].id, target_id) == 0) target_idx = i;
    }

    orch_step_t steps[ORCH_MAX_STEPS];
    size_t count = 0;

    /* 1. Stop sequence of departing activity */
    if (cur_idx >= 0 && strcmp(g_current_act, target_id) != 0) {
        struct activity_item *old_act = &act_inv->items[cur_idx];
        for (int k = 0; k < old_act->stop_count && count < ORCH_MAX_STEPS; k++) {
            const struct activity_step *st = &old_act->stop_steps[k];
            memset(&steps[count], 0, sizeof(orch_step_t));
            if (strcasecmp(st->type, "Delay") == 0) {
                steps[count].type = STEP_ACT_DELAY;
                steps[count].delay_ms = st->delay_ms ? (uint32_t)st->delay_ms : 50;
                snprintf(steps[count].description, sizeof(steps[count].description), "Stop: Delay %dms", st->delay_ms);
            } else if (strcasecmp(st->type, "BTCommand") == 0) {
                steps[count].type = STEP_ACT_BTHID;
                strncpy(steps[count].command_name, st->command, sizeof(steps[count].command_name) - 1);
                steps[count].delay_ms = st->delay_ms > 0 ? (uint32_t)st->delay_ms : 250;
                snprintf(steps[count].description, sizeof(steps[count].description), "Stop: %s", st->command);
            } else {
                steps[count].type = STEP_ACT_DEVICE_CMD;
                strncpy(steps[count].device_id, st->device_id, sizeof(steps[count].device_id) - 1);
                strncpy(steps[count].command_name, st->command, sizeof(steps[count].command_name) - 1);
                steps[count].delay_ms = st->delay_ms > 0 ? (uint32_t)st->delay_ms : 250;
                snprintf(steps[count].description, sizeof(steps[count].description), "Stop: %s", st->command[0] ? st->command : st->type);
            }
            count++;
        }
    }

    /* 2. Power OFF departing devices (not present in new activity) */
    if (has_ir && cur_idx >= 0 && strcmp(g_current_act, target_id) != 0) {
        struct activity_item *old_act = &act_inv->items[cur_idx];
        struct activity_item *new_act = (target_idx >= 0) ? &act_inv->items[target_idx] : NULL;

        for (int i = old_act->device_count - 1; i >= 0 && count < ORCH_MAX_STEPS; i--) {
            const char *dev_id = old_act->device_ids[i];
            int in_new = 0;
            if (new_act) {
                for (int j = 0; j < new_act->device_count; j++) {
                    if (strcmp(new_act->device_ids[j], dev_id) == 0) { in_new = 1; break; }
                }
            }
            if (!in_new) {
                if (strcmp(target_id, ORCH_POWEROFF_ID) == 0 && target_idx >= 0 && act_inv->items[target_idx].start_count > 0) {
                    int in_poweroff_seq = 0;
                    for (int j = 0; j < act_inv->items[target_idx].start_count; j++) {
                        if (strcmp(act_inv->items[target_idx].start_steps[j].device_id, dev_id) == 0) {
                            in_poweroff_seq = 1;
                            break;
                        }
                    }
                    if (in_poweroff_seq) continue;
                }

                struct ir_device *dev = NULL;
                for (int j = 0; j < ir_inv->device_count; j++) {
                    if (strcmp(ir_inv->devices[j].id, dev_id) == 0) {
                        dev = &ir_inv->devices[j];
                        break;
                    }
                }
                if (!dev || dev->is_power_always_on) continue;

                if (dev->power_off_count > 0 && dev->power_off_steps) {
                    for (int k = 0; k < dev->power_off_count && count < ORCH_MAX_STEPS; k++) {
                        const struct activity_step *st = &dev->power_off_steps[k];
                        memset(&steps[count], 0, sizeof(orch_step_t));
                        if (strcasecmp(st->type, "Delay") == 0) {
                            steps[count].type = STEP_ACT_DELAY;
                            steps[count].delay_ms = st->delay_ms ? (uint32_t)st->delay_ms : 50;
                            snprintf(steps[count].description, sizeof(steps[count].description), "Power off %s: Delay %dms", dev->name, st->delay_ms);
                        } else if (strcasecmp(st->type, "BTCommand") == 0) {
                            steps[count].type = STEP_ACT_BTHID;
                            strncpy(steps[count].command_name, st->command, sizeof(steps[count].command_name) - 1);
                            steps[count].delay_ms = st->delay_ms > 0 ? (uint32_t)st->delay_ms : 250;
                            snprintf(steps[count].description, sizeof(steps[count].description), "Power off %s: %s", dev->name, st->command);
                        } else {
                            steps[count].type = STEP_ACT_DEVICE_CMD;
                            strncpy(steps[count].device_id, st->device_id, sizeof(steps[count].device_id) - 1);
                            strncpy(steps[count].command_name, st->command, sizeof(steps[count].command_name) - 1);
                            steps[count].delay_ms = st->delay_ms > 0 ? (uint32_t)st->delay_ms : 250;
                            snprintf(steps[count].description, sizeof(steps[count].description), "Power off %s: %s", dev->name, st->command);
                        }
                        count++;
                    }
                } else {
                    const char *cmd_name = NULL;
                    for (int k = 0; k < dev->command_count; k++) {
                        if (strcasecmp(dev->commands[k].name, "PowerOff") == 0) {
                            cmd_name = dev->commands[k].name;
                            break;
                        }
                    }
                    if (!cmd_name) {
                        for (int k = 0; k < dev->command_count; k++) {
                            if (strcasecmp(dev->commands[k].name, "PowerToggle") == 0 ||
                                strcasecmp(dev->commands[k].name, "Power") == 0) {
                                cmd_name = dev->commands[k].name;
                                break;
                            }
                        }
                    }
                    if (cmd_name && count < ORCH_MAX_STEPS) {
                        memset(&steps[count], 0, sizeof(orch_step_t));
                        steps[count].type = STEP_ACT_DEVICE_CMD;
                        strncpy(steps[count].device_id, dev->id, sizeof(steps[count].device_id) - 1);
                        strncpy(steps[count].command_name, cmd_name, sizeof(steps[count].command_name) - 1);
                        steps[count].delay_ms = dev->inter_device_delay > 0 ? (uint32_t)dev->inter_device_delay : 250;
                        snprintf(steps[count].description, sizeof(steps[count].description), "Power off %s", dev->name);
                        count++;
                    }
                }
            }
        }
    }

    /* 3. Power ON arriving devices (not present in old activity) */
    if (has_ir && target_idx >= 0 && strcmp(target_id, ORCH_POWEROFF_ID) != 0) {
        struct activity_item *new_act = &act_inv->items[target_idx];
        struct activity_item *old_act = (cur_idx >= 0 && strcmp(g_current_act, ORCH_POWEROFF_ID) != 0) ? &act_inv->items[cur_idx] : NULL;
        int max_warmup = 0;

        for (int i = 0; i < new_act->device_count && count < ORCH_MAX_STEPS; i++) {
            const char *dev_id = new_act->device_ids[i];
            int in_old = 0;
            if (old_act) {
                for (int j = 0; j < old_act->device_count; j++) {
                    if (strcmp(old_act->device_ids[j], dev_id) == 0) { in_old = 1; break; }
                }
            }
            if (!in_old) {
                struct ir_device *dev = NULL;
                for (int j = 0; j < ir_inv->device_count; j++) {
                    if (strcmp(ir_inv->devices[j].id, dev_id) == 0) {
                        dev = &ir_inv->devices[j];
                        break;
                    }
                }
                if (!dev || dev->is_power_always_on) continue;

                if (dev->power_on_count > 0 && dev->power_on_steps) {
                    for (int k = 0; k < dev->power_on_count && count < ORCH_MAX_STEPS; k++) {
                        const struct activity_step *st = &dev->power_on_steps[k];
                        memset(&steps[count], 0, sizeof(orch_step_t));
                        if (strcasecmp(st->type, "Delay") == 0) {
                            steps[count].type = STEP_ACT_DELAY;
                            steps[count].delay_ms = st->delay_ms ? (uint32_t)st->delay_ms : 50;
                            snprintf(steps[count].description, sizeof(steps[count].description), "Power on %s: Delay %dms", dev->name, st->delay_ms);
                        } else if (strcasecmp(st->type, "BTCommand") == 0) {
                            steps[count].type = STEP_ACT_BTHID;
                            strncpy(steps[count].command_name, st->command, sizeof(steps[count].command_name) - 1);
                            steps[count].delay_ms = st->delay_ms > 0 ? (uint32_t)st->delay_ms : 250;
                            snprintf(steps[count].description, sizeof(steps[count].description), "Power on %s: %s", dev->name, st->command);
                        } else {
                            steps[count].type = STEP_ACT_DEVICE_CMD;
                            strncpy(steps[count].device_id, st->device_id, sizeof(steps[count].device_id) - 1);
                            strncpy(steps[count].command_name, st->command, sizeof(steps[count].command_name) - 1);
                            steps[count].delay_ms = st->delay_ms > 0 ? (uint32_t)st->delay_ms : 250;
                            snprintf(steps[count].description, sizeof(steps[count].description), "Power on %s: %s", dev->name, st->command);
                        }
                        count++;
                    }
                } else {
                    const char *cmd_name = NULL;
                    for (int k = 0; k < dev->command_count; k++) {
                        if (strcasecmp(dev->commands[k].name, "PowerOn") == 0) {
                            cmd_name = dev->commands[k].name;
                            break;
                        }
                    }
                    if (!cmd_name) {
                        for (int k = 0; k < dev->command_count; k++) {
                            if (strcasecmp(dev->commands[k].name, "PowerToggle") == 0 ||
                                strcasecmp(dev->commands[k].name, "Power") == 0) {
                                cmd_name = dev->commands[k].name;
                                break;
                            }
                        }
                    }
                    if (cmd_name && count < ORCH_MAX_STEPS) {
                        memset(&steps[count], 0, sizeof(orch_step_t));
                        steps[count].type = STEP_ACT_DEVICE_CMD;
                        strncpy(steps[count].device_id, dev->id, sizeof(steps[count].device_id) - 1);
                        strncpy(steps[count].command_name, cmd_name, sizeof(steps[count].command_name) - 1);
                        steps[count].delay_ms = dev->inter_device_delay > 0 ? (uint32_t)dev->inter_device_delay : 250;
                        snprintf(steps[count].description, sizeof(steps[count].description), "Power on %s", dev->name);
                        count++;
                    }
                }
                if (dev->power_on_delay > max_warmup) {
                    max_warmup = dev->power_on_delay;
                }
            }
        }
        if (max_warmup > 0 && count < ORCH_MAX_STEPS) {
            memset(&steps[count], 0, sizeof(orch_step_t));
            steps[count].type = STEP_ACT_DELAY;
            steps[count].delay_ms = (uint32_t)max_warmup;
            snprintf(steps[count].description, sizeof(steps[count].description), "Warmup delay %dms", max_warmup);
            count++;
        }
    }

    /* 4. Start sequence of arriving activity */
    if (target_idx >= 0) {
        struct activity_item *new_act = &act_inv->items[target_idx];
        for (int k = 0; k < new_act->start_count && count < ORCH_MAX_STEPS; k++) {
            const struct activity_step *st = &new_act->start_steps[k];
            memset(&steps[count], 0, sizeof(orch_step_t));
            if (strcasecmp(st->type, "Delay") == 0) {
                steps[count].type = STEP_ACT_DELAY;
                steps[count].delay_ms = st->delay_ms ? (uint32_t)st->delay_ms : 50;
                snprintf(steps[count].description, sizeof(steps[count].description), "%s: Delay %dms",
                         strcmp(target_id, ORCH_POWEROFF_ID) == 0 ? "PowerOff" : "Setup", st->delay_ms);
            } else if (strcasecmp(st->type, "BTCommand") == 0) {
                steps[count].type = STEP_ACT_BTHID;
                strncpy(steps[count].command_name, st->command, sizeof(steps[count].command_name) - 1);
                steps[count].delay_ms = st->delay_ms > 0 ? (uint32_t)st->delay_ms : 250;
                snprintf(steps[count].description, sizeof(steps[count].description), "%s: %s",
                         strcmp(target_id, ORCH_POWEROFF_ID) == 0 ? "PowerOff" : "Setup", st->command);
            } else {
                steps[count].type = STEP_ACT_DEVICE_CMD;
                strncpy(steps[count].device_id, st->device_id, sizeof(steps[count].device_id) - 1);
                strncpy(steps[count].command_name, st->command, sizeof(steps[count].command_name) - 1);
                steps[count].delay_ms = st->delay_ms > 0 ? (uint32_t)st->delay_ms : 250;
                snprintf(steps[count].description, sizeof(steps[count].description), "%s: %s",
                         strcmp(target_id, ORCH_POWEROFF_ID) == 0 ? "PowerOff" : "Setup",
                         st->command[0] ? st->command : st->type);
            }
            count++;
        }
    }

    if (has_ir) free_ir_inventory(ir_inv);
    free(act_inv);
    free(ir_inv);

    return orch_enqueue_steps(steps, count);
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

    int rc = orch_compile_transition(target_activity_id);
    if (rc != 0) {
        g_state = (strcmp(g_current_act, ORCH_POWEROFF_ID) == 0) ? ORCH_STATE_IDLE : ORCH_STATE_RUNNING;
        return -1;
    }

    if (g_queue_len == 0) {
        strncpy(g_current_act, g_target_act, sizeof(g_current_act) - 1);
        save_current_activity(g_current_act);
        g_state = ORCH_STATE_RUNNING;
        if (g_progress_cb) {
            g_progress_cb(g_current_act, g_state, 0, 0, "Transition completed", g_cb_user_data);
        }
        return 0;
    }

    if (g_progress_cb) {
        g_progress_cb(g_target_act, g_state, 0, (int)g_queue_len, "Starting switch...", g_cb_user_data);
    }
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

    int rc = orch_compile_transition(ORCH_POWEROFF_ID);
    if (rc != 0) {
        g_state = (strcmp(g_current_act, ORCH_POWEROFF_ID) == 0) ? ORCH_STATE_IDLE : ORCH_STATE_RUNNING;
        return -1;
    }

    if (g_queue_len == 0) {
        strncpy(g_current_act, ORCH_POWEROFF_ID, sizeof(g_current_act) - 1);
        save_current_activity(g_current_act);
        g_state = ORCH_STATE_IDLE;
        if (g_progress_cb) {
            g_progress_cb(g_current_act, g_state, 0, 0, "Transition completed", g_cb_user_data);
        }
        return 0;
    }

    if (g_progress_cb) {
        g_progress_cb(g_target_act, g_state, 0, (int)g_queue_len, "Starting switch...", g_cb_user_data);
    }
    return 0;
}
