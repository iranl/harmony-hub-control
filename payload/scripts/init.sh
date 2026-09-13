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
rm -f /data/codex/bin/pair_b25.sh /data/codex/bin/do_pair.sh /data/codex/bin/test_ble_diag /data/codex/bin/test_smp /data/codex/bin/test_hci_sniff /data/codex/bin/codex_ir_send 2>/dev/null || true

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

# 5.5 Boot crash watchdog & automatic rollback
BOOT_FAILS=/data/codex/boot_fails
if [ -f "$BOOT_FAILS" ]; then
  COUNT=$(cat "$BOOT_FAILS" 2>/dev/null || echo 0)
  COUNT=$((COUNT + 1))
else
  COUNT=1
fi
echo "$COUNT" > "$BOOT_FAILS" 2>/dev/null || true

if [ "$COUNT" -ge 3 ]; then
  echo "$(date) Boot crash loop detected ($COUNT fails). Attempting rollback..." >> "$LOG"
  LATEST_BACKUP=$(ls -td /data/codex/updates/backup/* 2>/dev/null | head -n 1)
  if [ -n "$LATEST_BACKUP" ] && [ -d "$LATEST_BACKUP" ]; then
    echo "$(date) Restoring backup from $LATEST_BACKUP" >> "$LOG"
    cp -f "$LATEST_BACKUP"/* /data/codex/bin/ 2>/dev/null || true
    chmod 755 /data/codex/bin/* 2>/dev/null || true
    rm -f "$BOOT_FAILS"
  fi
fi

(
  sleep 60
  rm -f "$BOOT_FAILS"
) &

# 6. Web UI (HTML frontend on 8080) and Core Daemon (orchestrator/MQTT on 8089)
if [ -x /data/codex/bin/codex_webui ]; then
  if ! ps | grep '[c]odex_webui' >/dev/null 2>&1; then
    /data/codex/bin/codex_webui 8080 >> "$LOG" 2>&1 &
  fi
fi
if [ -x /data/codex/bin/codex_daemon ]; then
  if ! ps | grep '[c]odex_daemon' >/dev/null 2>&1; then
    /data/codex/bin/codex_daemon 8089 >> "$LOG" 2>&1 &
  fi
fi

# 7. BTstack Bluetooth Engine (BlueZ 4 permanently disabled)
chmod -x /usr/sbin/bluetoothd 2>/dev/null || true
killall -9 bluetoothd codex_bthid_remote codex_bthid_keyboard 2>/dev/null || true
rm -f /data/codex/bin/codex_bthid_remote /data/codex/bin/codex_bthid_keyboard 2>/dev/null || true
if [ -x /data/codex/bin/codex_btstack ]; then
  echo "$(date) Starting BTstack backend" >> "$LOG"
  /data/codex/bin/codex_btstack >> "$LOG" 2>&1 &
fi

# 8. Recovery AP Monitor
if [ -x /data/codex/recovery_ap.sh ]; then
  if [ ! -f /var/run/codex-recovery-monitor.pid ]; then
    /data/codex/recovery_ap.sh monitor >> "$LOG" 2>&1 &
    echo $! > /var/run/codex-recovery-monitor.pid
  fi
fi

echo "$(date) codex init done" >> "$LOG"

