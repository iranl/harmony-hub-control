#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdint.h>
#include <time.h>
#include "cJSON.h"
#include "codex_webui_types.h"
#include "webui_utils.h"
#include "webui_update.h"
#include "webui_html.h"

void load_update_state(struct update_check_state *st) {
    char raw[768], value[256];
    memset(st, 0, sizeof(*st));
    strcpy(st->message, "Update status has not been checked yet.");
    if (read_text(UPDATE_STATE_CONFIG, raw, sizeof(raw)) <= 0) return;
    if (config_line_value(raw, "checkedAt", value, sizeof(value))) st->checked_at = atol(value);
    if (config_line_value(raw, "available", value, sizeof(value))) st->available = atoi(value) ? 1 : 0;
    if (config_line_value(raw, "changes", value, sizeof(value))) st->changes = atoi(value);
    if (config_line_value(raw, "message", value, sizeof(value))) snprintf(st->message, sizeof(st->message), "%s", value);
    if (config_line_value(raw, "source", value, sizeof(value))) snprintf(st->source, sizeof(st->source), "%s", value);
}

int save_update_state(const struct update_check_state *st) {
    char data[768], msg[192], source[192];
    snprintf(msg, sizeof(msg), "%s", st->message);
    snprintf(source, sizeof(source), "%s", st->source);
    clean_config_value(msg);
    clean_config_value(source);
    snprintf(data, sizeof(data), "checkedAt=%ld\navailable=%d\nchanges=%d\nmessage=%s\nsource=%s\n",
        st->checked_at, st->available ? 1 : 0, st->changes, msg, source);
    if (write_file_atomic(UPDATE_STATE_CONFIG, data, strlen(data)) != 0) return -1;
    chmod(UPDATE_STATE_CONFIG, 0644);
    return 0;
}


int update_file_allowed(const char *name) {
    size_t i;
    if (!name || !name[0]) return 0;
    for (i = 0; i < sizeof(UPDATE_FILES) / sizeof(UPDATE_FILES[0]); i++) {
        if (strcmp(name, UPDATE_FILES[i]) == 0) return 1;
    }
    return 0;
}

void update_stage_path(const char *name, char *out, size_t outlen) {
    snprintf(out, outlen, UPDATE_STAGE_DIR "/%s", name);
}

void update_dest_path(const char *name, char *out, size_t outlen) {
    snprintf(out, outlen, CODEX_BIN_DIR "/%s", name);
}

int is_hex32(const char *s) {
    int i;
    if (!s) return 0;
    for (i = 0; i < 32; i++) {
        if (!isxdigit((unsigned char)s[i])) return 0;
    }
    return s[32] == 0;
}

int manifest_expected_md5(const char *manifest, const char *name, char *out, size_t outlen) {
    const char *p = manifest;
    if (!manifest || !name || !out || outlen < 33) return 0;
    out[0] = 0;
    while (p && *p) {
        const char *end = strchr(p, '\n');
        const char *q;
        size_t len = end ? (size_t)(end - p) : strlen(p);
        if (len > 34) {
            char hash[33], file[96];
            size_t n = 0;
            memcpy(hash, p, 32);
            hash[32] = 0;
            q = p + 32;
            while (q < p + len && isspace((unsigned char)*q)) q++;
            while (q < p + len && *q != '\r' && !isspace((unsigned char)*q) && n + 1 < sizeof(file)) {
                file[n++] = *q++;
            }
            file[n] = 0;
            if (is_hex32(hash) && strcmp(file, name) == 0) {
                snprintf(out, outlen, "%s", hash);
                return 1;
            }
        }
        p = end ? end + 1 : NULL;
    }
    return 0;
}

int parse_md5_text(const char *reply, char *out, size_t outlen) {
    char hash[33];
    int i;
    if (!reply || !out || outlen < 33 || strlen(reply) < 32) return -1;
    for (i = 0; i < 32; i++) {
        if (!isxdigit((unsigned char)reply[i])) return -1;
        hash[i] = (char)tolower((unsigned char)reply[i]);
    }
    hash[32] = 0;
    snprintf(out, outlen, "%s", hash);
    return 0;
}

int file_md5(const char *path, char *out, size_t outlen) {
    /* Embedded MD5 (RFC 1321) — replaces fork+exec to busybox md5sum */
    static const unsigned char S[64] = {
        7,12,17,22, 7,12,17,22, 7,12,17,22, 7,12,17,22,
        5, 9,14,20, 5, 9,14,20, 5, 9,14,20, 5, 9,14,20,
        4,11,16,23, 4,11,16,23, 4,11,16,23, 4,11,16,23,
        6,10,15,21, 6,10,15,21, 6,10,15,21, 6,10,15,21
    };
    static const uint32_t K[64] = {
        0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,
        0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
        0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,
        0x6b901122,0xfd987193,0xa679438e,0x49b40821,
        0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,
        0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
        0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,
        0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
        0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,
        0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
        0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,
        0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
        0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,
        0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
        0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,
        0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391
    };
    uint32_t h0=0x67452301, h1=0xefcdab89, h2=0x98badcfe, h3=0x10325476;
    uint64_t total_bits = 0;
    unsigned char buf[4096], block[64];
    int buf_used = 0;
    FILE *f;
    size_t n;
    int i;

    if (!path || !out || outlen < 33) return -1;
    f = fopen(path, "rb");
    if (!f) return -1;

    #define MD5_LEFTROTATE(x, c) (((x) << (c)) | ((x) >> (32 - (c))))
    #define MD5_PROCESS_BLOCK(blk) do { \
        uint32_t M[16], a=h0, b=h1, c=h2, d=h3; \
        for (i=0;i<16;i++) M[i]=(uint32_t)(blk)[i*4]|((uint32_t)(blk)[i*4+1]<<8)|((uint32_t)(blk)[i*4+2]<<16)|((uint32_t)(blk)[i*4+3]<<24); \
        for (i=0;i<64;i++) { \
            uint32_t F,g; \
            if (i<16){F=(b&c)|((~b)&d);g=i;} \
            else if(i<32){F=(d&b)|((~d)&c);g=(5*i+1)%16;} \
            else if(i<48){F=b^c^d;g=(3*i+5)%16;} \
            else{F=c^(b|(~d));g=(7*i)%16;} \
            F+=a+K[i]+M[g]; a=d; d=c; c=b; b+=MD5_LEFTROTATE(F,S[i]); \
        } \
        h0+=a; h1+=b; h2+=c; h3+=d; \
    } while(0)

    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        size_t off = 0;
        if (buf_used) {
            size_t need = 64 - buf_used;
            if (n < need) { memcpy(block + buf_used, buf, n); buf_used += n; total_bits += n * 8; continue; }
            memcpy(block + buf_used, buf, need);
            MD5_PROCESS_BLOCK(block);
            off = need; buf_used = 0;
        }
        for (; off + 64 <= n; off += 64) MD5_PROCESS_BLOCK(buf + off);
        buf_used = n - off;
        if (buf_used) memcpy(block, buf + off, buf_used);
        total_bits += n * 8;
    }
    fclose(f);

    /* Padding */
    block[buf_used++] = 0x80;
    if (buf_used > 56) {
        memset(block + buf_used, 0, 64 - buf_used);
        MD5_PROCESS_BLOCK(block);
        buf_used = 0;
    }
    memset(block + buf_used, 0, 56 - buf_used);
    for (i = 0; i < 8; i++) block[56 + i] = (unsigned char)(total_bits >> (i * 8));
    MD5_PROCESS_BLOCK(block);
    #undef MD5_LEFTROTATE
    #undef MD5_PROCESS_BLOCK

    snprintf(out, outlen, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
        h0&0xff,(h0>>8)&0xff,(h0>>16)&0xff,(h0>>24)&0xff,
        h1&0xff,(h1>>8)&0xff,(h1>>16)&0xff,(h1>>24)&0xff,
        h2&0xff,(h2>>8)&0xff,(h2>>16)&0xff,(h2>>24)&0xff,
        h3&0xff,(h3>>8)&0xff,(h3>>16)&0xff,(h3>>24)&0xff);
    return 0;
}

int write_update_hex_chunk(const char *name, long offset, const char *hex, char *err, size_t errlen, long *bytes_out) {
    char path[256];
    struct stat st;
    FILE *f;
    size_t len, i;
    long current = 0, wrote = 0;
    if (bytes_out) *bytes_out = 0;
    if (!update_file_allowed(name)) {
        snprintf(err, errlen, "file is not part of the update allow-list");
        return -1;
    }
    if (!hex || !hex[0]) {
        snprintf(err, errlen, "missing update chunk");
        return -1;
    }
    len = strlen(hex);
    if ((len & 1) != 0) {
        snprintf(err, errlen, "chunk hex length is odd");
        return -1;
    }
    for (i = 0; i < len; i++) {
        if (!isxdigit((unsigned char)hex[i])) {
            snprintf(err, errlen, "chunk contains non-hex data");
            return -1;
        }
    }
    mkdir(UPDATE_STAGE_DIR, 0755);
    update_stage_path(name, path, sizeof(path));
    if (stat(path, &st) == 0) current = (long)st.st_size;
    if (offset != current) {
        snprintf(err, errlen, "chunk offset mismatch for %s: got %ld expected %ld", name, offset, current);
        return -1;
    }
    if (current + (long)(len / 2) > 2 * 1024 * 1024) {
        snprintf(err, errlen, "staged update file exceeds 2MB limit");
        return -1;
    }
    f = fopen(path, offset == 0 ? "wb" : "ab");
    if (!f) {
        snprintf(err, errlen, "cannot open staged update file");
        return -1;
    }
    for (i = 0; i < len; i += 2) {
        int hi = hexval(hex[i]), lo = hexval(hex[i + 1]);
        unsigned char b = (unsigned char)((hi << 4) | lo);
        if (hi < 0 || lo < 0 || fwrite(&b, 1, 1, f) != 1) {
            fclose(f);
            snprintf(err, errlen, "failed writing staged update chunk");
            return -1;
        }
        wrote++;
    }
    fclose(f);
    if (bytes_out) *bytes_out = wrote;
    return 0;
}

void render_update_status_json(int fd) {
    size_t i;
    struct stat st;
    char path[256], md5[40];
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", 1);
    cJSON_AddStringToObject(resp, "repo", "https://github.com/Ripthulhu/harmony-hub-control");
    cJSON_AddStringToObject(resp, "rawBase", "https://raw.githubusercontent.com/Ripthulhu/harmony-hub-control/main/payload/bin/");
    cJSON *files = cJSON_CreateArray();
    for (i = 0; i < sizeof(UPDATE_FILES) / sizeof(UPDATE_FILES[0]); i++) {
        update_dest_path(UPDATE_FILES[i], path, sizeof(path));
        cJSON *fobj = cJSON_CreateObject();
        cJSON_AddStringToObject(fobj, "name", UPDATE_FILES[i]);
        if (stat(path, &st) == 0) {
            cJSON_AddBoolToObject(fobj, "present", 1);
            cJSON_AddNumberToObject(fobj, "size", (double)st.st_size);
            if (file_md5(path, md5, sizeof(md5)) == 0) {
                cJSON_AddStringToObject(fobj, "md5", md5);
            }
        } else {
            cJSON_AddBoolToObject(fobj, "present", 0);
            cJSON_AddNumberToObject(fobj, "size", 0);
        }
        cJSON_AddItemToArray(files, fobj);
    }
    cJSON_AddItemToObject(resp, "files", files);
    send_cjson_resp(fd, "200 OK", resp);
    cJSON_Delete(resp);
}

void render_update_check_state_json(int fd) {
    struct update_check_state st;
    load_update_state(&st);
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", 1);
    cJSON_AddNumberToObject(resp, "checkedAt", (double)st.checked_at);
    cJSON_AddBoolToObject(resp, "available", st.available ? 1 : 0);
    cJSON_AddNumberToObject(resp, "changes", st.changes);
    cJSON_AddStringToObject(resp, "message", st.message);
    cJSON_AddStringToObject(resp, "source", st.source);
    send_cjson_resp(fd, "200 OK", resp);
    cJSON_Delete(resp);
}

void render_update_check_state_post_json(int fd, const struct request *req) {
    struct update_check_state st;
    char tmp[256];
    memset(&st, 0, sizeof(st));
    form_value(req->body, "checkedAt", tmp, sizeof(tmp));
    st.checked_at = atol(tmp);
    if (st.checked_at <= 0) st.checked_at = time(NULL);
    form_value(req->body, "available", tmp, sizeof(tmp));
    st.available = (strcmp(tmp, "1") == 0 || strcasecmp(tmp, "true") == 0 ||
        strcasecmp(tmp, "yes") == 0 || strcasecmp(tmp, "on") == 0) ? 1 : 0;
    form_value(req->body, "changes", tmp, sizeof(tmp));
    st.changes = tmp[0] ? atoi(tmp) : 0;
    form_value(req->body, "message", st.message, sizeof(st.message));
    if (!st.message[0]) snprintf(st.message, sizeof(st.message), "Update check completed.");
    form_value(req->body, "source", st.source, sizeof(st.source));
    if (save_update_state(&st) != 0) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "failed to save update check state");
        send_cjson_resp(fd, "500 Internal Server Error", err);
        cJSON_Delete(err);
        return;
    }
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", 1);
    cJSON_AddNumberToObject(resp, "checkedAt", (double)st.checked_at);
    cJSON_AddBoolToObject(resp, "available", st.available ? 1 : 0);
    cJSON_AddNumberToObject(resp, "changes", st.changes);
    cJSON_AddStringToObject(resp, "message", st.message);
    cJSON_AddStringToObject(resp, "source", st.source);
    send_cjson_resp(fd, "200 OK", resp);
    cJSON_Delete(resp);
}

void render_update_begin_json(int fd, const struct request *req) {
    char *manifest;
    size_t i;
    if (req->body_truncated) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "update manifest request is too large");
        send_cjson_resp(fd, "413 Payload Too Large", err);
        cJSON_Delete(err);
        return;
    }
    manifest = (char *)malloc(MAX_REQUEST_BODY);
    if (!manifest) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "not enough memory for update manifest");
        send_cjson_resp(fd, "500 Internal Server Error", err);
        cJSON_Delete(err);
        return;
    }
    form_value(req->body, "manifest", manifest, MAX_REQUEST_BODY);
    if (!strstr(manifest, "codex_daemon")) {
        free(manifest);
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "manifest does not look like a Harmony control build");
        send_cjson_resp(fd, "400 Bad Request", err);
        cJSON_Delete(err);
        return;
    }
    mkdir(UPDATE_STAGE_DIR, 0755);
    for (i = 0; i < sizeof(UPDATE_FILES) / sizeof(UPDATE_FILES[0]); i++) {
        char path[256];
        update_stage_path(UPDATE_FILES[i], path, sizeof(path));
        unlink(path);
    }
    if (write_file_atomic(UPDATE_STAGE_DIR "/MANIFEST.txt", manifest, strlen(manifest)) != 0) {
        free(manifest);
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "failed to stage update manifest");
        send_cjson_resp(fd, "500 Internal Server Error", err);
        cJSON_Delete(err);
        return;
    }
    free(manifest);
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", 1);
    cJSON_AddStringToObject(resp, "message", "update staging started");
    send_cjson_resp(fd, "200 OK", resp);
    cJSON_Delete(resp);
}

void render_update_chunk_json(int fd, const struct request *req) {
    char name[64], offset_text[32], err[256];
    char *hex;
    long offset, wrote = 0;
    if (req->body_truncated) {
        cJSON *err_obj = cJSON_CreateObject();
        cJSON_AddBoolToObject(err_obj, "ok", 0);
        cJSON_AddStringToObject(err_obj, "error", "update chunk is too large");
        send_cjson_resp(fd, "413 Payload Too Large", err_obj);
        cJSON_Delete(err_obj);
        return;
    }
    form_value(req->body, "file", name, sizeof(name));
    form_value(req->body, "offset", offset_text, sizeof(offset_text));
    offset = atol(offset_text);
    hex = (char *)malloc(MAX_REQUEST_BODY);
    if (!hex) {
        cJSON *err_obj = cJSON_CreateObject();
        cJSON_AddBoolToObject(err_obj, "ok", 0);
        cJSON_AddStringToObject(err_obj, "error", "not enough memory for update chunk");
        send_cjson_resp(fd, "500 Internal Server Error", err_obj);
        cJSON_Delete(err_obj);
        return;
    }
    form_value(req->body, "hex", hex, MAX_REQUEST_BODY);
    if (write_update_hex_chunk(name, offset, hex, err, sizeof(err), &wrote) != 0) {
        free(hex);
        cJSON *err_obj = cJSON_CreateObject();
        cJSON_AddBoolToObject(err_obj, "ok", 0);
        cJSON_AddStringToObject(err_obj, "error", err);
        send_cjson_resp(fd, "400 Bad Request", err_obj);
        cJSON_Delete(err_obj);
        return;
    }
    free(hex);
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", 1);
    cJSON_AddStringToObject(resp, "file", name);
    cJSON_AddNumberToObject(resp, "offset", offset);
    cJSON_AddNumberToObject(resp, "bytes", wrote);
    send_cjson_resp(fd, "200 OK", resp);
    cJSON_Delete(resp);
}

void render_update_apply_json(int fd, const struct request *req) {
    char *manifest;
    char restart_text[16], backup_dir[256], updated[512];
    char stage[256], dest[256], dest_tmp[288], backup[256], expected[40], actual[40];
    int restart, count = 0, rc = 0;
    size_t i;
    struct stat st;
    form_value(req->body, "restart", restart_text, sizeof(restart_text));
    restart = strcmp(restart_text, "0") != 0;
    manifest = read_file_alloc(UPDATE_STAGE_DIR "/MANIFEST.txt", MAX_REQUEST_BODY, NULL);
    if (!manifest) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "no staged update manifest found");
        send_cjson_resp(fd, "400 Bad Request", err);
        cJSON_Delete(err);
        return;
    }
    for (i = 0; i < sizeof(UPDATE_FILES) / sizeof(UPDATE_FILES[0]); i++) {
        update_stage_path(UPDATE_FILES[i], stage, sizeof(stage));
        if (stat(stage, &st) != 0) continue;
        count++;
        if (!manifest_expected_md5(manifest, UPDATE_FILES[i], expected, sizeof(expected))) {
            rc = -1;
            snprintf(updated, sizeof(updated), "manifest has no md5 for %s", UPDATE_FILES[i]);
            break;
        }
        if (file_md5(stage, actual, sizeof(actual)) != 0 || strcasecmp(expected, actual) != 0) {
            rc = -1;
            snprintf(updated, sizeof(updated), "md5 mismatch for %s", UPDATE_FILES[i]);
            break;
        }
    }
    if (rc == 0 && count <= 0) {
        rc = -1;
        snprintf(updated, sizeof(updated), "no staged binaries found");
    }
    if (rc != 0) {
        free(manifest);
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", updated);
        send_cjson_resp(fd, "400 Bad Request", err);
        cJSON_Delete(err);
        return;
    }

    mkdir(UPDATE_BACKUP_DIR, 0755);
    prune_update_backups(3);
    snprintf(backup_dir, sizeof(backup_dir), UPDATE_BACKUP_DIR "/%ld", (long)time(NULL));
    mkdir(backup_dir, 0755);
    updated[0] = 0;
    for (i = 0; i < sizeof(UPDATE_FILES) / sizeof(UPDATE_FILES[0]); i++) {
        update_stage_path(UPDATE_FILES[i], stage, sizeof(stage));
        if (stat(stage, &st) != 0) continue;
        update_dest_path(UPDATE_FILES[i], dest, sizeof(dest));
        snprintf(dest_tmp, sizeof(dest_tmp), "%s.update", dest);
        snprintf(backup, sizeof(backup), "%s/%s", backup_dir, UPDATE_FILES[i]);
        if (stat(dest, &st) == 0) copy_file_raw(dest, backup);
        unlink(dest_tmp);
        if (copy_file_raw(stage, dest_tmp) != 0) {
            char msg[160];
            int saved_errno = errno;
            unlink(dest_tmp);
            free(manifest);
            snprintf(msg, sizeof(msg), "failed to copy %s: %s", UPDATE_FILES[i], strerror(saved_errno));
            cJSON *err = cJSON_CreateObject();
            cJSON_AddBoolToObject(err, "ok", 0);
            cJSON_AddStringToObject(err, "error", msg);
            send_cjson_resp(fd, "500 Internal Server Error", err);
            cJSON_Delete(err);
            return;
        }
        if (chmod(dest_tmp, 0755) != 0) {
            char msg[160];
            int saved_errno = errno;
            unlink(dest_tmp);
            free(manifest);
            snprintf(msg, sizeof(msg), "failed to chmod %s: %s", UPDATE_FILES[i], strerror(saved_errno));
            cJSON *err = cJSON_CreateObject();
            cJSON_AddBoolToObject(err, "ok", 0);
            cJSON_AddStringToObject(err, "error", msg);
            send_cjson_resp(fd, "500 Internal Server Error", err);
            cJSON_Delete(err);
            return;
        }
        if (rename(dest_tmp, dest) != 0) {
            char msg[160];
            int saved_errno = errno;
            unlink(dest_tmp);
            free(manifest);
            snprintf(msg, sizeof(msg), "failed to rename %s: %s", UPDATE_FILES[i], strerror(saved_errno));
            cJSON *err = cJSON_CreateObject();
            cJSON_AddBoolToObject(err, "ok", 0);
            cJSON_AddStringToObject(err, "error", msg);
            send_cjson_resp(fd, "500 Internal Server Error", err);
            cJSON_Delete(err);
            return;
        }
        unlink(stage);
        if (updated[0]) strncat(updated, ", ", sizeof(updated) - strlen(updated) - 1);
        strncat(updated, UPDATE_FILES[i], sizeof(updated) - strlen(updated) - 1);
    }
    copy_file_raw(UPDATE_STAGE_DIR "/MANIFEST.txt", CODEX_BIN_DIR "/MANIFEST.txt");
    chmod(CODEX_BIN_DIR "/MANIFEST.txt", 0644);
    unlink(UPDATE_STAGE_DIR "/MANIFEST.txt");
    prune_update_backups(3);
    sync();
    free(manifest);
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", 1);
    cJSON_AddStringToObject(resp, "updated", updated);
    cJSON_AddStringToObject(resp, "backupDir", backup_dir);
    cJSON_AddBoolToObject(resp, "restart", restart ? 1 : 0);
    send_cjson_resp(fd, "200 OK", resp);
    cJSON_Delete(resp);
    if (restart) {
        pid_t pid;
        shutdown(fd, SHUT_RDWR);
        pid = fork();
        if (pid == 0) {
            close(fd);
            setsid();
            execl("/bin/sh", "sh", "-c",
                  "sleep 3; "
                  "killall codex_daemon 2>/dev/null; "
                  "/data/codex/bin/codex_daemon 8089 >> /tmp/codex-init.log 2>&1 &",
                  (char *)NULL);
            _exit(127);
        }
    }
}

