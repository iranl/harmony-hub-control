#!/bin/sh
PATH=/data/codex/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
LOG=/tmp/codex-init.log

echo "$(date) codex init start" > "$LOG"

# 0. Neuter watchdog and crash reporting immediately
touch /etc/nowatchdog 2>/dev/null || true
chmod -x /usr/bin/watchdog 2>/dev/null || true
killall -9 watchdog 2>/dev/null || true

export LD_PRELOAD=""

# Set kernel printk loglevel according to debug logging config
if [ -f /data/codex/debug_logging.conf ]; then
  echo "7 4 1 7" > /proc/sys/kernel/printk 2>/dev/null || true
else
  echo "3 4 1 7" > /proc/sys/kernel/printk 2>/dev/null || true
fi

# 1. Clean up legacy flash artifacts to minimize flash wear and reclaim space
rm -rf /data/codex-backups /data/codex/bin/*.tmp-handoff* /data/*.tmp-handoff* 2>/dev/null || true
rm -rf /cache/*.log /cache/bin /cache/codex-init.log /tmp/codex_bthid_remote.pid 2>/dev/null || true
rm -f /data/codex/bin/pair_b25.sh /data/codex/bin/do_pair.sh /data/codex/bin/test_ble_diag /data/codex/bin/test_smp /data/codex/bin/test_hci_sniff /data/codex/bin/codex_ir_send /data/codex/bin/codex_webui 2>/dev/null || true
rm -f /data/codex/g_serial.ko /data/codex/mknod_bin /data/codex/mknod 2>/dev/null || true

# 2. Disable and kill unneeded stock Logitech runtime (frees ~12MB RAM)
chmod -x /opt/luaworks/luaworks 2>/dev/null || true
(
  for i in 1 2 3 4 5; do
    killall -9 luaworks luadraws lua netmonitor codex_bthid_remote 2>/dev/null || true
    sleep 1
  done
) &

# 3. Dropbear SSH Server
if [ -x /data/codex/bin/dropbear ]; then
  echo '#!/bin/sh' > /usr/sbin/dropbear
  echo 'exec /data/codex/bin/dropbear -K 300 "$@"' >> /usr/sbin/dropbear
  chmod 755 /usr/sbin/dropbear
  if ! ps | grep '[d]ropbear' >/dev/null 2>&1; then
    /usr/sbin/dropbear
  fi
fi

# 4. Disable and kill Logitech HAL (direct /dev/i2s IR active, saves ~2.5MB RAM)
chmod -x /usr/bin/hal 2>/dev/null || true
killall -9 hal 2>/dev/null || true

# 5. Network Time Sync (SNTP)
if [ -x /data/codex/bin/codex_sntp ]; then
  (
    for i in 1 2 3 4 5 6 7 8 9 10; do
      if /data/codex/bin/codex_sntp pool.ntp.org --set >> "$LOG" 2>&1; then
        echo "$(date) NTP time sync succeeded" >> "$LOG"
        break
      fi
      sleep 3
    done
  ) &
fi

# 5.2 Network Manager (USB Gadget 15s check, Ethernet startup & Wi-Fi fallback)
if [ -x /data/codex/network_manager.sh ]; then
  /data/codex/network_manager.sh boot >> "$LOG" 2>&1 &
elif [ -x /data/codex/bin/network_manager.sh ]; then
  /data/codex/bin/network_manager.sh boot >> "$LOG" 2>&1 &
fi


# 6. Unified Codex Daemon (WebUI on 8080, Orchestrator/WS/MQTT on 8089)
(
  while true; do
    if [ -x /data/codex/bin/codex_daemon ]; then
      if ! pidof codex_daemon >/dev/null 2>&1; then
        echo "$(date) Starting codex_daemon..." >> "$LOG"
        /data/codex/bin/codex_daemon 8089 >> "$LOG" 2>&1
      fi
    fi
    sleep 5
  done
) &

# 7. BTstack Bluetooth Engine (BlueZ 4 permanently disabled)
chmod -x /usr/sbin/bluetoothd 2>/dev/null || true
killall -9 bluetoothd codex_bthid_remote codex_bthid_keyboard 2>/dev/null || true
rm -f /data/codex/bin/codex_bthid_remote /data/codex/bin/codex_bthid_keyboard 2>/dev/null || true
(
  # Wait up to 30 seconds for hci0 to be initialized by rcS.local
  for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30; do
    if hciconfig hci0 2>/dev/null | grep -q 'UP'; then
      break
    fi
    sleep 1
  done

  while true; do
    if [ -x /data/codex/bin/codex_btstack ]; then
      if ! pidof codex_btstack >/dev/null 2>&1; then
        hciconfig hci0 up 2>/dev/null || true
        echo "$(date) Starting BTstack backend..." >> "$LOG"
        /data/codex/bin/codex_btstack >> "$LOG" 2>&1
      fi
    fi
# 8. CC2544 RF Daemon (Logitech HAL replacement for Elite Remote RF)
(
  while true; do
    if [ -x /data/codex/bin/codex_rf ]; then
      if ! pidof codex_rf >/dev/null 2>&1; then
        echo "$(date) Starting codex_rf daemon..." >> "$LOG"
        /data/codex/bin/codex_rf >> "$LOG" 2>&1
      fi
    fi
    sleep 5
  done
) &

echo "$(date) codex init done" >> "$LOG"