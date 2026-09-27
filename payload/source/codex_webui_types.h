/* codex_webui_types.h - Defines, constants, and struct declarations.
 * Extracted from codex_webui.c for readability. Included only by codex_webui.c. */
#ifndef CODEX_WEBUI_TYPES_H
#define CODEX_WEBUI_TYPES_H

#define BT_REMOTE_MAP_FILE "/data/codex/bt_remote_map.json"
#define MQTT_CONFIG "/data/codexmqtt/config.json"
#define WPA_CONFIG "/etc/wpa_supplicant.conf"
#define ETHERNET_CONFIG "/data/codex/ethernet.conf"
#define HUB_ID_FILE "/data/codex/hub_id"
#define WEBUI_AUTH_CONFIG "/data/codex/webui_auth.conf"
#define UPDATE_STATE_CONFIG "/data/codex/update_state.conf"
#define CURRENT_ACTIVITY_FILE "/data/codex/current_activity"
#define ACTIVITY_LOCK_FILE "/tmp/codex_activity_transition"
#define DEVICE_LIST "/data/resources/DeviceList.json"
#define FUNCTION_LIST "/data/resources/FunctionList.json"
#define PROTOCOL_LIST "/data/resources/ProtocolList.json"
#define ACTIVITY_LIST "/data/resources/ActivityList.json"
#define RESOURCE_RELOAD_FLAG "/data/codex/reload_resources"
#define RESOURCE_BACKUP_DIR "/data/codex/resource-backups"
#define IR_EVENT_LOG "/tmp/ir-events.log"
#define DEBUG_LOG_CONFIG "/data/codex/debug_logging.conf"
#define BT_DEBUG_FLAG "/data/codex/bt_remote_debug"

#define IR_CANCEL_PREFIX "/tmp/codex_ir_cancel_"
#define BT_TEXT_FIFO "/tmp/bthid_input"
#define BT_TEXT_STATUS "/tmp/bthid_status"
#define BT_TARGET_FILE "/data/codex/bthid_target"
#define BT_REMOTE_TARGET "/data/codex/bt_remote_target"
#define BT_KEYS_TLV "/data/codex/btstack_keys.tlv"
#define BT_DEVICE_STORE "/data/codex/bt-devices.json"
#define CODEX_BIN_DIR "/data/codex/bin"
#define UPDATE_STAGE_DIR "/tmp/codex_update"
#define UPDATE_BACKUP_DIR "/data/codex/update-backups"
#define IR_EVENT_MAX_BYTES 65536
#define MAX_REQUEST_BODY (1536 * 1024)
#define MAX_REQUEST_BYTES (MAX_REQUEST_BODY + 8192)
#define MAX_RESOURCE_FILE (2 * 1024 * 1024)
#define MAX_IR_DEVICES 32
#define MAX_IR_COMMANDS 512
#define MAX_IR_STORED_COMMANDS 2048
#define MAX_IR_BATCH_COMMANDS 1024
#define MAX_ACTIVITIES 24
#define MAX_ACTIVITY_STEPS 32
#define MAX_BT_SEQUENCE_BODY 32768
#define MAX_BT_DEVICES 12
#define MAX_BT_COMMANDS 32
#define MAX_BT_SCRIPT_LEN 2048

static const char *UPDATE_FILES[] = {
    "codex_webui",
    "codex_daemon",
    "codex_btstack",
    "codex_sntp",
    "codex_portal",
    "codex_dhcpd",
    "register_ehci"
};

static const char *BUILTIN_PROTOCOL_TOSHIBA_32 =
    "{\"IRSegments\":[{\"Header\":[{\"Value\":8990,\"Type\":1,\"MinValue\":null,\"MaxValue\":null},{\"Value\":4490,\"Type\":0,\"MinValue\":null,\"MaxValue\":null}],\"Payload\":{\"NumberOfBits\":32,\"Encodings\":[{\"Atoms\":[{\"Value\":568,\"Type\":1,\"MinValue\":null,\"MaxValue\":null},{\"Value\":552,\"Type\":0,\"MinValue\":null,\"MaxValue\":null}],\"BitType\":0},{\"Atoms\":[{\"Value\":568,\"Type\":1,\"MinValue\":null,\"MaxValue\":null},{\"Value\":1662,\"Type\":0,\"MinValue\":null,\"MaxValue\":null}],\"BitType\":1}],\"ToggleBit\":null,\"EncodingType\":0},\"Trailer\":[{\"Value\":568,\"Type\":1,\"MinValue\":null,\"MaxValue\":null}],\"TotalLength\":107870,\"Name\":\"Toshiba 32 Bit\"}],\"Attributes\":[],\"IsPadded\":true,\"IsFullSequence\":true,\"Rating\":null,\"NumberOfLinkedLanguage\":0,\"Status\":null,\"IsPublic\":true,\"HoldDelay\":null,\"PressMinimumRepeats\":1,\"SendingType\":0,\"Name\":\"Toshiba 32 Bit\",\"ControlSection\":null,\"Flags\":[],\"__type\":\"IrProtocol\",\"CarrierFrequency\":38000,\"Id-\":2,\"CodeSegments\":[{\"Header\":[{\"Value\":8990,\"Type\":1,\"MinValue\":null,\"MaxValue\":null},{\"Value\":2230,\"Type\":0,\"MinValue\":null,\"MaxValue\":null}],\"Payload\":null,\"TotalLength\":0,\"Trailer\":[{\"Value\":568,\"Type\":1,\"MinValue\":null,\"MaxValue\":null},{\"Value\":96077,\"Type\":0,\"MinValue\":null,\"MaxValue\":null}],\"Atoms\":[{\"Value\":8990,\"Type\":1,\"MinValue\":null,\"MaxValue\":null},{\"Value\":2230,\"Type\":0,\"MinValue\":null,\"MaxValue\":null},{\"Value\":568,\"Type\":1,\"MinValue\":null,\"MaxValue\":null},{\"Value\":96077,\"Type\":0,\"MinValue\":null,\"MaxValue\":null}],\"Name\":\"Toshiba 32 Bit KeyCodeRepeat\"}],\"KeyCode\":{\"Start\":[{\"SegmentType\":1,\"SegmentName\":\"Toshiba 32 Bit\"}],\"Repeat\":[{\"SegmentType\":0,\"SegmentName\":\"Toshiba 32 Bit KeyCodeRepeat\"}],\"Finish\":null},\"HoldMinimumRepeats\":null,\"RelatedProtocols\":[]}";

static const char *BUILTIN_PROTOCOL_MEMOREX_O1 =
    "{\"IRSegments\":[{\"Header\":[{\"Value\":9000,\"Type\":1,\"MinValue\":null,\"MaxValue\":null},{\"Value\":4500,\"Type\":0,\"MinValue\":null,\"MaxValue\":null}],\"Payload\":{\"NumberOfBits\":32,\"Encodings\":[{\"Atoms\":[{\"Value\":560,\"Type\":1,\"MinValue\":null,\"MaxValue\":null},{\"Value\":560,\"Type\":0,\"MinValue\":null,\"MaxValue\":null}],\"BitType\":0},{\"Atoms\":[{\"Value\":560,\"Type\":1,\"MinValue\":null,\"MaxValue\":null},{\"Value\":1690,\"Type\":0,\"MinValue\":null,\"MaxValue\":null}],\"BitType\":1}],\"ToggleBit\":null,\"EncodingType\":0},\"Trailer\":[{\"Value\":560,\"Type\":1,\"MinValue\":null,\"MaxValue\":null}],\"TotalLength\":107600,\"Name\":\"MemorexO1 32 Bit\"}],\"Attributes\":[],\"IsPadded\":null,\"IsFullSequence\":null,\"Rating\":null,\"NumberOfLinkedLanguage\":0,\"Status\":null,\"IsPublic\":true,\"HoldDelay\":null,\"PressMinimumRepeats\":null,\"SendingType\":0,\"Name\":\"MemorexO1 32 Bit\",\"ControlSection\":null,\"Flags\":[],\"__type\":\"IrProtocol\",\"CarrierFrequency\":38000,\"Id-\":679,\"CodeSegments\":[],\"KeyCode\":{\"Start\":null,\"Repeat\":[{\"SegmentType\":1,\"SegmentName\":\"MemorexO1 32 Bit\"}],\"Finish\":null},\"HoldMinimumRepeats\":null,\"RelatedProtocols\":[]}";

static const char *BUILTIN_PROTOCOL_CUSTOM_MQTT =
    "{\"IRSegments\":[],\"Attributes\":[],\"IsPadded\":true,\"IsFullSequence\":true,\"Rating\":null,\"NumberOfLinkedLanguage\":0,\"Status\":null,\"IsPublic\":true,\"HoldDelay\":null,\"PressMinimumRepeats\":1,\"SendingType\":0,\"Name\":\"Custom MQTT\",\"ControlSection\":null,\"Flags\":[],\"__type\":\"IrProtocol\",\"CarrierFrequency\":38000,\"Id-\":999990,\"CodeSegments\":[],\"KeyCode\":{\"Start\":null,\"Repeat\":null,\"Finish\":null},\"HoldMinimumRepeats\":null,\"RelatedProtocols\":[]}";

struct request {
    char method[8];
    char path[256];
    char auth[512];
    int body_truncated;
    char *body;
    size_t body_len;
    int is_ajax;
};

struct mqtt_config {
    int enabled;
    int ha_discovery;
    int bt_remote_events;
    int port;
    int poll_seconds;
    int keep_alive;
    char host[128];
    char username[128];
    char password[256];
    char base_topic[128];
    char discovery_prefix[128];
    char client_id[128];
    char name[128];
};

struct device_mqtt_config {
    int enabled;
    char topic[256];
    int pulse_ms;
};

struct wifi_config {
    int hidden;
    int open;
    char ssid[256];
    char psk[256];
};

struct ethernet_config {
    int enabled;
    int fallback_wifi;
    int is_static;
    char ip[64];
    char netmask[64];
    char gateway[64];
    char dns[64];
};

struct network_status {
    char active_interface[32];
    char connection_type[32];
    char ip[64];
    char netmask[64];
    char gateway[64];
    char mac[32];
    int eth_present;
    int eth_carrier;
    char eth_ifname[32];
    char eth_ip[64];
    int wifi_connected;
    char wifi_ssid[128];
    char wifi_ip[64];
    int usb_host_mode;
    int usb_pc_connected;
};

struct webui_auth_config {
    int enabled;
    char username[64];
    char password[128];
};

struct update_check_state {
    long checked_at;
    int available;
    int changes;
    char message[192];
    char source[192];
};

struct activity_step {
    char type[32];
    char device_id[32];
    char command[80];
    int delay_ms;
    int order;
};

struct ir_command {
    char id[32];
    char name[128];
    char keycode[256];
    char raw[2048];
    int protocol_id;
    int learned;
    int has_raw;
};

struct ir_device {
    char id[32];
    char name[128];
    char manufacturer[128];
    char model[128];
    char type[80];
    int control_port;
    int transport;
    int command_count;
    int command_cap;
    struct ir_command *commands;
    int power_on_delay;
    int is_power_always_on;
    int power_on_count;
    struct activity_step *power_on_steps;
    int power_off_count;
    struct activity_step *power_off_steps;
    int mqtt_enabled;
    char mqtt_topic[256];
    int mqtt_pulse_ms;
    int inter_device_delay;
};

struct ir_inventory {
    int device_count;
    long max_device_id;
    long max_command_id;
    struct ir_device devices[MAX_IR_DEVICES];
};

struct bt_saved_command {
    char name[128];
    char script[MAX_BT_SCRIPT_LEN];
    int delay_ms;
};

struct bt_saved_device {
    char id[40];
    char name[128];
    char type[40];
    char bdaddr[32];
    int command_count;
    struct bt_saved_command commands[MAX_BT_COMMANDS];
};

struct bt_inventory {
    int device_count;
    struct bt_saved_device devices[MAX_BT_DEVICES];
};

#endif /* CODEX_WEBUI_TYPES_H */

