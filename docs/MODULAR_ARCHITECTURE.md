# Codex WebUI Modular Architecture Reference

## Overview
Originally `codex_webui.c` was an ~8,200 line monolithic C file containing HTTP parsing, HTML rendering, IR hardware encoding, Bluetooth stack IPC, Wi-Fi management, activity sequences, and firmware OTA updating.

It was partitioned into targeted modules with clearly delineated responsibilities, dedicated headers, and compile-time encapsulation.

---

## Module Breakdown & Historical Line Ranges

| Module | Source File | Header File | Responsibilities | Original Monolith Line Ranges |
| :--- | :--- | :--- | :--- | :--- |
| **Common Types** | `codex_webui_types.h` | `codex_webui_types.h` | Data structures, constants, buffer limits, file paths | Top declarations |
| **HTTP & String Utils** | `webui_utils.c` | `webui_utils.h` | Safe string manipulation, base64, URL decoding, request parsing, authentication | 34-890, 1768-1834, 3030-3048, 7788-7880 |
| **System & Config** | `webui_config.c` | `webui_config.h` | MQTT settings, Wi-Fi wpa_supplicant, resource backups, backup bundle export/import with CRC32 integrity check | 911-1181, 1835-1893, 4726-4862, 4864-5238 |
| **IR & Device Inventory**| `webui_ir.c` | `webui_ir.h` | DeviceList.json management, IR command database, NEC / Sony SIRC auto-decode, I2S pulse queues, protocol repair batching | 892-910, 1183-1766, 1895-3029, 3050-3587, 5240-5507, 7284-7365, 7367-7714 |
| **Activity Engine** | `webui_activity.c` | `webui_activity.h` | Activity sequence validation & execution, transition locks, step progress broadcast | 3610-4705, 7882-8173 |
| **Bluetooth Subsystem** | `webui_bt.c` | `webui_bt.h` | BT HID commands, keyboard reports, saved BT inventory & scripts, pairing status | 4707-4722, 5509-6487, 6950-7282, 7716-7786, plus data functions relocated from HTML |
| **OTA Firmware Update** | `webui_update.c` | `webui_update.h` | Staged binary uploads (2MB cap), MD5 verification, atomic rollout | 6489-6948 |
| **HTML UI Templates** | `codex_webui_html.c` | `webui_html.h` | Pure HTML page rendering, CSS styling, responsive layout (included into `codex_webui.c`) | ~1300-2400 (of html template) |
| **Main Web Server** | `codex_webui.c` | - | Server socket initialization, backlog (32), accept loop, request routing | 8175-end |

---

## Build Reference

In `build/compile_all.sh`:
```sh
"$CC" $CFLAGS -Ipayload/source -o "$BIN/codex_webui" \
    payload/source/codex_webui.c \
    payload/source/webui_utils.c \
    payload/source/webui_ir.c \
    payload/source/webui_activity.c \
    payload/source/webui_bt.c \
    payload/source/webui_update.c \
    payload/source/webui_config.c \
    payload/source/resource_cache.c \
    payload/source/cJSON.c \
    payload/source/hw_action.c \
    payload/source/ir_encoder.c \
    payload/source/ir_i2s.c -lm
```

## Verification

Syntax check for all webui modules:
```sh
gcc -fsyntax-only -Ipayload/source \
    payload/source/codex_webui.c \
    payload/source/webui_utils.c \
    payload/source/webui_ir.c \
    payload/source/webui_activity.c \
    payload/source/webui_bt.c \
    payload/source/webui_update.c \
    payload/source/webui_config.c \
    payload/source/resource_cache.c \
    payload/source/cJSON.c \
    payload/source/hw_action.c \
    payload/source/ir_encoder.c \
    payload/source/ir_i2s.c
```
