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
#include <time.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <linux/input.h>
#include <linux/fb.h>
#include <linux/i2c-dev.h>
#include "cJSON.h"
#include "codex_rf_proto.h"

#define FB_DEV              "/dev/fb0"
#define FB_BLANK_SYSFS      "/sys/class/graphics/fb0/blank"
#define BACKLIGHT_SYSFS     "/sys/class/backlight/ap3156_backlight/brightness"
#define BTN_LED_SYSFS       "/sys/class/leds/tps6507x:white/brightness"
#define BTN_EVENT_DEV       "/dev/input/event0"
#define GYRO_EVENT_DEV      "/dev/input/event1"
#define TOUCH_EVENT_DEV     "/dev/input/event2"
#define GYRO_DATA_RATE_SYS  "/sys/devices/platform/i2c_davinci.1/i2c-1/1-0019/data_rate"
#define GYRO_XYZ_SYS        "/sys/devices/platform/i2c_davinci.1/i2c-1/1-0019/xyz"
#define RFSPI_DEV           "/dev/rfspi"
#define ACTIVITY_JSON_FILE  "/data/resources/ActivityList.json"

#define FB_WIDTH            240
#define FB_HEIGHT           320
#define FB_SIZE             (FB_WIDTH * FB_HEIGHT * 2)

/* RGB565 Curated Palette */
#define RGB_BLACK           0x0000
#define RGB_BG              0x10A2  /* Deep slate blue/gray */
#define RGB_CARD            0x2126  /* Card background */
#define RGB_CARD_SEL        0x32A9  /* Selected item card */
#define RGB_WHITE           0xFFFF
#define RGB_TEXT_MUTED      0x8C71  /* Cool gray */
#define RGB_CYAN            0x05FD  /* Accent cyan */
#define RGB_GREEN           0x2E65  /* Success green */
#define RGB_RED             0xF986  /* Alert / Power red */
#define RGB_YELLOW          0xFE60  /* Warning yellow */
#define RGB_BLUE            0x2B9F  /* Highlight blue */
#define RGB_HEADER          0x18E4

/* UI Views */
typedef enum {
    VIEW_ACTIVITIES = 0,
    VIEW_DEVICES,
    VIEW_SETTINGS,
    VIEW_COUNT
} ui_view_t;

/* Activity item */
typedef struct {
    char id[32];
    char name[48];
    char type_str[24];
} activity_item_t;

#define MAX_ACTIVITIES 16
static activity_item_t g_activities[MAX_ACTIVITIES];
static int g_activity_count = 0;
static int g_selected_activity = 0;
static char g_active_activity_id[32] = "-1";
static char g_active_activity_name[48] = "PowerOff";
static void reset_idle_timer(void);

/* Device item */
typedef struct {
    char dev_id[32];
    char name[32];
    char command[32];
} device_item_t;

static const device_item_t g_device_commands[] = {
    {"75784548", "Samsung TV", "PowerToggle"},
    {"75784548", "Samsung TV", "Mute"},
    {"72738267", "Denon AVR",  "PowerToggle"},
    {"72738267", "Denon AVR",  "Mute"},
    {"72738272", "Shield TV",  "Home"},
    {"72738272", "Shield TV",  "Back"},
    {"78134038", "Domo Fan",   "PowerToggle"}
};
#define DEVICE_CMD_COUNT ((int)(sizeof(g_device_commands) / sizeof(g_device_commands[0])))
static int g_selected_device_cmd = 0;

#define MAX_RF_ACTIVITIES 16
#define MAX_RF_DEVICES    16
#define MAX_RF_BUTTONS    48

typedef struct {
    char id[13];
    char name[15];
} rf_activity_t;

typedef struct {
    char id[13];
    char name[15];
} rf_device_t;

typedef struct {
    char context_id[13];  /* activity or device ID */
    char label[13];       /* short button label */
    uint8_t context_type; /* 0=activity, 1=device */
    uint8_t action_type;  /* 1=device_cmd, 2=activity_start, 3=activity_stop */
} rf_button_t;

static rf_activity_t g_rf_activities[MAX_RF_ACTIVITIES];
static int g_rf_activity_count = 0;
static rf_device_t g_rf_devices[MAX_RF_DEVICES];
static int g_rf_device_count = 0;
static rf_button_t g_rf_buttons[MAX_RF_BUTTONS];
static int g_rf_button_count = 0;
static uint8_t g_config_version = 0;
static bool g_has_hub_config = false;
static uint8_t g_tx_seq = 0;

static int get_device_cmd_count(void) {
    if (g_has_hub_config && g_rf_button_count > 0) {
        return g_rf_button_count;
    }
    return (int)(sizeof(g_device_commands) / sizeof(g_device_commands[0]));
}

static void get_device_cmd_item(int idx, char *out_name, size_t name_len, char *out_cmd, size_t cmd_len, char *out_dev, size_t dev_len, uint8_t *out_atype) {
    if (g_has_hub_config && g_rf_button_count > 0 && idx >= 0 && idx < g_rf_button_count) {
        strncpy(out_name, g_rf_buttons[idx].context_id, name_len - 1);
        out_name[name_len - 1] = '\0';
        strncpy(out_cmd, g_rf_buttons[idx].label, cmd_len - 1);
        out_cmd[cmd_len - 1] = '\0';
        strncpy(out_dev, g_rf_buttons[idx].context_id, dev_len - 1);
        out_dev[dev_len - 1] = '\0';
        if (out_atype) *out_atype = g_rf_buttons[idx].action_type;
        return;
    }
    if (idx >= 0 && idx < (int)(sizeof(g_device_commands) / sizeof(g_device_commands[0]))) {
        strncpy(out_name, g_device_commands[idx].name, name_len - 1);
        out_name[name_len - 1] = '\0';
        strncpy(out_cmd, g_device_commands[idx].command, cmd_len - 1);
        out_cmd[cmd_len - 1] = '\0';
        strncpy(out_dev, g_device_commands[idx].dev_id, dev_len - 1);
        out_dev[dev_len - 1] = '\0';
        if (out_atype) *out_atype = 1;
        return;
    }
    out_name[0] = '\0';
    out_cmd[0] = '\0';
    out_dev[0] = '\0';
    if (out_atype) *out_atype = 0;
}

#define RF_CONFIG_FILE "/data/codex_elite_config.json"

static void save_rf_config(void) {
    cJSON *root = cJSON_CreateObject();
    if (!root) return;
    cJSON_AddNumberToObject(root, "version", g_config_version);

    cJSON *acts = cJSON_AddArrayToObject(root, "activities");
    for (int i = 0; i < g_rf_activity_count; i++) {
        cJSON *a = cJSON_CreateObject();
        cJSON_AddStringToObject(a, "id", g_rf_activities[i].id);
        cJSON_AddStringToObject(a, "name", g_rf_activities[i].name);
        cJSON_AddItemToArray(acts, a);
    }

    cJSON *devs = cJSON_AddArrayToObject(root, "devices");
    for (int i = 0; i < g_rf_device_count; i++) {
        cJSON *d = cJSON_CreateObject();
        cJSON_AddStringToObject(d, "id", g_rf_devices[i].id);
        cJSON_AddStringToObject(d, "name", g_rf_devices[i].name);
        cJSON_AddItemToArray(devs, d);
    }

    cJSON *btns = cJSON_AddArrayToObject(root, "buttons");
    for (int i = 0; i < g_rf_button_count; i++) {
        cJSON *b = cJSON_CreateObject();
        cJSON_AddStringToObject(b, "ctx", g_rf_buttons[i].context_id);
        cJSON_AddStringToObject(b, "label", g_rf_buttons[i].label);
        cJSON_AddNumberToObject(b, "ctype", g_rf_buttons[i].context_type);
        cJSON_AddNumberToObject(b, "atype", g_rf_buttons[i].action_type);
        cJSON_AddItemToArray(btns, b);
    }

    char *json = cJSON_PrintUnformatted(root);
    if (json) {
        FILE *f = fopen(RF_CONFIG_FILE, "w");
        if (f) { fputs(json, f); fclose(f); }
        free(json);
    }
    cJSON_Delete(root);
}

static void load_rf_config(void) {
    FILE *f = fopen(RF_CONFIG_FILE, "r");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 50000) { fclose(f); return; }
    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); return; }
    size_t rd = fread(buf, 1, sz, f);
    buf[rd] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) return;

    cJSON *j_ver = cJSON_GetObjectItem(root, "version");
    if (j_ver && cJSON_IsNumber(j_ver)) g_config_version = (uint8_t)j_ver->valueint;

    cJSON *acts = cJSON_GetObjectItem(root, "activities");
    g_rf_activity_count = 0;
    if (acts && cJSON_IsArray(acts)) {
        cJSON *a = NULL;
        cJSON_ArrayForEach(a, acts) {
            if (g_rf_activity_count >= MAX_RF_ACTIVITIES) break;
            cJSON *jid = cJSON_GetObjectItem(a, "id");
            cJSON *jname = cJSON_GetObjectItem(a, "name");
            if (jid && jname && cJSON_IsString(jid) && cJSON_IsString(jname)) {
                strncpy(g_rf_activities[g_rf_activity_count].id, jid->valuestring, 12);
                g_rf_activities[g_rf_activity_count].id[12] = '\0';
                strncpy(g_rf_activities[g_rf_activity_count].name, jname->valuestring, 14);
                g_rf_activities[g_rf_activity_count].name[14] = '\0';
                g_rf_activity_count++;
            }
        }
    }

    cJSON *devs = cJSON_GetObjectItem(root, "devices");
    g_rf_device_count = 0;
    if (devs && cJSON_IsArray(devs)) {
        cJSON *d = NULL;
        cJSON_ArrayForEach(d, devs) {
            if (g_rf_device_count >= MAX_RF_DEVICES) break;
            cJSON *jid = cJSON_GetObjectItem(d, "id");
            cJSON *jname = cJSON_GetObjectItem(d, "name");
            if (jid && jname && cJSON_IsString(jid) && cJSON_IsString(jname)) {
                strncpy(g_rf_devices[g_rf_device_count].id, jid->valuestring, 12);
                g_rf_devices[g_rf_device_count].id[12] = '\0';
                strncpy(g_rf_devices[g_rf_device_count].name, jname->valuestring, 14);
                g_rf_devices[g_rf_device_count].name[14] = '\0';
                g_rf_device_count++;
            }
        }
    }

    cJSON *btns = cJSON_GetObjectItem(root, "buttons");
    g_rf_button_count = 0;
    if (btns && cJSON_IsArray(btns)) {
        cJSON *b = NULL;
        cJSON_ArrayForEach(b, btns) {
            if (g_rf_button_count >= MAX_RF_BUTTONS) break;
            cJSON *jctx = cJSON_GetObjectItem(b, "ctx");
            cJSON *jlbl = cJSON_GetObjectItem(b, "label");
            cJSON *jctype = cJSON_GetObjectItem(b, "ctype");
            cJSON *jatype = cJSON_GetObjectItem(b, "atype");
            if (jctx && jlbl && cJSON_IsString(jctx) && cJSON_IsString(jlbl)) {
                strncpy(g_rf_buttons[g_rf_button_count].context_id, jctx->valuestring, 12);
                g_rf_buttons[g_rf_button_count].context_id[12] = '\0';
                strncpy(g_rf_buttons[g_rf_button_count].label, jlbl->valuestring, 12);
                g_rf_buttons[g_rf_button_count].label[12] = '\0';
                g_rf_buttons[g_rf_button_count].context_type = jctype ? (uint8_t)jctype->valueint : 0;
                g_rf_buttons[g_rf_button_count].action_type = jatype ? (uint8_t)jatype->valueint : 1;
                g_rf_button_count++;
            }
        }
    }

    cJSON_Delete(root);
    g_has_hub_config = (g_rf_activity_count > 0);
    if (g_has_hub_config) {
        g_activity_count = 0;
        for (int i = 0; i < g_rf_activity_count && g_activity_count < MAX_ACTIVITIES; i++) {
            strncpy(g_activities[g_activity_count].id, g_rf_activities[i].id, sizeof(g_activities[0].id) - 1);
            strncpy(g_activities[g_activity_count].name, g_rf_activities[i].name, sizeof(g_activities[0].name) - 1);
            g_activity_count++;
        }
        if (g_activity_count < MAX_ACTIVITIES) {
            strcpy(g_activities[g_activity_count].id, "-1");
            strcpy(g_activities[g_activity_count].name, "Power Off");
            g_activity_count++;
        }
    }
}

/* Settings item */
typedef enum {
    SETTING_TOUCH = 0,
    SETTING_GYRO,
    SETTING_TIMEOUT,
    SETTING_BRIGHTNESS,
    SETTING_BLANK,
    SETTING_COUNT
} setting_type_t;

static int g_selected_setting = 0;
static bool g_touch_enabled = true;
static bool g_gyro_enabled = true;
static int g_timeout_seconds = 120;
static int g_brightness_val = 3; /* 1..5 */
static bool g_screen_on = true;
static bool g_screen_nav_mode = false; /* When false: D-PAD controls device; when true: D-PAD navigates UI */

/* State */
static volatile bool g_running = true;
static int g_fb_fd = -1;
static uint16_t *g_fb_mem = NULL;
static size_t g_fb_size = FB_SIZE;
static int g_btn_fd = -1;
static int g_touch_fd = -1;
static int g_gyro_fd = -1;
static int g_rf_fd = -1;
static ui_view_t g_current_view = VIEW_ACTIVITIES;
static time_t g_last_activity_time = 0;
static pthread_mutex_t g_render_mutex = PTHREAD_MUTEX_INITIALIZER;

static void render_screen(void);

/* Touch tracking */
static int g_touch_cur_x = 0;
static int g_touch_cur_y = 0;
static bool g_touch_is_down = false;

/* Full 8x8 ASCII Font (Characters 32 to 126) */
static const uint8_t g_font_8x8[95][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 32   */
    {0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00}, /* 33 ! */
    {0x66, 0x66, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 34 " */
    {0x6C, 0x6C, 0xFE, 0x6C, 0xFE, 0x6C, 0x6C, 0x00}, /* 35 # */
    {0x18, 0x3E, 0x60, 0x3C, 0x06, 0x7C, 0x18, 0x00}, /* 36 $ */
    {0x00, 0x63, 0x66, 0x0C, 0x18, 0x33, 0x63, 0x00}, /* 37 % */
    {0x38, 0x6C, 0x38, 0x76, 0xDC, 0xCC, 0x76, 0x00}, /* 38 & */
    {0x18, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 39 ' */
    {0x0C, 0x18, 0x30, 0x30, 0x30, 0x18, 0x0C, 0x00}, /* 40 ( */
    {0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x18, 0x30, 0x00}, /* 41 ) */
    {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00}, /* 42 * */
    {0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00}, /* 43 + */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30}, /* 44 , */
    {0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00}, /* 45 - */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00}, /* 46 . */
    {0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0x80, 0x00}, /* 47 / */
    {0x3C, 0x66, 0x6E, 0x7E, 0x76, 0x66, 0x3C, 0x00}, /* 48 0 */
    {0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00}, /* 49 1 */
    {0x3C, 0x66, 0x06, 0x1C, 0x30, 0x66, 0x7E, 0x00}, /* 50 2 */
    {0x3C, 0x66, 0x06, 0x1C, 0x06, 0x66, 0x3C, 0x00}, /* 51 3 */
    {0x0E, 0x1E, 0x36, 0x66, 0x7F, 0x06, 0x0F, 0x00}, /* 52 4 */
    {0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3C, 0x00}, /* 53 5 */
    {0x1C, 0x30, 0x60, 0x7C, 0x66, 0x66, 0x3C, 0x00}, /* 54 6 */
    {0x7E, 0x66, 0x0C, 0x18, 0x18, 0x18, 0x18, 0x00}, /* 55 7 */
    {0x3C, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x3C, 0x00}, /* 56 8 */
    {0x3C, 0x66, 0x66, 0x3E, 0x06, 0x0C, 0x38, 0x00}, /* 57 9 */
    {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00}, /* 58 : */
    {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x30, 0x00}, /* 59 ; */
    {0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00}, /* 60 < */
    {0x00, 0x00, 0x7E, 0x00, 0x7E, 0x00, 0x00, 0x00}, /* 61 = */
    {0x60, 0x30, 0x18, 0x0C, 0x18, 0x30, 0x60, 0x00}, /* 62 > */
    {0x3C, 0x66, 0x0C, 0x18, 0x18, 0x00, 0x18, 0x00}, /* 63 ? */
    {0x3C, 0x66, 0x6E, 0x6E, 0x60, 0x62, 0x3C, 0x00}, /* 64 @ */
    {0x18, 0x3C, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x00}, /* 65 A */
    {0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x7C, 0x00}, /* 66 B */
    {0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00}, /* 67 C */
    {0x78, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0x78, 0x00}, /* 68 D */
    {0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x7E, 0x00}, /* 69 E */
    {0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x60, 0x00}, /* 70 F */
    {0x3C, 0x66, 0x60, 0x6E, 0x66, 0x66, 0x3C, 0x00}, /* 71 G */
    {0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00}, /* 72 H */
    {0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00}, /* 73 I */
    {0x0E, 0x06, 0x06, 0x06, 0x66, 0x66, 0x3C, 0x00}, /* 74 J */
    {0x66, 0x6C, 0x78, 0x70, 0x78, 0x6C, 0x66, 0x00}, /* 75 K */
    {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00}, /* 76 L */
    {0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00}, /* 77 M */
    {0x66, 0x76, 0x7E, 0x7E, 0x6E, 0x66, 0x66, 0x00}, /* 78 N */
    {0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00}, /* 79 O */
    {0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x00}, /* 80 P */
    {0x3C, 0x66, 0x66, 0x66, 0x6E, 0x3C, 0x0E, 0x00}, /* 81 Q */
    {0x7C, 0x66, 0x66, 0x7C, 0x78, 0x6C, 0x66, 0x00}, /* 82 R */
    {0x3C, 0x66, 0x60, 0x3C, 0x06, 0x66, 0x3C, 0x00}, /* 83 S */
    {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00}, /* 84 T */
    {0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00}, /* 85 U */
    {0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00}, /* 86 V */
    {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00}, /* 87 W */
    {0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00}, /* 88 X */
    {0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x00}, /* 89 Y */
    {0x7E, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00}, /* 90 Z */
    {0x3C, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3C, 0x00}, /* 91 [ */
    {0xC0, 0x60, 0x30, 0x18, 0x0C, 0x06, 0x02, 0x00}, /* 92 \ */
    {0x3C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3C, 0x00}, /* 93 ] */
    {0x10, 0x38, 0x6C, 0xC6, 0x00, 0x00, 0x00, 0x00}, /* 94 ^ */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF}, /* 95 _ */
    {0x18, 0x18, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 96 ` */
    {0x00, 0x00, 0x3C, 0x06, 0x3E, 0x66, 0x3E, 0x00}, /* 97 a */
    {0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x7C, 0x00}, /* 98 b */
    {0x00, 0x00, 0x3C, 0x66, 0x60, 0x66, 0x3C, 0x00}, /* 99 c */
    {0x06, 0x06, 0x3E, 0x66, 0x66, 0x66, 0x3E, 0x00}, /* 100 d */
    {0x00, 0x00, 0x3C, 0x66, 0x7E, 0x60, 0x3C, 0x00}, /* 101 e */
    {0x0C, 0x18, 0x3E, 0x18, 0x18, 0x18, 0x18, 0x00}, /* 102 f */
    {0x00, 0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x3C}, /* 103 g */
    {0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00}, /* 104 h */
    {0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3C, 0x00}, /* 105 i */
    {0x06, 0x00, 0x0E, 0x06, 0x06, 0x66, 0x3C, 0x00}, /* 106 j */
    {0x60, 0x60, 0x66, 0x6C, 0x78, 0x6C, 0x66, 0x00}, /* 107 k */
    {0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00}, /* 108 l */
    {0x00, 0x00, 0x66, 0x7F, 0x7F, 0x6B, 0x63, 0x00}, /* 109 m */
    {0x00, 0x00, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00}, /* 110 n */
    {0x00, 0x00, 0x3C, 0x66, 0x66, 0x66, 0x3C, 0x00}, /* 111 o */
    {0x00, 0x00, 0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60}, /* 112 p */
    {0x00, 0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x06}, /* 113 q */
    {0x00, 0x00, 0x7C, 0x66, 0x60, 0x60, 0x60, 0x00}, /* 114 r */
    {0x00, 0x00, 0x3E, 0x60, 0x3C, 0x06, 0x7C, 0x00}, /* 115 s */
    {0x18, 0x18, 0x7E, 0x18, 0x18, 0x18, 0x0E, 0x00}, /* 116 t */
    {0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x00}, /* 117 u */
    {0x00, 0x00, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00}, /* 118 v */
    {0x00, 0x00, 0x63, 0x6B, 0x7F, 0x3E, 0x36, 0x00}, /* 119 w */
    {0x00, 0x00, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x00}, /* 120 x */
    {0x00, 0x00, 0x66, 0x66, 0x66, 0x3E, 0x06, 0x3C}, /* 121 y */
    {0x00, 0x00, 0x7E, 0x0C, 0x18, 0x30, 0x7E, 0x00}, /* 122 z */
    {0x0E, 0x18, 0x18, 0x70, 0x18, 0x18, 0x0E, 0x00}, /* 123 { */
    {0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00}, /* 124 | */
    {0x70, 0x18, 0x18, 0x0E, 0x18, 0x18, 0x70, 0x00}, /* 125 } */
    {0x76, 0xDC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}  /* 126 ~ */
};

static void sig_handler(int sig) {
    (void)sig;
    g_running = false;
}

/* Backlight brightness control (0 to 31) */
static void set_backlight_level(int level) {
    int fd = open(BACKLIGHT_SYSFS, O_WRONLY);
    if (fd >= 0) {
        char buf[16];
        int val = level * 6; /* 1..5 maps to 6..30 */
        if (val > 31) val = 31;
        snprintf(buf, sizeof(buf), "%d\n", val);
        write(fd, buf, strlen(buf));
        close(fd);
    }
}

/* Screen Blank / Unblank */
static void fb_set_blank(bool blank) {
    int fd = open(FB_BLANK_SYSFS, O_WRONLY);
    if (fd >= 0) {
        const char *val = blank ? "1\n" : "0\n";
        write(fd, val, strlen(val));
        close(fd);
    }
    g_screen_on = !blank;
    if (g_screen_on) {
        set_backlight_level(g_brightness_val);
    } else {
        set_backlight_level(0);
    }
    int lfd = open(BTN_LED_SYSFS, O_WRONLY);
    if (lfd >= 0) {
        write(lfd, blank ? "0\n" : "100\n", blank ? 2 : 4);
        close(lfd);
    }
}

/* Touchscreen Enable / Disable */
static void set_touch_enabled(bool enable) {
    g_touch_enabled = enable;
}

/* Gyro / Accelerometer Enable / Disable */
static void set_gyro_enabled(bool enable) {
    g_gyro_enabled = enable;
}

static void load_settings_conf(void) {
    FILE *f = fopen("/data/codex_elite.conf", "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        char key[64] = {0};
        int val = 0;
        if (sscanf(line, "%63[^=]=%d", key, &val) == 2) {
            if (strcmp(key, "touch") == 0) g_touch_enabled = (val != 0);
            else if (strcmp(key, "gyro") == 0) g_gyro_enabled = (val != 0);
            else if (strcmp(key, "timeout") == 0) g_timeout_seconds = val;
            else if (strcmp(key, "brightness") == 0) g_brightness_val = (val >= 1 && val <= 5) ? val : 3;
            else if (strcmp(key, "screen") == 0) g_screen_on = (val != 0);
        }
    }
    fclose(f);
}

static void save_settings_conf(void) {
    FILE *f = fopen("/data/codex_elite.conf", "w");
    if (!f) return;
    fprintf(f, "touch=%d\n", g_touch_enabled ? 1 : 0);
    fprintf(f, "gyro=%d\n", g_gyro_enabled ? 1 : 0);
    fprintf(f, "timeout=%d\n", g_timeout_seconds);
    fprintf(f, "brightness=%d\n", g_brightness_val);
    fprintf(f, "screen=1\n");
    fclose(f);
}

/* Draw a filled rectangle in RGB565 */
static void fb_fill_rect(int x, int y, int w, int h, uint16_t color) {
    if (!g_fb_mem) return;
    for (int j = y; j < y + h && j < FB_HEIGHT; j++) {
        if (j < 0) continue;
        for (int i = x; i < x + w && i < FB_WIDTH; i++) {
            if (i < 0) continue;
            g_fb_mem[j * FB_WIDTH + i] = color;
        }
    }
}

/* Draw single character from font table */
static void fb_draw_char(int x, int y, char c, uint16_t fg, uint16_t bg, int scale) {
    if (!g_fb_mem || (uint8_t)c < 32 || (uint8_t)c > 126) return;
    const uint8_t *glyph = g_font_8x8[(uint8_t)c - 32];
    for (int row = 0; row < 8; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            uint16_t color = (bits & (1 << (7 - col))) ? fg : bg;
            if (color != bg || bg != 0) {
                fb_fill_rect(x + col * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}

/* Draw string */
static void fb_draw_text(int x, int y, const char *text, uint16_t fg, uint16_t bg, int scale) {
    int cur_x = x;
    while (*text) {
        fb_draw_char(cur_x, y, *text, fg, bg, scale);
        cur_x += 8 * scale;
        text++;
    }
}

/* Parse activities from local JSON cache */
static void load_activities(void) {
    g_activity_count = 0;
    FILE *f = fopen(ACTIVITY_JSON_FILE, "r");
    if (!f) goto add_defaults;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); goto add_defaults; }

    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); goto add_defaults; }
    size_t rd = fread(buf, 1, sz, f);
    buf[rd] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) goto add_defaults;

    cJSON *acts = cJSON_GetObjectItem(root, "Activities");
    if (acts && cJSON_IsArray(acts)) {
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, acts) {
            if (g_activity_count >= MAX_ACTIVITIES - 1) break;
            cJSON *j_name = cJSON_GetObjectItem(item, "Name");
            cJSON *j_id = cJSON_GetObjectItem(item, "Id-");
            if (!j_id) j_id = cJSON_GetObjectItem(item, "id");

            if (j_name && cJSON_IsString(j_name) && j_id) {
                const char *n = j_name->valuestring;
                char id_str[32] = {0};
                if (cJSON_IsNumber(j_id)) snprintf(id_str, sizeof(id_str), "%d", j_id->valueint);
                else if (cJSON_IsString(j_id)) strncpy(id_str, j_id->valuestring, sizeof(id_str) - 1);

                /* Replace non-breaking space \xA0 or \xC2\xA0 with standard space */
                char clean_name[48] = {0};
                int ci = 0;
                for (int i = 0; n[i] && ci < 47; i++) {
                    if ((unsigned char)n[i] == 0xA0) {
                        clean_name[ci++] = ' ';
                    } else if ((unsigned char)n[i] == 0xC2 && (unsigned char)n[i+1] == 0xA0) {
                        clean_name[ci++] = ' ';
                        i++;
                    } else {
                        clean_name[ci++] = n[i];
                    }
                }

                strncpy(g_activities[g_activity_count].name, clean_name, sizeof(g_activities[0].name) - 1);
                strncpy(g_activities[g_activity_count].id, id_str, sizeof(g_activities[0].id) - 1);
                g_activity_count++;
            }
        }
    }
    cJSON_Delete(root);

    if (g_activity_count == 0) {
add_defaults:
        strcpy(g_activities[0].id, "46786510");
        strcpy(g_activities[0].name, "SHIELD TV");
        strcpy(g_activities[1].id, "51843231");
        strcpy(g_activities[1].name, "Playstation 5");
        g_activity_count = 2;
    }

    /* Always ensure Power Off option exists at end */
    bool has_power_off = false;
    for (int i = 0; i < g_activity_count; i++) {
        if (strcmp(g_activities[i].id, "-1") == 0) { has_power_off = true; break; }
    }
    if (!has_power_off && g_activity_count < MAX_ACTIVITIES) {
        strcpy(g_activities[g_activity_count].id, "-1");
        strcpy(g_activities[g_activity_count].name, "Power Off");
        g_activity_count++;
    }
}

/* Map Linux event0 keycode to Logitech Harmony RF key code */
static uint16_t linux_to_harmony_rf_key(int code) {
    switch (code) {
        /* Activities & Power */
        case 116: return 0x01EC; /* KEY_POWER -> PowerOffActivity */

        /* Navigation */
        case 103: return 0x0052; /* KEY_UP -> DirectionUp */
        case 108: return 0x0051; /* KEY_DOWN -> DirectionDown */
        case 105: return 0x0050; /* KEY_LEFT -> DirectionLeft */
        case 106: return 0x004F; /* KEY_RIGHT -> DirectionRight */
        case 352: /* KEY_OK */
        case 28:  return 0x0058; /* KEY_ENTER -> Select */
        case 174: /* KEY_EXIT */
        case 158: return 0x0225; /* KEY_BACK -> Back */
        case 139: /* KEY_MENU */
        case 127: return 0x0065; /* KEY_COMPOSE -> Menu */
        case 159: return 0x0094; /* KEY_FORWARD -> Exit */
        case 358: return 0x01FF; /* KEY_INFO -> Info */
        case 395: return 0x009A; /* KEY_LIST -> Dvr */

        /* Volume & Channel */
        case 115: return 0x00E9; /* KEY_VOLUMEUP -> VolumeUp */
        case 114: return 0x00EA; /* KEY_VOLUMEDOWN -> VolumeDown */
        case 113: return 0x00E2; /* KEY_MUTE -> VolumeMute */
        case 402: return 0x009C; /* KEY_CHANNELUP -> ChannelUp */
        case 403: return 0x009D; /* KEY_CHANNELDOWN -> ChannelDown */
        case 412: return 0x0224; /* KEY_PREVIOUS -> PrevChannel */

        /* Transport / Playback */
        case 207: return 0x00B0; /* KEY_PLAY -> Play */
        case 119: return 0x00B1; /* KEY_PAUSE -> Pause */
        case 167: return 0x00B2; /* KEY_RECORD -> Record */
        case 389: return 0x00B3; /* KEY_FASTFORWARD -> FastForward */
        case 168: return 0x00B4; /* KEY_REWIND -> Rewind */
        case 128: return 0x00B7; /* KEY_STOP -> Stop */

        /* Color Buttons */
        case 398: return 0x01F7; /* KEY_RED -> Red */
        case 399: return 0x01F6; /* KEY_GREEN -> Green */
        case 400: return 0x01F5; /* KEY_YELLOW -> Yellow */
        case 401: return 0x01F4; /* KEY_BLUE -> Blue */

        /* Home Automation */
        case 148: return 0x0FF2; /* KEY_PROG1 -> Ha1 (Light) */
        case 149: return 0x0FF3; /* KEY_PROG2 -> Ha2 (Sun/Brightness) */
        case 202: return 0x0FF4; /* KEY_PROG3 -> Ha3 (Power/Plug) */
        case 203: return 0x0FF5; /* KEY_PROG4 -> Ha4 (Socket) */
        case 78:  return 0x0FF0; /* KEY_KPPLUS -> RockerUp */
        case 74:  return 0x0FF1; /* KEY_KPMINUS -> RockerDown */

        default:  return 0;
    }
}

static void rf_send_button_event(uint16_t key_code, int value);

static uint16_t g_held_rf_code = 0;
static pthread_mutex_t g_repeat_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_repeat_cond = PTHREAD_COND_INITIALIZER;

static void *key_repeat_thread(void *arg) {
    (void)arg;
    while (g_running) {
        pthread_mutex_lock(&g_repeat_mutex);
        while (g_held_rf_code == 0 && g_running) {
            pthread_cond_wait(&g_repeat_cond, &g_repeat_mutex);
        }
        if (!g_running) {
            pthread_mutex_unlock(&g_repeat_mutex);
            break;
        }
        uint16_t key = g_held_rf_code;
        pthread_mutex_unlock(&g_repeat_mutex);

        /* 300ms initial hold delay */
        usleep(300000);

        while (g_running) {
            pthread_mutex_lock(&g_repeat_mutex);
            if (g_held_rf_code != key) {
                pthread_mutex_unlock(&g_repeat_mutex);
                break;
            }
            pthread_mutex_unlock(&g_repeat_mutex);

            /* Transmit repeated press event */
            rf_send_button_event(key, 1);
            usleep(100000); /* 100ms repeat rate */
        }
    }
    return NULL;
}

static pthread_mutex_t g_rf_write_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint8_t g_remote_slot = 0x01;

/* Send a Codex RF frame over /dev/rfspi using 30-byte eQuad end-device framing */
static int rf_send_frame(rf_frame_t *frame) {
    if (g_rf_fd < 0) return -1;
    uint8_t raw[30];
    memset(raw, 0, sizeof(raw));
    raw[0] = 0x12;
    raw[1] = frame->seq;
    raw[2] = frame->hdr_flag ? frame->hdr_flag : (0x40 | (frame->seq & 0x0F));
    raw[3] = frame->msg_type;
    raw[4] = frame->flags;
    memcpy(&raw[5], frame->payload, 25);
    pthread_mutex_lock(&g_rf_write_mutex);
    int ret = write(g_rf_fd, raw, sizeof(raw));
    pthread_mutex_unlock(&g_rf_write_mutex);
    return ret;
}

/* Send activity sync query to Hub */
static void rf_send_sync_query(void) {
    if (g_rf_fd < 0) return;
    rf_frame_t f;
    uint8_t payload[25] = {0};
    payload[0] = 0x01; /* activity only */
    rf_build_frame(&f, g_remote_slot, RF_MSG_SYNC_REQUEST, ++g_tx_seq, 0, payload, 1);
    rf_send_frame(&f);
    printf("[*] RF TX SYNC_REQUEST seq=%d slot=%d (byte1=0x%02X)\n", f.seq, f.dev_slot & 0x0F, f.dev_slot);
    fflush(stdout);
}

/* Request full config from Hub */
static void rf_send_full_sync_request(void) {
    if (g_rf_fd < 0) return;
    rf_frame_t f;
    uint8_t payload[25] = {0};
    payload[0] = 0x02; /* full config */
    rf_build_frame(&f, g_remote_slot, RF_MSG_SYNC_REQUEST, ++g_tx_seq, 0, payload, 1);
    rf_send_frame(&f);
    printf("[*] RF TX FULL_SYNC_REQUEST seq=%d slot=%d (byte1=0x%02X)\n", f.seq, f.dev_slot & 0x0F, f.dev_slot);
    fflush(stdout);
}

/* Transmit button event over RF to Hub */
static void rf_send_button_event(uint16_t key_code, int value) {
    if (key_code == 0 || g_rf_fd < 0) return;
    if (value < 0 || value > 2) return;

    const char *short_name = rf_short_button_name(key_code);
    uint8_t state = (value == 1) ? 0x01 : ((value == 2) ? 0x02 : 0x00);

    rf_frame_t f;
    uint8_t payload[26] = {0};
    payload[0] = (uint8_t)(key_code >> 8);
    payload[1] = (uint8_t)(key_code & 0xFF);
    payload[2] = state;
    strncpy((char *)&payload[3], short_name, 12);
    rf_build_frame(&f, g_remote_slot, RF_MSG_BUTTON_PRESS, ++g_tx_seq, 0, payload, 15);

    rf_send_frame(&f);

    printf("[*] RF TX BUTTON_PRESS: 0x%04X %s %s\n", key_code,
           state == 0x01 ? "press" : (state == 0x02 ? "hold" : "release"), short_name);
}

/* Transmit Activity Switch command over RF */
static void rf_send_activity_start(const char *act_id) {
    if (g_rf_fd < 0) return;

    rf_frame_t f;
    uint8_t payload[26] = {0};
    strncpy((char *)payload, act_id, 12);
    rf_build_frame(&f, g_remote_slot, RF_MSG_ACTIVITY_REQUEST, ++g_tx_seq, 0, payload, 12);

    rf_send_frame(&f);

    printf("[*] RF TX ACTIVITY_REQUEST: %s\n", act_id);
}

/* Transmit Device command over RF */
static void rf_send_device_command(const char *dev_id, const char *cmd) {
    if (g_rf_fd < 0) return;

    rf_frame_t f;
    uint8_t payload[26] = {0};
    strncpy((char *)payload, dev_id, 10);
    strncpy((char *)&payload[10], cmd, 16);
    rf_build_frame(&f, g_remote_slot, RF_MSG_DEVICE_CMD_REQUEST, ++g_tx_seq, 0, payload, 26);

    rf_send_frame(&f);

    printf("[*] RF TX DEVICE_CMD: dev=%s cmd=%s\n", dev_id, cmd);
}

/* Execute command/activity for given item index */
static void execute_device_cmd_item(int idx) {
    char name[32] = {0}, cmd[32] = {0}, dev[32] = {0};
    uint8_t atype = 1;
    get_device_cmd_item(idx, name, sizeof(name), cmd, sizeof(cmd), dev, sizeof(dev), &atype);
    if (atype == 2) {
        printf("[*] Triggering Activity: %s\n", dev);
        rf_send_activity_start(dev);
    } else if (atype == 3) {
        printf("[*] Triggering Activity Stop\n");
        rf_send_activity_start("-1");
    } else {
        printf("[*] Triggering Device Command: dev=%s cmd=%s\n", dev, cmd);
        rf_send_device_command(dev, cmd);
    }
}

/* Handle incoming Codex RF protocol frame on Remote */
static void handle_codex_frame_remote(const rf_frame_t *f) {
    if (f->dev_slot & 0x0F) g_remote_slot = f->dev_slot & 0x0F;

    switch (f->msg_type) {

        case RF_MSG_ACTIVITY_SYNC: {
            char act_id[13] = {0};
            char act_name[17] = {0};
            memcpy(act_id, f->payload, 12);
            memcpy(act_name, &f->payload[12], 14);

            if (strcmp(g_active_activity_id, act_id) != 0) {
                printf("[*] Hub Activity Sync: id=%s name=%s\n", act_id, act_name);
                strncpy(g_active_activity_id, act_id, sizeof(g_active_activity_id) - 1);
                strncpy(g_active_activity_name, act_name, sizeof(g_active_activity_name) - 1);
                reset_idle_timer();
                render_screen();
            }
            break;
        }

        case RF_MSG_CONFIG_END: {
            uint8_t phase = f->payload[0];
            uint8_t version = f->payload[1];
            printf("[*] CONFIG_END: phase=%s version=%d\n", phase == 0 ? "clear" : "complete", version);
            if (phase == 0x00) {
                g_rf_activity_count = 0;
                g_rf_device_count = 0;
                g_rf_button_count = 0;
            } else {
                g_config_version = version;
                g_has_hub_config = true;
                save_rf_config();
                g_activity_count = 0;
                for (int i = 0; i < g_rf_activity_count && g_activity_count < MAX_ACTIVITIES; i++) {
                    strncpy(g_activities[g_activity_count].id, g_rf_activities[i].id, sizeof(g_activities[0].id) - 1);
                    strncpy(g_activities[g_activity_count].name, g_rf_activities[i].name, sizeof(g_activities[0].name) - 1);
                    g_activity_count++;
                }
                if (g_activity_count < MAX_ACTIVITIES) {
                    strcpy(g_activities[g_activity_count].id, "-1");
                    strcpy(g_activities[g_activity_count].name, "Power Off");
                    g_activity_count++;
                }
                render_screen();
            }
            rf_frame_t ack;
            rf_build_ack(&ack, g_remote_slot, f->seq, 0x00);
            rf_send_frame(&ack);
            break;
        }

        case RF_MSG_ACTIVITY_LIST_CHUNK: {
            uint8_t idx = f->payload[0];
            if (idx < MAX_RF_ACTIVITIES) {
                memcpy(g_rf_activities[idx].id, &f->payload[2], 10);
                g_rf_activities[idx].id[10] = '\0';
                memcpy(g_rf_activities[idx].name, &f->payload[12], 14);
                g_rf_activities[idx].name[14] = '\0';
                if (idx >= g_rf_activity_count) g_rf_activity_count = idx + 1;
                printf("[*] Activity chunk %d: %s (%s)\n", idx, g_rf_activities[idx].name, g_rf_activities[idx].id);
            }
            rf_frame_t ack;
            rf_build_ack(&ack, g_remote_slot, f->seq, 0x00);
            rf_send_frame(&ack);
            break;
        }

        case RF_MSG_DEVICE_LIST_CHUNK: {
            uint8_t idx = f->payload[0];
            if (idx < MAX_RF_DEVICES) {
                memcpy(g_rf_devices[idx].id, &f->payload[2], 10);
                g_rf_devices[idx].id[10] = '\0';
                memcpy(g_rf_devices[idx].name, &f->payload[12], 14);
                g_rf_devices[idx].name[14] = '\0';
                if (idx >= g_rf_device_count) g_rf_device_count = idx + 1;
            }
            rf_frame_t ack;
            rf_build_ack(&ack, g_remote_slot, f->seq, 0x00);
            rf_send_frame(&ack);
            break;
        }

        case RF_MSG_BUTTON_LIST_CHUNK: {
            uint8_t idx = f->payload[0];
            if (idx < MAX_RF_BUTTONS) {
                g_rf_buttons[idx].context_type = f->payload[2];
                memcpy(g_rf_buttons[idx].context_id, &f->payload[3], 9);
                g_rf_buttons[idx].context_id[9] = '\0';
                memcpy(g_rf_buttons[idx].label, &f->payload[12], 12);
                g_rf_buttons[idx].label[12] = '\0';
                g_rf_buttons[idx].action_type = f->payload[24];
                if (idx >= g_rf_button_count) g_rf_button_count = idx + 1;
            }
            rf_frame_t ack;
            rf_build_ack(&ack, g_remote_slot, f->seq, 0x00);
            rf_send_frame(&ack);
            break;
        }

        case RF_MSG_SETTINGS_PUSH: {
            g_brightness_val = f->payload[0];
            if (g_brightness_val < 1) g_brightness_val = 1;
            if (g_brightness_val > 5) g_brightness_val = 5;
            g_touch_enabled = (f->payload[1] != 0);
            g_gyro_enabled = (f->payload[2] != 0);
            g_timeout_seconds = ((int)f->payload[3] << 8) | f->payload[4];
            set_backlight_level(g_brightness_val);
            save_settings_conf();
            printf("[*] Settings push: bright=%d touch=%d gyro=%d timeout=%d\n",
                   g_brightness_val, g_touch_enabled, g_gyro_enabled, g_timeout_seconds);
            rf_frame_t ack;
            rf_build_ack(&ack, g_remote_slot, f->seq, 0x00);
            rf_send_frame(&ack);
            break;
        }

        case RF_MSG_PING: {
            rf_frame_t ack;
            rf_build_ack(&ack, g_remote_slot, f->seq, 0x00);
            rf_send_frame(&ack);
            break;
        }

        default:
            printf("[*] Remote: unknown Codex frame type 0x%02X\n", f->msg_type);
            break;
    }
}

/* Background thread: blocking read on /dev/rfspi for Activity Sync and commands from Hub */
static void *rf_rx_worker(void *arg) {
    (void)arg;
    uint8_t pkt[64];

    while (g_running) {
        if (g_rf_fd < 0) { usleep(100000); continue; }
        ssize_t n = read(g_rf_fd, pkt, sizeof(pkt));
        if (n <= 0) {
            if (n < 0 && errno != EINTR) usleep(50000);
            continue;
        }

        /* Verbose debug: log ALL raw RX */
        printf("[*] RF RX raw (%zd bytes): ", n);
        for (ssize_t i = 0; i < n && i < 40; i++) printf("%02X ", pkt[i]);
        printf("\n");
        fflush(stdout);

        if (n == 7 && pkt[0] == 0x10 && pkt[2] == 0x41) {
            bool link_lost = (pkt[4] & 0x40) != 0;
            printf("[*] Remote RF Link Status: %s (slot=%d byte4=0x%02X byte5=0x%02X byte6=0x%02X)\n",
                   link_lost ? "LOST" : "ESTABLISHED", pkt[1], pkt[4], pkt[5], pkt[6]);
            fflush(stdout);
        }

        if (rf_is_valid_frame(pkt, (size_t)n)) {
            rf_frame_t f;
            memset(&f, 0, sizeof(f));
            if (pkt[0] == 0x20 && n >= 32) {
                if ((pkt[4] & 0xE0) == 0x40 || (pkt[4] & 0xE0) == 0x60) {
                    f.report_id = 0x20;
                    f.dev_slot = pkt[1] & 0x0F;
                    f.sub_id = 0x12;
                    f.seq = pkt[3];
                    f.hdr_flag = pkt[4];
                    f.msg_type = pkt[5];
                    f.flags = pkt[6];
                    memcpy(f.payload, &pkt[7], sizeof(f.payload));
                } else {
                    memcpy(&f, pkt, sizeof(f));
                }
            } else if (pkt[0] == 0x12 && n >= 30) {
                if ((pkt[2] & 0xE0) == 0x40 || (pkt[2] & 0xE0) == 0x60) {
                    f.report_id = 0x20;
                    f.dev_slot = 0x01;
                    f.sub_id = 0x12;
                    f.seq = pkt[1];
                    f.hdr_flag = pkt[2];
                    f.msg_type = pkt[3];
                    f.flags = pkt[4];
                    memcpy(f.payload, &pkt[5], sizeof(f.payload));
                } else {
                    f.report_id = 0x20;
                    f.dev_slot = 0x01;
                    f.sub_id = 0x12;
                    f.seq = pkt[1];
                    f.hdr_flag = 0x40;
                    f.msg_type = pkt[2];
                    f.flags = pkt[3];
                    memcpy(f.payload, &pkt[4], sizeof(f.payload));
                }
            }
            printf("[*] RF RX -> Codex frame type=0x%02X seq=%d\n", f.msg_type, f.seq);
            fflush(stdout);
            handle_codex_frame_remote(&f);
        } else {
            /* Not a Codex frame — check if it's a 0x42 container with embedded data */
            if (n >= 32 && pkt[0] == 0x20 && pkt[2] == 0x42) {
                printf("[*] RF RX eQuad container (0x42): parsing payload at offset 3\n");
                /* The 0x42 container holds eQuad payload starting at byte 3 */
                rf_frame_t f;
                memset(&f, 0, sizeof(f));
                f.report_id = 0x20;
                f.dev_slot = pkt[1];
                f.sub_id = 0x12;
                if ((pkt[4] & 0xE0) == 0x40 || (pkt[4] & 0xE0) == 0x60) {
                    f.seq = pkt[3];
                    f.hdr_flag = pkt[4];
                    f.msg_type = pkt[5];
                    f.flags = pkt[6];
                    memcpy(f.payload, &pkt[7], sizeof(f.payload));
                } else {
                    f.seq = pkt[3];
                    f.msg_type = pkt[4];
                    f.flags = pkt[5];
                    memcpy(f.payload, &pkt[6], sizeof(f.payload));
                }
                /* Validate msg_type range */
                if ((f.msg_type >= RF_MSG_ACTIVITY_SYNC && f.msg_type <= RF_MSG_CONFIG_END) ||
                    (f.msg_type >= RF_MSG_BUTTON_PRESS && f.msg_type <= RF_MSG_SYNC_REQUEST) ||
                    f.msg_type == RF_MSG_ACK || f.msg_type == RF_MSG_PING) {
                    printf("[*] RF RX 0x42 -> Codex frame type=0x%02X seq=%d\n", f.msg_type, f.seq);
                    fflush(stdout);
                    handle_codex_frame_remote(&f);
                }
            }
            /* Log as legacy for any non-Codex packet */
            if (pkt[0] != 0x20 || pkt[2] != 0x42) {
                printf("[*] Remote RF RX legacy (%zd bytes): ", n);
                for (ssize_t i = 0; i < n; i++) printf("%02X ", pkt[i]);
                printf("\n");
                fflush(stdout);
            }
        }
    }
    return NULL;
}

/* Render Header Tab Bar */
static void render_header(void) {
    fb_fill_rect(0, 0, FB_WIDTH, 28, RGB_HEADER);

    const char *tabs[3] = {"ACTS", "DEVS", "SETS"};
    for (int i = 0; i < 3; i++) {
        int x = 6 + i * 78;
        bool is_sel = (g_current_view == (ui_view_t)i);
        if (is_sel) {
            fb_fill_rect(x, 2, 72, 24, RGB_CYAN);
            fb_draw_text(x + 16, 8, tabs[i], RGB_BLACK, RGB_CYAN, 1);
        } else {
            fb_draw_text(x + 16, 8, tabs[i], RGB_TEXT_MUTED, RGB_HEADER, 1);
        }
    }
}

/* Render Footer / Status Bar */
static void render_footer(void) {
    fb_fill_rect(0, FB_HEIGHT - 24, FB_WIDTH, 24, RGB_HEADER);
    char buf[48];
    snprintf(buf, sizeof(buf), "%s [%s]", g_active_activity_name, g_screen_nav_mode ? "NAV" : "DEV");
    fb_draw_text(8, FB_HEIGHT - 17, buf, g_screen_nav_mode ? RGB_CYAN : RGB_GREEN, RGB_HEADER, 1);

    const char *mode_hint = g_screen_nav_mode ? "Menu:Exit" : "Menu:Nav";
    fb_draw_text(FB_WIDTH - 76, FB_HEIGHT - 17, mode_hint, RGB_TEXT_MUTED, RGB_HEADER, 1);
}

/* Render Activity List View */
static void render_activities_view(void) {
    int start_y = 36;
    int card_h = 38;
    int spacing = 6;

    for (int i = 0; i < g_activity_count && i < 6; i++) {
        int y = start_y + i * (card_h + spacing);
        bool is_sel = (i == g_selected_activity);
        bool is_active = (strcmp(g_activities[i].id, g_active_activity_id) == 0);

        uint16_t bg = is_sel ? RGB_CARD_SEL : RGB_CARD;
        uint16_t fg = is_sel ? RGB_WHITE : (is_active ? RGB_GREEN : RGB_WHITE);

        fb_fill_rect(8, y, FB_WIDTH - 16, card_h, bg);

        /* Indicator dot */
        uint16_t dot_color = is_active ? RGB_GREEN : (is_sel ? RGB_CYAN : RGB_TEXT_MUTED);
        fb_fill_rect(16, y + 14, 8, 8, dot_color);

        /* Text */
        fb_draw_text(32, y + 12, g_activities[i].name, fg, bg, 1);

        if (is_sel) {
            fb_draw_text(FB_WIDTH - 30, y + 12, ">", RGB_CYAN, bg, 1);
        }
    }
}

/* Render Devices View */
static void render_devices_view(void) {
    int start_y = 36;
    int card_h = 34;
    int spacing = 5;
    int total = get_device_cmd_count();

    for (int i = 0; i < total && i < 7; i++) {
        int y = start_y + i * (card_h + spacing);
        bool is_sel = (i == g_selected_device_cmd);
        uint16_t bg = is_sel ? RGB_CARD_SEL : RGB_CARD;

        char name[32] = {0}, cmd[32] = {0}, dev[32] = {0};
        uint8_t atype = 1;
        get_device_cmd_item(i, name, sizeof(name), cmd, sizeof(cmd), dev, sizeof(dev), &atype);

        fb_fill_rect(8, y, FB_WIDTH - 16, card_h, bg);
        fb_draw_text(14, y + 10, name, is_sel ? RGB_CYAN : RGB_TEXT_MUTED, bg, 1);
        fb_draw_text(105, y + 10, cmd, RGB_WHITE, bg, 1);

        if (is_sel) {
            fb_draw_text(FB_WIDTH - 24, y + 10, ">", RGB_CYAN, bg, 1);
        }
    }
}

/* Render Settings View */
static void render_settings_view(void) {
    int start_y = 36;
    int card_h = 42;
    int spacing = 6;

    const char *labels[SETTING_COUNT] = {
        "Touchscreen",
        "Motion / Gyro Wake",
        "Display Timeout",
        "Display Brightness",
        "Screen Blank"
    };

    char values[SETTING_COUNT][24];
    snprintf(values[0], sizeof(values[0]), "[ %s ]", g_touch_enabled ? "ON" : "OFF");
    snprintf(values[1], sizeof(values[1]), "[ %s ]", g_gyro_enabled ? "ON" : "OFF");
    snprintf(values[2], sizeof(values[2]), "[ %ds ]", g_timeout_seconds);
    snprintf(values[3], sizeof(values[3]), "[ LVL %d ]", g_brightness_val);
    snprintf(values[4], sizeof(values[4]), "[ %s ]", g_screen_on ? "ACTIVE" : "BLANK");

    for (int i = 0; i < SETTING_COUNT; i++) {
        int y = start_y + i * (card_h + spacing);
        bool is_sel = (i == g_selected_setting);
        uint16_t bg = is_sel ? RGB_CARD_SEL : RGB_CARD;

        fb_fill_rect(8, y, FB_WIDTH - 16, card_h, bg);
        fb_draw_text(14, y + 8, labels[i], is_sel ? RGB_WHITE : RGB_TEXT_MUTED, bg, 1);

        uint16_t val_color = RGB_CYAN;
        if (i == 0) val_color = g_touch_enabled ? RGB_GREEN : RGB_RED;
        if (i == 1) val_color = g_gyro_enabled ? RGB_GREEN : RGB_RED;

        fb_draw_text(14, y + 24, values[i], val_color, bg, 1);
    }
}

/* Main Display Render Routine */
static void render_screen(void) {
    if (!g_fb_mem || !g_screen_on) return;
    pthread_mutex_lock(&g_render_mutex);

    /* Clear background */
    fb_fill_rect(0, 0, FB_WIDTH, FB_HEIGHT, RGB_BG);

    render_header();

    switch (g_current_view) {
        case VIEW_ACTIVITIES: render_activities_view(); break;
        case VIEW_DEVICES:    render_devices_view();    break;
        case VIEW_SETTINGS:   render_settings_view();   break;
        default: break;
    }

    render_footer();

    /* Replicate frame across all virtual buffers so any LCDC pan position displays UI */
    if (g_fb_size >= FB_SIZE * 2) {
        memcpy((uint8_t *)g_fb_mem + FB_SIZE, g_fb_mem, FB_SIZE);
    }
    if (g_fb_size >= FB_SIZE * 3) {
        memcpy((uint8_t *)g_fb_mem + (FB_SIZE * 2), g_fb_mem, FB_SIZE);
    }

    pthread_mutex_unlock(&g_render_mutex);
}

/* User interaction resets the idle sleep timer */
static void reset_idle_timer(void) {
    g_last_activity_time = time(NULL);
    if (!g_screen_on) {
        fb_set_blank(false);
        render_screen();
    }
}

/* Motion / Gyro Watcher Thread (polls sysfs xyz delta) */
static void *gyro_watcher(void *arg) {
    (void)arg;
    int last_x = 0, last_y = 0, last_z = 0;
    bool has_baseline = false;

    /* Initialize accelerometer sampling */
    int fdr = open(GYRO_DATA_RATE_SYS, O_WRONLY);
    if (fdr >= 0) {
        write(fdr, "25\n", 3);
        close(fdr);
    }

    while (g_running) {
        usleep(200000); /* 200ms */
        if (!g_gyro_enabled) continue;

        int fd = open(GYRO_XYZ_SYS, O_RDONLY);
        if (fd < 0) continue;

        char buf[64];
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        close(fd);

        if (n > 0) {
            buf[n] = '\0';
            int x = 0, y = 0, z = 0;
            if (sscanf(buf, "%d %d %d", &x, &y, &z) == 3) {
                if (!has_baseline) {
                    last_x = x; last_y = y; last_z = z;
                    has_baseline = true;
                    continue;
                }
                int dx = abs(x - last_x);
                int dy = abs(y - last_y);
                int dz = abs(z - last_z);
                last_x = x; last_y = y; last_z = z;

                /* Only wake screen if currently blanked/off (pick up to wake gesture) */
                if (!g_screen_on) {
                    if (dx > 200 || dy > 200 || dz > 200) {
                        printf("[*] Gyro wake gesture detected (dx=%d, dy=%d, dz=%d)\n", dx, dy, dz);
                        reset_idle_timer();
                    }
                }
            }
        }
    }
    return NULL;
}

static volatile sig_atomic_t g_wake_requested = 0;
static void sig_wake_handler(int sig) {
    (void)sig;
    g_wake_requested = 1;
}

/* Power / Timeout Watcher Thread */
static void *power_manager(void *arg) {
    (void)arg;
    while (g_running) {
        usleep(200000); /* 200ms check */
        if (g_wake_requested) {
            g_wake_requested = 0;
            reset_idle_timer();
        }
        if (g_timeout_seconds > 0 && g_screen_on) {
            time_t now = time(NULL);
            if (now - g_last_activity_time > g_timeout_seconds) {
                printf("[*] Idle timeout reached (%ds), blanking display\n", g_timeout_seconds);
                fb_set_blank(true);
            }
        }
    }
    return NULL;
}

/* Handle Touchscreen tap at coordinates (x, y) */
static void handle_screen_touch_tap(int x, int y) {
    reset_idle_timer();
    if (!g_screen_on) {
        fb_set_blank(false);
        render_screen();
        return;
    }
    if (!g_touch_enabled) return;
    printf("[*] Touch tap: screen x=%d, y=%d\n", x, y);

    /* Header Tab Area: y < 36 */
    if (y < 36) {
        if (x < 80) g_current_view = VIEW_ACTIVITIES;
        else if (x < 160) g_current_view = VIEW_DEVICES;
        else g_current_view = VIEW_SETTINGS;
        g_screen_nav_mode = true;
        render_screen();
        return;
    }

    /* Capacitive touch buttons below LCD screen (screen height is 320): y >= 320 */
    if (y >= 320) {
        if (x < 120) {
            printf("[*] Capacitive tap: Activities (x=%d, y=%d)\n", x, y);
            g_current_view = VIEW_ACTIVITIES;
        } else {
            printf("[*] Capacitive tap: Devices (x=%d, y=%d)\n", x, y);
            g_current_view = VIEW_DEVICES;
        }
        g_screen_nav_mode = true;
        render_screen();
        return;
    }

    /* Content Area */
    int start_y = 38;
    if (g_current_view == VIEW_ACTIVITIES) {
        int card_h = 38;
        int spacing = 6;
        for (int i = 0; i < g_activity_count && i < 6; i++) {
            int cy = start_y + i * (card_h + spacing);
            if (y >= cy && y < cy + card_h + spacing) {
                g_selected_activity = i;
                activity_item_t *sel = &g_activities[i];
                strncpy(g_active_activity_id, sel->id, sizeof(g_active_activity_id) - 1);
                strncpy(g_active_activity_name, sel->name, sizeof(g_active_activity_name) - 1);
                printf("[*] Touch started Activity: %s (id=%s)\n", sel->name, sel->id);
                rf_send_activity_start(sel->id);
                g_screen_nav_mode = false; /* Switch back to device control */
                render_screen();
                return;
            }
        }
    } else if (g_current_view == VIEW_DEVICES) {
        int card_h = 34;
        int spacing = 5;
        int total = get_device_cmd_count();
        for (int i = 0; i < total && i < 7; i++) {
            int cy = start_y + i * (card_h + spacing);
            if (y >= cy && y < cy + card_h + spacing) {
                g_selected_device_cmd = i;
                execute_device_cmd_item(i);
                render_screen();
                return;
            }
        }
    } else if (g_current_view == VIEW_SETTINGS) {
        int card_h = 42;
        int spacing = 6;
        for (int i = 0; i < SETTING_COUNT; i++) {
            int cy = start_y + i * (card_h + spacing);
            if (y >= cy && y < cy + card_h) {
                g_selected_setting = i;
                switch (i) {
                    case SETTING_TOUCH:
                        set_touch_enabled(!g_touch_enabled);
                        break;
                    case SETTING_GYRO:
                        set_gyro_enabled(!g_gyro_enabled);
                        break;
                    case SETTING_TIMEOUT:
                        g_timeout_seconds = (g_timeout_seconds == 30) ? 60 : ((g_timeout_seconds == 60) ? 120 : 30);
                        break;
                    case SETTING_BRIGHTNESS:
                        g_brightness_val = (g_brightness_val % 5) + 1;
                        set_backlight_level(g_brightness_val);
                        break;
                    case SETTING_BLANK:
                        fb_set_blank(true);
                        break;
                    default: break;
                }
                save_settings_conf();
                render_screen();
                return;
            }
        }
    }
}

/* Navigation & Button Handling */
static void handle_navigation_key(int code, int value) {
    reset_idle_timer();

    /* Capacitive touch buttons below screen reported on event2 */
    if (code == 102) { /* KEY_HOME: "Activities" touch button */
        printf("[*] Capacitive key pressed: Activities (code=102, val=%d)\n", value);
        g_current_view = VIEW_ACTIVITIES;
        g_screen_nav_mode = true;
        render_screen();
        return;
    }
    if (code == 364) { /* KEY_FAVORITES: "Devices" touch button */
        printf("[*] Capacitive key pressed: Devices (code=364, val=%d)\n", value);
        g_current_view = VIEW_DEVICES;
        g_screen_nav_mode = true;
        render_screen();
        return;
    }

    if (value != 1 && value != 2) return; /* Only on press/repeat for navigation/physical keys */

    /* Menu button (code 139) or Info (code 358) toggles screen navigation mode */
    if (code == 139 || code == 358) {
        g_screen_nav_mode = !g_screen_nav_mode;
        printf("[*] Mode toggled by Menu button: %s\n", g_screen_nav_mode ? "NAV (Screen)" : "DEV (Device)");
        render_screen();
        return;
    }

    /* In Screen Navigation Mode OR on Devices/Settings view: D-PAD and OK navigate on-screen cards */
    if (!g_screen_nav_mode && g_current_view != VIEW_DEVICES && g_current_view != VIEW_SETTINGS) {
        return;
    }

    switch (code) {
        case 103: /* KEY_UP (DirectionUp) */
            if (g_current_view == VIEW_ACTIVITIES) {
                if (g_selected_activity > 0) g_selected_activity--;
            } else if (g_current_view == VIEW_DEVICES) {
                if (g_selected_device_cmd > 0) g_selected_device_cmd--;
            } else if (g_current_view == VIEW_SETTINGS) {
                if (g_selected_setting > 0) g_selected_setting--;
            }
            render_screen();
            break;

        case 108: /* KEY_DOWN (DirectionDown) */
            if (g_current_view == VIEW_ACTIVITIES) {
                if (g_selected_activity < g_activity_count - 1) g_selected_activity++;
            } else if (g_current_view == VIEW_DEVICES) {
                if (g_selected_device_cmd < get_device_cmd_count() - 1) g_selected_device_cmd++;
            } else if (g_current_view == VIEW_SETTINGS) {
                if (g_selected_setting < SETTING_COUNT - 1) g_selected_setting++;
            }
            render_screen();
            break;

        case 105: /* KEY_LEFT (DirectionLeft) */
            g_current_view = (g_current_view + VIEW_COUNT - 1) % VIEW_COUNT;
            render_screen();
            break;

        case 106: /* KEY_RIGHT (DirectionRight) */
            g_current_view = (g_current_view + 1) % VIEW_COUNT;
            render_screen();
            break;

        case 352: /* KEY_OK / Select */
        case 28:  /* KEY_ENTER */
            if (g_current_view == VIEW_ACTIVITIES) {
                activity_item_t *sel = &g_activities[g_selected_activity];
                strncpy(g_active_activity_id, sel->id, sizeof(g_active_activity_id) - 1);
                strncpy(g_active_activity_name, sel->name, sizeof(g_active_activity_name) - 1);
                printf("[*] Starting Activity: %s (id=%s)\n", sel->name, sel->id);
                rf_send_activity_start(sel->id);
                g_screen_nav_mode = false; /* Switch back to device control */
                render_screen();
            } else if (g_current_view == VIEW_DEVICES) {
                execute_device_cmd_item(g_selected_device_cmd);
                render_screen();
            } else if (g_current_view == VIEW_SETTINGS) {
                switch (g_selected_setting) {
                    case SETTING_TOUCH:
                        set_touch_enabled(!g_touch_enabled);
                        break;
                    case SETTING_GYRO:
                        set_gyro_enabled(!g_gyro_enabled);
                        break;
                    case SETTING_TIMEOUT:
                        g_timeout_seconds = (g_timeout_seconds == 30) ? 60 : ((g_timeout_seconds == 60) ? 120 : 30);
                        break;
                    case SETTING_BRIGHTNESS:
                        g_brightness_val = (g_brightness_val % 5) + 1;
                        set_backlight_level(g_brightness_val);
                        break;
                    case SETTING_BLANK:
                        fb_set_blank(true);
                        break;
                    default: break;
                }
                save_settings_conf();
                render_screen();
            }
            break;

        case 158: /* KEY_BACK */
            g_screen_nav_mode = false;
            render_screen();
            break;

        default:
            break;
    }
}




/* Find and open touchscreen device by probing device names */
static int find_touchscreen_fd(void) {
    const char *candidates[] = {
        "/dev/input/event2",
        "/dev/input/event3",
        "/dev/input/touchscreen0",
        "/dev/input/event1",
        "/dev/input/event0"
    };
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        int fd = open(candidates[i], O_RDONLY | O_NONBLOCK);
        if (fd >= 0) {
            char name[128] = {0};
            if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) >= 0) {
                if (strstr(name, "Clearpad") || strstr(name, "clearpad") ||
                    strstr(name, "touch") || strstr(name, "Touch")) {
                    printf("[+] Discovered touchscreen at %s: %s (fd=%d)\n", candidates[i], name, fd);
                    return fd;
                }
            }
            close(fd);
        }
    }
    return open(TOUCH_EVENT_DEV, O_RDONLY | O_NONBLOCK);
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGUSR1, sig_wake_handler);

    printf("[+] Starting Harmony Elite Custom Interface (codex_elite)...\n");

    /* Load persistent config from /data/codex_elite.conf if present */
    load_settings_conf();

    /* Parse flags (CLI overrides) */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--no-touch") == 0) g_touch_enabled = false;
        if (strcmp(argv[i], "--no-gyro") == 0)  g_gyro_enabled = false;
        if (strcmp(argv[i], "--blank") == 0)    g_screen_on = false;
    }

    load_activities();
    printf("[+] Loaded %d activities\n", g_activity_count);

    /* 1. Open Framebuffer */
    g_fb_fd = open(FB_DEV, O_RDWR);
    if (g_fb_fd >= 0) {
        struct fb_fix_screeninfo finfo;
        struct fb_var_screeninfo vinfo;
        if (ioctl(g_fb_fd, FBIOGET_FSCREENINFO, &finfo) == 0 &&
            ioctl(g_fb_fd, FBIOGET_VSCREENINFO, &vinfo) == 0) {
            g_fb_size = finfo.smem_len;
            g_fb_mem = (uint16_t *)mmap(NULL, g_fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, g_fb_fd, 0);
            if (g_fb_mem == MAP_FAILED) g_fb_mem = NULL;

            /* Switch active display to buffer 0 */
            vinfo.xoffset = 0;
            vinfo.yoffset = 0;
            ioctl(g_fb_fd, FBIOPAN_DISPLAY, &vinfo);
        } else {
            g_fb_mem = (uint16_t *)mmap(NULL, FB_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, g_fb_fd, 0);
            if (g_fb_mem == MAP_FAILED) g_fb_mem = NULL;
        }
    }

    fb_set_blank(!g_screen_on);
    reset_idle_timer();

    /* 2. Open Touch and Gyro nodes */
    g_touch_fd = find_touchscreen_fd();
    if (g_touch_fd < 0) {
        perror("[-] Failed to open touchscreen node");
    } else {
        printf("[+] Opened touchscreen fd=%d\n", g_touch_fd);
    }
    if (g_touch_fd >= 0 && !g_touch_enabled) {
        set_touch_enabled(false);
    }

    g_gyro_fd = open(GYRO_EVENT_DEV, O_RDONLY | O_NONBLOCK);
    if (g_gyro_fd >= 0 && !g_gyro_enabled) {
        set_gyro_enabled(false);
    }

    /* 3. Open RF SPI */
    g_rf_fd = open(RFSPI_DEV, O_RDWR);
    if (g_rf_fd >= 0) {
        printf("[+] Opened %s for RF TX/RX\n", RFSPI_DEV);
        /* 1. Init message engine (matches stock hal_remote) */
        uint8_t pkt_init[7] = {0x10, 0xFF, 0x80, 0x00, 0x00, 0x01, 0x00};
        if (write(g_rf_fd, pkt_init, sizeof(pkt_init)) < 0) {}
        usleep(30000);

        /* 2. Enable extended frames (matches stock hal_remote libhal_rf_hid_write) */
        uint8_t pkt_ext[7] = {0x10, 0xFF, 0x80, 0xFD, 0x01, 0x00, 0x00};
        if (write(g_rf_fd, pkt_ext, sizeof(pkt_ext)) < 0) {}
        usleep(30000);

        /* 3. Query paired Hub address (pipe 0x03) */
        uint8_t pkt_pipe[7] = {0x10, 0xFF, 0x83, 0xB5, 0x03, 0x00, 0x00};
        if (write(g_rf_fd, pkt_pipe, sizeof(pkt_pipe)) < 0) {}
        usleep(30000);

        /* 4. Set CC2544 active power mode (matches stock hal_remote hot_set_powermode) */
        uint8_t pkt_power[7] = {0x20, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00};
        if (write(g_rf_fd, pkt_power, sizeof(pkt_power)) < 0) {}
        usleep(30000);

        /* 5. Send Connection Packet (10 bytes) to synchronize frequency hopping with Hub */
        uint8_t pkt_conn[10] = {0x12, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        if (write(g_rf_fd, pkt_conn, sizeof(pkt_conn)) < 0) {}
        usleep(50000);

        /* Load cached config */
        load_rf_config();
    }

    /* 4. Open Physical Buttons */
    g_btn_fd = open(BTN_EVENT_DEV, O_RDONLY | O_NONBLOCK);
    if (g_btn_fd < 0) {
        perror("open " BTN_EVENT_DEV);
        return 1;
    }
    printf("[+] Listening on %s for physical buttons...\n", BTN_EVENT_DEV);

    /* Background Threads */
    pthread_t th_power, th_gyro, th_repeat, th_rf_rx;
    pthread_create(&th_power, NULL, power_manager, NULL);
    pthread_create(&th_gyro, NULL, gyro_watcher, NULL);
    pthread_create(&th_repeat, NULL, key_repeat_thread, NULL);
    pthread_create(&th_rf_rx, NULL, rf_rx_worker, NULL);



    /* Initial Render */
    render_screen();

    /* Event Loop with select() multiplexing physical buttons and touchscreen */
    while (g_running) {
        fd_set rfds;
        FD_ZERO(&rfds);
        int max_fd = -1;

        if (g_btn_fd >= 0) {
            FD_SET(g_btn_fd, &rfds);
            if (g_btn_fd > max_fd) max_fd = g_btn_fd;
        }
        if (g_touch_fd >= 0) {
            FD_SET(g_touch_fd, &rfds);
            if (g_touch_fd > max_fd) max_fd = g_touch_fd;
        }

        struct timeval tv = { .tv_sec = 0, .tv_usec = 100000 };
        int sel = select(max_fd + 1, &rfds, NULL, NULL, &tv);
        if (sel < 0) {
            if (errno == EINTR) continue;
            break;
        }

        /* 1. Physical Buttons (event0) */
        if (g_btn_fd >= 0 && FD_ISSET(g_btn_fd, &rfds)) {
            struct input_event ev;
            while (read(g_btn_fd, &ev, sizeof(ev)) == sizeof(ev)) {
                if (ev.type == EV_KEY) {
                    reset_idle_timer();

                    /* Mode Toggle: Menu button (139) switches between NAV and DEV */
                    if (ev.code == 139) {
                        if (ev.value == 1) {
                            g_screen_nav_mode = !g_screen_nav_mode;
                            printf("[*] Mode toggled by Menu button: %s\n", g_screen_nav_mode ? "NAV (Screen)" : "DEV (Device)");
                            render_screen();
                        }
                        continue;
                    }

                    if (g_screen_nav_mode || g_current_view == VIEW_DEVICES || g_current_view == VIEW_SETTINGS) {
                        /* Screen Navigation Mode: D-Pad and OK navigate and select on screen */
                        handle_navigation_key(ev.code, ev.value);

                        /* Non-nav buttons (Volume, Channel, Transport, HA) still send RF */
                        if (ev.code != 103 && ev.code != 108 && ev.code != 105 && ev.code != 106 &&
                            ev.code != 352 && ev.code != 28 && ev.code != 139) {
                            uint16_t rf_code = linux_to_harmony_rf_key(ev.code);
                            if (rf_code != 0) {
                                if (ev.value == 1) {
                                    rf_send_button_event(rf_code, 1);
                                    pthread_mutex_lock(&g_repeat_mutex);
                                    g_held_rf_code = rf_code;
                                    pthread_cond_signal(&g_repeat_cond);
                                    pthread_mutex_unlock(&g_repeat_mutex);
                                } else if (ev.value == 0) {
                                    pthread_mutex_lock(&g_repeat_mutex);
                                    if (g_held_rf_code == rf_code) g_held_rf_code = 0;
                                    pthread_mutex_unlock(&g_repeat_mutex);
                                    rf_send_button_event(rf_code, 0);
                                }
                            }
                        }
                    } else {
                        /* Device Control Mode: Forward directly over RF with auto-repeat */
                        uint16_t rf_code = linux_to_harmony_rf_key(ev.code);
                        if (rf_code != 0) {
                            if (ev.value == 1) {
                                rf_send_button_event(rf_code, 1);
                                pthread_mutex_lock(&g_repeat_mutex);
                                g_held_rf_code = rf_code;
                                pthread_cond_signal(&g_repeat_cond);
                                pthread_mutex_unlock(&g_repeat_mutex);
                            } else if (ev.value == 0) {
                                pthread_mutex_lock(&g_repeat_mutex);
                                if (g_held_rf_code == rf_code) g_held_rf_code = 0;
                                pthread_mutex_unlock(&g_repeat_mutex);
                                rf_send_button_event(rf_code, 0);
                            }
                        }
                    }
                }
            }
        }

        /* 2. Touchscreen & Capacitive Buttons (event2) */
        if (g_touch_fd >= 0 && FD_ISSET(g_touch_fd, &rfds)) {
            struct input_event ev;
            while (read(g_touch_fd, &ev, sizeof(ev)) == sizeof(ev)) {
                if (ev.type == EV_KEY) {
                    /* Capacitive buttons below screen: 102=Home/Activities, 364=Favorites/Devices */
                    if (ev.code == 102 || ev.code == 364) {
                        reset_idle_timer();
                        handle_navigation_key(ev.code, ev.value);
                    } else if (ev.code == 330) { /* BTN_TOUCH */
                        if (ev.value == 0) {
                            if (g_touch_is_down) {
                                handle_screen_touch_tap(g_touch_cur_x, g_touch_cur_y);
                                g_touch_is_down = false;
                            }
                        } else {
                            reset_idle_timer();
                            g_touch_is_down = true;
                        }
                    }
                } else if (ev.type == EV_ABS) {
                    if (ev.code == ABS_X || ev.code == ABS_MT_POSITION_X) {
                        g_touch_cur_x = ev.value;
                    } else if (ev.code == ABS_Y || ev.code == ABS_MT_POSITION_Y) {
                        g_touch_cur_y = ev.value;
                    } else if (ev.code == ABS_PRESSURE) {
                        if (ev.value > 0) {
                            reset_idle_timer();
                            g_touch_is_down = true;
                        } else if (ev.value == 0 && g_touch_is_down) {
                            handle_screen_touch_tap(g_touch_cur_x, g_touch_cur_y);
                            g_touch_is_down = false;
                        }
                    }
                }
            }
        }
    }

    printf("[+] Cleaning up hardware resources...\n");
    if (g_touch_fd >= 0) { ioctl(g_touch_fd, EVIOCGRAB, 0); close(g_touch_fd); }
    if (g_gyro_fd >= 0)  { ioctl(g_gyro_fd, EVIOCGRAB, 0);  close(g_gyro_fd); }
    if (g_rf_fd >= 0)    close(g_rf_fd);
    if (g_btn_fd >= 0)   close(g_btn_fd);
    if (g_fb_mem)        munmap(g_fb_mem, g_fb_size);
    if (g_fb_fd >= 0)    close(g_fb_fd);

    return 0;
}
