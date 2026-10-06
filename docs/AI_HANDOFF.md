# AI Handoff

You are working on the Harmony Hub Control post-root runtime.

## Goal

Improve and maintain the local web UI and supporting hub services for an
already rooted Logitech Harmony Hub.

## Important Boundaries

- This repository is not the rooting tool.
- Do not add LAN or USB rooting material here.
- Do not include private keys, tokens, MQTT passwords, Home Assistant tokens, firmware dumps, or hub backups.
- HTTP authentication can be configured using the WebUI. The hub should only be accessible on a trusted LAN.
- The installer expects root SSH to already work.

## Main Files

- `payload/source/codex_daemon.c`: unified daemon orchestrating WebUI (port 8080), WebSocket (port 8089), MQTT, IR capture/tx, and system services.
- `payload/source/webui_server.c`: non-blocking select()-based WebUI HTTP server module running inside codex_daemon.
- `payload/source/codex_btstack/`: BTstack BLE remote daemon and HID keyboard/consumer input engine.
- `payload/scripts/init.sh`: hub boot startup for local services.
- `payload/scripts/recovery_ap.sh`: reset-button recovery AP flow.
- `install_webui.ps1`: Windows SSH uploader/installer.
- `install_webui.py`: Linux/macOS Python SSH uploader/installer.
- `restore_backup.ps1`: rollback helper.

## Runtime Paths On Hub

```text
/data/codex/bin/codex_daemon
/data/codex/bin/codex_btstack
/data/codex/bin/codex_sntp
/data/codex/bin/codex_dhcpd
/data/codex/bin/codex_portal
/data/codex/init.sh
/data/codex/recovery_ap.sh
/data/codex/network_manager.sh
/data/codex/ethernet.conf
/data/codex/hub_id
/data/codexmqtt/config.json
/usr/sbin/dropbear
/usr/sbin/dropbearkey
/etc/init.d/rcS.local
```

## Current Feature Notes

- IR import sources should stay separate in the UI, with an `All databases` option.
- Unsupported IR database rows should be visible with enough detail to debug parser coverage.
- Flipper parsed protocols currently include `RC5`, `RC6`, `SIRC`, `SIRC15` and `SIRC20` conversion to raw timing.
- Learned IR signals should be testable before saving.
- The IR sweep page should favor fast staging in browser memory and hub-side batch sends that can be stopped.
- Bluetooth HID: handled directly by `codex_btstack` reading `/tmp/bthid_input` FIFO.
- IR sending: direct `/dev/i2s` hardware modulation via `ir_i2s.c` (stock Logitech HAL disabled).
- MQTT should publish enough state for Home Assistant debugging, including IP address and bridge health.
- Optional HTTP Basic authentication is supported via `/data/codex/webui_auth.conf` with cached in-memory credentials.
- Network manager (`network_manager.sh`) manages Ethernet (USB host) and Wi-Fi (`ath0`). On boot, always starts USB gadget mode for 15s to allow PC detection (SOF interrupt sampling); switches to USB host/Ethernet if no PC connected and Ethernet enabled. Supports automatic Wi-Fi fallback.

## Verification Checklist

After changing web UI or runtime behavior:

1. Deploy with `install_webui.ps1` on Windows or `python3 install_webui.py` on Linux/macOS.
2. Open `http://<hub-ip>:8080/`.
3. Check Dashboard, IR Devices, IR Sweep, Bluetooth, MQTT, Network (Wi-Fi & Ethernet), Backup, and System sections.
4. Confirm no browser auth prompt appears.
5. Import a small IR database file and verify supported/unsupported counts.
6. Send one known-good IR command.
7. If Bluetooth changed, pair and send a short exact text script.
8. If MQTT changed, verify discovery and state topics in Home Assistant.
9. Tail `/tmp/codex-init.log` and `/tmp/ir-events.log`.
10. Confirm rollback can find the newest backup.
