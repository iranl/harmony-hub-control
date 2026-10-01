#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define RFSPI_DEV   "/dev/rfspi"
#define RFFW_DEV    "/dev/rffw"
#define CC2544_FW   "/lib/firmware/cc2544.bin"
#define DAEMON_PORT 8089
#define RF_API_PORT 8092

static volatile bool g_running = true;
static int g_rf_fd = -1;
static pthread_mutex_t g_rf_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint8_t g_paired_addr[4] = {0};
static bool g_has_paired_device = false;
static uint8_t g_fw_version = 0;
static bool g_pairing_mode = false;

static void sig_handler(int sig) {
    (void)sig;
    g_running = false;
}

/* Flash CC2544 firmware if needed */
static int flash_cc2544(void) {
    int fin = open(CC2544_FW, O_RDONLY);
    if (fin < 0) return -1;
    int fout = open(RFFW_DEV, O_WRONLY);
    if (fout < 0) { close(fin); return -1; }

    char buf[1024];
    ssize_t n, total = 0;
    while ((n = read(fin, buf, sizeof(buf))) > 0) {
        if (write(fout, buf, n) != n) {
            close(fin);
            close(fout);
            return -1;
        }
        total += n;
    }
    close(fin);
    close(fout);
    printf("[+] Programmed %zd bytes into CC2544 firmware\n", total);
    usleep(100000);
    return 0;
}

/* Send raw HID++ command with mutex */
static int rf_write_cmd(const uint8_t *cmd, size_t len) {
    pthread_mutex_lock(&g_rf_mutex);
    if (g_rf_fd < 0) {
        pthread_mutex_unlock(&g_rf_mutex);
        return -1;
    }
    ssize_t written = write(g_rf_fd, cmd, len);
    pthread_mutex_unlock(&g_rf_mutex);
    return (written == (ssize_t)len) ? 0 : -1;
}

/* Forward button event to codex_daemon on 127.0.0.1:8089 */
static void dispatch_button_to_daemon(const char *button_name, const char *action) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return;

    struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(DAEMON_PORT);
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (connect(sock, (struct sockaddr *)&sin, sizeof(sin)) == 0) {
        char body[256];
        int body_len = snprintf(body, sizeof(body),
            "{\"command\":\"%s\",\"action\":\"%s\",\"source\":\"elite_rf\"}",
            button_name, action ? action : "press");

        char req[512];
        int req_len = snprintf(req, sizeof(req),
            "POST /api/button-press HTTP/1.1\r\n"
            "Host: 127.0.0.1:%d\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n\r\n%s",
            DAEMON_PORT, body_len, body);

        write(sock, req, req_len);
    }
    close(sock);
}

/* Forward activity start to codex_daemon on 127.0.0.1:8089 */
static void dispatch_activity_to_daemon(const char *act_id) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return;

    struct timeval tv = { .tv_sec = 2, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(DAEMON_PORT);
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (connect(sock, (struct sockaddr *)&sin, sizeof(sin)) == 0) {
        char body[256];
        int body_len;
        char req[512];
        int req_len;

        if (strcmp(act_id, "-1") == 0) {
            req_len = snprintf(req, sizeof(req),
                "POST /api/poweroff HTTP/1.1\r\n"
                "Host: 127.0.0.1:%d\r\n"
                "Content-Length: 0\r\n"
                "Connection: close\r\n\r\n",
                DAEMON_PORT);
        } else {
            body_len = snprintf(body, sizeof(body), "{\"activity\":\"%s\"}", act_id);
            req_len = snprintf(req, sizeof(req),
                "POST /api/activity HTTP/1.1\r\n"
                "Host: 127.0.0.1:%d\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: %d\r\n"
                "Connection: close\r\n\r\n%s",
                DAEMON_PORT, body_len, body);
        }
        write(sock, req, req_len);
    }
    close(sock);
}

/* Map RF button raw key code to human-readable Harmony name */
static const char *map_button_code(uint8_t k0, uint8_t k1) {
    uint16_t key = ((uint16_t)k0 << 8) | k1;
    switch (key) {
        /* Activities */
        case 0x0101: return "WatchTVActivity";
        case 0x0102: return "MusicActivity";
        case 0x0103: return "MovieActivity";
        case 0x0104: return "PCTVActivity";
        case 0x0105: return "PowerOffActivity";

        /* Navigation */
        case 0x0201: return "DirectionUp";
        case 0x0202: return "DirectionDown";
        case 0x0203: return "DirectionLeft";
        case 0x0204: return "DirectionRight";
        case 0x0205: return "Select";
        case 0x0206: return "Back";
        case 0x0207: return "Menu";
        case 0x0208: return "Exit";
        case 0x0209: return "Info";
        case 0x020A: return "Guide";
        case 0x020B: return "Live";
        case 0x020C: return "Dvr";

        /* Volume & Channel */
        case 0x0301: return "VolumeUp";
        case 0x0302: return "VolumeDown";
        case 0x0303: return "VolumeMute";
        case 0x0304: return "ChannelUp";
        case 0x0305: return "ChannelDown";
        case 0x0306: return "PrevChannel";

        /* Playback */
        case 0x0401: return "Play";
        case 0x0402: return "Pause";
        case 0x0403: return "Record";
        case 0x0404: return "FastForward";
        case 0x0405: return "Rewind";

        /* Colors */
        case 0x0501: return "Red";
        case 0x0502: return "Green";
        case 0x0503: return "Yellow";
        case 0x0504: return "Blue";

        /* Home Automation */
        case 0x0601: return "Ha1";
        case 0x0602: return "Ha2";
        case 0x0603: return "Ha3";
        case 0x0604: return "Ha4";
        case 0x0605: return "RockerUp";
        case 0x0606: return "RockerDown";

        /* Keypad */
        case 0x0700: return "Number0";
        case 0x0701: return "Number1";
        case 0x0702: return "Number2";
        case 0x0703: return "Number3";
        case 0x0704: return "Number4";
        case 0x0705: return "Number5";
        case 0x0706: return "Number6";
        case 0x0707: return "Number7";
        case 0x0708: return "Number8";
        case 0x0709: return "Number9";
        case 0x070A: return "NumberEnter";
        case 0x070B: return "NumberPlus";

        default: return NULL;
    }
}

/* Parse incoming HID++ packet from CC2544 */
static void handle_rf_packet(const uint8_t *pkt, size_t len) {
    if (len < 7) return;

    uint8_t report_id = pkt[0];

    /* Short Report: 7 bytes */
    if (report_id == 0x10) {
        uint8_t sub_id = pkt[2];

        /* Firmware Version Reply */
        if (sub_id == 0x81 && pkt[3] == 0xF1) {
            g_fw_version = pkt[5];
            printf("[+] CC2544 FW Version: v%d.00 (0x%02X)\n", g_fw_version, g_fw_version);
            return;
        }

        /* 0x41: Button / Status Notification */
        if (sub_id == 0x41) {
            uint8_t k0 = pkt[3];
            uint8_t k1 = pkt[4];
            uint8_t state = pkt[5];
            const char *action = (state == 1) ? "press" : ((state == 2) ? "hold" : "release");
            const char *name = map_button_code(k0, k1);

            printf("[*] Remote Button: raw=[0x%02X, 0x%02X] action=%s name=%s\n",
                   k0, k1, action, name ? name : "Unknown");

            if (name) {
                dispatch_button_to_daemon(name, action);
            }
            return;
        }

        /* 0x8F: Error / NAK */
        if (sub_id == 0x8F) {
            printf("[-] CC2544 Error/NAK: for SubID=0x%02X err=0x%02X\n", pkt[3], pkt[5]);
            return;
        }
    }

    /* Long Report: 20 bytes */
    if (report_id == 0x11) {
        uint8_t sub_id = pkt[2];

        /* Paired Device List reply (0x83 0xB5 0x03) */
        if (sub_id == 0x83 && pkt[3] == 0xB5 && pkt[4] == 0x03) {
            memcpy(g_paired_addr, &pkt[5], 4);
            g_has_paired_device = (g_paired_addr[0] != 0 || g_paired_addr[1] != 0);
            printf("[+] Paired Remote RF Address: %02X %02X %02X %02X (paired=%d)\n",
                   g_paired_addr[0], g_paired_addr[1], g_paired_addr[2], g_paired_addr[3],
                   g_has_paired_device ? 1 : 0);
            return;
        }

        /* 0xFD: HOT (Harmony Object Transfer) Data Frame */
        if (sub_id == 0xFD) {
            uint8_t hot_cmd = pkt[3];
            if (hot_cmd == 0x01) {
                /* Start Activity command from Remote */
                char act_id[32] = {0};
                memcpy(act_id, &pkt[4], (len - 4 < 31) ? len - 4 : 31);
                printf("[*] Remote requested Activity Start: %s\n", act_id);
                dispatch_activity_to_daemon(act_id);
            } else {
                printf("[*] HOT Frame (len=%zd): ", len);
                for (size_t i = 0; i < len; i++) printf("%02X ", pkt[i]);
                printf("\n");
            }
            return;
        }
    }

    /* Generic packet log */
    printf("[*] RX (%zd bytes): ", len);
    for (size_t i = 0; i < len; i++) printf("%02X ", pkt[i]);
    printf("\n");
}

/* Background receiver thread (blocking read on /dev/rfspi) */
static void *rx_worker(void *arg) {
    (void)arg;
    uint8_t buf[64];

    while (g_running) {
        memset(buf, 0, sizeof(buf));
        ssize_t n = read(g_rf_fd, buf, sizeof(buf));
        if (n > 0) {
            handle_rf_packet(buf, (size_t)n);
        } else if (n < 0) {
            if (errno == EINTR) continue;
            perror("read /dev/rfspi");
            usleep(100000);
        }
    }
    return NULL;
}

/* Pairing control commands */
static void start_pairing_mode(void) {
    uint8_t open_lock[7] = {0x10, 0xFF, 0x80, 0xB2, 0x01, 0x00, 0x00};
    printf("[*] Opening pairing lock (10 FF 80 B2 01 00 00)...\n");
    rf_write_cmd(open_lock, sizeof(open_lock));
    g_pairing_mode = true;
}

static void stop_pairing_mode(void) {
    uint8_t close_lock[7] = {0x10, 0xFF, 0x80, 0xB2, 0x02, 0x00, 0x00};
    printf("[*] Closing pairing lock (10 FF 80 B2 02 00 00)...\n");
    rf_write_cmd(close_lock, sizeof(close_lock));
    g_pairing_mode = false;
}

static void unpair_device(void) {
    uint8_t unpair[7] = {0x10, 0xFF, 0x80, 0xB2, 0x03, 0x00, 0x00};
    printf("[*] Unpairing remote (10 FF 80 B2 03 00 00)...\n");
    rf_write_cmd(unpair, sizeof(unpair));
    g_has_paired_device = false;
    memset(g_paired_addr, 0, sizeof(g_paired_addr));
}

/* Minimal HTTP server on 8092 for WebUI / CLI control */
static void *api_server_worker(void *arg) {
    (void)arg;
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) return NULL;

    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(RF_API_PORT);
    sin.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(srv, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        close(srv);
        return NULL;
    }
    listen(srv, 4);

    while (g_running) {
        int cli = accept(srv, NULL, NULL);
        if (cli < 0) {
            if (errno == EINTR) continue;
            break;
        }

        char buf[512];
        ssize_t n = read(cli, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            if (strstr(buf, "GET /api/rf/pair") == buf || strstr(buf, "POST /api/rf/pair") == buf) {
                start_pairing_mode();
                const char *res = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n{\"status\":\"pairing_started\"}";
                write(cli, res, strlen(res));
            } else if (strstr(buf, "GET /api/rf/stop-pair") == buf) {
                stop_pairing_mode();
                const char *res = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n{\"status\":\"pairing_stopped\"}";
                write(cli, res, strlen(res));
            } else if (strstr(buf, "GET /api/rf/unpair") == buf) {
                unpair_device();
                const char *res = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n{\"status\":\"unpaired\"}";
                write(cli, res, strlen(res));
            } else {
                /* Status endpoint */
                char body[256];
                int blen = snprintf(body, sizeof(body),
                    "{\"fw_version\":\"v%d.00\",\"paired\":%s,\"rf_address\":\"%02X%02X%02X%02X\",\"pairing_active\":%s}",
                    g_fw_version, g_has_paired_device ? "true" : "false",
                    g_paired_addr[0], g_paired_addr[1], g_paired_addr[2], g_paired_addr[3],
                    g_pairing_mode ? "true" : "false");

                char header[256];
                int hlen = snprintf(header, sizeof(header),
                    "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n", blen);
                write(cli, header, hlen);
                write(cli, body, blen);
            }
        }
        close(cli);
    }
    close(srv);
    return NULL;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    printf("[+] Starting Harmony Hub RF Daemon (codex_rf)...\n");

    /* Open SPI driver */
    g_rf_fd = open(RFSPI_DEV, O_RDWR);
    if (g_rf_fd < 0) {
        fprintf(stderr, "[-] Warning: cannot open %s, attempting CC2544 flash...\n", RFSPI_DEV);
        flash_cc2544();
        g_rf_fd = open(RFSPI_DEV, O_RDWR);
        if (g_rf_fd < 0) {
            fprintf(stderr, "[-] Fatal: cannot open %s: %s\n", RFSPI_DEV, strerror(errno));
            return 1;
        }
    }
    printf("[+] Opened %s (fd=%d)\n", RFSPI_DEV, g_rf_fd);

    /* 1. Initialize CC2544 message engine */
    uint8_t pkt_init[7] = {0x10, 0xFF, 0x80, 0x00, 0x00, 0x01, 0x00};
    rf_write_cmd(pkt_init, sizeof(pkt_init));
    usleep(50000);

    /* 2. Start background RX thread */
    pthread_t rx_th;
    if (pthread_create(&rx_th, NULL, rx_worker, NULL) != 0) {
        perror("pthread_create rx_worker");
        close(g_rf_fd);
        return 1;
    }

    /* 3. Query initial state */
    uint8_t pkt_ver[7] = {0x10, 0xFF, 0x81, 0xF1, 0x01, 0x00, 0x00};
    rf_write_cmd(pkt_ver, sizeof(pkt_ver));
    usleep(50000);

    uint8_t pkt_paired_list[7] = {0x10, 0xFF, 0x83, 0xB5, 0x03, 0x00, 0x00};
    rf_write_cmd(pkt_paired_list, sizeof(pkt_paired_list));
    usleep(50000);

    /* 4. Start local REST API thread */
    pthread_t api_th;
    pthread_create(&api_th, NULL, api_server_worker, NULL);
    printf("[+] RF API listening on http://127.0.0.1:%d/api/rf/...\n", RF_API_PORT);

    /* Optional CLI flags */
    if (argc > 1) {
        if (strcmp(argv[1], "--pair") == 0) start_pairing_mode();
        else if (strcmp(argv[1], "--unpair") == 0) unpair_device();
    }

    printf("[+] codex_rf ready and operational.\n");

    /* Main loop wait */
    while (g_running) {
        sleep(1);
    }

    printf("[+] Shutting down codex_rf...\n");
    close(g_rf_fd);
    g_rf_fd = -1;
    pthread_join(rx_th, NULL);
    return 0;
}
