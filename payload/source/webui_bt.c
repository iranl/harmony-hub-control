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
#include <time.h>
#include <stdbool.h>
#include "cJSON.h"
#include "codex_webui_types.h"
#include "codex_bt_common.h"
#include "webui_utils.h"
#include "webui_config.h"
#include "webui_bt.h"
#include "webui_html.h"

void send_bt_devices_download(int fd) {
    char hdr[512];
    char *data;
    const char *empty = "{\"version\":1,\"devices\":[]}\n";
    size_t len = 0;
    int owned = 1;
    data = read_file_alloc(BT_DEVICE_STORE, MAX_RESOURCE_FILE, &len);
    if (!data) {
        data = (char *)empty;
        len = strlen(empty);
        owned = 0;
    }
    snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %lu\r\n"
        "Cache-Control: no-store\r\nContent-Disposition: attachment; filename=\"bt-devices.json\"\r\nConnection: close\r\n\r\n",
        (unsigned long)len);
    send_all(fd, hdr, strlen(hdr));
    send_all(fd, data, len);
    if (owned) free(data);
}

void send_remote_mapping_download(int fd) {
    char hdr[512];
    char *data;
    const char *empty = "{\"remote\":{\"name\":\"Homatics B25\"},\"activities\":{}}\n";
    size_t len = 0;
    int owned = 1;
    data = read_file_alloc(BT_REMOTE_MAP_FILE, MAX_RESOURCE_FILE, &len);
    if (!data) {
        data = (char *)empty;
        len = strlen(empty);
        owned = 0;
    }
    snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %lu\r\n"
        "Cache-Control: no-store\r\nContent-Disposition: attachment; filename=\"bt_remote_map.json\"\r\nConnection: close\r\n\r\n",
        (unsigned long)len);
    send_all(fd, hdr, strlen(hdr));
    send_all(fd, data, len);
    if (owned) free(data);
}


int is_btstack_running(void) {
    if (access("/tmp/codex_btstack.pid", F_OK) == 0) {
        FILE *pf = fopen("/tmp/codex_btstack.pid", "r");
        if (pf) {
            int pid = 0;
            if (fscanf(pf, "%d", &pid) == 1 && pid > 0) {
                fclose(pf);
                if (kill(pid, 0) == 0) return 1;
            } else {
                fclose(pf);
            }
        }
    }
    return 0;
}

int safe_bt_addr(const char *s) {
    return codex_valid_bdaddr(s);
}

int safe_bt_token(const char *s, size_t maxlen) {
    const unsigned char *p = (const unsigned char *)s;
    size_t n = strlen(s);
    if (n == 0 || n > maxlen) return 0;
    while (*p) {
        if (!isalnum(*p) && *p != '_' && *p != '-') return 0;
        p++;
    }
    return 1;
}

int safe_bt_pin(const char *s) {
    const unsigned char *p = (const unsigned char *)s;
    if (strlen(s) > 16) return 0;
    while (*p) {
        if (!isdigit(*p)) return 0;
        p++;
    }
    return 1;
}

int safe_bt_name(const char *s) {
    const unsigned char *p = (const unsigned char *)s;
    size_t n = strlen(s);
    if (n == 0 || n > 48) return 0;
    while (*p) {
        if (!isalnum(*p) && *p != ' ' && *p != '_' && *p != '-' && *p != '.') return 0;
        p++;
    }
    return 1;
}

int extract_bt_addr_from_text(const char *text, char *out, size_t outlen) {
    const char *p = text;
    if (!text || !out || outlen < 18) return 0;
    out[0] = 0;
    while (*p) {
        int i;
        if (isxdigit((unsigned char)p[0]) && isxdigit((unsigned char)p[1]) &&
            p[2] == ':' && isxdigit((unsigned char)p[3]) && isxdigit((unsigned char)p[4])) {
            char candidate[18];
            for (i = 0; i < 17 && p[i]; i++) candidate[i] = (char)toupper((unsigned char)p[i]);
            candidate[17] = 0;
            if (safe_bt_addr(candidate)) {
                snprintf(out, outlen, "%s", candidate);
                return 1;
            }
        }
        p++;
    }
    return 0;
}

int detect_connected_bt_addr(char *out, size_t outlen, char *raw, size_t rawlen) {
    char reply[2048];
    if (!out || outlen < 18) return 0;
    out[0] = 0;
    reply[0] = 0;

    if (is_btstack_running()) {
        char status_raw[1024];
        if (read_text("/tmp/codex_btstack_status.json", status_raw, sizeof(status_raw)) > 0) {
            cJSON *sj = cJSON_Parse(status_raw);
            if (sj) {
                cJSON *host = cJSON_GetObjectItem(sj, "host");
                if (host && cJSON_IsTrue(cJSON_GetObjectItem(host, "connected"))) {
                    cJSON *addr = cJSON_GetObjectItem(host, "addr");
                    if (addr && addr->valuestring && addr->valuestring[0]) {
                        snprintf(out, outlen, "%s", addr->valuestring);
                        if (raw && rawlen) snprintf(raw, rawlen, "BTstack Host: %s", out);
                        cJSON_Delete(sj);
                        return 1;
                    }
                }
                cJSON_Delete(sj);
            }
        }
    }

    run_cmd("hcitool con 2>&1", reply, sizeof(reply));
    if (raw && rawlen) {
        snprintf(raw, rawlen, "%s", reply);
    }
    return extract_bt_addr_from_text(reply, out, outlen);
}

int bt_type_allowed(const char *type) {
    return strcmp(type, "fire") == 0 ||
        strcmp(type, "btkeyboard") == 0 ||
        strcmp(type, "btkeyboard-nexus") == 0 ||
        strcmp(type, "ps3") == 0 ||
        strcmp(type, "wii") == 0;
}

void save_bthid_target(const char *type, const char *bdaddr) {
    char buf[128];
    if (!bt_type_allowed(type) || !safe_bt_addr(bdaddr) || strcmp(bdaddr, "00:00:00:00:00:00") == 0) return;
    snprintf(buf, sizeof(buf), "type=%s\nbdaddr=%s\n", type, bdaddr);
    write_file_atomic(BT_TARGET_FILE, buf, strlen(buf));
    chmod(BT_TARGET_FILE, 0644);
}

int bt_hex_payload_from_input(const char *s, char *out, size_t outlen) {
    const char *p = s;
    int saw_prefix = 0, saw_separator = 0;
    size_t n = 0, digits = 0;
    if (!s || !out || outlen < 3) return 0;
    out[0] = 0;
    if (strncasecmp(p, "hex:", 4) == 0) {
        saw_prefix = 1;
        p += 4;
    }
    while (*p) {
        if (*p == ' ' || *p == ':' || *p == '-' || *p == '\t' || *p == '\r' || *p == '\n') {
            saw_separator = 1;
            p++;
            continue;
        }
        if (*p == '0' && (p[1] == 'x' || p[1] == 'X')) {
            saw_prefix = 1;
            p += 2;
            continue;
        }
        if (!isxdigit((unsigned char)*p)) return 0;
        if (n + 2 >= outlen) return 0;
        out[n++] = (char)toupper((unsigned char)*p);
        digits++;
        p++;
    }
    out[n] = 0;
    if (!digits || (digits & 1)) return 0;
    if (!saw_prefix && !saw_separator && digits < 8) return 0;
    if (digits > 64) return 0;
    return 1;
}

int bt_key_usage(const char *key) {
    if (!key || !key[0]) return -1;
    if (strlen(key) == 1) {
        if (key[0] >= 'a' && key[0] <= 'z') return 0x04 + (key[0] - 'a');
        if (key[0] >= '1' && key[0] <= '9') return 0x1e + (key[0] - '1');
        if (key[0] == '0') return 0x27;
    }
    if (strncmp(key, "number", 6) == 0 && key[6] && !key[7]) {
        if (key[6] >= '1' && key[6] <= '9') return 0x1e + (key[6] - '1');
        if (key[6] == '0') return 0x27;
    }
    if (key[0] == 'f' && isdigit((unsigned char)key[1])) {
        int n = atoi(key + 1);
        if (n >= 1 && n <= 12) return 0x3a + (n - 1);
    }
    if (strcmp(key, "enter") == 0 || strcmp(key, "return") == 0) return 0x28;
    if (strcmp(key, "escape") == 0 || strcmp(key, "esc") == 0 || strcmp(key, "back") == 0) return 0x29;
    if (strcmp(key, "backspace") == 0) return 0x2a;
    if (strcmp(key, "tab") == 0) return 0x2b;
    if (strcmp(key, "space") == 0) return 0x2c;
    if (strcmp(key, "minus") == 0 || strcmp(key, "dash") == 0) return 0x2d;
    if (strcmp(key, "equal") == 0 || strcmp(key, "equals") == 0) return 0x2e;
    if (strcmp(key, "leftbracket") == 0 || strcmp(key, "openbracket") == 0) return 0x2f;
    if (strcmp(key, "rightbracket") == 0 || strcmp(key, "closebracket") == 0) return 0x30;
    if (strcmp(key, "backslash") == 0) return 0x31;
    if (strcmp(key, "semicolon") == 0) return 0x33;
    if (strcmp(key, "apostrophe") == 0 || strcmp(key, "quote") == 0) return 0x34;
    if (strcmp(key, "grave") == 0 || strcmp(key, "graveaccent") == 0) return 0x35;
    if (strcmp(key, "comma") == 0) return 0x36;
    if (strcmp(key, "period") == 0 || strcmp(key, "dot") == 0) return 0x37;
    if (strcmp(key, "slash") == 0) return 0x38;
    if (strcmp(key, "capslock") == 0) return 0x39;
    if (strcmp(key, "printscreen") == 0) return 0x46;
    if (strcmp(key, "scrolllock") == 0) return 0x47;
    if (strcmp(key, "pause") == 0) return 0x48;
    if (strcmp(key, "insert") == 0) return 0x49;
    if (strcmp(key, "home") == 0) return 0x4a;
    if (strcmp(key, "pageup") == 0 || strcmp(key, "pgup") == 0) return 0x4b;
    if (strcmp(key, "delete") == 0 || strcmp(key, "del") == 0) return 0x4c;
    if (strcmp(key, "end") == 0) return 0x4d;
    if (strcmp(key, "pagedown") == 0 || strcmp(key, "pgdn") == 0) return 0x4e;
    if (strcmp(key, "directionright") == 0 || strcmp(key, "right") == 0) return 0x4f;
    if (strcmp(key, "directionleft") == 0 || strcmp(key, "left") == 0) return 0x50;
    if (strcmp(key, "directiondown") == 0 || strcmp(key, "down") == 0) return 0x51;
    if (strcmp(key, "directionup") == 0 || strcmp(key, "up") == 0) return 0x52;
    if (strcmp(key, "menu") == 0 || strcmp(key, "application") == 0) return 0x65;
    return -1;
}

int bt_keyboard_report_hex(const char *input, char *press, size_t presslen, char *release, size_t releaselen, char *err, size_t errlen) {
    char norm[80], key[80];
    const char *p = input;
    size_t n = 0;
    unsigned int mod = 0;
    int usage;

    if (!input || !input[0]) {
        snprintf(err, errlen, "missing Bluetooth HID command");
        return -1;
    }
    if (bt_hex_payload_from_input(input, press, presslen)) {
        release[0] = 0;
        return 0;
    }
    while (*p && n + 1 < sizeof(norm)) {
        if (isalnum((unsigned char)*p)) norm[n++] = (char)tolower((unsigned char)*p);
        p++;
    }
    norm[n] = 0;
    copy_text(key, sizeof(key), norm);
    while (key[0]) {
        if (strncmp(key, "control", 7) == 0) {
            mod |= 0x01;
            memmove(key, key + 7, strlen(key + 7) + 1);
        } else if (strncmp(key, "ctrl", 4) == 0) {
            mod |= 0x01;
            memmove(key, key + 4, strlen(key + 4) + 1);
        } else if (strncmp(key, "shift", 5) == 0) {
            mod |= 0x02;
            memmove(key, key + 5, strlen(key + 5) + 1);
        } else if (strncmp(key, "altgr", 5) == 0) {
            mod |= 0x40;
            memmove(key, key + 5, strlen(key + 5) + 1);
        } else if (strncmp(key, "alt", 3) == 0) {
            mod |= 0x04;
            memmove(key, key + 3, strlen(key + 3) + 1);
        } else if (strncmp(key, "windows", 7) == 0) {
            mod |= 0x08;
            memmove(key, key + 7, strlen(key + 7) + 1);
        } else if (strncmp(key, "win", 3) == 0) {
            mod |= 0x08;
            memmove(key, key + 3, strlen(key + 3) + 1);
        } else if (strncmp(key, "cmd", 3) == 0 || strncmp(key, "meta", 4) == 0) {
            mod |= 0x08;
            memmove(key, key + (key[0] == 'm' ? 4 : 3), strlen(key + (key[0] == 'm' ? 4 : 3)) + 1);
        } else {
            break;
        }
    }
    usage = bt_key_usage(key);
    if (usage < 0) {
        snprintf(err, errlen, "unsupported Bluetooth keyboard command: %s", input);
        return -1;
    }
    snprintf(press, presslen, "A101%02X00%02X0000000000", mod & 0xff, usage & 0xff);
    snprintf(release, releaselen, "A1010000000000000000");
    return 0;
}

int bt_sequence_add_code(const char *input, char **seq, size_t *seq_len, size_t *seq_cap, int *keys, char *err, size_t errlen) {
    char code[128], press[96], release[32];
    char *clean;
    copy_text(code, sizeof(code), input);
    clean = trim_in_place(code);
    while (*clean && clean[strlen(clean) - 1] == '\r') clean[strlen(clean) - 1] = 0;
    if (!clean[0]) return 0;
    if (is_btstack_running()) {
        if (append_text(seq, seq_len, seq_cap, "KEY ", 0) != 0 ||
            append_text(seq, seq_len, seq_cap, clean, 0) != 0 ||
            append_text(seq, seq_len, seq_cap, "\n", 0) != 0) {
            snprintf(err, errlen, "not enough memory for Bluetooth key sequence");
            return -1;
        }
        if (keys) (*keys)++;
        return 0;
    }
    if (bt_keyboard_report_hex(clean, press, sizeof(press), release, sizeof(release), err, errlen) != 0) {
        return -1;
    }
    /* Keep press and release on one sequence line. The sequence uses
       '|' for the short intra-key release delay, then newline for the gap
       before the next key. */
    if (append_text(seq, seq_len, seq_cap, press, 0) != 0 ||
        (release[0] && (append_text(seq, seq_len, seq_cap, "|", 0) != 0 ||
        append_text(seq, seq_len, seq_cap, release, 0) != 0)) ||
        append_text(seq, seq_len, seq_cap, "\n", 0) != 0) {
        snprintf(err, errlen, "not enough memory for Bluetooth report sequence");
        return -1;
    }
    if (keys) (*keys)++;
    return 0;
}

int bthid_status_runtime_alive(const char *raw) {
    int pid;
    if (!raw || !json_bool(raw, "runtime", 0)) return 0;
    pid = json_int(raw, "pid", 0);
    if (pid <= 0) {
        return is_btstack_running();
    }
    if (kill((pid_t)pid, 0) == 0) return 1;
    return errno == EPERM;
}

int write_bt_text_fifo(const char *text, char *err, size_t errlen) {
    int fd, idle_waits = 0, max_idle_waits;
    size_t len, off = 0;
    if (!text || !text[0]) {
        snprintf(err, errlen, "missing Bluetooth text");
        return -1;
    }
    len = strlen(text);
    if (len > MAX_BT_SEQUENCE_BODY) {
        snprintf(err, errlen, "Bluetooth text is too large");
        return -1;
    }
    if (!is_btstack_running()) {
        snprintf(err, errlen, "Bluetooth engine (BTstack) is not running");
        return -1;
    }
    fd = open(BT_TEXT_FIFO, O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        snprintf(err, errlen, "Bluetooth text FIFO open failed (%s); daemon may be initializing", strerror(errno));
        return -1;
    }
    max_idle_waits = 600 + (int)(len / 16);
    if (max_idle_waits > 6000) max_idle_waits = 6000;
    while (off < len) {
        ssize_t n = write(fd, text + off, len - off);
        if (n > 0) {
            off += (size_t)n;
            idle_waits = 0;
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            fd_set wfds;
            struct timeval tv;
            if (idle_waits++ >= max_idle_waits) {
                snprintf(err, errlen, "Bluetooth FIFO write timed out after %lu/%lu bytes; target may be disconnected or busy",
                    (unsigned long)off, (unsigned long)len);
                close(fd);
                return -1;
            }
            FD_ZERO(&wfds);
            FD_SET(fd, &wfds);
            tv.tv_sec = 0;
            tv.tv_usec = 10000;
            select(fd + 1, NULL, &wfds, NULL, &tv);
            continue;
        }
        snprintf(err, errlen, "Bluetooth FIFO write failed at byte %lu: %s", (unsigned long)off, strerror(errno));
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

void append_run_status(char *out, size_t outlen, const char *text) {
    size_t used;
    if (!out || !outlen || !text || !text[0]) return;
    used = strlen(out);
    if (used + 2 >= outlen) return;
    snprintf(out + used, outlen - used, "%s%s", used ? "\n" : "", text);
}

int flush_bt_saved_sequence(const char *type, const char *bdaddr, char **seq, size_t *seq_len, size_t *seq_cap, int *chunk_keys, int gap_ms, int *total_keys, char *out, size_t outlen) {
    (void)type;
    (void)bdaddr;
    (void)gap_ms;
    char note[256];
    if (!seq || !*seq || !seq_len || *seq_len == 0 || !chunk_keys || *chunk_keys <= 0) return 0;
    if (is_btstack_running()) {
        char bterr[128];
        if (bdaddr && bdaddr[0]) {
            char cmdbuf[64];
            snprintf(cmdbuf, sizeof(cmdbuf), "TARGET %s\n", bdaddr);
            write_bt_text_fifo(cmdbuf, bterr, sizeof(bterr));
        }
        if (write_bt_text_fifo(*seq, bterr, sizeof(bterr)) == 0) {
            if (total_keys) *total_keys += *chunk_keys;
            snprintf(note, sizeof(note), "sent %d key%s via BTstack", *chunk_keys, *chunk_keys == 1 ? "" : "s");
            append_run_status(out, outlen, note);
            free(*seq);
            *seq = NULL;
            *seq_len = 0;
            *seq_cap = 0;
            *chunk_keys = 0;
            return 0;
        } else {
            snprintf(note, sizeof(note), "BTstack write failed: %s", bterr[0] ? bterr : "unknown error");
            append_run_status(out, outlen, note);
        }
    } else {
        append_run_status(out, outlen, "Bluetooth engine (BTstack) is not running");
    }
    free(*seq);
    *seq = NULL;
    *seq_len = 0;
    *seq_cap = 0;
    *chunk_keys = 0;
    return -1;
}

int run_bt_saved_script(const char *type, const char *bdaddr, const char *script, int gap_ms, char *out, size_t outlen) {
    char *copy, *line, *save;
    char *seq = NULL;
    size_t seq_len = 0, seq_cap = 0, script_len;
    int chunk_keys = 0, total_keys = 0, text_bytes = 0, waits = 0;
    char err[256], note[256];
    if (out && outlen) out[0] = 0;
    char target_addr[32] = {0};
    if (bdaddr && bdaddr[0] && safe_bt_addr(bdaddr)) {
        snprintf(target_addr, sizeof(target_addr), "%s", bdaddr);
    } else {
        detect_connected_bt_addr(target_addr, sizeof(target_addr), NULL, 0);
    }
    if (!bt_type_allowed(type) || (!target_addr[0] && !is_btstack_running())) {
        snprintf(out, outlen, "saved Bluetooth device has an invalid keyboard type or address");
        return -1;
    }
    if (!safe_bt_script_text(script)) {
        snprintf(out, outlen, "saved Bluetooth script is empty or contains unsupported control characters");
        return -1;
    }
    if (gap_ms < 15) gap_ms = 35;
    if (gap_ms > 5000) gap_ms = 5000;
    if (target_addr[0]) save_bthid_target(type, target_addr);
    script_len = strlen(script);
    copy = (char *)malloc(script_len + 1);
    if (!copy) {
        snprintf(out, outlen, "not enough memory to run Bluetooth script");
        return -1;
    }
    memcpy(copy, script, script_len + 1);
    line = strtok_r(copy, "\n", &save);
    while (line) {
        char *clean = trim_in_place(line);
        char *arg = clean;
        while (*clean && clean[strlen(clean) - 1] == '\r') clean[strlen(clean) - 1] = 0;
        if (!clean[0] || clean[0] == '#') {
            line = strtok_r(NULL, "\n", &save);
            continue;
        }
        while (*arg && !isspace((unsigned char)*arg)) arg++;
        if (*arg) {
            *arg++ = 0;
            while (*arg == ' ' || *arg == '\t') arg++;
        }
        if (strcasecmp(clean, "WAIT") == 0 || strcasecmp(clean, "SLEEP") == 0) {
            int ms = atoi(arg);
            if (flush_bt_saved_sequence(type, target_addr, &seq, &seq_len, &seq_cap, &chunk_keys, gap_ms, &total_keys, out, outlen) != 0) {
                free(copy);
                return -1;
            }
            if (ms < 0) ms = 0;
            if (ms > 60000) ms = 60000;
            usleep((useconds_t)ms * 1000);
            waits++;
        } else if (strcasecmp(clean, "TEXT") == 0 || strcasecmp(clean, "TYPE") == 0) {
            size_t chars = strlen(arg);
            int settle_ms;
            if (flush_bt_saved_sequence(type, target_addr, &seq, &seq_len, &seq_cap, &chunk_keys, gap_ms, &total_keys, out, outlen) != 0) {
                free(copy);
                return -1;
            }
            if (!chars) {
                snprintf(out, outlen, "TEXT step needs text after the command");
                free(copy);
                return -1;
            }
            char tbuf[1024];
            if (target_addr[0]) {
                snprintf(tbuf, sizeof(tbuf), "TARGET %s\nTEXT %s\n", target_addr, arg);
            } else {
                snprintf(tbuf, sizeof(tbuf), "TEXT %s\n", arg);
            }
            if (write_bt_text_fifo(tbuf, err, sizeof(err)) != 0) {
                snprintf(out, outlen, "Bluetooth text helper failed: %s", err);
                free(copy);
                return -1;
            }
            text_bytes += (int)chars;
            settle_ms = 100 + (int)chars * (gap_ms + 5);
            if (settle_ms > 120000) settle_ms = 120000;
            usleep((useconds_t)settle_ms * 1000);
            snprintf(note, sizeof(note), "typed %lu text byte%s", (unsigned long)chars, chars == 1 ? "" : "s");
            append_run_status(out, outlen, note);
        } else {
            const char *key = clean;
            if (strcasecmp(clean, "KEY") == 0 || strcasecmp(clean, "SEND") == 0 ||
                strcasecmp(clean, "PRESS") == 0 || strcasecmp(clean, "COMBO") == 0 ||
                strcasecmp(clean, "HOTKEY") == 0) {
                key = arg;
            }
            if (!key || !key[0] || bt_sequence_add_code(key, &seq, &seq_len, &seq_cap, &chunk_keys, err, sizeof(err)) != 0) {
                snprintf(out, outlen, "%s", err[0] ? err : "unsupported Bluetooth keyboard script line");
                free(seq);
                free(copy);
                return -1;
            }
            if (chunk_keys >= 64 || seq_len > 12000) {
                if (flush_bt_saved_sequence(type, target_addr, &seq, &seq_len, &seq_cap, &chunk_keys, gap_ms, &total_keys, out, outlen) != 0) {
                    free(copy);
                    return -1;
                }
            }
        }
        line = strtok_r(NULL, "\n", &save);
    }
    if (flush_bt_saved_sequence(type, target_addr, &seq, &seq_len, &seq_cap, &chunk_keys, gap_ms, &total_keys, out, outlen) != 0) {
        free(copy);
        return -1;
    }
    snprintf(note, sizeof(note), "script complete: %d key%s, %d text byte%s, %d wait%s",
        total_keys, total_keys == 1 ? "" : "s",
        text_bytes, text_bytes == 1 ? "" : "s",
        waits, waits == 1 ? "" : "s");
    append_run_status(out, outlen, note);
    free(copy);
    return 0;
}

static void resolve_bt_target_addr(const char *target, char *out_addr, size_t outlen) {
    if (!target || !target[0] || !out_addr || outlen < 18) return;
    out_addr[0] = 0;
    if (codex_valid_bdaddr(target)) {
        strncpy(out_addr, target, outlen - 1);
        return;
    }
    struct bt_inventory inv;
    if (load_bt_inventory(&inv) == 0) {
        for (int i = 0; i < inv.device_count; i++) {
            if ((strcmp(inv.devices[i].id, target) == 0 || strcasecmp(inv.devices[i].name, target) == 0) && inv.devices[i].bdaddr[0]) {
                strncpy(out_addr, inv.devices[i].bdaddr, outlen - 1);
                return;
            }
        }
    }
    char *dl_buf = read_file_alloc("/data/resources/DeviceList.json", 1000000, NULL);
    if (dl_buf) {
        cJSON *dl_root = cJSON_Parse(dl_buf);
        free(dl_buf);
        if (dl_root) {
            cJSON *devs_feat = cJSON_GetObjectItem(dl_root, "DevicesWithFeatures");
            if (devs_feat && cJSON_IsArray(devs_feat)) {
                cJSON *item = NULL;
                cJSON_ArrayForEach(item, devs_feat) {
                    cJSON *dev = cJSON_GetObjectItem(item, "Device");
                    if (!dev) continue;
                    cJSON *jid = cJSON_GetObjectItem(dev, "Id");
                    if (!jid) jid = cJSON_GetObjectItem(dev, "id");
                    char id_str[64] = {0};
                    if (jid && cJSON_IsNumber(jid)) snprintf(id_str, sizeof(id_str), "%ld", (long)jid->valuedouble);
                    else if (jid && cJSON_IsString(jid)) strncpy(id_str, jid->valuestring, sizeof(id_str) - 1);
                    cJSON *jname = cJSON_GetObjectItem(dev, "Name");
                    const char *nm = (jname && cJSON_IsString(jname)) ? jname->valuestring : "";
                    if ((id_str[0] && strcmp(id_str, target) == 0) || (nm[0] && strcasecmp(nm, target) == 0)) {
                        cJSON *jbta = cJSON_GetObjectItem(dev, "BTAddress");
                        if (jbta && cJSON_IsString(jbta) && jbta->valuestring[0]) {
                            strncpy(out_addr, jbta->valuestring, outlen - 1);
                        }
                        break;
                    }
                }
            }
            cJSON_Delete(dl_root);
        }
    }
}

void render_bluetooth_text_json(int fd, const struct request *req) {
    char text[MAX_BT_SEQUENCE_BODY], err[256];
    form_value(req->body, "text", text, sizeof(text));
    if (!text[0]) json_string(req->body, "text", text, sizeof(text));
    if (!text[0]) {
        cJSON *res = cJSON_CreateObject();
        cJSON_AddBoolToObject(res, "ok", 0);
        cJSON_AddStringToObject(res, "error", "No text provided");
        send_cjson_resp(fd, "400 Bad Request", res);
        cJSON_Delete(res);
        return;
    }
    char target[64] = {0}, resolved_addr[32] = {0};
    form_value(req->body, "target", target, sizeof(target));
    if (!target[0]) json_string(req->body, "target", target, sizeof(target));
    if (!target[0]) {
        form_value(req->body, "bdaddr", target, sizeof(target));
        if (!target[0]) json_string(req->body, "bdaddr", target, sizeof(target));
    }
    if (!target[0]) {
        form_value(req->body, "deviceId", target, sizeof(target));
        if (!target[0]) json_string(req->body, "deviceId", target, sizeof(target));
    }
    if (target[0]) resolve_bt_target_addr(target, resolved_addr, sizeof(resolved_addr));

    char *formatted = (char *)malloc(strlen(text) * 2 + 128);
    if (!formatted) {
        cJSON *res = cJSON_CreateObject();
        cJSON_AddBoolToObject(res, "ok", 0);
        cJSON_AddStringToObject(res, "error", "Out of memory");
        send_cjson_resp(fd, "500 Internal Server Error", res);
        cJSON_Delete(res);
        return;
    }
    formatted[0] = 0;
    if (resolved_addr[0]) {
        snprintf(formatted, 64, "TARGET %s\n", resolved_addr);
    }
    char *saveptr = NULL;
    char *dup = strdup(text);
    char *line = strtok_r(dup, "\r\n", &saveptr);
    int first = 1;
    while (line) {
        if (!first) {
            strcat(formatted, "KEY enter\n");
        }
        first = 0;
        strcat(formatted, "TEXT ");
        strcat(formatted, line);
        strcat(formatted, "\n");
        line = strtok_r(NULL, "\r\n", &saveptr);
    }
    free(dup);

    if (write_bt_text_fifo(formatted, err, sizeof(err)) != 0) {
        free(formatted);
        cJSON *res = cJSON_CreateObject();
        cJSON_AddBoolToObject(res, "ok", 0);
        cJSON_AddStringToObject(res, "error", err);
        send_cjson_resp(fd, "400 Bad Request", res);
        cJSON_Delete(res);
        return;
    }
    free(formatted);
    cJSON *res = cJSON_CreateObject();
    cJSON_AddBoolToObject(res, "ok", 1);
    cJSON_AddStringToObject(res, "path", "/api/bt-text");
    cJSON_AddNumberToObject(res, "bytes", (double)strlen(text));
    if (resolved_addr[0]) cJSON_AddStringToObject(res, "target", resolved_addr);
    send_cjson_resp(fd, "200 OK", res);
    cJSON_Delete(res);
}

static void attach_bt_devices_to_status(cJSON *root) {
    if (!root) return;
    cJSON *devs_out = cJSON_CreateArray();
    cJSON *hosts_arr = cJSON_GetObjectItem(root, "hosts");

    char *dl_raw = read_file_alloc(DEVICE_LIST, MAX_RESOURCE_FILE, NULL);
    if (dl_raw) {
        cJSON *dl_root = cJSON_Parse(dl_raw);
        free(dl_raw);
        if (dl_root) {
            cJSON *dwf = cJSON_GetObjectItemCaseSensitive(dl_root, "DevicesWithFeatures");
            if (dwf && cJSON_IsArray(dwf)) {
                cJSON *item = NULL;
                cJSON_ArrayForEach(item, dwf) {
                    cJSON *dev = cJSON_GetObjectItemCaseSensitive(item, "Device");
                    if (!dev) continue;
                    cJSON *j_trans = cJSON_GetObjectItemCaseSensitive(dev, "Transport");
                    if (!j_trans) j_trans = cJSON_GetObjectItemCaseSensitive(dev, "TransportType");
                    int is_bt_trans = (j_trans && cJSON_IsNumber(j_trans) && (int)j_trans->valuedouble == 32);

                    cJSON *j_bta = cJSON_GetObjectItemCaseSensitive(dev, "BTAddress");
                    const char *bta_str = (j_bta && cJSON_IsString(j_bta)) ? j_bta->valuestring : "";

                    cJSON *j_id = cJSON_GetObjectItemCaseSensitive(dev, "Id-");
                    if (!j_id) j_id = cJSON_GetObjectItemCaseSensitive(dev, "Id");
                    if (!j_id) j_id = cJSON_GetObjectItemCaseSensitive(dev, "id");
                    char id_str[64] = {0};
                    if (j_id) {
                        if (cJSON_IsNumber(j_id)) snprintf(id_str, sizeof(id_str), "%ld", (long)j_id->valuedouble);
                        else if (cJSON_IsString(j_id) && j_id->valuestring) strncpy(id_str, j_id->valuestring, sizeof(id_str) - 1);
                    }

                    cJSON *j_name = cJSON_GetObjectItemCaseSensitive(dev, "Name");
                    const char *name_str = (j_name && cJSON_IsString(j_name)) ? j_name->valuestring : "";
                    cJSON *j_model = cJSON_GetObjectItemCaseSensitive(dev, "Model");
                    const char *model_str = (j_model && cJSON_IsString(j_model)) ? j_model->valuestring : "";
                    cJSON *j_mfg = cJSON_GetObjectItemCaseSensitive(dev, "Manufacturer");
                    const char *mfg_str = (j_mfg && cJSON_IsString(j_mfg)) ? j_mfg->valuestring : "";

                    if (is_bt_trans || bta_str[0]) {
                        cJSON *d = cJSON_CreateObject();
                        cJSON_AddStringToObject(d, "id", id_str);
                        cJSON_AddStringToObject(d, "name", name_str);
                        cJSON_AddStringToObject(d, "model", model_str);
                        cJSON_AddStringToObject(d, "manufacturer", mfg_str);
                        cJSON_AddStringToObject(d, "bdaddr", bta_str);
                        cJSON_AddBoolToObject(d, "isBtTransport", is_bt_trans);

                        bool connected = false;
                        if (bta_str[0] && hosts_arr && cJSON_IsArray(hosts_arr)) {
                            cJSON *h = NULL;
                            cJSON_ArrayForEach(h, hosts_arr) {
                                cJSON *ha = cJSON_GetObjectItem(h, "addr");
                                cJSON *hc = cJSON_GetObjectItem(h, "connected");
                                if (ha && cJSON_IsString(ha) && strcasecmp(ha->valuestring, bta_str) == 0) {
                                    if (hc && cJSON_IsTrue(hc)) connected = true;
                                    break;
                                }
                            }
                        }
                        cJSON_AddBoolToObject(d, "connected", connected);
                        cJSON_AddItemToArray(devs_out, d);
                    }
                }
            }
            cJSON_Delete(dl_root);
        }
    }

    struct bt_inventory inv;
    if (load_bt_inventory(&inv) == 0) {
        for (int i = 0; i < inv.device_count; i++) {
            bool exists = false;
            cJSON *exist_item = NULL;
            cJSON_ArrayForEach(exist_item, devs_out) {
                cJSON *eid = cJSON_GetObjectItem(exist_item, "id");
                if (eid && cJSON_IsString(eid) && strcmp(eid->valuestring, inv.devices[i].id) == 0) {
                    exists = true;
                    cJSON *eaddr = cJSON_GetObjectItem(exist_item, "bdaddr");
                    if ((!eaddr || !eaddr->valuestring[0]) && inv.devices[i].bdaddr[0]) {
                        cJSON_ReplaceItemInObject(exist_item, "bdaddr", cJSON_CreateString(inv.devices[i].bdaddr));
                        bool connected = false;
                        if (hosts_arr && cJSON_IsArray(hosts_arr)) {
                            cJSON *h = NULL;
                            cJSON_ArrayForEach(h, hosts_arr) {
                                cJSON *ha = cJSON_GetObjectItem(h, "addr");
                                cJSON *hc = cJSON_GetObjectItem(h, "connected");
                                if (ha && cJSON_IsString(ha) && strcasecmp(ha->valuestring, inv.devices[i].bdaddr) == 0) {
                                    if (hc && cJSON_IsTrue(hc)) connected = true;
                                    break;
                                }
                            }
                        }
                        cJSON_ReplaceItemInObject(exist_item, "connected", cJSON_CreateBool(connected));
                    }
                    break;
                }
            }
            if (!exists) {
                cJSON *d = cJSON_CreateObject();
                cJSON_AddStringToObject(d, "id", inv.devices[i].id);
                cJSON_AddStringToObject(d, "name", inv.devices[i].name);
                cJSON_AddStringToObject(d, "model", "");
                cJSON_AddStringToObject(d, "manufacturer", "");
                cJSON_AddStringToObject(d, "bdaddr", inv.devices[i].bdaddr);
                cJSON_AddBoolToObject(d, "isBtTransport", 1);
                bool connected = false;
                if (inv.devices[i].bdaddr[0] && hosts_arr && cJSON_IsArray(hosts_arr)) {
                    cJSON *h = NULL;
                    cJSON_ArrayForEach(h, hosts_arr) {
                        cJSON *ha = cJSON_GetObjectItem(h, "addr");
                        cJSON *hc = cJSON_GetObjectItem(h, "connected");
                        if (ha && cJSON_IsString(ha) && strcasecmp(ha->valuestring, inv.devices[i].bdaddr) == 0) {
                            if (hc && cJSON_IsTrue(hc)) connected = true;
                            break;
                        }
                    }
                }
                cJSON_AddBoolToObject(d, "connected", connected);
                cJSON_AddItemToArray(devs_out, d);
            }
        }
    }

    cJSON_AddItemToObject(root, "devices", devs_out);
}

void render_bt_status_json(int fd) {
    if (!is_btstack_running()) {
        cJSON *res = cJSON_CreateObject();
        cJSON_AddBoolToObject(res, "ok", 1);
        cJSON_AddBoolToObject(res, "running", 0);
        cJSON_AddStringToObject(res, "state", "stopped");
        cJSON *host = cJSON_CreateObject();
        cJSON_AddBoolToObject(host, "connected", 0);
        cJSON_AddStringToObject(host, "addr", "");
        cJSON_AddStringToObject(host, "name", "");
        cJSON_AddBoolToObject(host, "pairing", 0);
        cJSON_AddItemToObject(res, "host", host);
        cJSON_AddItemToObject(res, "hosts", cJSON_CreateArray());
        attach_bt_devices_to_status(res);
        send_cjson_resp(fd, "200 OK", res);
        cJSON_Delete(res);
        return;
    }
    char raw[4096] = "";
    if (read_text("/tmp/codex_btstack_status.json", raw, sizeof(raw)) > 0 && raw[0] == '{') {
        cJSON *parsed = cJSON_Parse(raw);
        if (parsed) {
            cJSON_AddBoolToObject(parsed, "ok", 1);
            cJSON_AddBoolToObject(parsed, "running", 1);
            attach_bt_devices_to_status(parsed);
            send_cjson_resp(fd, "200 OK", parsed);
            cJSON_Delete(parsed);
            return;
        }
    }
    cJSON *res = cJSON_CreateObject();
    cJSON_AddBoolToObject(res, "ok", 1);
    cJSON_AddBoolToObject(res, "running", 1);
    cJSON_AddStringToObject(res, "state", "initializing");
    cJSON *host = cJSON_CreateObject();
    cJSON_AddBoolToObject(host, "connected", 0);
    cJSON_AddStringToObject(host, "addr", "");
    cJSON_AddStringToObject(host, "name", "");
    cJSON_AddBoolToObject(host, "pairing", 0);
    cJSON_AddItemToObject(res, "host", host);
    cJSON_AddItemToObject(res, "hosts", cJSON_CreateArray());
    attach_bt_devices_to_status(res);
    send_cjson_resp(fd, "200 OK", res);
    cJSON_Delete(res);
}

void render_bt_key_json(int fd, const struct request *req) {
    char key[64], action[32], err[128];
    form_value(req->body, "key", key, sizeof(key));
    if (!key[0]) json_string(req->body, "key", key, sizeof(key));
    form_value(req->body, "action", action, sizeof(action));
    if (!action[0]) json_string(req->body, "action", action, sizeof(action));
    if (!action[0]) {
        form_value(req->body, "event", action, sizeof(action));
        if (!action[0]) json_string(req->body, "event", action, sizeof(action));
    }
    if (!key[0]) {
        cJSON *res = cJSON_CreateObject();
        cJSON_AddBoolToObject(res, "ok", 0);
        cJSON_AddStringToObject(res, "error", "No key specified");
        send_cjson_resp(fd, "400 Bad Request", res);
        cJSON_Delete(res);
        return;
    }
    char target[64] = {0}, resolved_addr[32] = {0};
    form_value(req->body, "target", target, sizeof(target));
    if (!target[0]) json_string(req->body, "target", target, sizeof(target));
    if (!target[0]) {
        form_value(req->body, "bdaddr", target, sizeof(target));
        if (!target[0]) json_string(req->body, "bdaddr", target, sizeof(target));
    }
    if (!target[0]) {
        form_value(req->body, "deviceId", target, sizeof(target));
        if (!target[0]) json_string(req->body, "deviceId", target, sizeof(target));
    }
    if (target[0]) resolve_bt_target_addr(target, resolved_addr, sizeof(resolved_addr));

    char cmdbuf[256];
    cmdbuf[0] = 0;
    if (resolved_addr[0]) {
        snprintf(cmdbuf, sizeof(cmdbuf), "TARGET %s\n", resolved_addr);
    }
    char subcmd[128];
    if (strcasecmp(key, "release_all") == 0 || strcasecmp(action, "up") == 0 || strcasecmp(action, "release") == 0 || strcasecmp(action, "keyup") == 0) {
        if (strcasecmp(key, "release_all") == 0) {
            strcpy(subcmd, "RELEASE\n");
        } else {
            snprintf(subcmd, sizeof(subcmd), "KEYUP %s\n", key);
        }
    } else if (strcasecmp(action, "down") == 0 || strcasecmp(action, "press") == 0 || strcasecmp(action, "keydown") == 0) {
        snprintf(subcmd, sizeof(subcmd), "KEYDOWN %s\n", key);
    } else {
        snprintf(subcmd, sizeof(subcmd), "KEY %s\n", key);
    }
    strncat(cmdbuf, subcmd, sizeof(cmdbuf) - strlen(cmdbuf) - 1);

    if (write_bt_text_fifo(cmdbuf, err, sizeof(err)) != 0) {
        cJSON *res = cJSON_CreateObject();
        cJSON_AddBoolToObject(res, "ok", 0);
        cJSON_AddStringToObject(res, "error", err);
        send_cjson_resp(fd, "400 Bad Request", res);
        cJSON_Delete(res);
        return;
    }
    cJSON *res = cJSON_CreateObject();
    cJSON_AddBoolToObject(res, "ok", 1);
    cJSON_AddStringToObject(res, "key", key);
    if (action[0]) cJSON_AddStringToObject(res, "action", action);
    if (resolved_addr[0]) cJSON_AddStringToObject(res, "target", resolved_addr);
    send_cjson_resp(fd, "200 OK", res);
    cJSON_Delete(res);
}

void render_bt_pairing_json(int fd, const struct request *req) {
    char enable[16] = {0}, name[64] = {0}, device_id[64] = {0};
    form_value(req->body, "enable", enable, sizeof(enable));
    form_value(req->body, "name", name, sizeof(name));
    form_value(req->body, "deviceId", device_id, sizeof(device_id));
    if (!enable[0]) json_string(req->body, "enable", enable, sizeof(enable));
    if (!name[0]) json_string(req->body, "name", name, sizeof(name));
    if (!device_id[0]) json_string(req->body, "deviceId", device_id, sizeof(device_id));
    int on = (strcmp(enable, "1") == 0 || strcasecmp(enable, "true") == 0);
    if (!name[0]) strcpy(name, "Harmony Keyboard");

    if (on && device_id[0]) {
        FILE *pf = fopen("/tmp/bt_pending_pair_device", "w");
        if (pf) {
            fprintf(pf, "%s\n", device_id);
            fclose(pf);
        }
    } else if (!on) {
        unlink("/tmp/bt_pending_pair_device");
    }

    char cmd[128];
    if (on) {
        snprintf(cmd, sizeof(cmd), "pair_host_on %s\n", name);
    } else {
        strcpy(cmd, "pair_host_off\n");
    }
    codex_send_btstack_cmd(cmd);
    cJSON *res = cJSON_CreateObject();
    cJSON_AddBoolToObject(res, "ok", 1);
    cJSON_AddBoolToObject(res, "pairing", on);
    send_cjson_resp(fd, "200 OK", res);
    cJSON_Delete(res);
}

void render_bt_connect_json(int fd, const struct request *req) {
    char bdaddr[32] = {0};
    if (req && req->body) {
        form_value(req->body, "bdaddr", bdaddr, sizeof(bdaddr));
        if (!bdaddr[0]) json_string(req->body, "bdaddr", bdaddr, sizeof(bdaddr));
    }
    char cmd[128];
    if (bdaddr[0] && safe_bt_addr(bdaddr)) {
        snprintf(cmd, sizeof(cmd), "connect_host %s\n", bdaddr);
    } else {
        strcpy(cmd, "connect_host\n");
    }
    codex_send_btstack_cmd(cmd);
    cJSON *res = cJSON_CreateObject();
    cJSON_AddBoolToObject(res, "ok", 1);
    cJSON_AddStringToObject(res, "message", "Connect requested");
    send_cjson_resp(fd, "200 OK", res);
    cJSON_Delete(res);
}

void render_bt_disconnect_json(int fd, const struct request *req) {
    char bdaddr[32] = {0};
    if (req && req->body) {
        form_value(req->body, "bdaddr", bdaddr, sizeof(bdaddr));
        if (!bdaddr[0]) json_string(req->body, "bdaddr", bdaddr, sizeof(bdaddr));
    }
    char cmd[128];
    if (bdaddr[0] && safe_bt_addr(bdaddr)) {
        snprintf(cmd, sizeof(cmd), "disconnect_host %s\n", bdaddr);
    } else {
        strcpy(cmd, "disconnect_host\n");
    }
    codex_send_btstack_cmd(cmd);
    cJSON *res = cJSON_CreateObject();
    cJSON_AddBoolToObject(res, "ok", 1);
    cJSON_AddStringToObject(res, "message", "Disconnect requested");
    send_cjson_resp(fd, "200 OK", res);
    cJSON_Delete(res);
}

void render_bt_link_device_json(int fd, const struct request *req) {
    char device_id[64] = {0}, bdaddr[32] = {0};
    if (req && req->body) {
        form_value(req->body, "deviceId", device_id, sizeof(device_id));
        form_value(req->body, "bdaddr", bdaddr, sizeof(bdaddr));
        if (!device_id[0]) json_string(req->body, "deviceId", device_id, sizeof(device_id));
        if (!bdaddr[0]) json_string(req->body, "bdaddr", bdaddr, sizeof(bdaddr));
    }

    if (!device_id[0] || !bdaddr[0] || !safe_bt_addr(bdaddr)) {
        cJSON *res = cJSON_CreateObject();
        cJSON_AddBoolToObject(res, "ok", 0);
        cJSON_AddStringToObject(res, "error", "Invalid deviceId or MAC address");
        send_cjson_resp(fd, "400 Bad Request", res);
        cJSON_Delete(res);
        return;
    }

    struct bt_inventory inv;
    load_bt_inventory(&inv);
    int idx = find_bt_device_index(&inv, device_id);
    if (idx >= 0) {
        copy_text(inv.devices[idx].bdaddr, sizeof(inv.devices[idx].bdaddr), bdaddr);
        save_bt_inventory(&inv);
    } else {
        char msg[128];
        upsert_bt_device(device_id, device_id, "btkeyboard", bdaddr, msg, sizeof(msg));
    }

    char *dl_raw = read_file_alloc(DEVICE_LIST, MAX_RESOURCE_FILE, NULL);
    if (dl_raw) {
        cJSON *dl_root = cJSON_Parse(dl_raw);
        free(dl_raw);
        if (dl_root) {
            cJSON *dwf = cJSON_GetObjectItemCaseSensitive(dl_root, "DevicesWithFeatures");
            if (dwf && cJSON_IsArray(dwf)) {
                cJSON *item = NULL;
                bool updated = false;
                cJSON_ArrayForEach(item, dwf) {
                    cJSON *dev = cJSON_GetObjectItemCaseSensitive(item, "Device");
                    if (!dev) continue;
                    cJSON *j_id = cJSON_GetObjectItemCaseSensitive(dev, "Id-");
                    if (!j_id) j_id = cJSON_GetObjectItemCaseSensitive(dev, "Id");
                    if (!j_id) j_id = cJSON_GetObjectItemCaseSensitive(dev, "id");
                    char id_str[64] = {0};
                    if (j_id) {
                        if (cJSON_IsNumber(j_id)) snprintf(id_str, sizeof(id_str), "%ld", (long)j_id->valuedouble);
                        else if (cJSON_IsString(j_id) && j_id->valuestring) strncpy(id_str, j_id->valuestring, sizeof(id_str) - 1);
                    }
                    if (id_str[0] && strcmp(id_str, device_id) == 0) {
                        if (cJSON_GetObjectItemCaseSensitive(dev, "BTAddress")) {
                            cJSON_ReplaceItemInObject(dev, "BTAddress", cJSON_CreateString(bdaddr));
                        } else {
                            cJSON_AddStringToObject(dev, "BTAddress", bdaddr);
                        }
                        updated = true;
                        break;
                    }
                }
                if (updated) {
                    char *out = cJSON_Print(dl_root);
                    if (out) {
                        write_file_atomic(DEVICE_LIST, out, strlen(out));
                        free(out);
                    }
                }
            }
            cJSON_Delete(dl_root);
        }
    }

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "connect_host %s\n", bdaddr);
    codex_send_btstack_cmd(cmd);

    cJSON *res = cJSON_CreateObject();
    cJSON_AddBoolToObject(res, "ok", 1);
    cJSON_AddStringToObject(res, "message", "Linked device to Bluetooth address");
    cJSON_AddStringToObject(res, "deviceId", device_id);
    cJSON_AddStringToObject(res, "bdaddr", bdaddr);
    send_cjson_resp(fd, "200 OK", res);
    cJSON_Delete(res);
}

void render_bt_script_json(int fd, const struct request *req) {
    char *script = (char *)malloc(MAX_BT_SCRIPT_LEN);
    if (!script) {
        cJSON *res = cJSON_CreateObject();
        cJSON_AddBoolToObject(res, "ok", 0);
        cJSON_AddStringToObject(res, "error", "Out of memory");
        send_cjson_resp(fd, "500 Internal Server Error", res);
        cJSON_Delete(res);
        return;
    }
    char delay_str[16], reply[1024];
    form_value(req->body, "script", script, MAX_BT_SCRIPT_LEN);
    form_value(req->body, "delayMs", delay_str, sizeof(delay_str));
    if (!script[0]) json_string(req->body, "script", script, MAX_BT_SCRIPT_LEN);
    if (!delay_str[0]) json_string(req->body, "delayMs", delay_str, sizeof(delay_str));
    int delay_ms = atoi(delay_str);
    if (delay_ms <= 0) delay_ms = 35;
    char target[64] = {0}, resolved_addr[32] = {0};
    form_value(req->body, "target", target, sizeof(target));
    if (!target[0]) json_string(req->body, "target", target, sizeof(target));
    if (!target[0]) {
        form_value(req->body, "bdaddr", target, sizeof(target));
        if (!target[0]) json_string(req->body, "bdaddr", target, sizeof(target));
    }
    if (!target[0]) {
        form_value(req->body, "deviceId", target, sizeof(target));
        if (!target[0]) json_string(req->body, "deviceId", target, sizeof(target));
    }
    if (target[0]) resolve_bt_target_addr(target, resolved_addr, sizeof(resolved_addr));

    int rc = run_bt_saved_script("btkeyboard", resolved_addr, script, delay_ms, reply, sizeof(reply));
    free(script);
    cJSON *res = cJSON_CreateObject();
    cJSON_AddBoolToObject(res, "ok", rc == 0 ? 1 : 0);
    cJSON_AddStringToObject(res, "message", reply[0] ? reply : (rc == 0 ? "Script executed" : "Script failed"));
    if (resolved_addr[0]) cJSON_AddStringToObject(res, "target", resolved_addr);
    send_cjson_resp(fd, rc == 0 ? "200 OK" : "400 Bad Request", res);
    cJSON_Delete(res);
}

void render_bluetooth_text_status_json(int fd) {
    char raw[1024];
    if (read_text(BT_TEXT_STATUS, raw, sizeof(raw)) > 0 && raw[0] == '{') {
        cJSON *parsed = cJSON_Parse(raw);
        if (parsed) {
            cJSON *pitem = cJSON_GetObjectItemCaseSensitive(parsed, "pid");
            int pid = (pitem && cJSON_IsNumber(pitem)) ? pitem->valueint : 0;
            int alive = 0;
            if (pid <= 0) {
                alive = is_btstack_running();
            } else if (kill((pid_t)pid, 0) == 0 || errno == EPERM) {
                alive = 1;
            }
            if (alive) {
                send_cjson_resp(fd, "200 OK", parsed);
                cJSON_Delete(parsed);
                return;
            }
            cJSON *sitem = cJSON_GetObjectItemCaseSensitive(parsed, "sent");
            cJSON *skitem = cJSON_GetObjectItemCaseSensitive(parsed, "skipped");
            int sent = (sitem && cJSON_IsNumber(sitem)) ? sitem->valueint : 0;
            int skipped = (skitem && cJSON_IsNumber(skitem)) ? skitem->valueint : 0;
            cJSON_Delete(parsed);

            cJSON *resp = cJSON_CreateObject();
            cJSON_AddBoolToObject(resp, "ok", 1);
            cJSON_AddBoolToObject(resp, "runtime", 0);
            cJSON_AddStringToObject(resp, "state", "stale");
            cJSON_AddStringToObject(resp, "target", "");
            cJSON_AddNumberToObject(resp, "sent", sent);
            cJSON_AddNumberToObject(resp, "skipped", skipped);
            cJSON_AddStringToObject(resp, "error", "Bluetooth FIFO runtime status is stale; restart codex_btstack");
            send_cjson_resp(fd, "200 OK", resp);
            cJSON_Delete(resp);
            return;
        }
    }
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", 1);
    cJSON_AddBoolToObject(resp, "runtime", 0);
    cJSON_AddStringToObject(resp, "state", "missing");
    cJSON_AddStringToObject(resp, "target", "");
    cJSON_AddNumberToObject(resp, "sent", 0);
    cJSON_AddNumberToObject(resp, "skipped", 0);
    cJSON_AddStringToObject(resp, "error", "Bluetooth FIFO runtime (BTstack) is not running");
    send_cjson_resp(fd, "200 OK", resp);
    cJSON_Delete(resp);
}

void render_bt_sent_log_json(int fd) {
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", 1);
    cJSON *arr = cJSON_CreateArray();
    FILE *in = fopen("/tmp/bt_sent.log", "r");
    if (in) {
        char line[512];
        while (fgets(line, sizeof(line), in)) {
            trim_in_place(line);
            if (!line[0]) continue;
            cJSON_AddItemToArray(arr, cJSON_CreateString(line));
        }
        fclose(in);
    }
    cJSON_AddItemToObject(resp, "log", arr);
    send_cjson_resp(fd, "200 OK", resp);
    cJSON_Delete(resp);
}

void render_remote_mapping_json(int fd) {
    char target_addr[32] = "";
    cJSON *map_obj = NULL;
    char *raw = read_file_alloc(BT_REMOTE_MAP_FILE, 1048576, NULL);
    if (raw) {
        map_obj = cJSON_Parse(raw);
        free(raw);
        if (map_obj) {
            cJSON *rem = cJSON_GetObjectItemCaseSensitive(map_obj, "remote");
            cJSON *ba = rem ? cJSON_GetObjectItemCaseSensitive(rem, "bdaddr") : NULL;
            if (ba && cJSON_IsString(ba) && ba->valuestring) {
                strncpy(target_addr, ba->valuestring, sizeof(target_addr) - 1);
            }
        }
    }
    (void)target_addr;
    int running = 0, connected = 0;
    if (is_btstack_running()) {
        running = 1;
        char st_raw[1024];
        if (read_text("/tmp/codex_btstack_status.json", st_raw, sizeof(st_raw)) > 0) {
            cJSON *st = cJSON_Parse(st_raw);
            if (st) {
                cJSON *st_val = cJSON_GetObjectItemCaseSensitive(st, "state");
                if (st_val && cJSON_IsString(st_val) && strcmp(st_val->valuestring, "ready") == 0) {
                    connected = 1;
                }
                cJSON_Delete(st);
            }
        }
    }

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", 1);
    cJSON_AddBoolToObject(resp, "running", running ? 1 : 0);
    cJSON_AddBoolToObject(resp, "connected", connected ? 1 : 0);
    if (map_obj) {
        cJSON_AddItemToObject(resp, "mapping", map_obj);
    } else {
        cJSON *def_map = cJSON_CreateObject();
        cJSON *def_rem = cJSON_CreateObject();
        cJSON_AddStringToObject(def_rem, "name", "Homatics B25");
        cJSON_AddItemToObject(def_map, "remote", def_rem);
        cJSON_AddObjectToObject(def_map, "activities");
        cJSON_AddItemToObject(resp, "mapping", def_map);
    }
    send_cjson_resp(fd, "200 OK", resp);
    cJSON_Delete(resp);
}

void render_remote_mapping_save_json(int fd, const struct request *req) {
    const char *payload = NULL;
    char *alloc_buf = NULL;
    if (req->body && req->body[0] == '{') {
        payload = req->body;
    } else if (req->body) {
        alloc_buf = (char *)malloc(MAX_REQUEST_BODY);
        if (alloc_buf) {
            form_value(req->body, "json", alloc_buf, MAX_REQUEST_BODY);
            if (alloc_buf[0] == '{') payload = alloc_buf;
        }
    }
    cJSON *parsed = payload ? cJSON_Parse(payload) : NULL;
    if (!parsed) {
        if (alloc_buf) free(alloc_buf);
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "Invalid JSON payload");
        send_cjson_resp(fd, "400 Bad Request", err);
        cJSON_Delete(err);
        return;
    }
    char *out = cJSON_Print(parsed);
    cJSON_Delete(parsed);
    if (alloc_buf) free(alloc_buf);
    if (!out) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "Failed to format mappings");
        send_cjson_resp(fd, "500 Internal Server Error", err);
        cJSON_Delete(err);
        return;
    }
    FILE *mf = fopen(BT_REMOTE_MAP_FILE, "w");
    if (!mf) {
        free(out);
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "Failed to open mappings file for writing");
        send_cjson_resp(fd, "500 Internal Server Error", err);
        cJSON_Delete(err);
        return;
    }
    fputs(out, mf);
    fputc('\n', mf);
    fclose(mf);
    free(out);
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", 1);
    cJSON_AddStringToObject(resp, "message", "Remote mappings saved successfully");
    send_cjson_resp(fd, "200 OK", resp);
    cJSON_Delete(resp);
}

void render_remote_scan_json(int fd, const struct request *req) {
    if (!is_btstack_running()) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "BTstack service is not running. Check service status.");
        send_cjson_resp(fd, "400 Bad Request", err);
        cJSON_Delete(err);
        return;
    }

    unlink("/tmp/codex_btstack_scan.json");
    codex_send_btstack_cmd("scan\n");
    for (int i = 0; i < 85; i++) {
        usleep(100000); // 100ms
        char *raw = read_file_alloc("/tmp/codex_btstack_scan.json", 16384, NULL);
        if (raw) {
            cJSON *obj = cJSON_Parse(raw);
            free(raw);
            if (obj) {
                cJSON *sc = cJSON_GetObjectItemCaseSensitive(obj, "scanning");
                if (sc && cJSON_IsBool(sc) && !cJSON_IsTrue(sc)) {
                    send_cjson_resp(fd, "200 OK", obj);
                    cJSON_Delete(obj);
                    return;
                }
                cJSON_Delete(obj);
            }
        }
    }
    char *raw = read_file_alloc("/tmp/codex_btstack_scan.json", 16384, NULL);
    cJSON *obj = raw ? cJSON_Parse(raw) : NULL;
    free(raw);
    if (!obj) {
        obj = cJSON_CreateObject();
        cJSON_AddBoolToObject(obj, "ok", 1);
        cJSON_AddItemToObject(obj, "devices", cJSON_CreateArray());
    }
    send_cjson_resp(fd, "200 OK", obj);
    cJSON_Delete(obj);
}

void render_remote_pair_status_json(int fd) {
    if (!is_btstack_running()) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "state", "disabled");
        cJSON_AddStringToObject(err, "message", "BTstack mode is required for physical Bluetooth remotes.");
        send_cjson_resp(fd, "200 OK", err);
        cJSON_Delete(err);
        return;
    }

    char *raw = read_file_alloc("/tmp/codex_btstack_status.json", 1024, NULL);
    cJSON *stat_obj = raw ? cJSON_Parse(raw) : NULL;
    free(raw);

    cJSON *obj = cJSON_CreateObject();
    const char *state = "idle";
    const char *target = "";
    if (stat_obj) {
        cJSON *s = cJSON_GetObjectItemCaseSensitive(stat_obj, "state");
        if (s && cJSON_IsString(s)) state = s->valuestring;
        cJSON *t = cJSON_GetObjectItemCaseSensitive(stat_obj, "target");
        if (t && cJSON_IsString(t)) target = t->valuestring;
    }

    if (strcmp(state, "ready") == 0) {
        cJSON_AddStringToObject(obj, "state", "success");
        cJSON_AddStringToObject(obj, "message", "Remote connected and encrypted!");
        cJSON_AddStringToObject(obj, "bdaddr", target);
    } else if (strcmp(state, "pairing") == 0 || strcmp(state, "encrypting") == 0) {
        cJSON_AddStringToObject(obj, "state", "negotiating_smp");
        cJSON_AddStringToObject(obj, "message", "Authenticating and negotiating encryption keys...");
    } else if (strcmp(state, "disconnected") == 0) {
        cJSON_AddStringToObject(obj, "state", "failed");
        cJSON_AddStringToObject(obj, "message", "Remote disconnected");
    } else {
        cJSON_AddStringToObject(obj, "state", "idle");
        cJSON_AddStringToObject(obj, "message", "Idle");
        if (target[0]) cJSON_AddStringToObject(obj, "bdaddr", target);
    }

    if (stat_obj) cJSON_Delete(stat_obj);
    send_cjson_resp(fd, "200 OK", obj);
    cJSON_Delete(obj);
}

void render_remote_pair_json(int fd, const struct request *req) {
    char addr[32];
    form_value(req->body, "addr", addr, sizeof(addr));
    chomp(addr);
    if (!safe_bt_addr(addr)) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "Invalid Bluetooth address");
        send_cjson_resp(fd, "400 Bad Request", err);
        cJSON_Delete(err);
        return;
    }

    if (!is_btstack_running()) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "BTstack mode is required for physical Bluetooth remotes. Please enable BTstack on the System page.");
        send_cjson_resp(fd, "400 Bad Request", err);
        cJSON_Delete(err);
        return;
    }

    FILE *tf = fopen("/data/codex/bt_remote_target", "w");
    if (tf) {
        fprintf(tf, "%s\n", addr);
        fclose(tf);
    }
    char pcmd[64]; snprintf(pcmd, sizeof(pcmd), "pair %s\n", addr); codex_send_btstack_cmd(pcmd);
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", 1);
    cJSON_AddStringToObject(resp, "message", "Pairing initiated. Please hold pairing buttons on remote.");
    send_cjson_resp(fd, "200 OK", resp);
    cJSON_Delete(resp);
}

void render_bluetooth_call_json(int fd, const struct request *req) {
    char action[40], type[40], bdaddr[40], pin[40], code[MAX_BT_SEQUENCE_BODY], timeout_text[24], gap_text[24];
    char reply[4096], params[512], detected_addr[32], connection_raw[2048], esc_name[160], cmd[1024];
    char name[128];
    const char *cmd_name = NULL;
    int timeout, gap_ms, auto_detected_addr = 0, command_rc = 0, call_timeout = 5;

    form_value(req->body, "action", action, sizeof(action));
    form_value(req->body, "type", type, sizeof(type));
    form_value(req->body, "bdaddr", bdaddr, sizeof(bdaddr));
    form_value(req->body, "name", name, sizeof(name));
    form_value(req->body, "pin", pin, sizeof(pin));
    form_value(req->body, "timeout", timeout_text, sizeof(timeout_text));
    form_value(req->body, "gapMs", gap_text, sizeof(gap_text));
    if (req->body_truncated) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "Bluetooth request body is too large");
        send_cjson_resp(fd, "413 Payload Too Large", err);
        cJSON_Delete(err);
        return;
    }
    form_value(req->body, "code", code, sizeof(code));

    chomp(action); chomp(type); chomp(bdaddr); chomp(name); chomp(pin);
    if (!type[0]) strcpy(type, "btkeyboard");
    if (!name[0]) strcpy(name, "Harmony Hub Keyboard");
    timeout = atoi(timeout_text);
    if (timeout < 1) timeout = 2;
    if (timeout > 20) timeout = 20;
    gap_ms = atoi(gap_text);
    if (gap_ms < 15) gap_ms = 35;
    if (gap_ms > 5000) gap_ms = 5000;
    params[0] = 0;
    detected_addr[0] = 0;
    connection_raw[0] = 0;

    if (strcmp(action, "backend_status") == 0) {
        char status_raw[1024] = "";
        FILE *sf = fopen("/tmp/codex_btstack_status.json", "r");
        if (sf) {
            size_t n = fread(status_raw, 1, sizeof(status_raw) - 1, sf);
            status_raw[n] = 0;
            fclose(sf);
        }
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddBoolToObject(resp, "ok", 1);
        cJSON_AddStringToObject(resp, "backend", is_btstack_running() ? "btstack" : "stopped");
        cJSON_AddStringToObject(resp, "statusJson", status_raw[0] ? status_raw : (is_btstack_running() ? "running" : "not running"));
        send_cjson_resp(fd, "200 OK", resp);
        cJSON_Delete(resp);
        return;
    } else if (strcmp(action, "backend_switch") == 0) {
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddBoolToObject(resp, "ok", 1);
        cJSON_AddStringToObject(resp, "backend", "btstack");
        cJSON_AddStringToObject(resp, "output", "BTstack is the permanent Bluetooth engine (BlueZ 4 is disabled).");
        send_cjson_resp(fd, "200 OK", resp);
        cJSON_Delete(resp);
        return;
    } else if (strcmp(action, "adapter_status") == 0) {
        reply[0] = 0;
        if (is_btstack_running()) {
            char st_raw[1024] = "";
            read_text("/tmp/codex_btstack_status.json", st_raw, sizeof(st_raw));
            snprintf(reply, sizeof(reply),
                "--- BTstack Active ---\n"
                "Daemon: codex_btstack (MIPS32)\n"
                "Profile: BLE HID Central + Classic BR/EDR HID Device\n"
                "Status:\n%s",
                st_raw[0] ? st_raw : "no status");
        } else {
            run_cmd("echo '--- adapter ---'; hciconfig hci0 -a 2>&1; echo; echo '--- connections ---'; hcitool con 2>&1", reply, sizeof(reply));
        }
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddBoolToObject(resp, "ok", 1);
        cJSON_AddStringToObject(resp, "action", action);
        cJSON_AddStringToObject(resp, "cmd", "adapter_status");
        cJSON_AddStringToObject(resp, "params", "");
        cJSON_AddStringToObject(resp, "responseRaw", reply[0] ? reply : "no response");
        send_cjson_resp(fd, "200 OK", resp);
        cJSON_Delete(resp);
        return;
    } else if (strcmp(action, "pairing_on") == 0) {
        if (!safe_bt_name(name)) {
            cJSON *err = cJSON_CreateObject();
            cJSON_AddBoolToObject(err, "ok", 0);
            cJSON_AddStringToObject(err, "error", "invalid Bluetooth display name");
            send_cjson_resp(fd, "400 Bad Request", err);
            cJSON_Delete(err);
            return;
        }
        reply[0] = 0;
        if (is_btstack_running()) {
            char cmdbuf[128];
            snprintf(cmdbuf, sizeof(cmdbuf), "pair_host_on %s\n", name);
            codex_send_btstack_cmd(cmdbuf);
            snprintf(reply, sizeof(reply), "--- enabling BTstack keyboard pairing mode as '%s' ---\nDiscoverable and Connectable enabled in BTstack Classic HID Device", name);
        } else {
            snprintf(reply, sizeof(reply), "Bluetooth engine (BTstack) is not running");
        }
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddBoolToObject(resp, "ok", 1);
        cJSON_AddStringToObject(resp, "action", action);
        cJSON_AddStringToObject(resp, "cmd", "pairing_on");
        cJSON_AddStringToObject(resp, "params", name);
        cJSON_AddStringToObject(resp, "responseRaw", reply[0] ? reply : "no response");
        send_cjson_resp(fd, "200 OK", resp);
        cJSON_Delete(resp);
        return;
    } else if (strcmp(action, "pairing_off") == 0) {
        reply[0] = 0;
        if (is_btstack_running()) {
            codex_send_btstack_cmd("pair_host_off\n");
            snprintf(reply, sizeof(reply), "--- disabling BTstack discoverable mode ---\nPairing mode disabled in BTstack Classic HID Device");
        } else {
            snprintf(reply, sizeof(reply), "Bluetooth engine (BTstack) is not running");
        }
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddBoolToObject(resp, "ok", 1);
        cJSON_AddStringToObject(resp, "action", action);
        cJSON_AddStringToObject(resp, "cmd", "pairing_off");
        cJSON_AddStringToObject(resp, "params", "");
        cJSON_AddStringToObject(resp, "responseRaw", reply[0] ? reply : "no response");
        send_cjson_resp(fd, "200 OK", resp);
        cJSON_Delete(resp);
        return;
    } else if (strcmp(action, "scan") == 0) {
        if (is_btstack_running()) {
            codex_send_btstack_cmd("scan\n");
            snprintf(reply, sizeof(reply), "BTstack BLE scan started; see Physical Bluetooth Remote page for discovered devices.");
            command_rc = 0;
        } else {
            snprintf(reply, sizeof(reply), "Bluetooth engine (BTstack) is not running");
            command_rc = -1;
        }
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddBoolToObject(resp, "ok", command_rc == 0 ? 1 : 0);
        cJSON_AddStringToObject(resp, "action", action);
        cJSON_AddStringToObject(resp, "cmd", "scan");
        cJSON_AddStringToObject(resp, "params", "");
        cJSON_AddNumberToObject(resp, "exitCode", command_rc);
        cJSON_AddStringToObject(resp, "responseRaw", reply);
        send_cjson_resp(fd, command_rc == 0 ? "200 OK" : "502 Bad Gateway", resp);
        cJSON_Delete(resp);
        return;
    } else if (strcmp(action, "classic_scan") == 0) {
        reply[0] = 0;
        run_cmd("hcitool scan 2>&1", reply, sizeof(reply));
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddBoolToObject(resp, "ok", 1);
        cJSON_AddStringToObject(resp, "action", action);
        cJSON_AddStringToObject(resp, "cmd", "hcitool scan");
        cJSON_AddStringToObject(resp, "params", "");
        cJSON_AddStringToObject(resp, "responseRaw", reply[0] ? reply : "no response");
        send_cjson_resp(fd, "200 OK", resp);
        cJSON_Delete(resp);
        return;
    } else {
        if (!bt_type_allowed(type)) {
            cJSON *err = cJSON_CreateObject();
            cJSON_AddBoolToObject(err, "ok", 0);
            cJSON_AddStringToObject(err, "error", "unsupported Bluetooth HID type");
            send_cjson_resp(fd, "400 Bad Request", err);
            cJSON_Delete(err);
            return;
        }
        if (!bdaddr[0] && (strcmp(action, "report") == 0 || strcmp(action, "reportseq") == 0 || strcmp(action, "status") == 0 || strcmp(action, "disconnect") == 0)) {
            auto_detected_addr = detect_connected_bt_addr(detected_addr, sizeof(detected_addr), connection_raw, sizeof(connection_raw));
            if (auto_detected_addr) copy_text(bdaddr, sizeof(bdaddr), detected_addr);
        }
        if (!bdaddr[0] && strcmp(action, "status") == 0) {
            strcpy(bdaddr, "00:00:00:00:00:00");
        }
        if (!bdaddr[0] && (strcmp(action, "report") == 0 || strcmp(action, "reportseq") == 0)) {
            cJSON *err = cJSON_CreateObject();
            cJSON_AddBoolToObject(err, "ok", 0);
            cJSON_AddStringToObject(err, "error", "no connected Bluetooth target found; pair the target, click Adapter Status, or enter the paired target address");
            send_cjson_resp(fd, "400 Bad Request", err);
            cJSON_Delete(err);
            return;
        }
        if (!safe_bt_addr(bdaddr) || !safe_bt_pin(pin)) {
            cJSON *err = cJSON_CreateObject();
            cJSON_AddBoolToObject(err, "ok", 0);
            cJSON_AddStringToObject(err, "error", "invalid Bluetooth address or PIN");
            send_cjson_resp(fd, "400 Bad Request", err);
            cJSON_Delete(err);
            return;
        }
        if (strcmp(action, "disconnect") == 0) {
            unlink(BT_TARGET_FILE);
            if (is_btstack_running()) {
                codex_send_btstack_cmd("disconnect_host\n");
                cJSON *resp = cJSON_CreateObject();
                cJSON_AddBoolToObject(resp, "ok", 1);
                cJSON_AddStringToObject(resp, "action", action);
                cJSON_AddStringToObject(resp, "cmd", "disconnect_host");
                cJSON_AddStringToObject(resp, "params", "");
                cJSON_AddStringToObject(resp, "responseRaw", "BTstack host disconnect requested");
                send_cjson_resp(fd, "200 OK", resp);
                cJSON_Delete(resp);
                return;
            }
        } else if (strcmp(action, "connect") == 0 || strcmp(action, "status") == 0 ||
            strcmp(action, "report") == 0 || strcmp(action, "reportseq") == 0) {
            save_bthid_target(type, bdaddr);
        }

        cJSON *po = cJSON_CreateObject();
        cJSON_AddStringToObject(po, "type", type);
        cJSON_AddStringToObject(po, "bdaddr", bdaddr);

        if (strcmp(action, "status") == 0) {
            cmd_name = "bthid.status";
        } else if (strcmp(action, "connect") == 0) {
            cmd_name = "bthid.connect";
            if (pin[0]) cJSON_AddStringToObject(po, "pin", pin);
        } else if (strcmp(action, "disconnect") == 0) {
            cmd_name = "bthid.disconnect";
        } else if (strcmp(action, "report") == 0 || strcmp(action, "reportseq") == 0) {
            cmd_name = "bthid.report";
        }
        char *postr = cJSON_PrintUnformatted(po);
        snprintf(params, sizeof(params), "%s", postr ? postr : "{}");
        free(postr);
        cJSON_Delete(po);
        call_timeout = 8;
    }

    if (!cmd_name) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddBoolToObject(err, "ok", 0);
        cJSON_AddStringToObject(err, "error", "unknown Bluetooth action");
        send_cjson_resp(fd, "400 Bad Request", err);
        cJSON_Delete(err);
        return;
    }

    reply[0] = 0;
    if (strcmp(action, "report") == 0 || strcmp(action, "reportseq") == 0) {
        char *seq = NULL, *line, *save;
        size_t seq_len = 0, seq_cap = 0;
        int sent_keys = 0;
        char report_err[160];
        if (strcmp(action, "report") == 0) {
            if (bt_sequence_add_code(code, &seq, &seq_len, &seq_cap, &sent_keys, report_err, sizeof(report_err)) != 0) {
                free(seq);
                cJSON *err = cJSON_CreateObject();
                cJSON_AddBoolToObject(err, "ok", 0);
                cJSON_AddStringToObject(err, "error", report_err);
                send_cjson_resp(fd, "400 Bad Request", err);
                cJSON_Delete(err);
                return;
            }
        } else {
            line = strtok_r(code, "\n", &save);
            while (line) {
                if (bt_sequence_add_code(line, &seq, &seq_len, &seq_cap, &sent_keys, report_err, sizeof(report_err)) != 0) {
                    free(seq);
                    cJSON *err = cJSON_CreateObject();
                    cJSON_AddBoolToObject(err, "ok", 0);
                    cJSON_AddStringToObject(err, "error", report_err);
                    send_cjson_resp(fd, "400 Bad Request", err);
                    cJSON_Delete(err);
                    return;
                }
                line = strtok_r(NULL, "\n", &save);
            }
        }
        if (!seq || sent_keys <= 0) {
            free(seq);
            cJSON *err = cJSON_CreateObject();
            cJSON_AddBoolToObject(err, "ok", 0);
            cJSON_AddStringToObject(err, "error", "no Bluetooth keyboard reports to send");
            send_cjson_resp(fd, "400 Bad Request", err);
            cJSON_Delete(err);
            return;
        }
        if (is_btstack_running()) {
            char bterr[128];
            if (write_bt_text_fifo(seq, bterr, sizeof(bterr)) == 0) {
                free(seq);
                cJSON *resp = cJSON_CreateObject();
                cJSON_AddBoolToObject(resp, "ok", 1);
                cJSON_AddStringToObject(resp, "action", action);
                cJSON_AddStringToObject(resp, "cmd", cmd_name);
                cJSON_AddStringToObject(resp, "params", params);
                cJSON_AddNumberToObject(resp, "exitCode", 0);
                cJSON_AddStringToObject(resp, "responseRaw", "sent via BTstack");
                send_cjson_resp(fd, "200 OK", resp);
                cJSON_Delete(resp);
                return;
            }
            snprintf(reply, sizeof(reply), "BTstack write failed: %s", bterr[0] ? bterr : "unknown error");
            command_rc = -1;
        } else {
            snprintf(reply, sizeof(reply), "Bluetooth engine (BTstack) is not running");
            command_rc = -1;
        }
        free(seq);
    } else {
        if (strcmp(action, "connect") == 0 && is_btstack_running()) {
            char cmd_line[64];
            snprintf(cmd_line, sizeof(cmd_line), "connect_host %s\n", bdaddr[0] ? bdaddr : "");
            codex_send_btstack_cmd(cmd_line);
            snprintf(reply, sizeof(reply), "{\"ok\":true,\"state\":\"connecting\"}");
            command_rc = 0;
        } else if (strcmp(action, "status") == 0 && is_btstack_running()) {
            char bts[512] = "";
            read_text(BT_TEXT_STATUS, bts, sizeof(bts));
            snprintf(reply, sizeof(reply), "%s", bts[0] ? bts : "{\"ok\":true,\"runtime\":true,\"state\":\"listening\"}");
            command_rc = 0;
        } else {
            snprintf(reply, sizeof(reply), "Bluetooth engine (BTstack) is not running or action not supported");
            command_rc = -1;
        }
    }

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", command_rc == 0 ? 1 : 0);
    cJSON_AddStringToObject(resp, "action", action);
    cJSON_AddStringToObject(resp, "cmd", cmd_name);
    cJSON_AddStringToObject(resp, "params", params);
    cJSON_AddNumberToObject(resp, "exitCode", command_rc);
    cJSON_AddStringToObject(resp, "responseRaw", reply[0] ? reply : "no response");
    if (auto_detected_addr) {
        cJSON_AddStringToObject(resp, "detectedAddress", detected_addr);
        cJSON_AddStringToObject(resp, "connectionRaw", connection_raw);
    }
    send_cjson_resp(fd, command_rc == 0 ? "200 OK" : "502 Bad Gateway", resp);
    cJSON_Delete(resp);
}

int safe_bt_store_id(const char *s) {
    const unsigned char *p = (const unsigned char *)s;
    size_t n = strlen(s);
    if (n == 0 || n > 36) return 0;
    while (*p) {
        if (!isalnum(*p) && *p != '_' && *p != '-') return 0;
        p++;
    }
    return 1;
}

int safe_bt_script_text(const char *s) {
    const unsigned char *p = (const unsigned char *)s;
    size_t n = strlen(s);
    if (n == 0 || n >= MAX_BT_SCRIPT_LEN) return 0;
    while (*p) {
        if (*p < 32 && *p != '\n' && *p != '\r' && *p != '\t') return 0;
        if (*p == 127) return 0;
        p++;
    }
    return 1;
}

static void make_bt_device_id(char *out, size_t outlen) {
    snprintf(out, outlen, "bt_%ld_%d", (long)time(NULL), (int)(getpid() % 10000));
}

int load_bt_inventory(struct bt_inventory *inv) {
    memset(inv, 0, sizeof(*inv));
    char *raw = read_file_alloc(BT_DEVICE_STORE, MAX_RESOURCE_FILE, NULL);
    if (!raw) return 0;
    cJSON *root = cJSON_Parse(raw);
    free(raw);
    if (!root) return 0;

    cJSON *devices = cJSON_GetObjectItemCaseSensitive(root, "devices");
    if (devices && cJSON_IsArray(devices)) {
        cJSON *dev_item = NULL;
        cJSON_ArrayForEach(dev_item, devices) {
            if (inv->device_count >= MAX_BT_DEVICES) break;
            cJSON *jid = cJSON_GetObjectItemCaseSensitive(dev_item, "id");
            cJSON *jname = cJSON_GetObjectItemCaseSensitive(dev_item, "name");
            if (!jid || !cJSON_IsString(jid) || !jid->valuestring[0]) continue;
            if (!jname || !cJSON_IsString(jname) || !jname->valuestring[0]) continue;

            struct bt_saved_device *dev = &inv->devices[inv->device_count];
            copy_text(dev->id, sizeof(dev->id), jid->valuestring);
            copy_text(dev->name, sizeof(dev->name), jname->valuestring);

            cJSON *jtype = cJSON_GetObjectItemCaseSensitive(dev_item, "type");
            if (jtype && cJSON_IsString(jtype) && jtype->valuestring[0]) {
                copy_text(dev->type, sizeof(dev->type), jtype->valuestring);
            } else {
                strcpy(dev->type, "btkeyboard");
            }

            cJSON *jaddr = cJSON_GetObjectItemCaseSensitive(dev_item, "bdaddr");
            if (jaddr && cJSON_IsString(jaddr) && jaddr->valuestring) {
                copy_text(dev->bdaddr, sizeof(dev->bdaddr), jaddr->valuestring);
            }

            cJSON *cmds = cJSON_GetObjectItemCaseSensitive(dev_item, "commands");
            if (cmds && cJSON_IsArray(cmds)) {
                cJSON *citem = NULL;
                cJSON_ArrayForEach(citem, cmds) {
                    if (dev->command_count >= MAX_BT_COMMANDS) break;
                    cJSON *cname = cJSON_GetObjectItemCaseSensitive(citem, "name");
                    cJSON *cscript = cJSON_GetObjectItemCaseSensitive(citem, "script");
                    if (cname && cJSON_IsString(cname) && cname->valuestring[0] &&
                        cscript && cJSON_IsString(cscript) && cscript->valuestring[0]) {
                        struct bt_saved_command *cmd = &dev->commands[dev->command_count];
                        copy_text(cmd->name, sizeof(cmd->name), cname->valuestring);
                        copy_text(cmd->script, sizeof(cmd->script), cscript->valuestring);
                        cJSON *jdel = cJSON_GetObjectItemCaseSensitive(citem, "delayMs");
                        cmd->delay_ms = (jdel && cJSON_IsNumber(jdel)) ? jdel->valueint : 35;
                        if (cmd->delay_ms < 15) cmd->delay_ms = 35;
                        if (cmd->delay_ms > 5000) cmd->delay_ms = 5000;
                        dev->command_count++;
                    }
                }
            }
            inv->device_count++;
        }
    }
    cJSON_Delete(root);
    return 0;
}

int save_bt_inventory(const struct bt_inventory *inv) {
    cJSON *root = cJSON_CreateObject();
    if (!root) return -1;
    cJSON_AddNumberToObject(root, "version", 1);
    cJSON *devs_arr = cJSON_CreateArray();

    int i, j;
    for (i = 0; i < inv->device_count; i++) {
        const struct bt_saved_device *dev = &inv->devices[i];
        cJSON *d = cJSON_CreateObject();
        cJSON_AddStringToObject(d, "id", dev->id);
        cJSON_AddStringToObject(d, "name", dev->name);
        cJSON_AddStringToObject(d, "type", dev->type);
        cJSON_AddStringToObject(d, "bdaddr", dev->bdaddr);

        cJSON *cmds_arr = cJSON_CreateArray();
        for (j = 0; j < dev->command_count; j++) {
            const struct bt_saved_command *cmd = &dev->commands[j];
            cJSON *c = cJSON_CreateObject();
            cJSON_AddStringToObject(c, "name", cmd->name);
            cJSON_AddNumberToObject(c, "delayMs", cmd->delay_ms);
            cJSON_AddStringToObject(c, "script", cmd->script);
            cJSON_AddItemToArray(cmds_arr, c);
        }
        cJSON_AddItemToObject(d, "commands", cmds_arr);
        cJSON_AddItemToArray(devs_arr, d);
    }
    cJSON_AddItemToObject(root, "devices", devs_arr);

    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!out) return -1;

    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s.new", BT_DEVICE_STORE);
    FILE *f = fopen(tmp, "wb");
    if (!f) {
        free(out);
        return -1;
    }
    fputs(out, f);
    fputc('\n', f);
    free(out);
    if (fclose(f) != 0) {
        unlink(tmp);
        return -1;
    }
    if (rename(tmp, BT_DEVICE_STORE) != 0) {
        unlink(tmp);
        return -1;
    }
    chmod(BT_DEVICE_STORE, 0644);
    sync();
    return 0;
}

int find_bt_device_index(const struct bt_inventory *inv, const char *device_id) {
    int i;
    for (i = 0; i < inv->device_count; i++) {
        if (strcmp(inv->devices[i].id, device_id) == 0) return i;
    }
    return -1;
}

int find_bt_command_index(const struct bt_saved_device *dev, const char *name) {
    int i;
    for (i = 0; i < dev->command_count; i++) {
        if (strcmp(dev->commands[i].name, name) == 0) return i;
    }
    return -1;
}

int upsert_bt_device(const char *device_id, const char *name, const char *type, const char *bdaddr, char *msg, size_t msglen) {
    struct bt_inventory inv;
    int idx;
    if (!safe_label(name) || !bt_type_allowed(type) || !safe_bt_addr(bdaddr)) {
        snprintf(msg, msglen, "Bluetooth device needs a name, supported keyboard type, and address.");
        return -1;
    }
    load_bt_inventory(&inv);
    if (device_id && device_id[0]) {
        if (!safe_bt_store_id(device_id)) {
            snprintf(msg, msglen, "Invalid Bluetooth device id.");
            return -1;
        }
        idx = find_bt_device_index(&inv, device_id);
        if (idx < 0) {
            snprintf(msg, msglen, "Bluetooth device %s was not found.", device_id);
            return -1;
        }
    } else {
        if (inv.device_count >= MAX_BT_DEVICES) {
            snprintf(msg, msglen, "Bluetooth device limit reached.");
            return -1;
        }
        idx = inv.device_count++;
        memset(&inv.devices[idx], 0, sizeof(inv.devices[idx]));
        make_bt_device_id(inv.devices[idx].id, sizeof(inv.devices[idx].id));
    }
    copy_text(inv.devices[idx].name, sizeof(inv.devices[idx].name), name);
    copy_text(inv.devices[idx].type, sizeof(inv.devices[idx].type), type);
    copy_text(inv.devices[idx].bdaddr, sizeof(inv.devices[idx].bdaddr), bdaddr);
    backup_settings();
    if (save_bt_inventory(&inv) != 0) {
        snprintf(msg, msglen, "Failed to save Bluetooth devices.");
        return -1;
    }
    save_bthid_target(type, bdaddr);
    snprintf(msg, msglen, "Saved Bluetooth device %s.", name);
    return 0;
}

int delete_bt_device(const char *device_id, char *msg, size_t msglen) {
    struct bt_inventory inv;
    int idx, i;
    if (!safe_bt_store_id(device_id)) {
        snprintf(msg, msglen, "Invalid Bluetooth device id.");
        return -1;
    }
    load_bt_inventory(&inv);
    idx = find_bt_device_index(&inv, device_id);
    if (idx < 0) {
        snprintf(msg, msglen, "Bluetooth device %s was not found.", device_id);
        return -1;
    }
    for (i = idx; i + 1 < inv.device_count; i++) inv.devices[i] = inv.devices[i + 1];
    inv.device_count--;
    backup_settings();
    if (save_bt_inventory(&inv) != 0) {
        snprintf(msg, msglen, "Failed to delete Bluetooth device.");
        return -1;
    }
    snprintf(msg, msglen, "Deleted Bluetooth device %s.", device_id);
    return 0;
}

int upsert_bt_command(const char *device_id, const char *old_name, const char *name, const char *script, int delay_ms, char *msg, size_t msglen) {
    struct bt_inventory inv;
    struct bt_saved_device *dev;
    int didx, cidx;
    if (!safe_bt_store_id(device_id) || !safe_label(name) || !safe_bt_script_text(script)) {
        snprintf(msg, msglen, "Bluetooth command needs a saved device, command name, and script.");
        return -1;
    }
    if (delay_ms < 15) delay_ms = 35;
    if (delay_ms > 5000) delay_ms = 5000;
    load_bt_inventory(&inv);
    didx = find_bt_device_index(&inv, device_id);
    if (didx < 0) {
        snprintf(msg, msglen, "Bluetooth device %s was not found.", device_id);
        return -1;
    }
    dev = &inv.devices[didx];
    if (old_name && old_name[0]) {
        if (!safe_label(old_name)) {
            snprintf(msg, msglen, "Invalid old Bluetooth command name.");
            return -1;
        }
        cidx = find_bt_command_index(dev, old_name);
        if (cidx < 0) {
            snprintf(msg, msglen, "Bluetooth command %s was not found.", old_name);
            return -1;
        }
        if (strcmp(old_name, name) != 0 && find_bt_command_index(dev, name) >= 0) {
            snprintf(msg, msglen, "Bluetooth command %s already exists.", name);
            return -1;
        }
    } else {
        if (find_bt_command_index(dev, name) >= 0) {
            snprintf(msg, msglen, "Bluetooth command %s already exists.", name);
            return -1;
        }
        if (dev->command_count >= MAX_BT_COMMANDS) {
            snprintf(msg, msglen, "Bluetooth command limit reached for this device.");
            return -1;
        }
        cidx = dev->command_count++;
        memset(&dev->commands[cidx], 0, sizeof(dev->commands[cidx]));
    }
    copy_text(dev->commands[cidx].name, sizeof(dev->commands[cidx].name), name);
    copy_text(dev->commands[cidx].script, sizeof(dev->commands[cidx].script), script);
    dev->commands[cidx].delay_ms = delay_ms;
    backup_settings();
    if (save_bt_inventory(&inv) != 0) {
        snprintf(msg, msglen, "Failed to save Bluetooth command.");
        return -1;
    }
    snprintf(msg, msglen, "Saved Bluetooth command %s.", name);
    return 0;
}

int delete_bt_command(const char *device_id, const char *name, char *msg, size_t msglen) {
    struct bt_inventory inv;
    struct bt_saved_device *dev;
    int didx, cidx, i;
    if (!safe_bt_store_id(device_id) || !safe_label(name)) {
        snprintf(msg, msglen, "Invalid Bluetooth command request.");
        return -1;
    }
    load_bt_inventory(&inv);
    didx = find_bt_device_index(&inv, device_id);
    if (didx < 0) {
        snprintf(msg, msglen, "Bluetooth device %s was not found.", device_id);
        return -1;
    }
    dev = &inv.devices[didx];
    cidx = find_bt_command_index(dev, name);
    if (cidx < 0) {
        snprintf(msg, msglen, "Bluetooth command %s was not found.", name);
        return -1;
    }
    for (i = cidx; i + 1 < dev->command_count; i++) dev->commands[i] = dev->commands[i + 1];
    dev->command_count--;
    backup_settings();
    if (save_bt_inventory(&inv) != 0) {
        snprintf(msg, msglen, "Failed to delete Bluetooth command.");
        return -1;
    }
    snprintf(msg, msglen, "Deleted Bluetooth command %s.", name);
    return 0;
}

int send_bt_saved_command(const char *device_id, const char *name, char *msg, size_t msglen) {
    struct bt_inventory inv;
    struct bt_saved_device *dev;
    struct bt_saved_command *cmd;
    int didx, cidx, rc;
    char reply[8192];
    if (!safe_bt_store_id(device_id) || !safe_label(name)) {
        snprintf(msg, msglen, "Invalid Bluetooth command request.");
        return -1;
    }
    load_bt_inventory(&inv);
    didx = find_bt_device_index(&inv, device_id);
    if (didx < 0) {
        snprintf(msg, msglen, "Bluetooth device %s was not found.", device_id);
        return -1;
    }
    dev = &inv.devices[didx];
    cidx = find_bt_command_index(dev, name);
    if (cidx < 0) {
        snprintf(msg, msglen, "Bluetooth command %s was not found.", name);
        return -1;
    }
    cmd = &dev->commands[cidx];
    reply[0] = 0;
    rc = run_bt_saved_script(dev->type, dev->bdaddr, cmd->script, cmd->delay_ms, reply, sizeof(reply));
    snprintf(msg, msglen, "%s Bluetooth command %s on %s. %s",
        rc == 0 ? "Sent" : "Failed to send", name, dev->name, reply[0] ? reply : "");
    return rc;
}

const char *bt_type_label(const char *type) {
    if (strcmp(type, "btkeyboard-nexus") == 0) return "Nexus keyboard";
    if (strcmp(type, "fire") == 0) return "Fire TV / media keys";
    if (strcmp(type, "ps3") == 0) return "PlayStation 3";
    if (strcmp(type, "wii") == 0) return "Nintendo Wii";
    return "Standard keyboard";
}

void handle_bt_device(int fd, const struct request *req) {
    char device_id[64], name[128], type[40], bdaddr[32], msg[512];
    form_value(req->body, "deviceId", device_id, sizeof(device_id));
    form_value(req->body, "name", name, sizeof(name));
    form_value(req->body, "type", type, sizeof(type));
    form_value(req->body, "bdaddr", bdaddr, sizeof(bdaddr));
    if (!type[0]) strcpy(type, "btkeyboard");
    upsert_bt_device(device_id, name, type, bdaddr, msg, sizeof(msg));
    render_page(fd, req, msg);
}

void handle_bt_delete_device(int fd, const struct request *req) {
    char device_id[64], msg[512];
    form_value(req->body, "deviceId", device_id, sizeof(device_id));
    delete_bt_device(device_id, msg, sizeof(msg));
    render_page(fd, req, msg);
}

void handle_bt_command(int fd, const struct request *req) {
    char device_id[64], old_name[128], name[128], delay_text[24], msg[512];
    char *script;
    int delay_ms;
    if (req->body_truncated) {
        render_page(fd, req, "Bluetooth script was too large.");
        return;
    }
    form_value(req->body, "deviceId", device_id, sizeof(device_id));
    form_value(req->body, "oldName", old_name, sizeof(old_name));
    form_value(req->body, "name", name, sizeof(name));
    form_value(req->body, "delayMs", delay_text, sizeof(delay_text));
    script = (char *)malloc(MAX_BT_SCRIPT_LEN);
    if (!script) {
        render_page(fd, req, "Not enough memory to save Bluetooth command.");
        return;
    }
    form_value(req->body, "script", script, MAX_BT_SCRIPT_LEN);
    delay_ms = atoi(delay_text);
    upsert_bt_command(device_id, old_name, name, script, delay_ms, msg, sizeof(msg));
    render_page(fd, req, msg);
    free(script);
}

void handle_bt_delete_command(int fd, const struct request *req) {
    char device_id[64], command[128], msg[512];
    form_value(req->body, "deviceId", device_id, sizeof(device_id));
    form_value(req->body, "command", command, sizeof(command));
    delete_bt_command(device_id, command, msg, sizeof(msg));
    render_page(fd, req, msg);
}

void handle_bt_send_command(int fd, const struct request *req) {
    char device_id[64], command[128], msg[1024];
    form_value(req->body, "deviceId", device_id, sizeof(device_id));
    form_value(req->body, "command", command, sizeof(command));
    send_bt_saved_command(device_id, command, msg, sizeof(msg));
    render_page(fd, req, msg);
}

void render_bt_saved_command_json(int fd, const struct request *req) {
    char device_id[64], command[128], msg[1024];
    form_value(req->body, "deviceId", device_id, sizeof(device_id));
    form_value(req->body, "command", command, sizeof(command));
    if (!device_id[0]) json_string(req->body, "deviceId", device_id, sizeof(device_id));
    if (!command[0]) json_string(req->body, "command", command, sizeof(command));
    int rc = send_bt_saved_command(device_id, command, msg, sizeof(msg));
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", rc == 0 ? 1 : 0);
    cJSON_AddStringToObject(resp, "message", msg);
    send_cjson_resp(fd, rc == 0 ? "200 OK" : "400 Bad Request", resp);
    cJSON_Delete(resp);
}

