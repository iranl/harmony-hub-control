#define _GNU_SOURCE
#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "cJSON.h"
#include "codex_webui_types.h"
#include "resource_cache.h"
#include "hw_action.h"
#include "ir_i2s.h"
#include "remote_skin_jpg.h"
#include "remote_b25_skin_jpg.h"

#include "webui_utils.h"
#include "webui_ir.h"
#include "webui_activity.h"
#include "webui_bt.h"
#include "webui_update.h"
#include "webui_config.h"
#include "webui_html.h"

#include "codex_webui_html.c"

static void handle_client(int client) {
    struct request req;
    if (read_request(client, &req) != 0) {
        free_request(&req);
        return;
    }
    if (strcmp(req.method, "OPTIONS") == 0) {
        const char *resp = "HTTP/1.1 204 No Content\r\n"
                           "Access-Control-Allow-Origin: *\r\n"
                           "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                           "Access-Control-Allow-Headers: Authorization, Content-Type\r\n"
                           "Access-Control-Max-Age: 86400\r\n"
                           "Content-Length: 0\r\n"
                           "Connection: close\r\n\r\n";
        send(client, resp, strlen(resp), 0);
        free_request(&req);
        return;
    }
    if (!webui_auth_ok(&req)) {
        send_auth_required(client);
        free_request(&req);
        return;
    }
    if (req.body_truncated) {
        send_payload_too_large(client, &req);
        free_request(&req);
        return;
    }
    if (strcmp(req.method, "GET") == 0 && (strcmp(req.path, "/") == 0 || strcmp(req.path, "/index.html") == 0)) {
        render_page(client, &req, "");
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/api/inventory") == 0) {
        render_inventory_json(client);
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/api/activities") == 0) {
        render_activities_json(client);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/activity-start") == 0) {
        render_activity_start_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/activity-stop") == 0) {
        render_activity_stop_json(client);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/activity-save") == 0) {
        render_activity_save_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/activity-delete") == 0) {
        render_activity_delete_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/activity-test-sequence") == 0) {
        render_activity_test_sequence_json(client, &req);
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/export/activities") == 0) {
        send_file_download(client, ACTIVITY_LIST, "ActivityList.json", "application/json");
    } else if ((strcmp(req.method, "GET") == 0 || strcmp(req.method, "POST") == 0) &&
        strncmp(req.path, "/api/device-commands", 20) == 0 &&
        (req.path[20] == 0 || req.path[20] == '?')) {
        render_device_commands_json(client, &req);
    } else if ((strcmp(req.method, "GET") == 0 || strcmp(req.method, "POST") == 0) &&
        strncmp(req.path, "/api/remotecentral-fetch", 24) == 0 &&
        (req.path[24] == 0 || req.path[24] == '?')) {
        render_remotecentral_fetch_json(client, &req);
    } else if ((strcmp(req.method, "GET") == 0 || strcmp(req.method, "POST") == 0) && strcmp(req.path, "/api/capture") == 0) {
        render_capture_json(client);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/ir-send") == 0) {
        render_ir_send_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/ir-test-learned") == 0) {
        render_ir_test_learned_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/ir-batch-send") == 0) {
        render_ir_batch_send_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/ir-cancel") == 0) {
        render_ir_cancel_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/ir-lab-target") == 0) {
        render_ir_lab_target_json(client);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/ir-lab-clear") == 0) {
        render_ir_lab_clear_json(client, &req);
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/api/bt-status") == 0) {
        render_bt_status_json(client);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/bt-key") == 0) {
        render_bt_key_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/bt-pairing") == 0) {
        render_bt_pairing_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/bt-connect") == 0) {
        render_bt_connect_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/bt-disconnect") == 0) {
        render_bt_disconnect_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/bt-link-device") == 0) {
        render_bt_link_device_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/bt-script") == 0) {
        render_bt_script_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/bt-call") == 0) {
        render_bluetooth_call_json(client, &req);
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/api/bt-text-status") == 0) {
        render_bluetooth_text_status_json(client);
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/api/bt-sent-log") == 0) {
        render_bt_sent_log_json(client);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/bt-text") == 0) {
        render_bluetooth_text_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/bt-saved-command") == 0) {
        render_bt_saved_command_json(client, &req);
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/api/remote-mapping") == 0) {
        render_remote_mapping_json(client);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/remote-mapping-save") == 0) {
        render_remote_mapping_save_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/remote-scan") == 0) {
        render_remote_scan_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/remote-pair") == 0) {
        render_remote_pair_json(client, &req);
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/api/remote-pair-status") == 0) {
        render_remote_pair_status_json(client);
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/api/update-status") == 0) {
        render_update_status_json(client);
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/api/update-check-state") == 0) {
        render_update_check_state_json(client);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/update-check-state") == 0) {
        render_update_check_state_post_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/update-begin") == 0) {
        render_update_begin_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/update-chunk") == 0) {
        render_update_chunk_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/update-apply") == 0) {
        render_update_apply_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/irdb-import") == 0) {
        render_irdb_import_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/device-mqtt") == 0) {
        render_device_mqtt_save_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/device-mqtt-test") == 0) {
        render_device_mqtt_test_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/device-save") == 0) {
        render_device_save_json(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/device-power-save") == 0) {
        render_device_power_save_json(client, &req);
    } else if ((strcmp(req.method, "GET") == 0 || strcmp(req.method, "POST") == 0) && strcmp(req.path, "/export/bundle") == 0) {
        send_bundle_download(client);
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/export/devices") == 0) {
        send_file_download(client, DEVICE_LIST, "DeviceList.json", "application/json");
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/export/functions") == 0) {
        send_file_download(client, FUNCTION_LIST, "FunctionList.json", "application/json");
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/export/protocols") == 0) {
        send_file_download(client, PROTOCOL_LIST, "ProtocolList.json", "application/json");
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/export/mqtt") == 0) {
        send_file_download(client, MQTT_CONFIG, "mqtt-config.json", "application/json");
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/export/wifi") == 0) {
        send_file_download(client, WPA_CONFIG, "wpa_supplicant.conf", "text/plain");
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/export/bluetooth") == 0) {
        send_bt_devices_download(client);
    } else if (strcmp(req.method, "GET") == 0 && (strcmp(req.path, "/export/remote-mapping") == 0 || strcmp(req.path, "/export/remotemap") == 0)) {
        send_remote_mapping_download(client);
    } else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/export/auth") == 0) {
        send_file_download(client, WEBUI_AUTH_CONFIG, "webui_auth.conf", "text/plain");
    } else if (strcmp(req.method, "GET") == 0 && (strcmp(req.path, "/export/hub-id") == 0 || strcmp(req.path, "/export/hub_id") == 0)) {
        send_file_download(client, HUB_ID_FILE, "hub_id", "text/plain");
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/activity/start") == 0) {
        handle_activity_start(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/activity/stop") == 0) {
        handle_activity_stop(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/activity/save") == 0) {
        handle_activity_save(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/activity/delete") == 0) {
        handle_activity_delete(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/mqtt") == 0) {
        handle_mqtt(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/wifi") == 0) {
        handle_wifi(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/system") == 0) {
        handle_system(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/import") == 0) {
        handle_import(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/ir/send") == 0) {
        handle_ir_send(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/ir/device") == 0) {
        handle_ir_device(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/ir/device-power") == 0) {
        handle_ir_device_power(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/ir/device-mqtt") == 0) {
        handle_ir_device_mqtt(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/ir/new-device") == 0) {
        handle_ir_new_device(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/ir/command") == 0) {
        handle_ir_command(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/ir/update-command") == 0) {
        handle_ir_update_command(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/ir/irdb-import") == 0) {
        handle_irdb_import(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/ir/capture") == 0) {
        handle_ir_capture(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/ir/delete-device") == 0) {
        handle_ir_delete_device(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/ir/delete-command") == 0) {
        handle_ir_delete_command(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/bt/device") == 0) {
        handle_bt_device(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/bt/delete-device") == 0) {
        handle_bt_delete_device(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/bt/command") == 0) {
        handle_bt_command(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/bt/delete-command") == 0) {
        handle_bt_delete_command(client, &req);
    } else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/bt/send-command") == 0) {
        handle_bt_send_command(client, &req);
    } else {
        send_text(client, "404 Not Found", "not found\n");
    }
    free_request(&req);
}

static void reap_children(void) {
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0) {}
    errno = saved_errno;
}

static void handle_sigchld(int signo) {
    (void)signo;
    reap_children();
}

int main(int argc, char **argv) {
    int port = argc > 1 ? atoi(argv[1]) : 8080;
    int fd, one = 1;
    struct sigaction sa;
    struct sockaddr_in addr;
    signal(SIGPIPE, SIG_IGN);
    activity_startup_cleanup();
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return 1;
    }
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        perror("bind");
        return 1;
    }
    if (listen(fd, 32) != 0) {
        perror("listen");
        return 1;
    }
    fprintf(stderr, "codex_webui listening on %d\n", port);
    while (1) {
        int client;
        pid_t pid;
        reap_children();
        client = accept(fd, NULL, NULL);
        if (client < 0) {
            if (errno == EINTR) continue;
            continue;
        }
        pid = fork();
        if (pid == 0) {
            close(fd);
            handle_client(client);
            close(client);
            _exit(0);
        }
        if (pid < 0) {
            handle_client(client);
        }
        close(client);
    }
}

