#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <dirent.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>
#include "cJSON.h"
#include "codex_webui_types.h"
#include "resource_cache.h"
#include "hw_action.h"
#include "ir_encoder.h"
#include "ir_i2s.h"
#include "webui_utils.h"
#include "webui_ir.h"
#include "webui_config.h"
#include "webui_html.h"

cJSON *find_device_item(cJSON *root, const char *device_id) {
    if (!root || !device_id || !device_id[0]) return NULL;
    cJSON *dwf = cJSON_GetObjectItemCaseSensitive(root, "DevicesWithFeatures");
    if (!dwf || !cJSON_IsArray(dwf)) return NULL;
    long did = atol(device_id);
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, dwf) {
        cJSON *dev = cJSON_GetObjectItemCaseSensitive(item, "Device");
        if (!dev) continue;
        cJSON *jid = cJSON_GetObjectItemCaseSensitive(dev, "Id-");
        if (!jid) jid = cJSON_GetObjectItemCaseSensitive(dev, "Id");
        if (jid) {
            if (cJSON_IsNumber(jid) && (long)jid->valuedouble == did) return item;
            if (cJSON_IsString(jid) && jid->valuestring && strcmp(jid->valuestring, device_id) == 0) return item;
        }
    }
    return NULL;
}


void parse_power_actions_cjson(cJSON *arr, const char *device_id, struct activity_step **steps_out, int *count_out) {
    *steps_out = NULL;
    *count_out = 0;
    if (!arr || !cJSON_IsArray(arr)) return;
    int size = cJSON_GetArraySize(arr);
    if (size <= 0) return;
    struct activity_step *steps = (struct activity_step *)calloc((size_t)size, sizeof(*steps));
    if (!steps) return;
    int cnt = 0;
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, arr) {
        if (device_id && device_id[0]) snprintf(steps[cnt].device_id, sizeof(steps[cnt].device_id), "%s", device_id);
        cJSON *order = cJSON_GetObjectItemCaseSensitive(item, "Order");
        steps[cnt].order = order && cJSON_IsNumber(order) ? order->valueint : cnt;

        cJSON *type = cJSON_GetObjectItemCaseSensitive(item, "__type");
        const char *tstr = (type && cJSON_IsString(type)) ? type->valuestring : "";
        if (strcmp(tstr, "IRDelayAction") == 0 || strcasecmp(tstr, "Delay") == 0) {
            strcpy(steps[cnt].type, "Delay");
            cJSON *del = cJSON_GetObjectItemCaseSensitive(item, "Delay");
            steps[cnt].delay_ms = del && cJSON_IsNumber(del) ? del->valueint : 1000;
            cnt++;
        } else {
            cJSON *cmd = cJSON_GetObjectItemCaseSensitive(item, "IRCommandName");
            if (!cmd || !cJSON_IsString(cmd) || !cmd->valuestring[0]) cmd = cJSON_GetObjectItemCaseSensitive(item, "CommandName");
            if (cmd && cJSON_IsString(cmd) && cmd->valuestring[0]) {
                strcpy(steps[cnt].type, "IRCommand");
                snprintf(steps[cnt].command, sizeof(steps[cnt].command), "%s", cmd->valuestring);
                cJSON *dur = cJSON_GetObjectItemCaseSensitive(item, "Duration");
                if (!dur || !cJSON_IsNumber(dur)) dur = cJSON_GetObjectItemCaseSensitive(item, "Delay");
                if (dur && cJSON_IsNumber(dur) && dur->valueint > 0) steps[cnt].delay_ms = dur->valueint;
                cnt++;
            }
        }
    }
    *steps_out = steps;
    *count_out = cnt;
}

int scan_ir_resource_stats(int *device_count, int *total_commands, long *max_device_id, long *max_command_id) {
    if (device_count) *device_count = 0;
    if (total_commands) *total_commands = 0;
    if (max_device_id) *max_device_id = 0;
    if (max_command_id) *max_command_id = 0;

    char *raw = read_file_alloc(DEVICE_LIST, MAX_RESOURCE_FILE, NULL);
    if (!raw) return -1;
    cJSON *root = cJSON_Parse(raw);
    free(raw);
    if (!root) return -1;

    cJSON *dwf = cJSON_GetObjectItemCaseSensitive(root, "DevicesWithFeatures");
    if (dwf && cJSON_IsArray(dwf)) {
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, dwf) {
            cJSON *d = cJSON_GetObjectItemCaseSensitive(item, "Device");
            if (!d) continue;
            cJSON *jid = cJSON_GetObjectItemCaseSensitive(d, "Id-");
            long did = (jid && cJSON_IsNumber(jid)) ? (long)jid->valuedouble : 0;
            if (did > 0) {
                if (device_count) (*device_count)++;
                if (max_device_id && did > *max_device_id) *max_device_id = did;
            }
            cJSON *cmds = cJSON_GetObjectItemCaseSensitive(item, "Commands");
            if (cmds && cJSON_IsArray(cmds)) {
                if (total_commands) *total_commands += cJSON_GetArraySize(cmds);
                if (max_command_id) {
                    cJSON *c = NULL;
                    cJSON_ArrayForEach(c, cmds) {
                        cJSON *cid = cJSON_GetObjectItemCaseSensitive(c, "Id-");
                        long cnum = (cid && cJSON_IsNumber(cid)) ? (long)cid->valuedouble : 0;
                        if (cnum > *max_command_id) *max_command_id = cnum;
                    }
                }
            }
        }
    }
    cJSON_Delete(root);
    return 0;
}

int load_ir_inventory(struct ir_inventory *inv) {
    memset(inv, 0, sizeof(*inv));
    char *raw = read_file_alloc(DEVICE_LIST, MAX_RESOURCE_FILE, NULL);
    if (!raw) return -1;
    cJSON *root = cJSON_Parse(raw);
    free(raw);
    if (!root) return -1;

    cJSON *dwf = cJSON_GetObjectItemCaseSensitive(root, "DevicesWithFeatures");
    if (dwf && cJSON_IsArray(dwf)) {
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, dwf) {
            if (inv->device_count >= MAX_IR_DEVICES) break;
            cJSON *d = cJSON_GetObjectItemCaseSensitive(item, "Device");
            if (!d) continue;
            cJSON *jid = cJSON_GetObjectItemCaseSensitive(d, "Id-");
            long id = jid && cJSON_IsNumber(jid) ? (long)jid->valuedouble : 0;
            if (id <= 0) continue;

            struct ir_device *dev = &inv->devices[inv->device_count];
            snprintf(dev->id, sizeof(dev->id), "%ld", id);
            if (id > inv->max_device_id) inv->max_device_id = id;

            cJSON *name = cJSON_GetObjectItemCaseSensitive(d, "Name");
            if (name && cJSON_IsString(name) && name->valuestring) copy_text(dev->name, sizeof(dev->name), name->valuestring);

            cJSON *mfg = cJSON_GetObjectItemCaseSensitive(d, "Manufacturer");
            if (mfg && cJSON_IsString(mfg) && mfg->valuestring) copy_text(dev->manufacturer, sizeof(dev->manufacturer), mfg->valuestring);

            cJSON *model = cJSON_GetObjectItemCaseSensitive(d, "Model");
            if (model && cJSON_IsString(model) && model->valuestring) copy_text(dev->model, sizeof(dev->model), model->valuestring);

            cJSON *type = cJSON_GetObjectItemCaseSensitive(d, "DeviceTypeDisplayName");
            if (type && cJSON_IsString(type) && type->valuestring) copy_text(dev->type, sizeof(dev->type), type->valuestring);

            cJSON *cp = cJSON_GetObjectItemCaseSensitive(d, "ControlPort");
            dev->control_port = cp && cJSON_IsNumber(cp) ? cp->valueint : 7;

            cJSON *tr = cJSON_GetObjectItemCaseSensitive(d, "Transport");
            dev->transport = tr && cJSON_IsNumber(tr) ? tr->valueint : 1;

            cJSON *idd = cJSON_GetObjectItemCaseSensitive(d, "InterDeviceDelay");
            dev->inter_device_delay = idd && cJSON_IsNumber(idd) ? idd->valueint : 100;

            struct device_mqtt_config dmcfg;
            if (load_device_mqtt_config(dev->id, &dmcfg) == 0) {
                dev->mqtt_enabled = dmcfg.enabled;
                strncpy(dev->mqtt_topic, dmcfg.topic, sizeof(dev->mqtt_topic) - 1);
                dev->mqtt_pulse_ms = dmcfg.pulse_ms > 0 ? dmcfg.pulse_ms : 1000;
            } else if (strcasestr(dev->manufacturer, "MQTT") != NULL || strcasestr(dev->model, "MQTT") != NULL || strcasestr(dev->type, "MQTT") != NULL) {
                dev->mqtt_enabled = 1;
                strcpy(dev->mqtt_topic, "{root}/button/{device}/{command}");
                dev->mqtt_pulse_ms = 1000;
            } else {
                dev->mqtt_enabled = 0;
                dev->mqtt_topic[0] = 0;
                dev->mqtt_pulse_ms = 1000;
            }

            cJSON *cmds = cJSON_GetObjectItemCaseSensitive(item, "Commands");
            if (cmds && cJSON_IsArray(cmds)) {
                cJSON *c = NULL;
                cJSON_ArrayForEach(c, cmds) {
                    if (dev->command_count >= MAX_IR_COMMANDS) break;
                    if (dev->command_count >= dev->command_cap) {
                        int ncap = dev->command_cap ? dev->command_cap * 2 : 8;
                        if (ncap > MAX_IR_COMMANDS) ncap = MAX_IR_COMMANDS;
                        struct ir_command *grown = (struct ir_command *)realloc(dev->commands, (size_t)ncap * sizeof(*grown));
                        if (!grown) break;
                        dev->commands = grown;
                        dev->command_cap = ncap;
                    }
                    struct ir_command *cmd = &dev->commands[dev->command_count];
                    memset(cmd, 0, sizeof(*cmd));

                    cJSON *cid = cJSON_GetObjectItemCaseSensitive(c, "Id-");
                    long cid_val = cid && cJSON_IsNumber(cid) ? (long)cid->valuedouble : 0;
                    snprintf(cmd->id, sizeof(cmd->id), "%ld", cid_val);
                    if (cid_val > inv->max_command_id) inv->max_command_id = cid_val;

                    cJSON *cname = cJSON_GetObjectItemCaseSensitive(c, "Name");
                    if (cname && cJSON_IsString(cname) && cname->valuestring) copy_text(cmd->name, sizeof(cmd->name), cname->valuestring);

                    cJSON *ckey = cJSON_GetObjectItemCaseSensitive(c, "KeyCode");
                    if (ckey && cJSON_IsString(ckey) && ckey->valuestring) copy_text(cmd->keycode, sizeof(cmd->keycode), ckey->valuestring);

                    cJSON *craw = cJSON_GetObjectItemCaseSensitive(c, "Raw");
                    if (craw && cJSON_IsString(craw) && craw->valuestring) copy_text(cmd->raw, sizeof(cmd->raw), craw->valuestring);

                    cJSON *cproto = cJSON_GetObjectItemCaseSensitive(c, "ProtocolId");
                    cmd->protocol_id = cproto && cJSON_IsNumber(cproto) ? cproto->valueint : 0;

                    cJSON *clearned = cJSON_GetObjectItemCaseSensitive(c, "IsLearned");
                    cmd->learned = clearned && cJSON_IsTrue(clearned) ? 1 : 0;

                    cmd->has_raw = craw && !cJSON_IsNull(craw) && (!cmd->keycode[0]);
                    dev->command_count++;
                }
            }

            /* Power feature */
            dev->power_on_delay = 1500;
            dev->is_power_always_on = 0;
            cJSON *feats = cJSON_GetObjectItemCaseSensitive(item, "DeviceFeatures");
            if (feats && cJSON_IsArray(feats)) {
                cJSON *f = NULL;
                cJSON_ArrayForEach(f, feats) {
                    cJSON *ftype = cJSON_GetObjectItemCaseSensitive(f, "__type");
                    if (ftype && cJSON_IsString(ftype) && strcmp(ftype->valuestring, "PowerFeature") == 0) {
                        cJSON *pod = cJSON_GetObjectItemCaseSensitive(f, "PowerOnDelay");
                        if (pod && cJSON_IsNumber(pod)) dev->power_on_delay = pod->valueint;
                        cJSON *pao = cJSON_GetObjectItemCaseSensitive(f, "IsPowerAlwaysOn");
                        if (pao && cJSON_IsTrue(pao)) dev->is_power_always_on = 1;

                        cJSON *pon = cJSON_GetObjectItemCaseSensitive(f, "PowerOnActions");
                        if (pon && cJSON_IsArray(pon)) {
                            parse_power_actions_cjson(pon, dev->id, &dev->power_on_steps, &dev->power_on_count);
                        }
                        cJSON *poff = cJSON_GetObjectItemCaseSensitive(f, "PowerOffActions");
                        if (poff && cJSON_IsArray(poff)) {
                            parse_power_actions_cjson(poff, dev->id, &dev->power_off_steps, &dev->power_off_count);
                        }
                        break;
                    }
                }
            }
            inv->device_count++;
        }
    }
    cJSON_Delete(root);
    return 0;
}

void free_ir_inventory(struct ir_inventory *inv) {
    int i;
    if (!inv) return;
    for (i = 0; i < inv->device_count; i++) {
        free(inv->devices[i].commands);
        inv->devices[i].commands = NULL;
        free(inv->devices[i].power_on_steps);
        inv->devices[i].power_on_steps = NULL;
        free(inv->devices[i].power_off_steps);
        inv->devices[i].power_off_steps = NULL;
        inv->devices[i].command_cap = 0;
        inv->devices[i].command_count = 0;
        inv->devices[i].power_on_count = 0;
        inv->devices[i].power_off_count = 0;
    }
    inv->device_count = 0;
}

void destroy_ir_inventory(struct ir_inventory *inv) {
    if (!inv) return;
    free_ir_inventory(inv);
    free(inv);
}

int find_ir_device_by_name(const char *name, char *device_id, size_t device_id_len) {
    struct ir_inventory inv;
    int i, rc = -1;
    if (!name || !name[0] || !device_id || !device_id_len) return -1;
    if (load_ir_inventory(&inv) != 0) return -1;
    for (i = 0; i < inv.device_count; i++) {
        if (strcasecmp(inv.devices[i].name, name) == 0) {
            snprintf(device_id, device_id_len, "%s", inv.devices[i].id);
            rc = 0;
            break;
        }
    }
    free_ir_inventory(&inv);
    return rc;
}

void capture_ir_command_action(char *out, size_t outlen);

void render_inventory_json(int fd) {
    repair_known_protocols_for_current_commands();
    char *raw = read_file_alloc(DEVICE_LIST, MAX_RESOURCE_FILE, NULL);
    if (!raw) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "unable to read inventory");
        send_cjson_resp(fd, "200 OK", err);
        cJSON_Delete(err);
        return;
    }
    cJSON *root = cJSON_Parse(raw);
    free(raw);
    if (!root) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "unable to parse inventory");
        send_cjson_resp(fd, "200 OK", err);
        cJSON_Delete(err);
        return;
    }

    int device_count = 0, total_commands = 0;
    cJSON *dwf = cJSON_GetObjectItemCaseSensitive(root, "DevicesWithFeatures");
    cJSON *dev_arr = cJSON_CreateArray();

    if (dwf && cJSON_IsArray(dwf)) {
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, dwf) {
            cJSON *dev = cJSON_GetObjectItemCaseSensitive(item, "Device");
            if (!dev) continue;
            cJSON *jid = cJSON_GetObjectItemCaseSensitive(dev, "Id-");
            long did = (jid && cJSON_IsNumber(jid)) ? (long)jid->valuedouble : 0;
            if (did <= 0) continue;
            device_count++;

            char id_str[32];
            snprintf(id_str, sizeof(id_str), "%ld", did);
            cJSON *d_out = cJSON_CreateObject();
            cJSON_AddStringToObject(d_out, "id", id_str);

            cJSON *name = cJSON_GetObjectItemCaseSensitive(dev, "Name");
            const char *name_str = (name && cJSON_IsString(name) && name->valuestring) ? name->valuestring : "";
            cJSON_AddStringToObject(d_out, "name", name_str);

            cJSON *mfg = cJSON_GetObjectItemCaseSensitive(dev, "Manufacturer");
            const char *mfg_str = (mfg && cJSON_IsString(mfg) && mfg->valuestring) ? mfg->valuestring : "";
            cJSON_AddStringToObject(d_out, "manufacturer", mfg_str);

            cJSON *model = cJSON_GetObjectItemCaseSensitive(dev, "Model");
            const char *model_str = (model && cJSON_IsString(model) && model->valuestring) ? model->valuestring : "";
            cJSON_AddStringToObject(d_out, "model", model_str);

            cJSON *type = cJSON_GetObjectItemCaseSensitive(dev, "DeviceTypeDisplayName");
            const char *type_str = (type && cJSON_IsString(type) && type->valuestring) ? type->valuestring : "";
            cJSON_AddStringToObject(d_out, "type", type_str);

            struct device_mqtt_config dmcfg;
            int is_m = (load_device_mqtt_config(id_str, &dmcfg) == 0 && dmcfg.enabled) ||
                       strcasestr(mfg_str, "MQTT") != NULL || strcasestr(model_str, "MQTT") != NULL || strcasestr(type_str, "MQTT") != NULL;
            if (is_m) cJSON_AddBoolToObject(d_out, "mqtt", 1);

            int is_always_on = 0;
            int power_on_delay = 1500;
            struct activity_step *pon_steps = NULL;
            int pon_count = 0;
            struct activity_step *poff_steps = NULL;
            int poff_count = 0;

            cJSON *feats = cJSON_GetObjectItemCaseSensitive(item, "DeviceFeatures");
            if (feats && cJSON_IsArray(feats)) {
                cJSON *f = NULL;
                cJSON_ArrayForEach(f, feats) {
                    cJSON *ftype = cJSON_GetObjectItemCaseSensitive(f, "__type");
                    if (ftype && cJSON_IsString(ftype) && strcmp(ftype->valuestring, "PowerFeature") == 0) {
                        cJSON *pod = cJSON_GetObjectItemCaseSensitive(f, "PowerOnDelay");
                        if (pod && cJSON_IsNumber(pod)) power_on_delay = pod->valueint;
                        cJSON *pao = cJSON_GetObjectItemCaseSensitive(f, "IsPowerAlwaysOn");
                        if (pao && cJSON_IsTrue(pao)) is_always_on = 1;

                        cJSON *pon = cJSON_GetObjectItemCaseSensitive(f, "PowerOnActions");
                        if (pon && cJSON_IsArray(pon)) {
                            parse_power_actions_cjson(pon, id_str, &pon_steps, &pon_count);
                        }
                        cJSON *poff = cJSON_GetObjectItemCaseSensitive(f, "PowerOffActions");
                        if (poff && cJSON_IsArray(poff)) {
                            parse_power_actions_cjson(poff, id_str, &poff_steps, &poff_count);
                        }
                        break;
                    }
                }
            }
            cJSON_AddBoolToObject(d_out, "isPowerAlwaysOn", is_always_on);
            cJSON_AddNumberToObject(d_out, "powerOnDelay", power_on_delay);

            cJSON *idd = cJSON_GetObjectItemCaseSensitive(dev, "InterDeviceDelay");
            int inter_device_delay = (idd && cJSON_IsNumber(idd)) ? idd->valueint : 100;
            cJSON_AddNumberToObject(d_out, "interDeviceDelay", inter_device_delay);

            cJSON *pon_arr = cJSON_CreateArray();
            for (int s = 0; s < pon_count; s++) {
                cJSON *so = cJSON_CreateObject();
                cJSON_AddStringToObject(so, "type", pon_steps[s].type);
                cJSON_AddStringToObject(so, "command", pon_steps[s].command);
                cJSON_AddNumberToObject(so, "delayMs", pon_steps[s].delay_ms);
                cJSON_AddNumberToObject(so, "order", pon_steps[s].order);
                cJSON_AddItemToArray(pon_arr, so);
            }
            cJSON_AddItemToObject(d_out, "powerOnSteps", pon_arr);
            if (pon_steps) free(pon_steps);

            cJSON *poff_arr = cJSON_CreateArray();
            for (int s = 0; s < poff_count; s++) {
                cJSON *so = cJSON_CreateObject();
                cJSON_AddStringToObject(so, "type", poff_steps[s].type);
                cJSON_AddStringToObject(so, "command", poff_steps[s].command);
                cJSON_AddNumberToObject(so, "delayMs", poff_steps[s].delay_ms);
                cJSON_AddNumberToObject(so, "order", poff_steps[s].order);
                cJSON_AddItemToArray(poff_arr, so);
            }
            cJSON_AddItemToObject(d_out, "powerOffSteps", poff_arr);
            if (poff_steps) free(poff_steps);

            cJSON *cp = cJSON_GetObjectItemCaseSensitive(dev, "ControlPort");
            cJSON_AddNumberToObject(d_out, "controlPort", (cp && cJSON_IsNumber(cp)) ? cp->valueint : 7);

            cJSON *tr = cJSON_GetObjectItemCaseSensitive(dev, "Transport");
            cJSON_AddNumberToObject(d_out, "transport", (tr && cJSON_IsNumber(tr)) ? tr->valueint : 1);

            cJSON *cmds_out = cJSON_CreateArray();
            cJSON *cmds = cJSON_GetObjectItemCaseSensitive(item, "Commands");
            if (cmds && cJSON_IsArray(cmds)) {
                total_commands += cJSON_GetArraySize(cmds);
                cJSON *c = NULL;
                cJSON_ArrayForEach(c, cmds) {
                    cJSON *cid = cJSON_GetObjectItemCaseSensitive(c, "Id-");
                    long cid_val = (cid && cJSON_IsNumber(cid)) ? (long)cid->valuedouble : 0;
                    char cid_str[32];
                    snprintf(cid_str, sizeof(cid_str), "%ld", cid_val);

                    cJSON *cname = cJSON_GetObjectItemCaseSensitive(c, "Name");
                    cJSON *ckey = cJSON_GetObjectItemCaseSensitive(c, "KeyCode");
                    cJSON *craw = cJSON_GetObjectItemCaseSensitive(c, "Raw");
                    const char *key_str = (ckey && cJSON_IsString(ckey) && ckey->valuestring) ? ckey->valuestring : "";
                    int has_raw = (craw && !cJSON_IsNull(craw) && !key_str[0]);

                    cJSON *co = cJSON_CreateObject();
                    cJSON_AddStringToObject(co, "id", cid_str);
                    cJSON_AddStringToObject(co, "name", (cname && cJSON_IsString(cname) && cname->valuestring) ? cname->valuestring : "");
                    cJSON_AddStringToObject(co, "keycode", key_str);
                    cJSON *proto = cJSON_GetObjectItemCaseSensitive(c, "ProtocolId");
                    cJSON_AddNumberToObject(co, "protocolId", (proto && cJSON_IsNumber(proto)) ? proto->valueint : 0);
                    cJSON *lrn = cJSON_GetObjectItemCaseSensitive(c, "IsLearned");
                    cJSON_AddBoolToObject(co, "learned", (lrn && cJSON_IsTrue(lrn)) ? 1 : 0);
                    cJSON_AddBoolToObject(co, "raw", has_raw ? 1 : 0);
                    cJSON_AddItemToArray(cmds_out, co);
                }
            }
            cJSON_AddItemToObject(d_out, "commands", cmds_out);
            cJSON_AddItemToArray(dev_arr, d_out);
        }
    }
    cJSON_Delete(root);

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", 1);
    cJSON_AddNumberToObject(resp, "deviceCount", device_count);
    cJSON_AddNumberToObject(resp, "totalCommandCount", total_commands);
    cJSON_AddNumberToObject(resp, "displayDeviceLimit", MAX_IR_DEVICES);
    cJSON_AddNumberToObject(resp, "displayCommandLimit", MAX_IR_COMMANDS);
    cJSON_AddNumberToObject(resp, "storageCommandLimit", MAX_IR_STORED_COMMANDS);
    cJSON_AddNumberToObject(resp, "batchCommandLimit", MAX_IR_BATCH_COMMANDS);
    cJSON_AddNumberToObject(resp, "requestBodyLimit", MAX_REQUEST_BODY);
    cJSON_AddNumberToObject(resp, "resourceFileLimit", MAX_RESOURCE_FILE);
    cJSON_AddItemToObject(resp, "devices", dev_arr);

    send_cjson_resp(fd, "200 OK", resp);
    cJSON_Delete(resp);
}

void render_device_commands_json(int fd, const struct request *req) {
    char device_id[64];
    if (strcmp(req->method, "POST") == 0) {
        form_value(req->body, "deviceId", device_id, sizeof(device_id));
    } else {
        query_value(req->path, "deviceId", device_id, sizeof(device_id));
    }
    if (!safe_label(device_id)) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "invalid IR device id");
        send_cjson_resp(fd, "400 Bad Request", err);
        cJSON_Delete(err);
        return;
    }
    repair_known_protocols_for_current_commands();
    char *raw = read_file_alloc(DEVICE_LIST, MAX_RESOURCE_FILE, NULL);
    if (!raw) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "device not found");
        send_cjson_resp(fd, "404 Not Found", err);
        cJSON_Delete(err);
        return;
    }
    cJSON *root = cJSON_Parse(raw);
    free(raw);
    if (!root) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "device not found");
        send_cjson_resp(fd, "404 Not Found", err);
        cJSON_Delete(err);
        return;
    }
    cJSON *dev_item = find_device_item(root, device_id);
    if (!dev_item) {
        cJSON_Delete(root);
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "device not found");
        send_cjson_resp(fd, "404 Not Found", err);
        cJSON_Delete(err);
        return;
    }

    cJSON *cmds = cJSON_GetObjectItemCaseSensitive(dev_item, "Commands");
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", 1);
    cJSON_AddStringToObject(resp, "deviceId", device_id);
    cJSON *cmds_out = cJSON_CreateArray();
    int count = 0;
    if (cmds && cJSON_IsArray(cmds)) {
        cJSON *c = NULL;
        cJSON_ArrayForEach(c, cmds) {
            cJSON *cid = cJSON_GetObjectItemCaseSensitive(c, "Id-");
            long cid_val = (cid && cJSON_IsNumber(cid)) ? (long)cid->valuedouble : 0;
            char cid_str[32];
            snprintf(cid_str, sizeof(cid_str), "%ld", cid_val);

            cJSON *cname = cJSON_GetObjectItemCaseSensitive(c, "Name");
            cJSON *ckey = cJSON_GetObjectItemCaseSensitive(c, "KeyCode");
            cJSON *craw = cJSON_GetObjectItemCaseSensitive(c, "Raw");
            const char *key_str = (ckey && cJSON_IsString(ckey) && ckey->valuestring) ? ckey->valuestring : "";
            int has_raw = (craw && !cJSON_IsNull(craw) && !key_str[0]);

            cJSON *co = cJSON_CreateObject();
            cJSON_AddStringToObject(co, "id", cid_str);
            cJSON_AddStringToObject(co, "name", (cname && cJSON_IsString(cname) && cname->valuestring) ? cname->valuestring : "");
            cJSON_AddStringToObject(co, "keycode", key_str);
            cJSON *proto = cJSON_GetObjectItemCaseSensitive(c, "ProtocolId");
            cJSON_AddNumberToObject(co, "protocolId", (proto && cJSON_IsNumber(proto)) ? proto->valueint : 0);
            cJSON *lrn = cJSON_GetObjectItemCaseSensitive(c, "IsLearned");
            cJSON_AddBoolToObject(co, "learned", (lrn && cJSON_IsTrue(lrn)) ? 1 : 0);
            cJSON_AddBoolToObject(co, "raw", has_raw ? 1 : 0);
            cJSON_AddItemToArray(cmds_out, co);
            count++;
        }
    }
    cJSON_Delete(root);

    cJSON_AddItemToObject(resp, "commands", cmds_out);
    cJSON_AddNumberToObject(resp, "count", count);
    send_cjson_resp(fd, "200 OK", resp);
    cJSON_Delete(resp);
}

void render_capture_json(int fd) {
    char reply[4096];
    char mode[16], keycode[512], nec[64], summary[160];
    int protocol_id = 2;
    capture_ir_command_action(reply, sizeof(reply));
    analyze_capture_storage(reply, "", "", "2", mode, sizeof(mode), keycode, sizeof(keycode), nec, sizeof(nec), &protocol_id, summary, sizeof(summary));

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", 1);
    cJSON_AddStringToObject(resp, "raw", reply);
    cJSON_AddStringToObject(resp, "mode", mode);
    cJSON_AddNumberToObject(resp, "protocolId", protocol_id);
    cJSON_AddStringToObject(resp, "keycode", keycode);
    cJSON_AddStringToObject(resp, "nec", nec);
    cJSON_AddStringToObject(resp, "analysis", summary);
    send_cjson_resp(fd, "200 OK", resp);
    cJSON_Delete(resp);
}

int safe_remotecentral_path(const char *path) {
    const unsigned char *p = (const unsigned char *)path;
    size_t n = strlen(path);
    if (n == 0 || n > 360) return 0;
    if (strncmp(path, "/cgi-bin/codes/", 15) != 0) return 0;
    if (strstr(path, "..") || strstr(path, "://")) return 0;
    while (*p) {
        if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
            (*p >= '0' && *p <= '9') || *p == '/' || *p == '_' ||
            *p == '-' || *p == '.' || *p == '?' || *p == '=' ||
            *p == '&') {
            p++;
            continue;
        }
        if (*p == '%') {
            int a, b;
            if (!isxdigit((unsigned char)p[1]) || !isxdigit((unsigned char)p[2])) return 0;
            a = tolower((unsigned char)p[1]);
            b = tolower((unsigned char)p[2]);
            if (a == '2' && b == 'e') return 0;
            p += 3;
            continue;
        }
        return 0;
    }
    return 1;
}

void normalize_remotecentral_path(char *path) {
    const char *hosts[] = {
        "https://www.remotecentral.com",
        "http://www.remotecentral.com",
        "https://remotecentral.com",
        "http://remotecentral.com"
    };
    int i;
    for (i = 0; i < 4; i++) {
        const char *host = hosts[i];
        size_t n = strlen(host);
        if (strncasecmp(path, host, n) == 0) {
            memmove(path, path + n, strlen(path + n) + 1);
            return;
        }
    }
}

void render_remotecentral_fetch_json(int fd, const struct request *req) {
    char path[384], err[128];
    char *remote = NULL;
    if (strcmp(req->method, "POST") == 0) {
        form_value(req->body, "path", path, sizeof(path));
    } else {
        query_value(req->path, "path", path, sizeof(path));
    }
    chomp(path);
    normalize_remotecentral_path(path);
    if (!safe_remotecentral_path(path)) {
        cJSON *err_obj = cJSON_CreateObject();
        cJSON_AddBoolToObject(err_obj, "ok", 0);
        cJSON_AddStringToObject(err_obj, "source", "remotecentral");
        cJSON_AddStringToObject(err_obj, "error", "invalid RemoteCentral path");
        send_cjson_resp(fd, "400 Bad Request", err_obj);
        cJSON_Delete(err_obj);
        return;
    }
    if (http_get_body("www.remotecentral.com", path, &remote, MAX_REQUEST_BODY, err, sizeof(err)) != 0) {
        cJSON *err_obj = cJSON_CreateObject();
        cJSON_AddBoolToObject(err_obj, "ok", 0);
        cJSON_AddStringToObject(err_obj, "source", "remotecentral");
        cJSON_AddStringToObject(err_obj, "path", path);
        cJSON_AddStringToObject(err_obj, "error", err[0] ? err : "RemoteCentral request failed");
        send_cjson_resp(fd, "200 OK", err_obj);
        cJSON_Delete(err_obj);
        return;
    }
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", 1);
    cJSON_AddStringToObject(resp, "source", "remotecentral");
    cJSON_AddStringToObject(resp, "path", path);
    cJSON_AddStringToObject(resp, "html", remote ? remote : "");
    send_cjson_resp(fd, "200 OK", resp);
    cJSON_Delete(resp);
    free(remote);
}


int append_top_array_item(const char *path, const char *array_key, const char *item) {
    char clean_key[64];
    const char *k = array_key;
    while (*k == '\"') k++;
    size_t kl = 0;
    while (*k && *k != '\"' && kl < sizeof(clean_key) - 1) clean_key[kl++] = *k++;
    clean_key[kl] = '\0';

    size_t len = 0;
    char *raw = read_file_alloc(path, MAX_RESOURCE_FILE, &len);
    if (!raw) return -1;

    cJSON *root = cJSON_Parse(raw);
    free(raw);
    if (!root) return -1;

    cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, clean_key);
    if (!arr) arr = cJSON_GetObjectItem(root, clean_key);
    if (!arr || !cJSON_IsArray(arr)) {
        cJSON_Delete(root);
        return -1;
    }

    cJSON *item_obj = cJSON_Parse(item);
    if (!item_obj) {
        cJSON_Delete(root);
        return -1;
    }

    cJSON_AddItemToArray(arr, item_obj);
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!out) return -1;

    int rc = write_file_atomic(path, out, strlen(out));
    free(out);
    return rc;
}

int protocol_list_has_id(const char *raw, int protocol_id) {
    char needle[32];
    snprintf(needle, sizeof(needle), "\"Id-\":%d", protocol_id);
    return raw && strstr(raw, needle) != NULL;
}

int ensure_protocol_object(int protocol_id, const char *protocol_json) {
    char *raw = read_file_alloc(PROTOCOL_LIST, MAX_RESOURCE_FILE, NULL);
    int present;
    if (!raw) return -1;
    present = protocol_list_has_id(raw, protocol_id);
    free(raw);
    if (present) return 0;
    return append_top_array_item(PROTOCOL_LIST, "\"Protocols\"", protocol_json);
}

int ensure_builtin_protocol_for_id(int protocol_id) {
    if (protocol_id == 2) return ensure_protocol_object(2, BUILTIN_PROTOCOL_TOSHIBA_32);
    if (protocol_id == 679) return ensure_protocol_object(679, BUILTIN_PROTOCOL_MEMOREX_O1);
    if (protocol_id == 999990) return ensure_protocol_object(999990, BUILTIN_PROTOCOL_CUSTOM_MQTT);
    return 0;
}

int protocol_file_has_id(int protocol_id) {
    char *raw = read_file_alloc(PROTOCOL_LIST, MAX_RESOURCE_FILE, NULL);
    int present;
    if (!raw) return -1;
    present = protocol_list_has_id(raw, protocol_id);
    free(raw);
    return present ? 1 : 0;
}

static int s_protocol_repair_needed = 1;

void mark_protocol_repair_needed(void) {
    s_protocol_repair_needed = 1;
}

int repair_known_protocols_for_current_commands(void) {
    if (!s_protocol_repair_needed) return 0;
    char *devices = read_file_alloc(DEVICE_LIST, MAX_RESOURCE_FILE, NULL);
    int need2, need679, missing2 = 0, missing679 = 0, changed = 0;
    if (!devices) return -1;
    need2 = strstr(devices, "\"ProtocolId\":2") != NULL;
    need679 = strstr(devices, "\"ProtocolId\":679") != NULL;
    free(devices);
    if (need2) {
        int present = protocol_file_has_id(2);
        if (present < 0) return -1;
        missing2 = !present;
    }
    if (need679) {
        int present = protocol_file_has_id(679);
        if (present < 0) return -1;
        missing679 = !present;
    }
    s_protocol_repair_needed = 0;
    if (!missing2 && !missing679) return 0;
    backup_resources();
    if (missing2 && ensure_builtin_protocol_for_id(2) != 0) return -1;
    if (missing679 && ensure_builtin_protocol_for_id(679) != 0) return -1;
    changed = missing2 || missing679;
    if (changed) request_resource_reload();
    return changed ? 1 : 0;
}


int append_command_to_device(const char *device_id, const char *command_json) {
    size_t len;
    char *raw = read_file_alloc(DEVICE_LIST, MAX_RESOURCE_FILE, &len);
    if (!raw) return -1;
    cJSON *root = cJSON_Parse(raw);
    free(raw);
    if (!root) return -1;
    cJSON *dev_item = find_device_item(root, device_id);
    if (!dev_item) {
        cJSON_Delete(root);
        return -1;
    }
    cJSON *cmds = cJSON_GetObjectItemCaseSensitive(dev_item, "Commands");
    if (!cmds || !cJSON_IsArray(cmds)) {
        cmds = cJSON_CreateArray();
        cJSON_AddItemToObject(dev_item, "Commands", cmds);
    }
    cJSON *cmd_obj = cJSON_Parse(command_json);
    if (!cmd_obj) {
        cJSON_Delete(root);
        return -1;
    }
    cJSON_AddItemToArray(cmds, cmd_obj);
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!out) return -1;
    int rc = write_file_atomic(DEVICE_LIST, out, strlen(out));
    if (rc == 0) {
        invalidate_device_list_cache();
        mark_protocol_repair_needed();
    }
    free(out);
    return rc;
}

int delete_ir_device(const char *device_id, char *msg, size_t msglen) {
    size_t len;
    char *raw = read_file_alloc(DEVICE_LIST, MAX_RESOURCE_FILE, &len);
    if (!raw) {
        snprintf(msg, msglen, "Failed to read DeviceList.");
        return -1;
    }
    cJSON *root = cJSON_Parse(raw);
    free(raw);
    if (!root) {
        snprintf(msg, msglen, "Failed to parse DeviceList.");
        return -1;
    }
    cJSON *dwf = cJSON_GetObjectItemCaseSensitive(root, "DevicesWithFeatures");
    cJSON *item = find_device_item(root, device_id);
    if (!item || !dwf) {
        cJSON_Delete(root);
        snprintf(msg, msglen, "Device %s not found.", device_id);
        return -1;
    }
    backup_resources();
    cJSON_DetachItemViaPointer(dwf, item);
    cJSON_Delete(item);
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!out || write_file_atomic(DEVICE_LIST, out, strlen(out)) != 0) {
        free(out);
        snprintf(msg, msglen, "Failed to write DeviceList.");
        return -1;
    }
    invalidate_device_list_cache();
    free(out);
    request_resource_reload();
    snprintf(msg, msglen, "Deleted device %s.", device_id);
    return 0;
}

int delete_ir_command(const char *device_id, const char *command_name, char *msg, size_t msglen) {
    size_t len;
    char *raw = read_file_alloc(DEVICE_LIST, MAX_RESOURCE_FILE, &len);
    if (!raw) {
        snprintf(msg, msglen, "Failed to read DeviceList.");
        return -1;
    }
    cJSON *root = cJSON_Parse(raw);
    free(raw);
    if (!root) {
        snprintf(msg, msglen, "Failed to parse DeviceList.");
        return -1;
    }
    cJSON *dev_item = find_device_item(root, device_id);
    if (!dev_item) {
        cJSON_Delete(root);
        snprintf(msg, msglen, "Device %s not found.", device_id);
        return -1;
    }
    cJSON *cmds = cJSON_GetObjectItemCaseSensitive(dev_item, "Commands");
    if (!cmds || !cJSON_IsArray(cmds)) {
        cJSON_Delete(root);
        snprintf(msg, msglen, "No commands found on device %s.", device_id);
        return -1;
    }
    cJSON *cmd_item = NULL;
    cJSON *found = NULL;
    cJSON_ArrayForEach(cmd_item, cmds) {
        cJSON *jname = cJSON_GetObjectItemCaseSensitive(cmd_item, "Name");
        if (jname && cJSON_IsString(jname) && jname->valuestring && strcmp(jname->valuestring, command_name) == 0) {
            found = cmd_item;
            break;
        }
    }
    if (!found) {
        cJSON_Delete(root);
        snprintf(msg, msglen, "Command %s not found on device %s.", command_name, device_id);
        return -1;
    }
    backup_resources();
    cJSON_DetachItemViaPointer(cmds, found);
    cJSON_Delete(found);
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!out || write_file_atomic(DEVICE_LIST, out, strlen(out)) != 0) {
        free(out);
        snprintf(msg, msglen, "Failed to write DeviceList.");
        return -1;
    }
    invalidate_device_list_cache();
    free(out);
    request_resource_reload();
    snprintf(msg, msglen, "Deleted command %s from device %s.", command_name, device_id);
    return 0;
}

int clear_ir_commands(const char *device_id, char *msg, size_t msglen) {
    size_t len;
    if (!safe_label(device_id)) {
        snprintf(msg, msglen, "Invalid device %s.", device_id ? device_id : "");
        return -1;
    }
    char *raw = read_file_alloc(DEVICE_LIST, MAX_RESOURCE_FILE, &len);
    if (!raw) {
        snprintf(msg, msglen, "Failed to read DeviceList.");
        return -1;
    }
    cJSON *root = cJSON_Parse(raw);
    free(raw);
    if (!root) {
        snprintf(msg, msglen, "Failed to parse DeviceList.");
        return -1;
    }
    cJSON *dev_item = find_device_item(root, device_id);
    if (!dev_item) {
        cJSON_Delete(root);
        snprintf(msg, msglen, "Device %s not found.", device_id);
        return -1;
    }
    cJSON *cmds = cJSON_GetObjectItemCaseSensitive(dev_item, "Commands");
    int count = (cmds && cJSON_IsArray(cmds)) ? cJSON_GetArraySize(cmds) : 0;
    backup_resources();
    cJSON_ReplaceItemInObject(dev_item, "Commands", cJSON_CreateArray());
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!out || write_file_atomic(DEVICE_LIST, out, strlen(out)) != 0) {
        free(out);
        snprintf(msg, msglen, "Failed to clear commands from device %s.", device_id);
        return -1;
    }
    invalidate_device_list_cache();
    free(out);
    request_resource_reload();
    snprintf(msg, msglen, "Cleared %d commands from device %s.", count, device_id);
    return 0;
}

int harmony_device_type_id(const char *type) {
    if (strcasecmp(type, "Amplifier") == 0) return 19;
    if (strcasecmp(type, "Television") == 0 || strcasecmp(type, "TV") == 0) return 2;
    if (strcasecmp(type, "Media Player") == 0) return 22;
    if (strcasecmp(type, "Game Console") == 0) return 32;
    if (strcasecmp(type, "HomeAppliance") == 0 || strcasecmp(type, "Home Appliance") == 0) return 44;
    return 44;
}

int create_ir_device_ex(const char *name, const char *manufacturer, const char *model, const char *type, char *msg, size_t msglen, char *created_id, size_t created_id_len) {
    long id = 0, max_device_id = 0;
    int dtype;
    if (!safe_label(name) || !safe_label(manufacturer) || !safe_label(model) || !safe_label(type)) {
        snprintf(msg, msglen, "Device fields cannot be empty or contain control characters.");
        return -1;
    }
    scan_ir_resource_stats(NULL, NULL, &max_device_id, NULL);
    id = max_device_id + 1;
    if (id < 90000000) id = 90000000 + (long)(time(NULL) % 9000000);
    int is_mqtt = (strcasestr(type, "MQTT") != NULL || strcasestr(manufacturer, "MQTT") != NULL);
    if (is_mqtt) {
        ensure_builtin_protocol_for_id(999990);
    }
    dtype = harmony_device_type_id(type);

    cJSON *item = cJSON_CreateObject();
    cJSON *dev = cJSON_CreateObject();
    cJSON_AddNumberToObject(dev, "DongleIndex", 0);
    cJSON_AddNumberToObject(dev, "InterDeviceDelay", 500);
    cJSON_AddNumberToObject(dev, "HoldInterKeyDelay", 100);
    cJSON_AddNumberToObject(dev, "DeviceType", dtype);
    cJSON_AddNullToObject(dev, "RegisterSection");
    cJSON_AddNumberToObject(dev, "ContentProfileKey", id);
    cJSON_AddNumberToObject(dev, "DeviceOrder", 0);
    cJSON_AddStringToObject(dev, "DeviceProfileUri", "");
    cJSON_AddNumberToObject(dev, "SetupState", 1);
    cJSON_AddNumberToObject(dev, "DeviceSearchType", -1);
    cJSON_AddNumberToObject(dev, "GlobalDeviceVersionId-", 0);
    cJSON_AddStringToObject(dev, "DeviceTypeDisplayName", is_mqtt ? "MQTT Device" : type);
    cJSON_AddNullToObject(dev, "FriendlyName");
    cJSON_AddNumberToObject(dev, "Transport", 1);
    cJSON_AddNumberToObject(dev, "PressMinRepeats", 3);
    cJSON_AddNumberToObject(dev, "PrivateAddType", 1);
    cJSON_AddNullToObject(dev, "RegionalCharset");
    cJSON_AddNullToObject(dev, "Tokens");
    cJSON_AddNullToObject(dev, "BTAddress");
    cJSON_AddNumberToObject(dev, "DongleRFID", 0);
    cJSON_AddNullToObject(dev, "AutoDetectedDevice");
    cJSON_AddNumberToObject(dev, "GlobalLanguageVersionId-", 0);
    cJSON_AddNullToObject(dev, "ParentDeviceId");
    cJSON_AddNullToObject(dev, "DecodedEdid");
    cJSON_AddNumberToObject(dev, "DefaultPressMinRepeats", 3);
    cJSON_AddStringToObject(dev, "ParentDeviceModel", "");
    cJSON_AddNullToObject(dev, "RenewSection");
    cJSON_AddBoolToObject(dev, "IsMultiCode", 0);
    cJSON_AddNullToObject(dev, "GroupName");
    cJSON_AddNumberToObject(dev, "DefaultInterDeviceDelay", 0);
    cJSON_AddNumberToObject(dev, "ParentDevice-", 0);
    char date_added[64];
    snprintf(date_added, sizeof(date_added), "/Date(%ld000+0000)/", (long)time(NULL));
    cJSON_AddStringToObject(dev, "DeviceAddedDate", date_added);
    cJSON_AddStringToObject(dev, "ParentDeviceManufacturer", manufacturer);
    cJSON_AddNullToObject(dev, "AppLaunchConfigs");
    cJSON_AddNullToObject(dev, "PictureId");
    cJSON_AddBoolToObject(dev, "IsKeyboardAssociated", 0);
    cJSON_AddBoolToObject(dev, "IsScartCableSupported", 0);
    cJSON_AddStringToObject(dev, "Manufacturer", manufacturer);
    cJSON_AddNumberToObject(dev, "Icon", dtype);
    cJSON_AddNumberToObject(dev, "Id-", id);
    cJSON_AddBoolToObject(dev, "IsInterKeyDelayOptimized", 0);
    cJSON_AddStringToObject(dev, "SuggestedDisplay", "DEFAULT");
    cJSON_AddNumberToObject(dev, "ControlPort", 7);
    cJSON_AddNullToObject(dev, "EncodedEdid");
    cJSON_AddNumberToObject(dev, "InterKeyDelay", 300);
    cJSON_AddNumberToObject(dev, "DeviceClassification", 0);
    cJSON_AddArrayToObject(dev, "DeviceCapabilitiesWithPriority");
    cJSON_AddNumberToObject(dev, "CopiedDeviceSource", -1);
    cJSON_AddNumberToObject(dev, "State", 1);
    cJSON_AddNumberToObject(dev, "HoldInterDeviceDelay", 0);
    cJSON_AddStringToObject(dev, "Name", name);
    cJSON_AddStringToObject(dev, "Model", model);
    cJSON_AddNullToObject(dev, "ActivityIds");
    cJSON_AddNumberToObject(dev, "Characterization", 0);
    cJSON_AddNumberToObject(dev, "HoldMinRepeats", -1);
    cJSON_AddNumberToObject(dev, "DefaultInterKeyDelay", 0);
    cJSON_AddItemToObject(item, "Device", dev);

    cJSON *cmds = cJSON_CreateArray();
    if (is_mqtt) {
        static const char *mqtt_btn_names[] = {
            "PowerToggle", "PowerOn", "PowerOff", "Play", "Pause", "Stop",
            "DirectionUp", "DirectionDown", "DirectionLeft", "DirectionRight",
            "OK", "Back", "Home", "Menu", "VolumeUp", "VolumeDown", "Mute", "Exit"
        };
        for (int b = 0; b < 18; b++) {
            cJSON *cmd = cJSON_CreateObject();
            cJSON_AddNumberToObject(cmd, "Id-", 2100001 + b);
            cJSON_AddStringToObject(cmd, "Name", mqtt_btn_names[b]);
            cJSON_AddStringToObject(cmd, "KeyCode", "G:Custom MQTT:()():1");
            cJSON_AddNumberToObject(cmd, "ProtocolId", 999990);
            cJSON_AddBoolToObject(cmd, "IsLearned", 0);
            cJSON_AddItemToArray(cmds, cmd);
        }
    }
    cJSON_AddItemToObject(item, "Commands", cmds);
    cJSON_AddArrayToObject(item, "DeviceFeatures");

    char *item_json = cJSON_PrintUnformatted(item);
    cJSON_Delete(item);
    if (!item_json) {
        snprintf(msg, msglen, "Failed to serialize device JSON.");
        return -1;
    }

    backup_resources();
    int rc = append_top_array_item(DEVICE_LIST, "\"DevicesWithFeatures\"", item_json);
    free(item_json);
    if (rc != 0) {
        snprintf(msg, msglen, "Failed to append device to DeviceList.");
        return -1;
    }
    invalidate_device_list_cache();

    request_resource_reload();
    if (is_mqtt) {
        struct device_mqtt_config dmcfg;
        char id_str[32];
        memset(&dmcfg, 0, sizeof(dmcfg));
        dmcfg.enabled = 1;
        strcpy(dmcfg.topic, "{root}/button/{device}/{command}");
        dmcfg.pulse_ms = 1000;
        snprintf(id_str, sizeof(id_str), "%ld", id);
        save_device_mqtt_config(id_str, &dmcfg);
    }
    if (created_id && created_id_len) snprintf(created_id, created_id_len, "%ld", id);
    snprintf(msg, msglen, is_mqtt ? "Created MQTT device %s (%ld) with standard buttons." : "Created IR device %s (%ld).", name, id);
    return 0;
}

int create_ir_device(const char *name, const char *manufacturer, const char *model, const char *type, char *msg, size_t msglen) {
    return create_ir_device_ex(name, manufacturer, model, type, msg, msglen, NULL, 0);
}

int ensure_lab_target_device(char *device_id, size_t device_id_len, int *created, char *msg, size_t msglen) {
    const char *name = "Temporary IR Test";
    if (created) *created = 0;
    if (find_ir_device_by_name(name, device_id, device_id_len) == 0) {
        snprintf(msg, msglen, "Using existing %s (%s).", name, device_id);
        return 0;
    }
    if (create_ir_device_ex(name, "Local", "Command Sweep", "HomeAppliance", msg, msglen, device_id, device_id_len) == 0) {
        if (created) *created = 1;
        return 0;
    }
    return -1;
}

int update_ir_device(const char *device_id, const char *name, const char *manufacturer, const char *model, const char *type, int control_port, char *msg, size_t msglen) {
    if (!safe_label(device_id) || !safe_label(name) || !safe_label(manufacturer) || !safe_label(model) || !safe_label(type)) {
        snprintf(msg, msglen, "Device fields cannot be empty or contain control characters.");
        return -1;
    }
    size_t len;
    char *raw = read_file_alloc(DEVICE_LIST, MAX_RESOURCE_FILE, &len);
    if (!raw) {
        snprintf(msg, msglen, "Failed to read DeviceList.");
        return -1;
    }
    cJSON *root = cJSON_Parse(raw);
    free(raw);
    if (!root) {
        snprintf(msg, msglen, "Failed to parse DeviceList.");
        return -1;
    }
    cJSON *dev_item = find_device_item(root, device_id);
    if (!dev_item) {
        cJSON_Delete(root);
        snprintf(msg, msglen, "Device %s not found.", device_id);
        return -1;
    }
    cJSON *dev = cJSON_GetObjectItemCaseSensitive(dev_item, "Device");
    if (!dev) {
        cJSON_Delete(root);
        snprintf(msg, msglen, "Device object not found for %s.", device_id);
        return -1;
    }
    backup_resources();
    cJSON_ReplaceItemInObject(dev, "Name", cJSON_CreateString(name));
    cJSON_ReplaceItemInObject(dev, "Manufacturer", cJSON_CreateString(manufacturer));
    cJSON_ReplaceItemInObject(dev, "Model", cJSON_CreateString(model));
    cJSON_ReplaceItemInObject(dev, "DeviceTypeDisplayName", cJSON_CreateString(type));
    if (control_port > 0 && control_port <= 7) {
        cjson_set_or_replace(dev, "ControlPort", cJSON_CreateNumber(control_port));
    }

    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!out || write_file_atomic(DEVICE_LIST, out, strlen(out)) != 0) {
        free(out);
        snprintf(msg, msglen, "Failed to save DeviceList.");
        return -1;
    }
    invalidate_device_list_cache();
    free(out);
    request_resource_reload();
    snprintf(msg, msglen, "Updated device %s.", device_id);
    return 0;
}

int load_device_mqtt_config(const char *device_id, struct device_mqtt_config *dmcfg) {
    char raw[32768];
    if (!device_id || !device_id[0] || !dmcfg) return -1;
    memset(dmcfg, 0, sizeof(*dmcfg));
    dmcfg->pulse_ms = 1000;
    strcpy(dmcfg->topic, "{root}/button/{device}/{command}");
    if (read_text(MQTT_CONFIG, raw, sizeof(raw)) <= 0) return -1;

    cJSON *root = cJSON_Parse(raw);
    if (!root) return -1;

    cJSON *mdevs = cJSON_GetObjectItemCaseSensitive(root, "mqttDevices");
    if (!mdevs) { cJSON_Delete(root); return -1; }

    cJSON *dev = cJSON_GetObjectItemCaseSensitive(mdevs, device_id);
    if (!dev) { cJSON_Delete(root); return -1; }

    cJSON *item = cJSON_GetObjectItemCaseSensitive(dev, "enabled");
    if (item && cJSON_IsBool(item)) dmcfg->enabled = cJSON_IsTrue(item) ? 1 : 0;

    item = cJSON_GetObjectItemCaseSensitive(dev, "topic");
    if (item && cJSON_IsString(item) && item->valuestring)
        strncpy(dmcfg->topic, item->valuestring, sizeof(dmcfg->topic) - 1);

    item = cJSON_GetObjectItemCaseSensitive(dev, "pulseMs");
    if (item && cJSON_IsNumber(item)) dmcfg->pulse_ms = item->valueint;
    if (dmcfg->pulse_ms <= 0) dmcfg->pulse_ms = 1000;

    cJSON_Delete(root);
    return 0;
}

int save_device_mqtt_config(const char *device_id, const struct device_mqtt_config *dmcfg) {
    if (!safe_label(device_id) || !dmcfg) return -1;
    char *raw = NULL;
    size_t len = 0;
    raw = read_file_alloc(MQTT_CONFIG, MAX_REQUEST_BODY, &len);

    cJSON *root = NULL;
    if (raw) {
        root = cJSON_Parse(raw);
        free(raw);
    }
    if (!root) {
        root = cJSON_CreateObject();
        cJSON_AddBoolToObject(root, "enabled", 1);
        cJSON_AddStringToObject(root, "baseTopic", "harmony/hub");
    }

    cJSON *mdevs = cJSON_GetObjectItemCaseSensitive(root, "mqttDevices");
    if (!mdevs) {
        mdevs = cJSON_CreateObject();
        cJSON_AddItemToObject(root, "mqttDevices", mdevs);
    }

    cJSON *dev = cJSON_CreateObject();
    cJSON_AddBoolToObject(dev, "enabled", dmcfg->enabled);
    cJSON_AddStringToObject(dev, "topic", dmcfg->topic[0] ? dmcfg->topic : "{root}/button/{device}/{command}");
    cJSON_AddNumberToObject(dev, "pulseMs", dmcfg->pulse_ms > 0 ? dmcfg->pulse_ms : 1000);

    cJSON_DeleteItemFromObjectCaseSensitive(mdevs, device_id);
    cJSON_AddItemToObject(mdevs, device_id, dev);

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

int build_nec_keycode(const char *hex, int protocol_id, char *out, size_t outlen) {
    const char *p = hex;
    char clean[16];
    int n = 0;
    unsigned long value;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
    while (*p && n < 8) {
        if (!isxdigit((unsigned char)*p)) return -1;
        clean[n++] = *p++;
    }
    if (*p || n == 0 || n > 8) return -1;
    clean[n] = 0;
    value = strtoul(clean, NULL, 16);
    if (protocol_id == 679) {
        snprintf(out, outlen, "G:MemorexO1 32 Bit:()(0x%08lX)():3", value & 0xffffffffUL);
    } else {
        snprintf(out, outlen, "G:Toshiba 32 Bit:(0x%08lX)(Repeat)():3", value & 0xffffffffUL);
    }
    return 0;
}


int contains_ci(const char *haystack, const char *needle) {
    size_t n;
    if (!haystack || !needle || !needle[0]) return 0;
    n = strlen(needle);
    while (*haystack) {
        if (strncasecmp(haystack, needle, n) == 0) return 1;
        haystack++;
    }
    return 0;
}

int extract_harmony_keycode(const char *src, char *out, size_t outlen) {
    const char *p;
    size_t n = 0;
    if (!src || !outlen) return 0;
    p = strstr(src, "G:");
    if (!p) return 0;
    while (p[n] && p[n] != '"' && p[n] != '\'' && p[n] != '<' && p[n] != '>' &&
           p[n] != '\r' && p[n] != '\n' && n + 1 < outlen) {
        n++;
    }
    while (n && (isspace((unsigned char)p[n - 1]) || p[n - 1] == ',')) n--;
    if (n < 4) return 0;
    memcpy(out, p, n);
    out[n] = 0;
    return 1;
}

int clean_hex_token(const char *src, char *out, size_t outlen) {
    const char *p;
    size_t n = 0;
    if (!src || !outlen) return 0;
    while (*src && isspace((unsigned char)*src)) src++;
    p = src;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
    while (isxdigit((unsigned char)p[n]) && n < 8 && n + 1 < outlen) n++;
    if (n == 0 || n > 8) return 0;
    if (isxdigit((unsigned char)p[n])) return 0;
    while (p[n] && isspace((unsigned char)p[n])) n++;
    if (p[n]) return 0;
    p = src;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
    n = 0;
    while (isxdigit((unsigned char)p[n]) && n < 8 && n + 1 < outlen) {
        out[n] = (char)toupper((unsigned char)p[n]);
        n++;
    }
    out[n] = 0;
    return 1;
}

int extract_nec_hex(const char *src, char *out, size_t outlen) {
    const char *p;
    int hinted;
    if (!src || !outlen) return 0;
    if (clean_hex_token(src, out, outlen)) return 1;
    for (p = src; *p; p++) {
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
            size_t i;
            p += 2;
            for (i = 0; i < 8 && isxdigit((unsigned char)p[i]); i++) {
                if (i + 1 < outlen) out[i] = (char)toupper((unsigned char)p[i]);
            }
            if (i == 8 && !isxdigit((unsigned char)p[i])) {
                out[8] = 0;
                return 1;
            }
            p--;
        }
    }
    hinted = contains_ci(src, "nec") || contains_ci(src, "samsung") ||
             contains_ci(src, "toshiba") || contains_ci(src, "memorex") ||
             contains_ci(src, "protocol") || contains_ci(src, "keycode");
    if (!hinted) return 0;
    for (p = src; *p; p++) {
        size_t i;
        if (isxdigit((unsigned char)*p) && (p == src || !isxdigit((unsigned char)p[-1]))) {
            for (i = 0; i < 8 && isxdigit((unsigned char)p[i]); i++) {
                if (i + 1 < outlen) out[i] = (char)toupper((unsigned char)p[i]);
            }
            if (i == 8 && !isxdigit((unsigned char)p[i])) {
                out[8] = 0;
                return 1;
            }
        }
    }
    return 0;
}

int infer_capture_protocol(const char *raw_code, const char *keycode) {
    if (contains_ci(keycode, "MemorexO1") || contains_ci(raw_code, "MemorexO1")) return 679;
    return 2;
}

int decode_nec_from_raw(const char *raw, char *nec_out, size_t nec_len) {
    if (!raw || !nec_out || nec_len < 9) return 0;
    while (*raw && isspace((unsigned char)*raw)) raw++;
    if (*raw != 'F' && *raw != 'f') return 0;
    raw++;
    /* Skip frequency */
    while (*raw && isxdigit((unsigned char)*raw)) raw++;

    uint32_t timings[128];
    size_t count = 0;
    while (*raw && count < sizeof(timings)/sizeof(timings[0])) {
        char type = *raw++;
        if (type == 'P' || type == 'p' || type == 'S' || type == 's') {
            char *endp = NULL;
            unsigned long dur = strtoul(raw, &endp, 16);
            raw = endp;
            timings[count++] = (uint32_t)dur;
        } else if (isspace((unsigned char)type)) {
            continue;
        } else {
            break;
        }
    }

    if (count < 67) return 0;

    /* Leader mark: 7000..11000 us, space: 3000..6000 us */
    if (timings[0] < 7000 || timings[0] > 11000) return 0;
    if (timings[1] < 3000 || timings[1] > 6000) return 0;

    uint32_t value = 0;
    for (int b = 0; b < 32; b++) {
        uint32_t mark = timings[2 + b * 2];
        uint32_t space = timings[3 + b * 2];
        if (mark < 200 || mark > 900) return 0;
        if (space >= 200 && space <= 950) {
            /* 0 bit */
        } else if (space >= 1150 && space <= 2200) {
            /* 1 bit */
            value |= (1UL << (31 - b));
        } else {
            return 0;
        }
    }

    snprintf(nec_out, nec_len, "%08lX", (unsigned long)value);
    return 1;
}

int decode_sony_from_raw(const char *raw, char *sony_out, size_t sony_len) {
    if (!raw || !sony_out || sony_len < 6) return 0;
    while (*raw && isspace((unsigned char)*raw)) raw++;
    if (*raw != 'F' && *raw != 'f') return 0;
    raw++;
    /* Skip frequency */
    while (*raw && isxdigit((unsigned char)*raw)) raw++;

    uint32_t timings[128];
    size_t count = 0;
    while (*raw && count < sizeof(timings)/sizeof(timings[0])) {
        char type = *raw++;
        if (type == 'P' || type == 'p' || type == 'S' || type == 's') {
            char *endp = NULL;
            unsigned long dur = strtoul(raw, &endp, 16);
            raw = endp;
            timings[count++] = (uint32_t)dur;
        } else if (isspace((unsigned char)type)) {
            continue;
        } else {
            break;
        }
    }

    if (count < 25) return 0; /* 1 leader pair + 12 bit pulses = 25 pulses */

    /* Leader mark: 2000..2800 us, space: 400..900 us */
    if (timings[0] < 2000 || timings[0] > 2800) return 0;
    if (timings[1] < 400 || timings[1] > 900) return 0;

    int nbits = 0;
    if (count >= 41) nbits = 20;
    else if (count >= 31) nbits = 15;
    else if (count >= 25) nbits = 12;
    else return 0;

    uint32_t value = 0;
    for (int b = 0; b < nbits; b++) {
        uint32_t mark = timings[2 + b * 2];
        if (mark >= 400 && mark <= 900) {
            /* 0 bit */
        } else if (mark >= 1000 && mark <= 1600) {
            /* 1 bit */
            value |= (1UL << (nbits - 1 - b));
        } else {
            return 0;
        }
    }

    if (nbits == 12) {
        snprintf(sony_out, sony_len, "%03lX", (unsigned long)value);
    } else if (nbits == 15) {
        snprintf(sony_out, sony_len, "%04lX", (unsigned long)value);
    } else {
        snprintf(sony_out, sony_len, "%05lX", (unsigned long)value);
    }
    return 1;
}

void analyze_capture_storage(const char *raw_code, const char *keycode_in, const char *nec_in, const char *protocol_text, char *mode_out, size_t mode_len, char *keycode_out, size_t keycode_len, char *nec_out, size_t nec_len, int *protocol_id_out, char *summary, size_t summary_len) {
    int protocol_id = protocol_text && protocol_text[0] ? atoi(protocol_text) : 2;
    if (protocol_id <= 0) protocol_id = 2;
    if (mode_out && mode_len) mode_out[0] = 0;
    if (keycode_out && keycode_len) keycode_out[0] = 0;
    if (nec_out && nec_len) nec_out[0] = 0;
    if (summary && summary_len) summary[0] = 0;
    if (extract_harmony_keycode(keycode_in, keycode_out, keycode_len) ||
        extract_harmony_keycode(raw_code, keycode_out, keycode_len)) {
        protocol_id = infer_capture_protocol(raw_code, keycode_out);
        copy_text(mode_out, mode_len, "keycode");
        copy_text(summary, summary_len, "Decoded Harmony KeyCode; storing compact protocol/keycode data.");
    } else if (extract_nec_hex(nec_in, nec_out, nec_len) ||
               extract_nec_hex(raw_code, nec_out, nec_len) ||
               decode_nec_from_raw(raw_code, nec_out, nec_len)) {
        protocol_id = infer_capture_protocol(raw_code, keycode_in);
        copy_text(mode_out, mode_len, "nec");
        if (build_nec_keycode(nec_out, protocol_id, keycode_out, keycode_len) == 0) {
            copy_text(summary, summary_len, "Decoded NEC-style 32-bit value; storing as compact Harmony KeyCode.");
        } else {
            copy_text(summary, summary_len, "Decoded a hex value, but it was not valid for compact storage.");
        }
    } else if (decode_sony_from_raw(raw_code, nec_out ? nec_out : keycode_out, nec_out ? nec_len : keycode_len)) {
        if (keycode_out && keycode_len && nec_out) copy_text(keycode_out, keycode_len, nec_out);
        copy_text(mode_out, mode_len, "sony");
        copy_text(summary, summary_len, "Decoded Sony SIRC command; storing as hex value.");
    } else {
        copy_text(mode_out, mode_len, "raw");
        copy_text(summary, summary_len, "No supported protocol signature found; storing raw timing data.");
    }
    if (protocol_id_out) *protocol_id_out = protocol_id;
}

unsigned int reverse8(unsigned int v) {
    v = ((v & 0xf0) >> 4) | ((v & 0x0f) << 4);
    v = ((v & 0xcc) >> 2) | ((v & 0x33) << 2);
    v = ((v & 0xaa) >> 1) | ((v & 0x55) << 1);
    return v & 0xff;
}

int build_irdb_nec_keycode(const char *protocol, const char *device, const char *subdevice, const char *function, char *out, size_t outlen) {
    unsigned long d, s, fn, inv;
    unsigned long value;
    if (strncasecmp(protocol, "NEC", 3) != 0 && strncasecmp(protocol, "Pioneer", 7) != 0) return -1;
    d = strtoul(device, NULL, 10);
    s = (!subdevice[0] || strcmp(subdevice, "-1") == 0) ? (d ^ 0xff) : strtoul(subdevice, NULL, 10);
    fn = strtoul(function, NULL, 10);
    if (d > 255 || s > 255 || fn > 255) return -1;
    inv = (~fn) & 0xff;
    value = (reverse8((unsigned int)d) << 24) |
            (reverse8((unsigned int)s) << 16) |
            (reverse8((unsigned int)fn) << 8) |
            reverse8((unsigned int)inv);
    snprintf(out, outlen, "G:Toshiba 32 Bit:(0x%08lX)(Repeat)():3", value & 0xffffffffUL);
    return 0;
}

cJSON *build_ir_command_object(long id, const char *name, const char *mode, int protocol_id, const char *code, const char *raw_code) {
    cJSON *cmd = cJSON_CreateObject();
    if (!cmd) return NULL;
    char date_taught[64];
    snprintf(date_taught, sizeof(date_taught), "/Date(%ld000+0000)/", (long)time(NULL));

    if (strcmp(mode, "raw") == 0) {
        cJSON_AddStringToObject(cmd, "Raw", raw_code ? raw_code : "");
        cJSON_AddNumberToObject(cmd, "Id-", id);
        cJSON_AddStringToObject(cmd, "KeyCode", "");
        cJSON_AddStringToObject(cmd, "DateTaught", date_taught);
        cJSON_AddNullToObject(cmd, "FunctionId");
        cJSON_AddNullToObject(cmd, "Parameters");
        cJSON_AddStringToObject(cmd, "Name", name ? name : "");
        cJSON_AddNumberToObject(cmd, "FunctionGroupId", 0);
        cJSON_AddNumberToObject(cmd, "TransportType", 1);
        cJSON_AddNullToObject(cmd, "ProtocolId");
        cJSON_AddStringToObject(cmd, "CommandTypeId", "");
        cJSON_AddBoolToObject(cmd, "IsLearned", 1);
    } else {
        cJSON_AddNullToObject(cmd, "Raw");
        cJSON_AddNumberToObject(cmd, "Id-", id);
        cJSON_AddStringToObject(cmd, "KeyCode", code ? code : "");
        cJSON_AddStringToObject(cmd, "DateTaught", date_taught);
        cJSON_AddNullToObject(cmd, "FunctionId");
        cJSON_AddNullToObject(cmd, "Parameters");
        cJSON_AddStringToObject(cmd, "Name", name ? name : "");
        cJSON_AddNumberToObject(cmd, "FunctionGroupId", 0);
        cJSON_AddNumberToObject(cmd, "TransportType", 1);
        cJSON_AddNumberToObject(cmd, "ProtocolId", protocol_id);
        cJSON_AddStringToObject(cmd, "CommandTypeId", "");
        cJSON_AddBoolToObject(cmd, "IsLearned", 1);
    }
    return cmd;
}

int device_has_command_name(cJSON *cmds, const char *name) {
    if (!cmds || !cJSON_IsArray(cmds) || !name) return 0;
    cJSON *c = NULL;
    cJSON_ArrayForEach(c, cmds) {
        cJSON *jname = cJSON_GetObjectItemCaseSensitive(c, "Name");
        if (jname && cJSON_IsString(jname) && jname->valuestring && strcmp(jname->valuestring, name) == 0) return 1;
    }
    return 0;
}

int update_ir_command(const char *device_id, const char *old_name, const char *new_name, const char *mode, const char *protocol_text, const char *nec, const char *keycode, const char *raw_code, char *msg, size_t msglen) {
    if (!safe_label(device_id) || !safe_label(old_name) || !safe_label(new_name)) {
        snprintf(msg, msglen, "Device, current command name, and new command name are required.");
        return -1;
    }
    int protocol_id = atoi(protocol_text);
    if (protocol_id <= 0) protocol_id = 2;

    char effective_mode[16];
    char code[512];
    copy_text(effective_mode, sizeof(effective_mode), mode && mode[0] ? mode : "keycode");
    code[0] = 0;
    if (strcmp(effective_mode, "raw") == 0) {
        if (!raw_code[0]) {
            snprintf(msg, msglen, "Raw command data is required.");
            return -1;
        }
    } else if (strcmp(effective_mode, "nec") == 0) {
        if (build_nec_keycode(nec, protocol_id, code, sizeof(code)) != 0) {
            snprintf(msg, msglen, "NEC value must be 1-8 hex digits.");
            return -1;
        }
    } else {
        copy_text(effective_mode, sizeof(effective_mode), "keycode");
        if (!keycode[0]) {
            snprintf(msg, msglen, "Harmony compact code is required.");
            return -1;
        }
        copy_text(code, sizeof(code), keycode);
    }

    size_t len;
    char *raw = read_file_alloc(DEVICE_LIST, MAX_RESOURCE_FILE, &len);
    if (!raw) {
        snprintf(msg, msglen, "Command %s was not found on device %s.", old_name, device_id);
        return -1;
    }
    cJSON *root = cJSON_Parse(raw);
    free(raw);
    if (!root) {
        snprintf(msg, msglen, "Command %s was not found on device %s.", old_name, device_id);
        return -1;
    }
    cJSON *dev_item = find_device_item(root, device_id);
    if (!dev_item) {
        cJSON_Delete(root);
        snprintf(msg, msglen, "Command %s was not found on device %s.", old_name, device_id);
        return -1;
    }
    cJSON *cmds = cJSON_GetObjectItemCaseSensitive(dev_item, "Commands");
    if (!cmds || !cJSON_IsArray(cmds)) {
        cJSON_Delete(root);
        snprintf(msg, msglen, "Command %s was not found on device %s.", old_name, device_id);
        return -1;
    }

    cJSON *existing = NULL;
    cJSON *target_cmd = NULL;
    int idx = 0, found_idx = -1;
    cJSON_ArrayForEach(existing, cmds) {
        cJSON *jname = cJSON_GetObjectItemCaseSensitive(existing, "Name");
        if (jname && cJSON_IsString(jname) && jname->valuestring) {
            if (strcmp(jname->valuestring, old_name) == 0) {
                target_cmd = existing;
                found_idx = idx;
            } else if (strcmp(old_name, new_name) != 0 && strcmp(jname->valuestring, new_name) == 0) {
                cJSON_Delete(root);
                snprintf(msg, msglen, "Command %s already exists on device %s.", new_name, device_id);
                return -1;
            }
        }
        idx++;
    }

    if (!target_cmd || found_idx < 0) {
        cJSON_Delete(root);
        snprintf(msg, msglen, "Command %s was not found on device %s.", old_name, device_id);
        return -1;
    }

    cJSON *jid = cJSON_GetObjectItemCaseSensitive(target_cmd, "Id-");
    long id = (jid && cJSON_IsNumber(jid)) ? (long)jid->valuedouble : 0;
    if (id <= 0) id = 39000000 + (long)(time(NULL) % 9000000);

    cJSON *new_cmd = build_ir_command_object(id, new_name, effective_mode, protocol_id, code, raw_code);
    if (!new_cmd) {
        cJSON_Delete(root);
        snprintf(msg, msglen, "Failed to build updated command.");
        return -1;
    }

    backup_resources();
    if (strcmp(effective_mode, "raw") != 0 && ensure_builtin_protocol_for_id(protocol_id) != 0) {
        cJSON_Delete(new_cmd);
        cJSON_Delete(root);
        snprintf(msg, msglen, "Failed to ensure IR protocol %d.", protocol_id);
        return -1;
    }

    cJSON_ReplaceItemInArray(cmds, found_idx, new_cmd);

    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!out || write_file_atomic(DEVICE_LIST, out, strlen(out)) != 0) {
        free(out);
        snprintf(msg, msglen, "Failed to save updated command %s.", new_name);
        return -1;
    }
    invalidate_device_list_cache();
    free(out);
    request_resource_reload();
    snprintf(msg, msglen, "Updated command %s on device %s.", new_name, device_id);
    return 0;
}

int add_ir_command(const char *device_id, const char *name, const char *mode, const char *protocol_text, const char *nec, const char *keycode, const char *raw_code, char *msg, size_t msglen) {
    long id = 0, max_command_id = 0;
    int protocol_id = atoi(protocol_text);
    char effective_mode[16];
    char effective_keycode[512];
    char effective_nec[64];
    char storage_note[160];
    char code[512];
    int auto_mode;
    if (!safe_label(device_id) || !safe_label(name)) {
        snprintf(msg, msglen, "Device and command name are required.");
        return -1;
    }
    if (protocol_id <= 0) protocol_id = 2;
    copy_text(effective_mode, sizeof(effective_mode), mode && mode[0] ? mode : "auto");
    copy_text(effective_keycode, sizeof(effective_keycode), keycode);
    copy_text(effective_nec, sizeof(effective_nec), nec);
    auto_mode = strcmp(effective_mode, "auto") == 0;
    if (auto_mode) {
        analyze_capture_storage(raw_code, keycode, nec, protocol_text, effective_mode, sizeof(effective_mode), effective_keycode, sizeof(effective_keycode), effective_nec, sizeof(effective_nec), &protocol_id, storage_note, sizeof(storage_note));
    }
    code[0] = 0;
    if (strcmp(effective_mode, "raw") == 0) {
        if (!raw_code[0]) {
            snprintf(msg, msglen, "Raw command data is required.");
            return -1;
        }
    } else if (strcmp(effective_mode, "keycode") == 0) {
        if (!effective_keycode[0]) {
            snprintf(msg, msglen, "KeyCode is required.");
            return -1;
        }
        strncpy(code, effective_keycode, sizeof(code) - 1);
        code[sizeof(code) - 1] = 0;
    } else {
        if (build_nec_keycode(effective_nec, protocol_id, code, sizeof(code)) != 0) {
            snprintf(msg, msglen, "NEC value must be 1-8 hex digits.");
            return -1;
        }
    }

    char *raw = read_file_alloc(DEVICE_LIST, MAX_RESOURCE_FILE, NULL);
    if (!raw) {
        snprintf(msg, msglen, "Device %s not found.", device_id);
        return -1;
    }
    cJSON *root = cJSON_Parse(raw);
    free(raw);
    if (!root) {
        snprintf(msg, msglen, "Device %s not found.", device_id);
        return -1;
    }

    cJSON *dev_item = find_device_item(root, device_id);
    if (!dev_item) {
        cJSON_Delete(root);
        snprintf(msg, msglen, "Device %s not found.", device_id);
        return -1;
    }

    cJSON *cmds = cJSON_GetObjectItemCaseSensitive(dev_item, "Commands");
    if (!cmds || !cJSON_IsArray(cmds)) {
        cmds = cJSON_CreateArray();
        cJSON_AddItemToObject(dev_item, "Commands", cmds);
    }

    if (cJSON_GetArraySize(cmds) >= MAX_IR_STORED_COMMANDS) {
        cJSON_Delete(root);
        snprintf(msg, msglen, "Device %s already has the local storage limit of %d commands.", device_id, MAX_IR_STORED_COMMANDS);
        return -1;
    }

    if (device_has_command_name(cmds, name)) {
        cJSON_Delete(root);
        snprintf(msg, msglen, "Command %s already exists on device %s.", name, device_id);
        return -1;
    }

    scan_ir_resource_stats(NULL, NULL, NULL, &max_command_id);
    id = max_command_id + 1;
    if (id < 39000000) id = 39000000 + (long)(time(NULL) % 9000000);

    cJSON *cmd_obj = build_ir_command_object(id, name, effective_mode, protocol_id, code, raw_code);
    if (!cmd_obj) {
        cJSON_Delete(root);
        snprintf(msg, msglen, "Failed to save IR command.");
        return -1;
    }

    backup_resources();
    if (strcmp(effective_mode, "raw") != 0 && ensure_builtin_protocol_for_id(protocol_id) != 0) {
        cJSON_Delete(cmd_obj);
        cJSON_Delete(root);
        snprintf(msg, msglen, "Failed to ensure IR protocol %d.", protocol_id);
        return -1;
    }

    cJSON_AddItemToArray(cmds, cmd_obj);
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!out || write_file_atomic(DEVICE_LIST, out, strlen(out)) != 0) {
        free(out);
        snprintf(msg, msglen, "Failed to save IR command.");
        return -1;
    }
    invalidate_device_list_cache();
    free(out);
    request_resource_reload();
    if (auto_mode && storage_note[0]) snprintf(msg, msglen, "Saved command %s on device %s. %s", name, device_id, storage_note);
    else snprintf(msg, msglen, "Saved command %s on device %s.", name, device_id);
    return 0;
}


int safe_raw_import_value(const char *s) {
    const unsigned char *p = (const unsigned char *)s;
    size_t n = strlen(s);
    if (n == 0 || n > 4096) return 0;
    while (*p) {
        if (*p < 32 || *p == 127) return 0;
        p++;
    }
    return 1;
}

int bulk_import_irdb_commands(const char *device_id, char *payload, char *msg, size_t msglen) {
    char *line, *save, *raw;
    size_t raw_len;
    long next_id, max_command_id = 0;
    int imported = 0, skipped = 0, remaining, existing_commands = 0, imported_keycodes = 0;
    if (!safe_label(device_id)) {
        snprintf(msg, msglen, "Invalid device for IRDB import.");
        return -1;
    }
    raw = read_file_alloc(DEVICE_LIST, MAX_RESOURCE_FILE, &raw_len);
    if (!raw) {
        snprintf(msg, msglen, "Failed to locate device command list.");
        return -1;
    }
    cJSON *root = cJSON_Parse(raw);
    free(raw);
    if (!root) {
        snprintf(msg, msglen, "Failed to locate device command list.");
        return -1;
    }
    cJSON *dev_item = find_device_item(root, device_id);
    if (!dev_item) {
        cJSON_Delete(root);
        snprintf(msg, msglen, "Failed to locate device command list.");
        return -1;
    }
    cJSON *cmds = cJSON_GetObjectItemCaseSensitive(dev_item, "Commands");
    if (!cmds || !cJSON_IsArray(cmds)) {
        cmds = cJSON_CreateArray();
        cJSON_AddItemToObject(dev_item, "Commands", cmds);
    }
    existing_commands = cJSON_GetArraySize(cmds);
    remaining = MAX_IR_STORED_COMMANDS - existing_commands;
    if (remaining <= 0) {
        cJSON_Delete(root);
        snprintf(msg, msglen, "Device %s already has the local storage limit of %d commands.", device_id, MAX_IR_STORED_COMMANDS);
        return -1;
    }

    scan_ir_resource_stats(NULL, NULL, NULL, &max_command_id);
    next_id = max_command_id + 1;
    if (next_id < 39000000) next_id = 39000000 + (long)(time(NULL) % 9000000);

    line = strtok_r(payload, "\n", &save);
    while (line) {
        char *fields[8], *name = NULL, *mode = "keycode", *keycode = NULL, *raw_code = NULL;
        char generated[512];
        int field_count;
        line = trim_in_place(line);
        if (!line[0]) {
            line = strtok_r(NULL, "\n", &save);
            continue;
        }
        if (strchr(line, '|')) {
            field_count = split_fields(line, '|', fields, 8);
            if (field_count >= 2) {
                name = trim_in_place(fields[0]);
                if (field_count >= 3) {
                    mode = trim_in_place(fields[1]);
                    if (strcasecmp(mode, "raw") == 0) {
                        raw_code = trim_in_place(fields[2]);
                    } else if (strcasecmp(mode, "keycode") == 0) {
                        keycode = trim_in_place(fields[2]);
                    } else {
                        keycode = trim_in_place(fields[1]);
                        mode = "keycode";
                    }
                } else {
                    keycode = trim_in_place(fields[1]);
                }
            }
        } else {
            field_count = split_fields(line, ',', fields, 8);
            if (field_count >= 5 && strcasecmp(trim_in_place(fields[0]), "functionname") != 0) {
                name = trim_in_place(fields[0]);
                if (build_irdb_nec_keycode(trim_in_place(fields[1]), trim_in_place(fields[2]), trim_in_place(fields[3]), trim_in_place(fields[4]), generated, sizeof(generated)) == 0) {
                    keycode = generated;
                }
            }
        }
        if (!name || !name[0] || !safe_label(name) || device_has_command_name(cmds, name) ||
            (strcasecmp(mode, "raw") == 0 ? (!raw_code || !safe_raw_import_value(raw_code)) : (!keycode || !keycode[0]))) {
            skipped++;
            line = strtok_r(NULL, "\n", &save);
            continue;
        }
        if (imported >= remaining) {
            skipped++;
            line = strtok_r(NULL, "\n", &save);
            continue;
        }
        cJSON *cmd_obj = strcasecmp(mode, "raw") == 0 ?
            build_ir_command_object(next_id++, name, "raw", 2, "", raw_code) :
            build_ir_command_object(next_id++, name, "keycode", 2, keycode, "");
        if (!cmd_obj) {
            skipped++;
            line = strtok_r(NULL, "\n", &save);
            continue;
        }
        cJSON_AddItemToArray(cmds, cmd_obj);
        imported++;
        if (strcasecmp(mode, "raw") != 0) imported_keycodes++;
        line = strtok_r(NULL, "\n", &save);
    }

    if (imported == 0) {
        cJSON_Delete(root);
        snprintf(msg, msglen, "No supported new IRDB commands were selected.");
        return -1;
    }

    backup_resources();
    if (imported_keycodes > 0 && ensure_builtin_protocol_for_id(2) != 0) {
        cJSON_Delete(root);
        snprintf(msg, msglen, "Failed to ensure NEC-compatible IR protocol.");
        return -1;
    }

    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!out || write_file_atomic(DEVICE_LIST, out, strlen(out)) != 0) {
        free(out);
        snprintf(msg, msglen, "Failed to write imported IRDB commands.");
        return -1;
    }
    invalidate_device_list_cache();
    free(out);
    request_resource_reload();
    snprintf(msg, msglen, "Imported %d IRDB commands to %s. Skipped %d.", imported, device_id, skipped);
    return 0;
}

cJSON *build_power_actions_cjson(const char *text) {
    if (!text || !text[0]) return cJSON_CreateArray();
    const char *chk = text;
    while (*chk && isspace((unsigned char)*chk)) chk++;
    if (*chk == '[') {
        cJSON *parsed = cJSON_Parse(chk);
        if (parsed && cJSON_IsArray(parsed)) return parsed;
        if (parsed) cJSON_Delete(parsed);
        return cJSON_CreateArray();
    }
    cJSON *arr = cJSON_CreateArray();
    if (!arr) return NULL;
    const char *p = text;
    int order = 0;
    while (*p) {
        char line[256];
        size_t l = 0;
        char *f[4];
        int count, delay = 0;
        char type[32] = "", cmd[80] = "";
        while (*p && *p != '\n' && *p != '\r' && l < sizeof(line) - 1) line[l++] = *p++;
        line[l] = 0;
        while (*p == '\n' || *p == '\r') p++;
        trim_in_place(line);
        if (!line[0] || line[0] == '#') continue;

        count = split_fields(line, '|', f, 4);
        if (count >= 1) snprintf(type, sizeof(type), "%s", trim_in_place(f[0]));
        if (count >= 2) {
            if (strcasecmp(type, "Delay") == 0) delay = atoi(trim_in_place(f[1]));
            else snprintf(cmd, sizeof(cmd), "%s", trim_in_place(f[1]));
        }
        if (count >= 3 && !cmd[0]) snprintf(cmd, sizeof(cmd), "%s", trim_in_place(f[2]));
        if (count >= 4 && delay == 0) delay = atoi(trim_in_place(f[3]));

        cJSON *item = cJSON_CreateObject();
        if (strcasecmp(type, "Delay") == 0) {
            cJSON_AddStringToObject(item, "__type", "IRDelayAction");
            cJSON_AddNumberToObject(item, "Delay", delay > 0 ? delay : 1000);
            cJSON_AddNumberToObject(item, "Order", order);
            cJSON_AddNumberToObject(item, "ActionId", 3);
        } else {
            cJSON_AddStringToObject(item, "__type", "IRPressAction");
            cJSON_AddStringToObject(item, "IRCommandName", cmd);
            cJSON_AddNumberToObject(item, "Order", order);
            cJSON_AddNullToObject(item, "Duration");
            cJSON_AddNumberToObject(item, "ActionId", 0);
        }
        cJSON_AddItemToArray(arr, item);
        order++;
    }
    return arr;
}

int save_device_power_settings(const char *device_id, int power_on_delay, int is_power_always_on, const char *power_on_seq, const char *power_off_seq, char *msg, size_t msglen) {
    if (!device_id || !device_id[0] || !msg || msglen == 0) return -1;
    size_t len = 0;
    char *raw = read_file_alloc(DEVICE_LIST, MAX_RESOURCE_FILE, &len);
    if (!raw) {
        snprintf(msg, msglen, "Failed to read DeviceList.");
        return -1;
    }
    cJSON *root = cJSON_Parse(raw);
    free(raw);
    if (!root) {
        snprintf(msg, msglen, "Failed to parse DeviceList.");
        return -1;
    }

    cJSON *dev_item = find_device_item(root, device_id);
    if (!dev_item) {
        cJSON_Delete(root);
        snprintf(msg, msglen, "Device %s not found.", device_id);
        return -1;
    }

    cJSON *feats = cJSON_GetObjectItemCaseSensitive(dev_item, "DeviceFeatures");
    if (!feats || !cJSON_IsArray(feats)) {
        feats = cJSON_CreateArray();
        cJSON_AddItemToObject(dev_item, "DeviceFeatures", feats);
    }

    cJSON *new_pon = build_power_actions_cjson(power_on_seq);
    cJSON *new_poff = build_power_actions_cjson(power_off_seq);

    cJSON *power_feat = NULL;
    cJSON *f = NULL;
    cJSON_ArrayForEach(f, feats) {
        cJSON *ftype = cJSON_GetObjectItemCaseSensitive(f, "__type");
        if (ftype && cJSON_IsString(ftype) && strcmp(ftype->valuestring, "PowerFeature") == 0) {
            power_feat = f;
            break;
        }
    }

    long dev_id_val = atol(device_id);

    if (power_feat) {
        cJSON_ReplaceItemInObject(power_feat, "PowerOnDelay", cJSON_CreateNumber(power_on_delay));
        cJSON_ReplaceItemInObject(power_feat, "DefaultPowerOnDelay", cJSON_CreateNumber(power_on_delay));
        cJSON_ReplaceItemInObject(power_feat, "IsPowerAlwaysOn", cJSON_CreateBool(is_power_always_on));
        cJSON_ReplaceItemInObject(power_feat, "PowerOnActions", new_pon);
        cJSON_ReplaceItemInObject(power_feat, "PowerOffActions", new_poff);
    } else {
        cJSON *pf = cJSON_CreateObject();
        cJSON_AddArrayToObject(pf, "PowerOnResetActions");
        cJSON_AddNumberToObject(pf, "ConnectedAppPowerOnDelay", 0);
        cJSON_AddNumberToObject(pf, "PowerOnActionId", 0);
        cJSON_AddNumberToObject(pf, "DeviceId-", dev_id_val);
        cJSON_AddNumberToObject(pf, "PowerTypeId", 1);
        cJSON_AddArrayToObject(pf, "PowerToggleActions");
        cJSON_AddNumberToObject(pf, "PowerToggleActionId", 0);
        cJSON_AddNumberToObject(pf, "PowerOnSetType", 0);
        cJSON_AddNumberToObject(pf, "PowerOffActionId", 0);
        cJSON_AddBoolToObject(pf, "IsPoweredOnBetweenActivities", 0);
        cJSON_AddItemToObject(pf, "PowerOffActions", new_poff);
        cJSON_AddNumberToObject(pf, "Id", 0);
        cJSON_AddNullToObject(pf, "PowerOnResetInputName");
        cJSON_AddNumberToObject(pf, "PowerOnDelay", power_on_delay);
        cJSON_AddBoolToObject(pf, "HasAdditionalActions", 0);
        cJSON_AddNumberToObject(pf, "State", 1);
        cJSON_AddStringToObject(pf, "__type", "PowerFeature");
        cJSON_AddNumberToObject(pf, "GlobalDeviceVersionId-", 0);
        cJSON_AddNumberToObject(pf, "DefaultPowerOnDelay", power_on_delay);
        cJSON_AddBoolToObject(pf, "IsPowerAlwaysOn", is_power_always_on ? 1 : 0);
        cJSON_AddNullToObject(pf, "DateModified");
        cJSON_AddNumberToObject(pf, "PowerOffSetType", 0);
        cJSON_AddItemToObject(pf, "PowerOnActions", new_pon);
        cJSON_AddItemToArray(feats, pf);
    }

    backup_resources();
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!out || write_file_atomic(DEVICE_LIST, out, strlen(out)) != 0) {
        free(out);
        snprintf(msg, msglen, "Failed to write DeviceList.json.");
        return -1;
    }
    invalidate_device_list_cache();
    free(out);

    request_resource_reload();
    snprintf(msg, msglen, "Updated power settings for device %s.", device_id);
    return 0;
}



int ir_cancel_path(const char *run_id, char *path, size_t pathlen) {
    if (!run_id || !run_id[0]) return 0;
    if (!safe_run_id(run_id)) return -1;
    snprintf(path, pathlen, "%s%s", IR_CANCEL_PREFIX, run_id);
    return 1;
}

int ir_run_canceled(const char *run_id) {
    char path[192];
    int ok = ir_cancel_path(run_id, path, sizeof(path));
    if (ok <= 0) return 0;
    return access(path, F_OK) == 0;
}

int mark_ir_run_canceled(const char *run_id) {
    char path[192];
    FILE *f;
    int ok = ir_cancel_path(run_id, path, sizeof(path));
    if (ok <= 0) return -1;
    f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "%ld\n", (long)time(NULL));
    fclose(f);
    return 0;
}

int cancelable_sleep_ms(int delay_ms, const char *run_id) {
    int slept = 0;
    while (slept < delay_ms) {
        int step = delay_ms - slept;
        if (ir_run_canceled(run_id)) return 1;
        if (step > 100) step = 100;
        usleep((useconds_t)step * 1000);
        slept += step;
    }
    return ir_run_canceled(run_id);
}

void rotate_ir_event_log(void) {
    struct stat st;
    if (stat(IR_EVENT_LOG, &st) == 0 && st.st_size > IR_EVENT_MAX_BYTES) {
        unlink(IR_EVENT_LOG ".1");
        rename(IR_EVENT_LOG, IR_EVENT_LOG ".1");
    }
}

void log_ir_event(const char *source, const char *run_id, const char *device_id, const char *command, const char *reply) {
    if (!is_debug_log_enabled()) return;
    rotate_ir_event_log();
    FILE *f = fopen(IR_EVENT_LOG, "a");
    if (!f) return;
    cJSON *obj = cJSON_CreateObject();
    if (obj) {
        cJSON_AddStringToObject(obj, "event", "ir_send");
        cJSON_AddNumberToObject(obj, "ts", (double)time(NULL));
        cJSON_AddStringToObject(obj, "source", source ? source : "");
        cJSON_AddStringToObject(obj, "runId", run_id ? run_id : "");
        cJSON_AddStringToObject(obj, "deviceId", device_id ? device_id : "");
        cJSON_AddStringToObject(obj, "command", command ? command : "");
        cJSON_AddStringToObject(obj, "reply", reply ? reply : "");
        char *s = cJSON_PrintUnformatted(obj);
        if (s) {
            fputs(s, f);
            fputc('\n', f);
            free(s);
        }
        cJSON_Delete(obj);
    }
    fclose(f);
}

void log_ir_note_event(const char *event, const char *source, const char *run_id, const char *detail) {
    if (!is_debug_log_enabled()) return;
    rotate_ir_event_log();
    FILE *f = fopen(IR_EVENT_LOG, "a");
    if (!f) return;
    cJSON *obj = cJSON_CreateObject();
    if (obj) {
        cJSON_AddStringToObject(obj, "event", event ? event : "ir_note");
        cJSON_AddNumberToObject(obj, "ts", (double)time(NULL));
        cJSON_AddStringToObject(obj, "source", source ? source : "");
        cJSON_AddStringToObject(obj, "runId", run_id ? run_id : "");
        cJSON_AddStringToObject(obj, "detail", detail ? detail : "");
        char *s = cJSON_PrintUnformatted(obj);
        if (s) {
            fputs(s, f);
            fputc('\n', f);
            free(s);
        }
        cJSON_Delete(obj);
    }
    fclose(f);
}

void send_ir_command_action_ex(const char *device_id, const char *command, const char *source, const char *run_id, char *out, size_t outlen) {
    if (!device_id || !device_id[0] || !command || !command[0]) {
        if (out && outlen) snprintf(out, outlen, "Missing device or command parameter");
        return;
    }
    int rc = hw_device_command_send(device_id, command);
    if (rc != 0 && (strncmp(command, "0000 ", 5) == 0 || command[0] == 'F' || command[0] == 'f')) {
        rc = hw_ir_send_harmony_keycode(command, IR_PORT_ALL, 3);
    }
    if (rc == 0) {
        if (out && outlen) snprintf(out, outlen, "ok");
    } else {
        if (out && outlen) snprintf(out, outlen, "IR dispatch failed (rc=%d)", rc);
    }
    log_ir_event(source, run_id, device_id, command, out && out[0] ? out : "ok");
}

void send_ir_command_action(const char *device_id, const char *command, char *out, size_t outlen) {
    send_ir_command_action_ex(device_id, command, "webui", "", out, outlen);
}

void capture_ir_command_action(char *out, size_t outlen) {
    int rc = ir_i2s_capture(out, outlen, 5);
    if (rc != 0 && (!out || !out[0])) {
        snprintf(out, outlen, "No IR signal received: timeout waiting for remote button press");
    }
}


void handle_ir_send(int fd, const struct request *req) {
    char device_id[64], command[128], reply[4096], message[4608];
    form_value(req->body, "deviceId", device_id, sizeof(device_id));
    form_value(req->body, "command", command, sizeof(command));
    if (!safe_label(device_id) || !safe_label(command)) {
        render_page(fd, req, "Invalid IR command request.");
        return;
    }
    repair_known_protocols_for_current_commands();
    send_ir_command_action(device_id, command, reply, sizeof(reply));
    snprintf(message, sizeof(message), "Sent %s to %s. Reply: %s", command, device_id, reply[0] ? reply : "no response");
    render_page(fd, req, message);
}

void render_ir_send_json(int fd, const struct request *req) {
    char device_id[64], command[128], reply[4096];
    form_value(req->body, "deviceId", device_id, sizeof(device_id));
    form_value(req->body, "command", command, sizeof(command));
    if (!device_id[0]) json_string(req->body, "deviceId", device_id, sizeof(device_id));
    if (!command[0]) json_string(req->body, "command", command, sizeof(command));
    if (!safe_label(device_id) || !safe_label(command)) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "invalid IR command request");
        send_cjson_resp(fd, "400 Bad Request", err);
        cJSON_Delete(err);
        return;
    }
    repair_known_protocols_for_current_commands();
    send_ir_command_action_ex(device_id, command, "api", "", reply, sizeof(reply));
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", 1);
    cJSON_AddStringToObject(resp, "deviceId", device_id);
    cJSON_AddStringToObject(resp, "command", command);
    cJSON_AddStringToObject(resp, "reply", reply[0] ? reply : "no response");
    send_cjson_resp(fd, "200 OK", resp);
    cJSON_Delete(resp);
}

void render_ir_batch_send_json(int fd, const struct request *req) {
    char device_id[64], delay_text[32], dry_text[16], run_id[128], reply[1024], last_reply[1024];
    char *commands, *line, *save;
    int delay_ms, dry_run, sent = 0, skipped = 0, attempted = 0, failed = 0, canceled = 0;
    struct timeval start, end;
    form_value(req->body, "deviceId", device_id, sizeof(device_id));
    form_value(req->body, "delayMs", delay_text, sizeof(delay_text));
    form_value(req->body, "dryRun", dry_text, sizeof(dry_text));
    form_value(req->body, "runId", run_id, sizeof(run_id));
    if (!safe_label(device_id)) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "invalid IR batch device");
        send_cjson_resp(fd, "400 Bad Request", err);
        cJSON_Delete(err);
        return;
    }
    if (run_id[0] && !safe_run_id(run_id)) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "invalid IR run id");
        send_cjson_resp(fd, "400 Bad Request", err);
        cJSON_Delete(err);
        return;
    }
    dry_run = strcmp(dry_text, "1") == 0 || strcasecmp(dry_text, "true") == 0;
    delay_ms = atoi(delay_text);
    if (delay_ms < 40) delay_ms = 40;
    if (delay_ms > 10000) delay_ms = 10000;
    if (!dry_run) repair_known_protocols_for_current_commands();
    commands = (char *)malloc(MAX_REQUEST_BODY);
    if (!commands) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "not enough memory for IR batch");
        send_cjson_resp(fd, "500 Internal Server Error", err);
        cJSON_Delete(err);
        return;
    }
    form_value(req->body, "commands", commands, MAX_REQUEST_BODY);
    gettimeofday(&start, NULL);
    last_reply[0] = 0;
    line = strtok_r(commands, "\n", &save);
    while (line && attempted < MAX_IR_BATCH_COMMANDS) {
        char *cmd_name = trim_in_place(line);
        size_t n = strlen(cmd_name);
        if (ir_run_canceled(run_id)) {
            canceled = 1;
            break;
        }
        while (n && cmd_name[n - 1] == '\r') cmd_name[--n] = 0;
        if (!cmd_name[0] || !safe_label(cmd_name)) {
            skipped++;
            line = strtok_r(NULL, "\n", &save);
            continue;
        }
        attempted++;
        if (!dry_run) {
            send_ir_command_action_ex(device_id, cmd_name, "api-batch", run_id, reply, sizeof(reply));
            copy_text(last_reply, sizeof(last_reply), reply[0] ? reply : "no response");
            if (strstr(reply, "\"code\":500") || strstr(reply, "Invalid command")) failed++;
            if (delay_ms > 0 && cancelable_sleep_ms(delay_ms, run_id)) {
                canceled = 1;
                sent++;
                break;
            }
        }
        sent++;
        line = strtok_r(NULL, "\n", &save);
    }
    while (line) {
        skipped++;
        line = strtok_r(NULL, "\n", &save);
    }
    gettimeofday(&end, NULL);
    free(commands);
    if (sent == 0 && !canceled) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "no valid commands selected");
        send_cjson_resp(fd, "400 Bad Request", err);
        cJSON_Delete(err);
        return;
    }
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", 1);
    cJSON_AddStringToObject(resp, "deviceId", device_id);
    cJSON_AddStringToObject(resp, "runId", run_id);
    cJSON_AddBoolToObject(resp, "dryRun", dry_run ? 1 : 0);
    cJSON_AddBoolToObject(resp, "canceled", canceled ? 1 : 0);
    cJSON_AddNumberToObject(resp, "sent", sent);
    cJSON_AddNumberToObject(resp, "skipped", skipped);
    cJSON_AddNumberToObject(resp, "attempted", attempted);
    cJSON_AddNumberToObject(resp, "failed", failed);
    cJSON_AddNumberToObject(resp, "delayMs", delay_ms);
    cJSON_AddNumberToObject(resp, "elapsedMs", (long)((end.tv_sec - start.tv_sec) * 1000L + (end.tv_usec - start.tv_usec) / 1000L));
    cJSON_AddStringToObject(resp, "lastReply", last_reply[0] ? last_reply : "no response");
    send_cjson_resp(fd, "200 OK", resp);
    cJSON_Delete(resp);
}

void render_ir_cancel_json(int fd, const struct request *req) {
    char run_id[128];
    form_value(req->body, "runId", run_id, sizeof(run_id));
    if (!safe_run_id(run_id)) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "invalid IR run id");
        send_cjson_resp(fd, "400 Bad Request", err);
        cJSON_Delete(err);
        return;
    }
    if (mark_ir_run_canceled(run_id) != 0) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "unable to mark IR run canceled");
        send_cjson_resp(fd, "500 Internal Server Error", err);
        cJSON_Delete(err);
        return;
    }
    log_ir_note_event("ir_cancel", "api", run_id, "cancel requested");
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", 1);
    cJSON_AddStringToObject(resp, "runId", run_id);
    cJSON_AddBoolToObject(resp, "canceled", 1);
    send_cjson_resp(fd, "200 OK", resp);
    cJSON_Delete(resp);
}

void render_ir_lab_target_json(int fd) {
    char device_id[64], msg[512];
    int created = 0;
    int rc = ensure_lab_target_device(device_id, sizeof(device_id), &created, msg, sizeof(msg));
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", rc == 0 ? 1 : 0);
    cJSON_AddStringToObject(resp, "deviceId", rc == 0 ? device_id : "");
    cJSON_AddStringToObject(resp, "name", "Temporary IR Test");
    cJSON_AddBoolToObject(resp, "created", created ? 1 : 0);
    cJSON_AddStringToObject(resp, "message", msg);
    send_cjson_resp(fd, rc == 0 ? "200 OK" : "500 Internal Server Error", resp);
    cJSON_Delete(resp);
}

void render_ir_lab_clear_json(int fd, const struct request *req) {
    char device_id[64], lab_id[64], msg[512];
    int rc;
    form_value(req->body, "deviceId", device_id, sizeof(device_id));
    rc = ensure_lab_target_device(lab_id, sizeof(lab_id), NULL, msg, sizeof(msg));
    if (rc != 0) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", msg);
        send_cjson_resp(fd, "500 Internal Server Error", err);
        cJSON_Delete(err);
        return;
    }
    if (!device_id[0]) copy_text(device_id, sizeof(device_id), lab_id);
    if (strcmp(device_id, lab_id) != 0) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "clear only supports the temporary sweep device");
        send_cjson_resp(fd, "400 Bad Request", err);
        cJSON_Delete(err);
        return;
    }
    rc = clear_ir_commands(device_id, msg, sizeof(msg));
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", rc == 0 ? 1 : 0);
    cJSON_AddStringToObject(resp, "deviceId", device_id);
    cJSON_AddStringToObject(resp, "message", msg);
    send_cjson_resp(fd, rc == 0 ? "200 OK" : "500 Internal Server Error", resp);
    cJSON_Delete(resp);
}


void render_ir_test_learned_json(int fd, const struct request *req) {
    char device_id[64], name[128], mode[32], protocol[32], nec[64], keycode[512], raw[2048];
    char temp_name[128], add_msg[512], cleanup_msg[512], reply[4096], run_id[128];
    int add_rc, cleanup_rc = -1;

    form_value(req->body, "deviceId", device_id, sizeof(device_id));
    form_value(req->body, "name", name, sizeof(name));
    form_value(req->body, "mode", mode, sizeof(mode));
    form_value(req->body, "protocol", protocol, sizeof(protocol));
    form_value(req->body, "nec", nec, sizeof(nec));
    form_value(req->body, "keycode", keycode, sizeof(keycode));
    form_value(req->body, "raw", raw, sizeof(raw));
    if (!mode[0]) strcpy(mode, "auto");
    if (!protocol[0]) strcpy(protocol, "2");
    if (!safe_label(device_id)) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "choose a device before testing");
        send_cjson_resp(fd, "400 Bad Request", err);
        cJSON_Delete(err);
        return;
    }
    if (!raw[0] && !keycode[0] && !nec[0]) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "learn or enter a signal before testing");
        send_cjson_resp(fd, "400 Bad Request", err);
        cJSON_Delete(err);
        return;
    }
    snprintf(temp_name, sizeof(temp_name), "Signal Test %ld %d", (long)time(NULL), (int)(getpid() % 10000));
    add_rc = add_ir_command(device_id, temp_name, mode, protocol, nec, keycode, raw, add_msg, sizeof(add_msg));
    reply[0] = 0;
    if (add_rc == 0) {
        snprintf(run_id, sizeof(run_id), "learn_%ld_%d", (long)time(NULL), (int)(getpid() % 10000));
        usleep(800000);
        send_ir_command_action_ex(device_id, temp_name, "api-learn-test", run_id, reply, sizeof(reply));
        cleanup_rc = delete_ir_command(device_id, temp_name, cleanup_msg, sizeof(cleanup_msg));
    } else {
        cleanup_msg[0] = 0;
    }
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", add_rc == 0 ? 1 : 0);
    cJSON_AddStringToObject(resp, "deviceId", device_id);
    cJSON_AddStringToObject(resp, "tempCommand", add_rc == 0 ? temp_name : "");
    cJSON_AddStringToObject(resp, "addMessage", add_msg);
    cJSON_AddStringToObject(resp, "reply", reply[0] ? reply : "");
    cJSON_AddBoolToObject(resp, "cleanupOk", cleanup_rc == 0 ? 1 : 0);
    cJSON_AddStringToObject(resp, "cleanupMessage", cleanup_msg);
    send_cjson_resp(fd, add_rc == 0 ? "200 OK" : "400 Bad Request", resp);
    cJSON_Delete(resp);
}

static void mqtt_config_from_request(const struct request *req, char *device_id, size_t did_len,
                                     struct device_mqtt_config *dmcfg) {
    char topic[256], pulse_str[32], enabled_str[32];
    memset(dmcfg, 0, sizeof(*dmcfg));
    topic[0] = pulse_str[0] = enabled_str[0] = '\0';
    if (req->body && req->body[0] == '{') {
        json_string(req->body, "deviceId", device_id, did_len);
        json_string(req->body, "mqttTopic", topic, sizeof(topic));
        json_string(req->body, "mqttPulseMs", pulse_str, sizeof(pulse_str));
        if (!pulse_str[0]) {
            int pval = json_int(req->body, "mqttPulseMs", 1000);
            snprintf(pulse_str, sizeof(pulse_str), "%d", pval);
        }
        json_string(req->body, "mqttEnabled", enabled_str, sizeof(enabled_str));
        if (!enabled_str[0]) {
            int ben = json_bool(req->body, "mqttEnabled", 0);
            strcpy(enabled_str, ben ? "1" : "0");
        }
    } else {
        form_value(req->body, "deviceId", device_id, did_len);
        form_value(req->body, "mqttTopic", topic, sizeof(topic));
        form_value(req->body, "mqttPulseMs", pulse_str, sizeof(pulse_str));
        form_value(req->body, "mqttEnabled", enabled_str, sizeof(enabled_str));
    }
    dmcfg->enabled = (strcmp(enabled_str, "1") == 0 || strcasecmp(enabled_str, "true") == 0);
    strncpy(dmcfg->topic, topic[0] ? topic : "{root}/button/{device}/{command}", sizeof(dmcfg->topic) - 1);
    dmcfg->pulse_ms = atoi(pulse_str);
    if (dmcfg->pulse_ms <= 0) dmcfg->pulse_ms = 1000;
}

void render_device_mqtt_save_json(int fd, const struct request *req) {
    char device_id[64];
    struct device_mqtt_config dmcfg;
    int rc;

    mqtt_config_from_request(req, device_id, sizeof(device_id), &dmcfg);

    if (!safe_label(device_id)) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "invalid or missing device ID");
        send_cjson_resp(fd, "400 Bad Request", err);
        cJSON_Delete(err);
        return;
    }

    rc = save_device_mqtt_config(device_id, &dmcfg);
    if (rc != 0) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "failed to save MQTT config");
        send_cjson_resp(fd, "500 Internal Server Error", err);
        cJSON_Delete(err);
        return;
    }

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", 1);
    cJSON_AddStringToObject(resp, "message", "MQTT configuration saved");
    send_cjson_resp(fd, "200 OK", resp);
    cJSON_Delete(resp);
}

void render_device_mqtt_test_json(int fd, const struct request *req) {
    char device_id[64], cmd_name[128], reply[1024];

    device_id[0] = 0;
    cmd_name[0] = 0;
    if (req->body && req->body[0] == '{') {
        json_string(req->body, "deviceId", device_id, sizeof(device_id));
        json_string(req->body, "command", cmd_name, sizeof(cmd_name));
    } else {
        form_value(req->body, "deviceId", device_id, sizeof(device_id));
        form_value(req->body, "command", cmd_name, sizeof(cmd_name));
    }

    if (!safe_label(device_id)) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "invalid device ID");
        send_cjson_resp(fd, "400 Bad Request", err);
        cJSON_Delete(err);
        return;
    }

    if (!cmd_name[0]) strcpy(cmd_name, "PowerToggle");

    reply[0] = 0;
    send_ir_command_action_ex(device_id, cmd_name, "api-mqtt-test", "", reply, sizeof(reply));

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", 1);
    cJSON_AddStringToObject(resp, "command", cmd_name);
    cJSON_AddStringToObject(resp, "reply", reply[0] ? reply : "ok");
    send_cjson_resp(fd, "200 OK", resp);
    cJSON_Delete(resp);
}

void handle_ir_device_mqtt(int fd, const struct request *req) {
    char device_id[64], msg[512];
    struct device_mqtt_config dmcfg;

    mqtt_config_from_request(req, device_id, sizeof(device_id), &dmcfg);

    if (!safe_label(device_id)) {
        render_page(fd, req, "Invalid or missing device ID.");
        return;
    }

    if (save_device_mqtt_config(device_id, &dmcfg) == 0) {
        snprintf(msg, sizeof(msg), "Saved MQTT settings for device %s.", device_id);
    } else {
        snprintf(msg, sizeof(msg), "Failed to save MQTT settings for device %s.", device_id);
    }
    render_page(fd, req, msg);
}

void handle_ir_device(int fd, const struct request *req) {
    char device_id[64], name[128], manufacturer[128], model[128], type[80], msg[512];
    char cp_str[32] = "", b_int[16] = "", b_p1[16] = "", b_p2[16] = "", has_cfg[16] = "";
    int control_port = 7;

    form_value(req->body, "deviceId", device_id, sizeof(device_id));
    if (!device_id[0]) json_string(req->body, "deviceId", device_id, sizeof(device_id));
    form_value(req->body, "name", name, sizeof(name));
    if (!name[0]) json_string(req->body, "name", name, sizeof(name));
    form_value(req->body, "manufacturer", manufacturer, sizeof(manufacturer));
    if (!manufacturer[0]) json_string(req->body, "manufacturer", manufacturer, sizeof(manufacturer));
    form_value(req->body, "model", model, sizeof(model));
    if (!model[0]) json_string(req->body, "model", model, sizeof(model));
    form_value(req->body, "type", type, sizeof(type));
    if (!type[0]) json_string(req->body, "type", type, sizeof(type));

    form_value(req->body, "has_blaster_config", has_cfg, sizeof(has_cfg));
    form_value(req->body, "blaster_internal", b_int, sizeof(b_int));
    form_value(req->body, "blaster_port1", b_p1, sizeof(b_p1));
    form_value(req->body, "blaster_port2", b_p2, sizeof(b_p2));
    form_value(req->body, "controlPort", cp_str, sizeof(cp_str));
    if (!cp_str[0]) json_string(req->body, "controlPort", cp_str, sizeof(cp_str));

    if (has_cfg[0] || b_int[0] || b_p1[0] || b_p2[0]) {
        control_port = (b_p1[0] ? 1 : 0) | (b_p2[0] ? 2 : 0) | (b_int[0] ? 4 : 0);
        if (control_port == 0) control_port = 7;
    } else if (cp_str[0]) {
        control_port = atoi(cp_str);
        if (control_port <= 0 || control_port > 7) control_port = 7;
    }

    update_ir_device(device_id, name, manufacturer, model, type, control_port, msg, sizeof(msg));
    render_page(fd, req, msg);
}

void handle_ir_device_power(int fd, const struct request *req) {
    char device_id[64], delay_str[32], always_on_str[32], *pon = NULL, *poff = NULL, msg[512];
    int delay = 1500, always_on = 0;
    form_value(req->body, "deviceId", device_id, sizeof(device_id));
    form_value(req->body, "powerOnDelay", delay_str, sizeof(delay_str));
    form_value(req->body, "isPowerAlwaysOn", always_on_str, sizeof(always_on_str));
    if (delay_str[0]) delay = atoi(delay_str);
    if (strcmp(always_on_str, "1") == 0 || strcasecmp(always_on_str, "true") == 0) always_on = 1;
    pon = (char *)calloc(1, 16384);
    poff = (char *)calloc(1, 16384);
    if (!pon || !poff) {
        free(pon); free(poff);
        render_page(fd, req, "Out of memory.");
        return;
    }
    form_value(req->body, "powerOnSeq", pon, 16384);
    form_value(req->body, "powerOffSeq", poff, 16384);
    save_device_power_settings(device_id, delay, always_on, pon, poff, msg, sizeof(msg));
    free(pon);
    free(poff);
    render_page(fd, req, msg);
}

void handle_ir_new_device(int fd, const struct request *req) {
    char name[128], manufacturer[128], model[128], type[80], msg[512], created_id[64] = "";
    form_value(req->body, "name", name, sizeof(name));
    form_value(req->body, "manufacturer", manufacturer, sizeof(manufacturer));
    form_value(req->body, "model", model, sizeof(model));
    form_value(req->body, "type", type, sizeof(type));
    int rc = create_ir_device_ex(name, manufacturer, model, type, msg, sizeof(msg), created_id, sizeof(created_id));
    if (req && req->is_ajax) {
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddBoolToObject(resp, "ok", rc == 0 ? 1 : 0);
        if (created_id[0]) cJSON_AddStringToObject(resp, "deviceId", created_id);
        cJSON_AddStringToObject(resp, "message", msg);
        send_cjson_resp(fd, rc == 0 ? "200 OK" : "400 Bad Request", resp);
        cJSON_Delete(resp);
        return;
    }
    render_page(fd, req, msg);
}

void handle_ir_command(int fd, const struct request *req) {
    char device_id[64], name[128], mode[32], protocol[32], nec[64], keycode[512], raw[2048], msg[512];
    form_value(req->body, "deviceId", device_id, sizeof(device_id));
    form_value(req->body, "name", name, sizeof(name));
    form_value(req->body, "mode", mode, sizeof(mode));
    form_value(req->body, "protocol", protocol, sizeof(protocol));
    form_value(req->body, "nec", nec, sizeof(nec));
    form_value(req->body, "keycode", keycode, sizeof(keycode));
    form_value(req->body, "raw", raw, sizeof(raw));
    if (!mode[0]) strcpy(mode, "auto");
    add_ir_command(device_id, name, mode, protocol, nec, keycode, raw, msg, sizeof(msg));
    render_page(fd, req, msg);
}

void handle_ir_update_command(int fd, const struct request *req) {
    char device_id[64], old_name[128], name[128], mode[32], protocol[32], nec[64], keycode[512], raw[2048], msg[512];
    form_value(req->body, "deviceId", device_id, sizeof(device_id));
    form_value(req->body, "oldName", old_name, sizeof(old_name));
    form_value(req->body, "name", name, sizeof(name));
    form_value(req->body, "mode", mode, sizeof(mode));
    form_value(req->body, "protocol", protocol, sizeof(protocol));
    form_value(req->body, "nec", nec, sizeof(nec));
    form_value(req->body, "keycode", keycode, sizeof(keycode));
    form_value(req->body, "raw", raw, sizeof(raw));
    if (!mode[0]) strcpy(mode, "keycode");
    update_ir_command(device_id, old_name, name, mode, protocol, nec, keycode, raw, msg, sizeof(msg));
    render_page(fd, req, msg);
}

void handle_irdb_import(int fd, const struct request *req) {
    char device_id[64], msg[512];
    char *payload;
    if (req->body_truncated) {
        render_page(fd, req, "IRDB import payload was too large.");
        return;
    }
    form_value(req->body, "deviceId", device_id, sizeof(device_id));
    payload = (char *)malloc(MAX_REQUEST_BODY);
    if (!payload) {
        render_page(fd, req, "Not enough memory to receive IRDB import.");
        return;
    }
    form_value(req->body, "payload", payload, MAX_REQUEST_BODY);
    if (bulk_import_irdb_commands(device_id, payload, msg, sizeof(msg)) != 0) {
        render_page(fd, req, msg);
    } else {
        render_page(fd, req, msg);
    }
    free(payload);
}

void render_device_save_json(int fd, const struct request *req) {
    char device_id[64], name[128], manufacturer[128], model[128], type[80], msg[512];
    char cp_str[32] = "", b_int[16] = "", b_p1[16] = "", b_p2[16] = "", has_cfg[16] = "";
    int control_port = 7, rc;

    form_value(req->body, "deviceId", device_id, sizeof(device_id));
    if (!device_id[0]) json_string(req->body, "deviceId", device_id, sizeof(device_id));
    form_value(req->body, "name", name, sizeof(name));
    if (!name[0]) json_string(req->body, "name", name, sizeof(name));
    form_value(req->body, "manufacturer", manufacturer, sizeof(manufacturer));
    if (!manufacturer[0]) json_string(req->body, "manufacturer", manufacturer, sizeof(manufacturer));
    form_value(req->body, "model", model, sizeof(model));
    if (!model[0]) json_string(req->body, "model", model, sizeof(model));
    form_value(req->body, "type", type, sizeof(type));
    if (!type[0]) json_string(req->body, "type", type, sizeof(type));

    form_value(req->body, "has_blaster_config", has_cfg, sizeof(has_cfg));
    form_value(req->body, "blaster_internal", b_int, sizeof(b_int));
    form_value(req->body, "blaster_port1", b_p1, sizeof(b_p1));
    form_value(req->body, "blaster_port2", b_p2, sizeof(b_p2));
    form_value(req->body, "controlPort", cp_str, sizeof(cp_str));
    if (!cp_str[0]) json_string(req->body, "controlPort", cp_str, sizeof(cp_str));

    if (has_cfg[0] || b_int[0] || b_p1[0] || b_p2[0]) {
        control_port = (b_p1[0] ? 1 : 0) | (b_p2[0] ? 2 : 0) | (b_int[0] ? 4 : 0);
        if (control_port == 0) control_port = 7;
    } else if (cp_str[0]) {
        control_port = atoi(cp_str);
        if (control_port <= 0 || control_port > 7) control_port = 7;
    }

    rc = update_ir_device(device_id, name, manufacturer, model, type, control_port, msg, sizeof(msg));

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", rc == 0 ? 1 : 0);
    cJSON_AddStringToObject(resp, "deviceId", device_id);
    cJSON_AddNumberToObject(resp, "controlPort", control_port);
    cJSON_AddStringToObject(resp, "message", msg);
    send_cjson_resp(fd, rc == 0 ? "200 OK" : "400 Bad Request", resp);
    cJSON_Delete(resp);
}

void render_device_power_save_json(int fd, const struct request *req) {
    char device_id[64], delay_str[32], *pon = NULL, *poff = NULL, msg[512];
    int delay = 1500, always_on = 0, rc;
    form_value(req->body, "deviceId", device_id, sizeof(device_id));
    if (!device_id[0]) json_string(req->body, "deviceId", device_id, sizeof(device_id));
    form_value(req->body, "powerOnDelay", delay_str, sizeof(delay_str));
    if (!delay_str[0]) json_string(req->body, "powerOnDelay", delay_str, sizeof(delay_str));
    if (delay_str[0]) delay = atoi(delay_str);

    char ao[16] = "";
    form_value(req->body, "isPowerAlwaysOn", ao, sizeof(ao));
    if (!ao[0]) json_string(req->body, "isPowerAlwaysOn", ao, sizeof(ao));
    if (strcmp(ao, "1") == 0 || strcasecmp(ao, "true") == 0) always_on = 1;
    pon = (char *)calloc(1, 16384);
    poff = (char *)calloc(1, 16384);
    if (!pon || !poff) {
        free(pon); free(poff);
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "out of memory");
        send_cjson_resp(fd, "500 Internal Server Error", err);
        cJSON_Delete(err);
        return;
    }
    form_value(req->body, "powerOnSeq", pon, 16384);
    if (!pon[0]) json_string(req->body, "powerOnSeq", pon, 16384);
    form_value(req->body, "powerOffSeq", poff, 16384);
    if (!poff[0]) json_string(req->body, "powerOffSeq", poff, 16384);
    rc = save_device_power_settings(device_id, delay, always_on, pon, poff, msg, sizeof(msg));
    free(pon);
    free(poff);
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", rc == 0 ? 1 : 0);
    cJSON_AddStringToObject(resp, "message", msg);
    send_cjson_resp(fd, rc == 0 ? "200 OK" : "400 Bad Request", resp);
    cJSON_Delete(resp);
}

void render_irdb_import_json(int fd, const struct request *req) {
    char device_id[64], msg[512];
    char *payload;
    int rc;
    if (req->body_truncated) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "IRDB import payload was too large");
        send_cjson_resp(fd, "413 Payload Too Large", err);
        cJSON_Delete(err);
        return;
    }
    form_value(req->body, "deviceId", device_id, sizeof(device_id));
    payload = (char *)malloc(MAX_REQUEST_BODY);
    if (!payload) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "not enough memory to receive IRDB import");
        send_cjson_resp(fd, "500 Internal Server Error", err);
        cJSON_Delete(err);
        return;
    }
    form_value(req->body, "payload", payload, MAX_REQUEST_BODY);
    rc = bulk_import_irdb_commands(device_id, payload, msg, sizeof(msg));
    free(payload);
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", rc == 0 ? 1 : 0);
    cJSON_AddStringToObject(resp, "message", msg);
    send_cjson_resp(fd, rc == 0 ? "200 OK" : "400 Bad Request", resp);
    cJSON_Delete(resp);
}

void handle_ir_capture(int fd, const struct request *req) {
    char reply[4096], msg[4608];
    capture_ir_command_action(reply, sizeof(reply));
    snprintf(msg, sizeof(msg), "Capture result: %s", reply);
    render_page(fd, req, msg);
}

void handle_ir_delete_device(int fd, const struct request *req) {
    char device_id[64], msg[512];
    form_value(req->body, "deviceId", device_id, sizeof(device_id));
    delete_ir_device(device_id, msg, sizeof(msg));
    render_page(fd, req, msg);
}

void handle_ir_delete_command(int fd, const struct request *req) {
    char device_id[64], command[128], msg[512];
    form_value(req->body, "deviceId", device_id, sizeof(device_id));
    form_value(req->body, "command", command, sizeof(command));
    delete_ir_command(device_id, command, msg, sizeof(msg));
    render_page(fd, req, msg);
}

