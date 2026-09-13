#ifndef WEBUI_IR_H
#define WEBUI_IR_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "cJSON.h"
#include "codex_webui_types.h"

cJSON *find_device_item(cJSON *root, const char *device_id);
void parse_power_actions_cjson(cJSON *arr, const char *device_id, struct activity_step **steps_out, int *count_out);
int scan_ir_resource_stats(int *device_count, int *total_commands, long *max_device_id, long *max_command_id);
int load_ir_inventory(struct ir_inventory *inv);
void free_ir_inventory(struct ir_inventory *inv);
void destroy_ir_inventory(struct ir_inventory *inv);
int find_ir_device_by_name(const char *name, char *device_id, size_t device_id_len);

int safe_remotecentral_path(const char *path);
void normalize_remotecentral_path(char *path);
int append_top_array_item(const char *path, const char *array_key, const char *item);
int protocol_list_has_id(const char *raw, int protocol_id);
int ensure_protocol_object(int protocol_id, const char *protocol_json);
int ensure_builtin_protocol_for_id(int protocol_id);
int protocol_file_has_id(int protocol_id);
int repair_known_protocols_for_current_commands(void);
void mark_protocol_repair_needed(void);
int append_command_to_device(const char *device_id, const char *command_json);
int delete_ir_device(const char *device_id, char *msg, size_t msglen);
int delete_ir_command(const char *device_id, const char *command_name, char *msg, size_t msglen);
int clear_ir_commands(const char *device_id, char *msg, size_t msglen);
int harmony_device_type_id(const char *type);
int create_ir_device_ex(const char *name, const char *manufacturer, const char *model, const char *type, char *msg, size_t msglen, char *created_id, size_t created_id_len);
int create_ir_device(const char *name, const char *manufacturer, const char *model, const char *type, char *msg, size_t msglen);
int ensure_lab_target_device(char *device_id, size_t device_id_len, int *created, char *msg, size_t msglen);
int update_ir_device(const char *device_id, const char *name, const char *manufacturer, const char *model, const char *type, int control_port, char *msg, size_t msglen);
int load_device_mqtt_config(const char *device_id, struct device_mqtt_config *dmcfg);
int save_device_mqtt_config(const char *device_id, const struct device_mqtt_config *dmcfg);
int build_nec_keycode(const char *hex, int protocol_id, char *out, size_t outlen);
int contains_ci(const char *haystack, const char *needle);
int extract_harmony_keycode(const char *src, char *out, size_t outlen);
int clean_hex_token(const char *src, char *out, size_t outlen);
int extract_nec_hex(const char *src, char *out, size_t outlen);
int infer_capture_protocol(const char *raw_code, const char *keycode);
size_t parse_raw_timings(const char *raw, uint32_t *timings, size_t max_timings);
int decode_nec_from_raw(const char *raw, char *nec_out, size_t nec_len);
int decode_sony_from_raw(const char *raw, char *sony_out, size_t sony_len);
int decode_rc5_from_raw(const char *raw, char *rc5_out, size_t rc5_len);
void analyze_capture_storage(const char *raw_code, const char *keycode_in, const char *nec_in, const char *protocol_text, char *mode_out, size_t mode_len, char *keycode_out, size_t keycode_len, char *nec_out, size_t nec_len, int *protocol_id_out, char *summary, size_t summary_len);
unsigned int reverse8(unsigned int v);
int build_irdb_nec_keycode(const char *protocol, const char *device, const char *subdevice, const char *function, char *out, size_t outlen);
cJSON *build_ir_command_object(long id, const char *name, const char *mode, int protocol_id, const char *code, const char *raw_code);
int device_has_command_name(cJSON *cmds, const char *name);
int update_ir_command(const char *device_id, const char *old_name, const char *new_name, const char *mode, const char *protocol_text, const char *nec, const char *keycode, const char *raw_code, char *msg, size_t msglen);
int add_ir_command(const char *device_id, const char *name, const char *mode, const char *protocol_text, const char *nec, const char *keycode, const char *raw_code, char *msg, size_t msglen);
int safe_raw_import_value(const char *s);
int bulk_import_irdb_commands(const char *device_id, char *payload, char *msg, size_t msglen);
cJSON *build_power_actions_cjson(const char *text);
int save_device_power_settings(const char *device_id, int power_on_delay, int is_power_always_on, const char *power_on_seq, const char *power_off_seq, char *msg, size_t msglen);

void update_active_run_progress(const char *run_id, int current, int total, int sent, int failed);
int ir_cancel_path(const char *run_id, char *path, size_t pathlen);
void register_active_run(const char *run_id);
void unregister_active_run(const char *run_id);
int ir_run_canceled(const char *run_id);
int mark_ir_run_canceled(const char *run_id);
int cancelable_sleep_ms(int delay_ms, const char *run_id);
void rotate_ir_event_log(void);
void log_ir_event(const char *source, const char *run_id, const char *device_id, const char *command, const char *reply);
void log_ir_note_event(const char *event, const char *source, const char *run_id, const char *detail);
void send_ir_command_action_ex(const char *device_id, const char *command, const char *source, const char *run_id, char *out, size_t outlen);
void send_ir_command_action(const char *device_id, const char *command, char *out, size_t outlen);
void capture_ir_command_action(char *out, size_t outlen);

/* Renderers & Handlers */
void render_inventory_json(int fd);
void render_device_commands_json(int fd, const struct request *req);
void render_capture_json(int fd);
void render_remotecentral_fetch_json(int fd, const struct request *req);
void render_ir_send_json(int fd, const struct request *req);
void render_ir_batch_send_json(int fd, const struct request *req);
void render_ir_cancel_json(int fd, const struct request *req);
void render_ir_batch_status_json(int fd, const struct request *req);
void render_ir_lab_target_json(int fd);
void render_ir_lab_clear_json(int fd, const struct request *req);
void render_ir_test_learned_json(int fd, const struct request *req);
void render_device_mqtt_save_json(int fd, const struct request *req);
void render_device_mqtt_test_json(int fd, const struct request *req);
void render_device_save_json(int fd, const struct request *req);
void render_device_power_save_json(int fd, const struct request *req);
void render_irdb_import_json(int fd, const struct request *req);

void handle_ir_send(int fd, const struct request *req);
void handle_ir_device(int fd, const struct request *req);
void handle_ir_device_power(int fd, const struct request *req);
void handle_ir_device_mqtt(int fd, const struct request *req);
void handle_ir_new_device(int fd, const struct request *req);
void handle_ir_command(int fd, const struct request *req);
void handle_ir_update_command(int fd, const struct request *req);
void handle_irdb_import(int fd, const struct request *req);
void handle_ir_capture(int fd, const struct request *req);
void handle_ir_delete_device(int fd, const struct request *req);
void handle_ir_delete_command(int fd, const struct request *req);

#endif /* WEBUI_IR_H */
