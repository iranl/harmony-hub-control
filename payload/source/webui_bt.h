#ifndef WEBUI_BT_H
#define WEBUI_BT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "codex_webui_types.h"

int is_btstack_running(void);
int safe_bt_addr(const char *s);
int safe_bt_token(const char *s, size_t maxlen);
int safe_bt_pin(const char *s);
int safe_bt_name(const char *s);
int extract_bt_addr_from_text(const char *text, char *out, size_t outlen);
int detect_connected_bt_addr(char *out, size_t outlen, char *raw, size_t rawlen);
int bt_type_allowed(const char *type);
const char *bt_type_label(const char *type);
void save_bthid_target(const char *type, const char *bdaddr);
int bt_hex_payload_from_input(const char *s, char *out, size_t outlen);
int bt_key_usage(const char *key);
int bt_keyboard_report_hex(const char *input, char *press, size_t presslen, char *release, size_t releaselen, char *err, size_t errlen);
int bt_sequence_add_code(const char *input, char **seq, size_t *seq_len, size_t *seq_cap, int *keys, char *err, size_t errlen);
int bthid_status_runtime_alive(const char *raw);
int write_bt_text_fifo(const char *text, char *err, size_t errlen);
void append_run_status(char *out, size_t outlen, const char *text);
int flush_bt_saved_sequence(const char *type, const char *bdaddr, char **seq, size_t *seq_len, size_t *seq_cap, int *chunk_keys, int gap_ms, int *total_keys, char *out, size_t outlen);
int run_bt_saved_script(const char *type, const char *bdaddr, const char *script, int gap_ms, char *out, size_t outlen);
int safe_bt_script_text(const char *s);

/* BT Device Store (HTML forms) */
int safe_bt_store_id(const char *s);
int load_bt_inventory(struct bt_inventory *inv);
int save_bt_inventory(const struct bt_inventory *inv);
int find_bt_device_index(const struct bt_inventory *inv, const char *id);
int upsert_bt_device(const char *id, const char *name, const char *type, const char *bdaddr, char *msg, size_t msglen);
int delete_bt_device(const char *device_id, char *msg, size_t msglen);
int find_bt_command_index(const struct bt_saved_device *dev, const char *name);
int upsert_bt_command(const char *device_id, const char *old_name, const char *name, const char *script, int delay_ms, char *msg, size_t msglen);
int delete_bt_command(const char *device_id, const char *command_name, char *msg, size_t msglen);
int send_bt_saved_command(const char *device_id, const char *name, char *msg, size_t msglen);

/* Renderers & Handlers */
void render_bluetooth_text_json(int fd, const struct request *req);
void render_bt_status_json(int fd);
void render_bt_key_json(int fd, const struct request *req);
void render_bt_pairing_json(int fd, const struct request *req);
void render_bt_connect_json(int fd, const struct request *req);
void render_bt_disconnect_json(int fd, const struct request *req);
void render_bt_link_device_json(int fd, const struct request *req);
void render_bt_script_json(int fd, const struct request *req);
void render_bluetooth_text_status_json(int fd);
void render_bt_sent_log_json(int fd);
void render_remote_mapping_json(int fd);
void render_remote_mapping_save_json(int fd, const struct request *req);
void render_remote_scan_json(int fd, const struct request *req);
void render_remote_scan_result_json(int fd);
void render_remote_pair_status_json(int fd);
void render_remote_pair_json(int fd, const struct request *req);
void render_bluetooth_call_json(int fd, const struct request *req);
void send_bt_devices_download(int fd);
void send_remote_mapping_download(int fd);

void handle_bt_device(int fd, const struct request *req);
void handle_bt_delete_device(int fd, const struct request *req);
void handle_bt_command(int fd, const struct request *req);
void handle_bt_delete_command(int fd, const struct request *req);
void handle_bt_send_command(int fd, const struct request *req);
void render_bt_saved_command_json(int fd, const struct request *req);

#endif /* WEBUI_BT_H */
