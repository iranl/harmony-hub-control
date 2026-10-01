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

#define FB_DEV              "/dev/fb0"
#define FB_BLANK_SYSFS      "/sys/class/graphics/fb0/blank"
#define BACKLIGHT_SYSFS     "/sys/class/backlight/ap3156_backlight/brightness"
#define BTN_LED_SYSFS       "/sys/class/leds/tps6507x:white/brightness"
#define BTN_EVENT_DEV       "/dev/input/event0"
#define GYRO_EVENT_DEV      "/dev/input/event1"
#define TOUCH_EVENT_DEV     "/dev/input/event2"
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

/* Device item */
typedef struct {
    char name[32];
    char command[32];
} device_item_t;

static const device_item_t g_device_commands[] = {
    {"TV", "PowerToggle"},
    {"TV", "InputHdmi1"},
    {"TV", "InputHdmi2"},
    {"Receiver", "PowerToggle"},
    {"Receiver", "Mute"},
    {"Shield", "Home"},
    {"Shield", "Back"}
};
#define DEVICE_CMD_COUNT ((int)(sizeof(g_device_commands) / sizeof(g_device_commands[0])))
static int g_selected_device_cmd = 0;

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
}

/* Touchscreen Enable / Disable (via EVIOCGRAB) */
static void set_touch_enabled(bool enable) {
    if (g_touch_fd >= 0) {
        int grab = enable ? 0 : 1;
        ioctl(g_touch_fd, EVIOCGRAB, grab);
    }
    g_touch_enabled = enable;
}

/* Gyro / Accelerometer Enable / Disable (via EVIOCGRAB) */
static void set_gyro_enabled(bool enable) {
    if (g_gyro_fd >= 0) {
        int grab = enable ? 0 : 1;
        ioctl(g_gyro_fd, EVIOCGRAB, grab);
    }
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
    fprintf(f, "screen=%d\n", g_screen_on ? 1 : 0);
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
    if (!f) {
        /* Fallback defaults */
        strcpy(g_activities[0].id, "53591842");
        strcpy(g_activities[0].name, "SHIELDTV");
        strcpy(g_activities[1].id, "53591844");
        strcpy(g_activities[1].name, "Playstation 5");
        strcpy(g_activities[2].id, "53591849");
        strcpy(g_activities[2].name, "Switch");
        strcpy(g_activities[3].id, "-1");
        strcpy(g_activities[3].name, "Power Off");
        g_activity_count = 4;
        return;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); return; }
    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);

    /* Lightweight string search for Name and Id- */
    char *ptr = buf;
    while (g_activity_count < MAX_ACTIVITIES - 1) {
        char *name_tag = strstr(ptr, "\"Name\":\"");
        if (!name_tag) break;
        name_tag += 8;
        char *name_end = strchr(name_tag, '\"');
        if (!name_end) break;

        char *id_tag = strstr(name_end, "\"Id-\":");
        if (!id_tag) break;
        id_tag += 6;

        int len = name_end - name_tag;
        if (len > 40) len = 40;
        strncpy(g_activities[g_activity_count].name, name_tag, len);
        g_activities[g_activity_count].name[len] = '\0';

        char id_buf[32] = {0};
        sscanf(id_tag, "%31[0-9-]", id_buf);
        strncpy(g_activities[g_activity_count].id, id_buf, sizeof(g_activities[g_activity_count].id) - 1);

        g_activity_count++;
        ptr = id_tag + 1;
    }
    free(buf);

    /* Always ensure Power Off option exists at end */
    strcpy(g_activities[g_activity_count].id, "-1");
    strcpy(g_activities[g_activity_count].name, "Power Off");
    g_activity_count++;
}

/* Transmit button event over RF to Hub */
static void rf_send_button_event(uint16_t key_code, int value) {
    if (g_rf_fd < 0) return;
    uint8_t pkt[7];
    pkt[0] = 0x10;
    pkt[1] = 0xFF;
    pkt[2] = 0x41;
    pkt[3] = (uint8_t)(key_code >> 8);
    pkt[4] = (uint8_t)(key_code & 0xFF);
    pkt[5] = (value == 1) ? 0x01 : ((value == 2) ? 0x02 : 0x00);
    pkt[6] = 0x00;
    write(g_rf_fd, pkt, sizeof(pkt));
}

/* Transmit Activity Switch command over RF */
static void rf_send_activity_start(const char *act_id) {
    if (g_rf_fd < 0) return;
    /* Format: HID++ Long Report 0x11, DevIdx=0xFF, SubID=0xFD (HOT), Cmd=0x01, ActID */
    uint8_t pkt[20] = {0};
    pkt[0] = 0x11;
    pkt[1] = 0xFF;
    pkt[2] = 0xFD;
    pkt[3] = 0x01; /* Command: Start Activity */
    strncpy((char *)&pkt[4], act_id, 15);
    write(g_rf_fd, pkt, sizeof(pkt));
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
    snprintf(buf, sizeof(buf), "CURRENT: %s", g_active_activity_name);
    fb_draw_text(10, FB_HEIGHT - 17, buf, RGB_GREEN, RGB_HEADER, 1);
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

    for (int i = 0; i < DEVICE_CMD_COUNT && i < 7; i++) {
        int y = start_y + i * (card_h + spacing);
        bool is_sel = (i == g_selected_device_cmd);
        uint16_t bg = is_sel ? RGB_CARD_SEL : RGB_CARD;

        fb_fill_rect(8, y, FB_WIDTH - 16, card_h, bg);
        fb_draw_text(16, y + 10, g_device_commands[i].name, is_sel ? RGB_CYAN : RGB_TEXT_MUTED, bg, 1);
        fb_draw_text(90, y + 10, g_device_commands[i].command, RGB_WHITE, bg, 1);

        if (is_sel) {
            fb_draw_text(FB_WIDTH - 28, y + 10, "*", RGB_CYAN, bg, 1);
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

/* Motion / Gyro Watcher Thread */
static void *gyro_watcher(void *arg) {
    (void)arg;
    struct input_event ev;
    while (g_running) {
        if (g_gyro_fd < 0 || !g_gyro_enabled) {
            sleep(1);
            continue;
        }
        ssize_t n = read(g_gyro_fd, &ev, sizeof(ev));
        if (n == sizeof(ev)) {
            if (ev.type == EV_ABS) {
                /* Motion detected, wake screen if sleeping */
                if (!g_screen_on) {
                    reset_idle_timer();
                }
            }
        }
    }
    return NULL;
}

/* Power / Timeout Watcher Thread */
static void *power_manager(void *arg) {
    (void)arg;
    while (g_running) {
        sleep(1);
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

/* Navigation & Button Handling */
static void handle_navigation_key(int code, int value) {
    if (value != 1 && value != 2) return; /* Only on press/repeat */
    reset_idle_timer();

    /* View Switching via Ha1 / Ha2 / Ha3 buttons */
    if (code == 402) { /* ChannelUp -> Next Tab */
        g_current_view = (g_current_view + 1) % VIEW_COUNT;
        render_screen();
        return;
    }
    if (code == 403) { /* ChannelDown -> Prev Tab */
        g_current_view = (g_current_view + VIEW_COUNT - 1) % VIEW_COUNT;
        render_screen();
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
                if (g_selected_device_cmd < DEVICE_CMD_COUNT - 1) g_selected_device_cmd++;
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
                render_screen();
            } else if (g_current_view == VIEW_DEVICES) {
                const device_item_t *d = &g_device_commands[g_selected_device_cmd];
                printf("[*] Triggering Device Command: %s -> %s\n", d->name, d->command);
                /* Send via RF generic button code */
                rf_send_button_event(0x0205, 1);
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
            if (g_current_view != VIEW_ACTIVITIES) {
                g_current_view = VIEW_ACTIVITIES;
                render_screen();
            }
            break;

        default:
            break;
    }
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

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
    g_touch_fd = open(TOUCH_EVENT_DEV, O_RDONLY);
    if (g_touch_fd >= 0 && !g_touch_enabled) {
        set_touch_enabled(false);
    }

    g_gyro_fd = open(GYRO_EVENT_DEV, O_RDONLY);
    if (g_gyro_fd >= 0 && !g_gyro_enabled) {
        set_gyro_enabled(false);
    }

    /* 3. Open RF SPI */
    g_rf_fd = open(RFSPI_DEV, O_RDWR);
    if (g_rf_fd >= 0) {
        printf("[+] Opened %s for RF TX\n", RFSPI_DEV);
    }

    /* 4. Open Physical Buttons */
    g_btn_fd = open(BTN_EVENT_DEV, O_RDONLY);
    if (g_btn_fd < 0) {
        perror("open " BTN_EVENT_DEV);
        return 1;
    }
    printf("[+] Listening on %s for physical buttons...\n", BTN_EVENT_DEV);

    /* Background Threads */
    pthread_t th_power, th_gyro;
    pthread_create(&th_power, NULL, power_manager, NULL);
    pthread_create(&th_gyro, NULL, gyro_watcher, NULL);

    /* Initial Render */
    render_screen();

    /* Event Loop */
    struct input_event ev;
    while (g_running) {
        ssize_t n = read(g_btn_fd, &ev, sizeof(ev));
        if (n == (ssize_t)sizeof(ev)) {
            if (ev.type == EV_KEY) {
                /* Navigation vs RF passthrough */
                handle_navigation_key(ev.code, ev.value);

                /* Also send RF button event to Hub for standard playback/volume */
                rf_send_button_event(ev.code, ev.value);
            }
        } else if (n < 0 && errno != EINTR) {
            perror("read btn_fd");
            break;
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
