#!/bin/sh
# Codex BT Reconnect: fast reconnect tries for 30 seconds after activity start
PID_FILE="/tmp/codex_bt_reconnect.pid"
LOG="/tmp/codex-bt-reconnect.log"
STATUS_FILE="/tmp/bthid_status"
CMD_FILE="/tmp/codex_btstack_cmd"
TARGET_FILE="/data/codex/bthid_target"
HOST_TARGET_FILE="/data/codex/bthid_host_target"

# Log rotation
if [ -f "$LOG" ] && [ "$(wc -l < "$LOG" 2>/dev/null || echo 0)" -gt 300 ]; then
    tail -n 100 "$LOG" > "$LOG.tmp" && mv "$LOG.tmp" "$LOG"
fi

log() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') $1" >> "$LOG"
}

# Single instance check: kill previous instance to restart 30s window
if [ -f "$PID_FILE" ]; then
    OLD_PID=$(cat "$PID_FILE" 2>/dev/null)
    if [ -n "$OLD_PID" ] && kill -0 "$OLD_PID" 2>/dev/null; then
        kill -9 "$OLD_PID" 2>/dev/null
    fi
fi
echo $$ > "$PID_FILE"
trap 'rm -f "$PID_FILE"' EXIT INT TERM

TARGET="$1"

if [ -z "$TARGET" ]; then
    if [ -f "$HOST_TARGET_FILE" ]; then
        TARGET=$(grep -E -o '([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}' "$HOST_TARGET_FILE" 2>/dev/null | head -1)
    fi
    if [ -z "$TARGET" ] && [ -f "$TARGET_FILE" ]; then
        TARGET=$(grep -E -o '([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}' "$TARGET_FILE" 2>/dev/null | head -1)
    fi
fi

if [ -z "$TARGET" ]; then
    log "BT reconnect: no target Bluetooth MAC found"
    exit 0
fi

TARGET=$(echo "$TARGET" | tr '[:lower:]' '[:upper:]')

is_connected() {
    [ -f "$STATUS_FILE" ] && grep -q '"state":"listening"' "$STATUS_FILE" 2>/dev/null
}

if is_connected; then
    log "BT reconnect: $TARGET already connected"
    exit 0
fi

log "BT fast reconnect started for $TARGET (window=30s, retry=2s)"

ATTEMPT=1
MAX_ATTEMPTS=15

while [ "$ATTEMPT" -le "$MAX_ATTEMPTS" ]; do
    # Trigger BTstack connection
    if pidof codex_btstack >/dev/null 2>&1; then
        echo "connect_host $TARGET" > "$CMD_FILE"
    fi

    sleep 2

    if is_connected; then
        log "BT reconnect: $TARGET connected successfully on attempt $ATTEMPT"
        exit 0
    fi

    ATTEMPT=$((ATTEMPT + 1))
done

log "BT reconnect: 30s timeout reached for $TARGET"
exit 1
