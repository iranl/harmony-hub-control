#ifndef WEBUI_UTILS_H
#define WEBUI_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "cJSON.h"
#include "codex_webui_types.h"

void chomp(char *s);
int read_text(const char *path, char *out, size_t outlen);
char *read_file_alloc(const char *path, size_t maxlen, size_t *outlen);
int write_file_atomic(const char *path, const char *data, size_t len);
int write_file_atomic_nosync(const char *path, const char *data, size_t len);
int config_line_value(const char *raw, const char *key, char *out, size_t outlen);
void clean_config_value(char *s);
int safe_auth_field(const char *s, int allow_colon);
void load_webui_auth(struct webui_auth_config *cfg);
void invalidate_auth_cache(void);
void ensure_auth_loaded(void);
int save_webui_auth(const struct webui_auth_config *cfg);
int copy_file_raw(const char *src, const char *dst);
void cjson_set_or_replace(cJSON *obj, const char *key, cJSON *item);
void remove_tree_simple(const char *path);
void remove_dir_entries_with_prefix(const char *dir, const char *prefix);
int is_digit_name(const char *s);
void prune_update_backups(int keep);
void prune_resource_backups(int keep);
int safe_label(const char *s);
int safe_run_id(const char *s);
void shell_escape_single(const char *s, char *out, size_t outlen);
int run_cmd(const char *cmd, char *out, size_t outlen);
void html(FILE *f, const char *s);
int hexval(char c);
void url_decode(char *s);
void form_value(const char *body, const char *name, char *out, size_t outlen);
void query_value(const char *path, const char *name, char *out, size_t outlen);
int form_checked(const char *body, const char *name);
int content_length(const char *headers);
void send_text(int fd, const char *status, const char *body);
void send_all(int fd, const char *data, size_t len);
int b64_value(char c);
int base64_decode_text(const char *in, char *out, size_t outlen);
int constant_time_strcmp(const char *a, const char *b);
int webui_auth_ok(const struct request *req);
void send_auth_required(int fd);
int http_get_body(const char *host, const char *path, char **body, size_t max_body, char *err, size_t errlen);
void send_file_download(int fd, const char *path, const char *filename, const char *ctype);
void send_cjson_resp(int fd, const char *status, cJSON *root);
void copy_text(char *out, size_t outlen, const char *value);
int append_text(char **buf, size_t *len, size_t *cap, const char *text, int comma);
int json_string(const char *json, const char *key, char *out, size_t outlen);
int json_int(const char *json, const char *key, int def);
int json_bool(const char *json, const char *key, int def);
char *base64_encode(const unsigned char *src, size_t len);
unsigned char *base64_decode(const char *src, size_t *out_len);
char *trim_in_place(char *s);
int split_fields(char *line, char sep, char **fields, int max_fields);
void free_request(struct request *req);
int read_request(int fd, struct request *req);
void send_payload_too_large(int fd, const struct request *req);

#endif /* WEBUI_UTILS_H */
