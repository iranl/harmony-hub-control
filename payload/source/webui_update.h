#ifndef WEBUI_UPDATE_H
#define WEBUI_UPDATE_H

#include <stdio.h>
#include <stdlib.h>
#include "cJSON.h"
#include "codex_webui_types.h"

void load_update_state(struct update_check_state *st);
int save_update_state(const struct update_check_state *st);
int update_file_allowed(const char *name);
void update_stage_path(const char *name, char *out, size_t outlen);
void update_dest_path(const char *name, char *out, size_t outlen);
int is_hex32(const char *s);
int manifest_expected_md5(const char *manifest, const char *name, char *out, size_t outlen);
int parse_md5_text(const char *reply, char *out, size_t outlen);
int file_md5(const char *path, char *out, size_t outlen);
int write_update_hex_chunk(const char *name, long offset, const char *hex, char *err, size_t errlen, long *bytes_out);

/* Renderers */
void render_update_status_json(int fd);
void render_update_check_state_json(int fd);
void render_update_check_state_post_json(int fd, const struct request *req);
void render_update_begin_json(int fd, const struct request *req);
void render_update_chunk_json(int fd, const struct request *req);
void render_update_apply_json(int fd, const struct request *req);

#endif /* WEBUI_UPDATE_H */
