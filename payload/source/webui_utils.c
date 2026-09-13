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
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include "cJSON.h"
#include "codex_webui_types.h"
#include "webui_utils.h"

void chomp(char *s) {
    size_t n = strlen(s);
    while (n && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ' || s[n - 1] == '\t')) {
        s[--n] = 0;
    }
}

int read_text(const char *path, char *out, size_t outlen) {
    FILE *f = fopen(path, "r");
    size_t n;
    if (!f) {
        if (outlen) out[0] = 0;
        return -1;
    }
    n = fread(out, 1, outlen - 1, f);
    out[n] = 0;
    fclose(f);
    return (int)n;
}

char *read_file_alloc(const char *path, size_t maxlen, size_t *outlen) {
    FILE *f = fopen(path, "rb");
    char *buf;
    long n;
    size_t got;
    if (outlen) *outlen = 0;
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    n = ftell(f);
    if (n < 0 || (size_t)n > maxlen) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    buf = (char *)malloc((size_t)n + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = 0;
    if (outlen) *outlen = got;
    return buf;
}

int write_file_atomic(const char *path, const char *data, size_t len) {
    char tmp[256];
    FILE *f;
    snprintf(tmp, sizeof(tmp), "%s.new", path);
    f = fopen(tmp, "wb");
    if (!f) return -1;
    if (fwrite(data, 1, len, f) != len) {
        fclose(f);
        unlink(tmp);
        return -1;
    }
    fputc('\n', f);
    fclose(f);
    if (rename(tmp, path) != 0) {
        unlink(tmp);
        return -1;
    }
    sync();
    return 0;
}

int config_line_value(const char *raw, const char *key, char *out, size_t outlen) {
    size_t keylen = strlen(key), n;
    const char *p = raw, *v, *e;
    if (!outlen) return 0;
    out[0] = 0;
    while (p && *p) {
        if (strncmp(p, key, keylen) == 0 && p[keylen] == '=') {
            v = p + keylen + 1;
            e = strchr(v, '\n');
            if (!e) e = v + strlen(v);
            n = (size_t)(e - v);
            if (n >= outlen) n = outlen - 1;
            memcpy(out, v, n);
            out[n] = 0;
            chomp(out);
            return 1;
        }
        p = strchr(p, '\n');
        if (p) p++;
    }
    return 0;
}

void clean_config_value(char *s) {
    while (*s) {
        if (*s == '\n' || *s == '\r' || *s == '\t') *s = ' ';
        s++;
    }
}

int safe_auth_field(const char *s, int allow_colon) {
    if (!s || !*s) return 0;
    while (*s) {
        unsigned char c = (unsigned char)*s++;
        if (c < 32 || c == 127) return 0;
        if (!allow_colon && c == ':') return 0;
    }
    return 1;
}

void load_webui_auth(struct webui_auth_config *cfg) {
    char raw[512], value[160];
    memset(cfg, 0, sizeof(*cfg));
    strcpy(cfg->username, "admin");
    if (read_text(WEBUI_AUTH_CONFIG, raw, sizeof(raw)) <= 0) return;
    if (config_line_value(raw, "enabled", value, sizeof(value))) cfg->enabled = atoi(value) ? 1 : 0;
    if (config_line_value(raw, "username", value, sizeof(value)) && safe_auth_field(value, 0)) {
        snprintf(cfg->username, sizeof(cfg->username), "%s", value);
    }
    if (config_line_value(raw, "password", value, sizeof(value)) && safe_auth_field(value, 1)) {
        snprintf(cfg->password, sizeof(cfg->password), "%s", value);
    }
    if (cfg->enabled && (!cfg->username[0] || !cfg->password[0])) cfg->enabled = 0;
}

int save_webui_auth(const struct webui_auth_config *cfg) {
    char data[384], user[64], pass[128];
    snprintf(user, sizeof(user), "%s", cfg->username);
    snprintf(pass, sizeof(pass), "%s", cfg->password);
    clean_config_value(user);
    clean_config_value(pass);
    snprintf(data, sizeof(data), "enabled=%d\nusername=%s\npassword=%s\n",
        cfg->enabled ? 1 : 0, user, pass);
    if (write_file_atomic(WEBUI_AUTH_CONFIG, data, strlen(data)) != 0) return -1;
    chmod(WEBUI_AUTH_CONFIG, 0600);
    return 0;
}


int copy_file_raw(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    FILE *out;
    char buf[4096];
    size_t n;
    if (!in) return -1;
    out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return -1;
    }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            int saved_errno = errno;
            fclose(in);
            fclose(out);
            unlink(dst);
            errno = saved_errno;
            return -1;
        }
    }
    if (ferror(in)) {
        int saved_errno = errno;
        fclose(in);
        fclose(out);
        unlink(dst);
        errno = saved_errno;
        return -1;
    }
    fclose(in);
    if (fclose(out) != 0) {
        unlink(dst);
        return -1;
    }
    return 0;
}

void cjson_set_or_replace(cJSON *obj, const char *key, cJSON *item) {
    if (!obj || !key || !item) {
        if (item) cJSON_Delete(item);
        return;
    }
    if (cJSON_GetObjectItem(obj, key)) {
        cJSON_ReplaceItemInObject(obj, key, item);
    } else {
        cJSON_AddItemToObject(obj, key, item);
    }
}

void remove_tree_simple(const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) return;
    if (S_ISDIR(st.st_mode)) {
        DIR *d = opendir(path);
        struct dirent *de;
        if (d) {
            while ((de = readdir(d)) != NULL) {
                char child[512];
                if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
                snprintf(child, sizeof(child), "%s/%s", path, de->d_name);
                remove_tree_simple(child);
            }
            closedir(d);
        }
        rmdir(path);
    } else {
        unlink(path);
    }
}

void remove_dir_entries_with_prefix(const char *dir, const char *prefix) {
    DIR *d = opendir(dir);
    struct dirent *de;
    size_t prefix_len = strlen(prefix);
    if (!d) return;
    while ((de = readdir(d)) != NULL) {
        char path[512];
        if (strncmp(de->d_name, prefix, prefix_len) != 0) continue;
        snprintf(path, sizeof(path), "%s/%s", dir, de->d_name);
        remove_tree_simple(path);
    }
    closedir(d);
}

int is_digit_name(const char *s) {
    if (!s || !*s) return 0;
    while (*s) {
        if (!isdigit((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

void prune_update_backups(int keep) {
    struct backup_entry { long stamp; char name[64]; } entries[32], tmp;
    DIR *d = opendir(UPDATE_BACKUP_DIR);
    struct dirent *de;
    int count = 0, i, j;
    if (!d) return;
    while ((de = readdir(d)) != NULL) {
        if (!is_digit_name(de->d_name)) continue;
        if (count >= (int)(sizeof(entries) / sizeof(entries[0]))) break;
        entries[count].stamp = atol(de->d_name);
        snprintf(entries[count].name, sizeof(entries[count].name), "%s", de->d_name);
        count++;
    }
    closedir(d);
    for (i = 0; i < count; i++) {
        for (j = i + 1; j < count; j++) {
            if (entries[j].stamp > entries[i].stamp) {
                tmp = entries[i];
                entries[i] = entries[j];
                entries[j] = tmp;
            }
        }
    }
    for (i = keep; i < count; i++) {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", UPDATE_BACKUP_DIR, entries[i].name);
        remove_tree_simple(path);
    }
}

void prune_resource_backups(int keep) {
    char entries[64][64], tmp[64];
    DIR *d = opendir(RESOURCE_BACKUP_DIR);
    struct dirent *de;
    int count = 0, i, j;
    if (!d) return;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        if (count >= 64) break;
        snprintf(entries[count], sizeof(entries[count]), "%s", de->d_name);
        count++;
    }
    closedir(d);
    for (i = 0; i < count; i++) {
        for (j = i + 1; j < count; j++) {
            if (strcmp(entries[j], entries[i]) > 0) {
                memcpy(tmp, entries[i], sizeof(tmp));
                memcpy(entries[i], entries[j], sizeof(entries[i]));
                memcpy(entries[j], tmp, sizeof(entries[j]));
            }
        }
    }
    for (i = keep; i < count; i++) {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", RESOURCE_BACKUP_DIR, entries[i]);
        remove_tree_simple(path);
    }
}

/* json_escape_alloc removed - replaced by cJSON */

int safe_label(const char *s) {
    const unsigned char *p = (const unsigned char *)s;
    if (!s[0]) return 0;
    while (*p) {
        if (*p < 32 || *p == 127) return 0;
        if (*p == '"' || *p == '\\') return 0;
        p++;
    }
    return 1;
}

int safe_run_id(const char *s) {
    const unsigned char *p = (const unsigned char *)s;
    size_t n = strlen(s);
    if (n == 0 || n > 96) return 0;
    while (*p) {
        if (!isalnum(*p) && *p != '_' && *p != '-' && *p != '.') return 0;
        p++;
    }
    return 1;
}

void shell_escape_single(const char *s, char *out, size_t outlen) {
    size_t w = 0;
    if (outlen == 0) return;
    while (*s && w + 5 < outlen) {
        if (*s == '\'') {
            memcpy(out + w, "'\\''", 4);
            w += 4;
        } else {
            out[w++] = *s;
        }
        s++;
    }
    out[w] = 0;
}

int run_cmd(const char *cmd, char *out, size_t outlen) {
    FILE *p = popen(cmd, "r");
    size_t n = 0;
    if (!p) {
        if (outlen) out[0] = 0;
        return -1;
    }
    if (outlen) {
        n = fread(out, 1, outlen - 1, p);
        out[n] = 0;
    }
    int status = pclose(p);
    if (status == -1 && errno == ECHILD) {
        return 0;
    }
    return status;
}

void html(FILE *f, const char *s) {
    while (*s) {
        switch (*s) {
        case '&': fputs("&amp;", f); break;
        case '<': fputs("&lt;", f); break;
        case '>': fputs("&gt;", f); break;
        case '"': fputs("&quot;", f); break;
        case '\'': fputs("&#39;", f); break;
        default: fputc(*s, f); break;
        }
        s++;
    }
}

int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

void url_decode(char *s) {
    char *r = s, *w = s;
    while (*r) {
        if (*r == '+') {
            *w++ = ' ';
            r++;
        } else if (*r == '%' && isxdigit((unsigned char)r[1]) && isxdigit((unsigned char)r[2])) {
            *w++ = (char)(hexval(r[1]) * 16 + hexval(r[2]));
            r += 3;
        } else {
            *w++ = *r++;
        }
    }
    *w = 0;
}

void form_value(const char *body, const char *name, char *out, size_t outlen) {
    size_t namelen = strlen(name);
    const char *p = body;
    if (!outlen) return;
    out[0] = 0;
    if (!body) return;
    while (p && *p) {
        const char *eq = strchr(p, '=');
        const char *amp = strchr(p, '&');
        size_t keylen, vallen;
        if (!eq) return;
        if (amp && amp < eq) {
            p = amp + 1;
            continue;
        }
        keylen = (size_t)(eq - p);
        if (keylen == namelen && strncmp(p, name, namelen) == 0) {
            const char *vstart = eq + 1;
            const char *vend = amp ? amp : p + strlen(p);
            vallen = (size_t)(vend - vstart);
            if (vallen >= outlen) vallen = outlen - 1;
            memcpy(out, vstart, vallen);
            out[vallen] = 0;
            url_decode(out);
            return;
        }
        p = amp ? amp + 1 : NULL;
    }
}

void query_value(const char *path, const char *name, char *out, size_t outlen) {
    const char *q = strchr(path, '?');
    if (!q) {
        if (outlen) out[0] = 0;
        return;
    }
    form_value(q + 1, name, out, outlen);
}

int form_checked(const char *body, const char *name) {
    char tmp[8];
    form_value(body, name, tmp, sizeof(tmp));
    return tmp[0] != 0;
}

int content_length(const char *headers) {
    const char *p = headers;
    const char *needle = "content-length:";
    size_t nlen = strlen(needle);
    while (*p) {
        size_t i;
        for (i = 0; i < nlen; i++) {
            if (!p[i] || tolower((unsigned char)p[i]) != needle[i]) break;
        }
        if (i == nlen) {
            long value;
            char *end;
            p += nlen;
            while (*p == ' ' || *p == '\t') p++;
            errno = 0;
            value = strtol(p, &end, 10);
            if (errno != 0 || end == p || value < 0) return 0;
            if (value > MAX_REQUEST_BODY) return MAX_REQUEST_BODY + 1;
            return (int)value;
        }
        p++;
    }
    return 0;
}

void send_text(int fd, const char *status, const char *body) {
    char hdr[256];
    snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 %s\r\nContent-Type: text/plain\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n",
        status);
    send(fd, hdr, strlen(hdr), 0);
    send(fd, body, strlen(body), 0);
}

void send_all(int fd, const char *data, size_t len) {
    while (len > 0) {
        ssize_t sent = send(fd, data, len, 0);
        if (sent <= 0) return;
        data += sent;
        len -= (size_t)sent;
    }
}

int b64_value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

int base64_decode_text(const char *in, char *out, size_t outlen) {
    int val = 0, valb = -8;
    size_t w = 0;
    if (!outlen) return -1;
    while (*in && *in != '\r' && *in != '\n') {
        int d;
        if (*in == '=') break;
        if (isspace((unsigned char)*in)) {
            in++;
            continue;
        }
        d = b64_value(*in++);
        if (d < 0) return -1;
        val = (val << 6) | d;
        valb += 6;
        if (valb >= 0) {
            if (w + 1 >= outlen) return -1;
            out[w++] = (char)((val >> valb) & 0xff);
            valb -= 8;
        }
    }
    out[w] = 0;
    return (int)w;
}

int webui_auth_ok(const struct request *req) {
    struct webui_auth_config cfg;
    char decoded[256], expected[256];
    const char *prefix = "Basic ";
    load_webui_auth(&cfg);
    if (!cfg.enabled) return 1;
    if (strncasecmp(req->auth, prefix, strlen(prefix)) != 0) return 0;
    if (base64_decode_text(req->auth + strlen(prefix), decoded, sizeof(decoded)) < 0) return 0;
    snprintf(expected, sizeof(expected), "%s:%s", cfg.username, cfg.password);
    return strcmp(decoded, expected) == 0;
}

void send_auth_required(int fd) {
    const char *body = "authentication required\n";
    char hdr[512];
    snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 401 Unauthorized\r\n"
        "WWW-Authenticate: Basic realm=\"Harmony Hub Control\", charset=\"UTF-8\"\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Headers: Authorization, Content-Type\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Content-Type: text/plain\r\nContent-Length: %lu\r\n"
        "Cache-Control: no-store\r\nConnection: close\r\n\r\n",
        (unsigned long)strlen(body));
    send_all(fd, hdr, strlen(hdr));
    send_all(fd, body, strlen(body));
}

int http_get_body(const char *host, const char *path, char **body, size_t max_body, char *err, size_t errlen) {
    struct hostent *he;
    struct sockaddr_in addr;
    struct timeval tv;
    int fd, status = 0;
    char req[1024];
    char *buf, *hdr_end, *p;
    size_t cap = max_body + 8192, n = 0, body_len;
    ssize_t got;
    *body = NULL;
    if (errlen) err[0] = 0;
    he = gethostbyname(host);
    if (!he || !he->h_addr_list || !he->h_addr_list[0]) {
        snprintf(err, errlen, "dns failed");
        return -1;
    }
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        snprintf(err, errlen, "socket failed");
        return -1;
    }
    tv.tv_sec = 8;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        snprintf(err, errlen, "connect failed");
        return -1;
    }
    snprintf(req, sizeof(req),
        "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: HarmonyHubControl/1.0\r\nAccept: */*\r\nConnection: close\r\n\r\n",
        path, host);
    send_all(fd, req, strlen(req));
    buf = (char *)malloc(cap + 1);
    if (!buf) {
        close(fd);
        snprintf(err, errlen, "out of memory");
        return -1;
    }
    while (n < cap) {
        got = recv(fd, buf + n, cap - n, 0);
        if (got <= 0) break;
        n += (size_t)got;
    }
    close(fd);
    buf[n] = 0;
    if (n == cap) {
        free(buf);
        snprintf(err, errlen, "response too large");
        return -1;
    }
    sscanf(buf, "HTTP/%*s %d", &status);
    hdr_end = strstr(buf, "\r\n\r\n");
    if (!hdr_end || status != 200) {
        free(buf);
        snprintf(err, errlen, "http %d", status);
        return -1;
    }
    p = hdr_end + 4;
    while (*p && isspace((unsigned char)*p)) p++;
    body_len = strlen(p);
    if (body_len > max_body) {
        free(buf);
        snprintf(err, errlen, "body too large");
        return -1;
    }
    *body = (char *)malloc(body_len + 1);
    if (!*body) {
        free(buf);
        snprintf(err, errlen, "out of memory");
        return -1;
    }
    memcpy(*body, p, body_len + 1);
    free(buf);
    return 0;
}

void send_file_download(int fd, const char *path, const char *filename, const char *ctype) {
    char hdr[512];
    char *data;
    size_t len = 0;
    data = read_file_alloc(path, MAX_RESOURCE_FILE, &len);
    if (!data) {
        send_text(fd, "404 Not Found", "file not available\n");
        return;
    }
    snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %lu\r\n"
        "Cache-Control: no-store\r\nContent-Disposition: attachment; filename=\"%s\"\r\nConnection: close\r\n\r\n",
        ctype, (unsigned long)len, filename);
    send_all(fd, hdr, strlen(hdr));
    send_all(fd, data, len);
    free(data);
}


/* send_json_start removed - replaced by send_cjson_resp */

void send_cjson_resp(int fd, const char *status, cJSON *root) {
    char *out = cJSON_PrintUnformatted(root);
    char hdr[512];
    size_t len = out ? strlen(out) : 0;
    snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %lu\r\n"
        "Cache-Control: no-store\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Headers: Authorization, Content-Type\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Connection: close\r\n\r\n",
        status, (unsigned long)len);
    send(fd, hdr, strlen(hdr), 0);
    if (out) {
        send(fd, out, len, 0);
        free(out);
    }
}

void copy_text(char *out, size_t outlen, const char *value) {
    if (!out || !outlen) return;
    strncpy(out, value ? value : "", outlen - 1);
    out[outlen - 1] = 0;
}

int append_text(char **buf, size_t *len, size_t *cap, const char *text, int comma) {
    size_t n = strlen(text);
    size_t need = *len + n + (comma ? 1 : 0) + 1;
    char *next;
    if (need > *cap) {
        size_t newcap = *cap ? *cap : 4096;
        while (newcap < need) newcap *= 2;
        next = (char *)realloc(*buf, newcap);
        if (!next) return -1;
        *buf = next;
        *cap = newcap;
    }
    if (comma) (*buf)[(*len)++] = ',';
    memcpy(*buf + *len, text, n);
    *len += n;
    (*buf)[*len] = 0;
    return 0;
}

int json_string(const char *json, const char *key, char *out, size_t outlen) {
    if (!json || !key || !out || outlen == 0) return 0;
    out[0] = 0;
    cJSON *root = cJSON_Parse(json);
    if (!root) return 0;
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!item) item = cJSON_GetObjectItem(root, key);
    int ok = 0;
    if (cJSON_IsString(item) && item->valuestring) {
        snprintf(out, outlen, "%s", item->valuestring);
        ok = 1;
    } else if (cJSON_IsNumber(item)) {
        snprintf(out, outlen, "%.0f", item->valuedouble);
        ok = 1;
    }
    cJSON_Delete(root);
    return ok;
}

int json_int(const char *json, const char *key, int def) {
    if (!json || !key) return def;
    cJSON *root = cJSON_Parse(json);
    if (!root) return def;
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!item) item = cJSON_GetObjectItem(root, key);
    int res = def;
    if (cJSON_IsNumber(item)) {
        res = item->valueint;
    } else if (cJSON_IsString(item) && item->valuestring) {
        res = atoi(item->valuestring);
    }
    cJSON_Delete(root);
    return res;
}

int json_bool(const char *json, const char *key, int def) {
    if (!json || !key) return def;
    cJSON *root = cJSON_Parse(json);
    if (!root) return def;
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!item) item = cJSON_GetObjectItem(root, key);
    int res = def;
    if (cJSON_IsBool(item)) {
        res = cJSON_IsTrue(item) ? 1 : 0;
    } else if (cJSON_IsString(item) && item->valuestring) {
        if (strcasecmp(item->valuestring, "true") == 0 || strcmp(item->valuestring, "1") == 0) res = 1;
        else if (strcasecmp(item->valuestring, "false") == 0 || strcmp(item->valuestring, "0") == 0) res = 0;
    } else if (cJSON_IsNumber(item)) {
        res = item->valueint != 0;
    }
    cJSON_Delete(root);
    return res;
}


static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char *base64_encode(const unsigned char *src, size_t len) {
    if (!src || len == 0) return NULL;
    size_t out_len = 4 * ((len + 2) / 3);
    char *out = (char *)malloc(out_len + 1);
    if (!out) return NULL;
    size_t i, j = 0;
    for (i = 0; i < len; i += 3) {
        uint32_t octet_a = src[i];
        uint32_t octet_b = (i + 1 < len) ? src[i + 1] : 0;
        uint32_t octet_c = (i + 2 < len) ? src[i + 2] : 0;
        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;
        out[j++] = b64_table[(triple >> 18) & 0x3F];
        out[j++] = b64_table[(triple >> 12) & 0x3F];
        out[j++] = (i + 1 < len) ? b64_table[(triple >> 6) & 0x3F] : '=';
        out[j++] = (i + 2 < len) ? b64_table[triple & 0x3F] : '=';
    }
    out[j] = '\0';
    return out;
}

unsigned char *base64_decode(const char *src, size_t *out_len) {
    if (!src) return NULL;
    size_t len = strlen(src);
    if (len % 4 != 0) return NULL;
    size_t padding = 0;
    if (len > 0 && src[len - 1] == '=') padding++;
    if (len > 1 && src[len - 2] == '=') padding++;
    size_t alloc_len = (len / 4) * 3 - padding;
    unsigned char *out = (unsigned char *)malloc(alloc_len + 1);
    if (!out) return NULL;

    static const int b64_inv[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1, 0,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };

    size_t i, j = 0;
    for (i = 0; i < len; i += 4) {
        int a = b64_inv[(unsigned char)src[i]];
        int b = b64_inv[(unsigned char)src[i+1]];
        int c = b64_inv[(unsigned char)src[i+2]];
        int d = b64_inv[(unsigned char)src[i+3]];
        if (a < 0 || b < 0) { free(out); return NULL; }
        uint32_t triple = (a << 18) | (b << 12) | ((c >= 0 ? c : 0) << 6) | (d >= 0 ? d : 0);
        if (j < alloc_len) out[j++] = (triple >> 16) & 0xFF;
        if (j < alloc_len) out[j++] = (triple >> 8) & 0xFF;
        if (j < alloc_len) out[j++] = triple & 0xFF;
    }
    *out_len = j;
    return out;
}

char *trim_in_place(char *s) {
    char *end;
    while (*s && isspace((unsigned char)*s)) s++;
    end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) *--end = 0;
    return s;
}

int split_fields(char *line, char sep, char **fields, int max_fields) {
    int count = 0;
    char *p = line;
    while (count < max_fields) {
        fields[count++] = p;
        p = strchr(p, sep);
        if (!p) break;
        *p++ = 0;
    }
    return count;
}

void free_request(struct request *req) {
    if (!req) return;
    free(req->body);
    req->body = NULL;
    req->body_len = 0;
}

int read_request(int fd, struct request *req) {
    char *buf;
    char *header_end, *line_end, *p;
    int n = 0, clen, orig_clen, header_len;
    memset(req, 0, sizeof(*req));
    buf = (char *)malloc(MAX_REQUEST_BYTES);
    if (!buf) return -1;
    while (n < MAX_REQUEST_BYTES - 1) {
        int got = recv(fd, buf + n, MAX_REQUEST_BYTES - 1 - n, 0);
        if (got <= 0) {
            free(buf);
            return -1;
        }
        n += got;
        buf[n] = 0;
        header_end = strstr(buf, "\r\n\r\n");
        if (header_end) {
            header_len = (int)(header_end + 4 - buf);
            orig_clen = content_length(buf);
            if (orig_clen < 0) orig_clen = 0;
            clen = orig_clen;
            while (n < header_len + orig_clen && n < MAX_REQUEST_BYTES - 1) {
                got = recv(fd, buf + n, MAX_REQUEST_BYTES - 1 - n, 0);
                if (got <= 0) break;
                n += got;
                buf[n] = 0;
            }
            line_end = strstr(buf, "\r\n");
            if (!line_end) {
                free(buf);
                return -1;
            }
            *line_end = 0;
            sscanf(buf, "%7s %255s", req->method, req->path);
            *line_end = '\r';
            p = line_end + 2;
            while (p < header_end) {
                char *e = strstr(p, "\r\n");
                if (!e) break;
                if (strncasecmp(p, "Authorization:", 14) == 0) {
                    char *v = p + 14;
                    while (*v == ' ' || *v == '\t') v++;
                    snprintf(req->auth, sizeof(req->auth), "%.*s", (int)(e - v), v);
                }
                if (strncasecmp(p, "X-Requested-With:", 17) == 0 ||
                    (strncasecmp(p, "Accept:", 7) == 0 && strstr(p, "application/json")) ||
                    strncasecmp(p, "X-Codex-Ajax:", 13) == 0) {
                    req->is_ajax = 1;
                }
                p = e + 2;
            }
            if (orig_clen > 0) {
                int available = n - header_len;
                if (available < 0) available = 0;
                if (available < orig_clen) req->body_truncated = 1;
                if (orig_clen > MAX_REQUEST_BODY) req->body_truncated = 1;
                clen = available < orig_clen ? available : orig_clen;
                if (clen > MAX_REQUEST_BODY) clen = MAX_REQUEST_BODY;
                req->body = (char *)malloc((size_t)clen + 1);
                if (!req->body) {
                    free(buf);
                    return -1;
                }
                memcpy(req->body, buf + header_len, clen);
                req->body[clen] = 0;
                req->body_len = (size_t)clen;
            }
            free(buf);
            return 0;
        }
    }
    free(buf);
    return -1;
}

void send_payload_too_large(int fd, const struct request *req) {
    if (strncmp(req->path, "/api/", 5) == 0) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "request body is too large for this hub");
        send_cjson_resp(fd, "413 Payload Too Large", err);
        cJSON_Delete(err);
        return;
    }
    send_text(fd, "413 Payload Too Large", "request body is too large for this hub\n");
}

