#!/bin/sh
# Codex Network Manager - Ethernet / USB Host & Wi-Fi supervisor
# Handles 15s USB gadget safety boot, Ethernet initialization, and Wi-Fi fallback.

PATH=/data/codex/bin:/mnt/data/usb_eth:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
CONF=/data/codex/ethernet.conf
WPA_CONF=/etc/wpa_supplicant.conf
LOG=/tmp/codex-network.log
STATE_FILE=/tmp/codex_active_net_state

log() {
  echo "$(date '+%Y-%m-%d %H:%M:%S') [NET] $*" >> "$LOG"
}

# Load configuration with safe defaults
ETH_ENABLED=0
ETH_FALLBACK_WIFI=1
ETH_USB_SERIAL=0
ETH_MODE="dhcp"
ETH_IP=""
ETH_NETMASK=""
ETH_GATEWAY=""
ETH_DNS=""

load_config() {
  if [ -f "$CONF" ]; then
    # Parse KEY=VALUE safely
    while IFS='=' read -r key val || [ -n "$key" ]; do
      key=$(echo "$key" | tr -d ' \t\r\n')
      val=$(echo "$val" | tr -d '\r\n"')
      case "$key" in
        ETH_ENABLED) ETH_ENABLED="$val" ;;
        ETH_FALLBACK_WIFI) ETH_FALLBACK_WIFI="$val" ;;
        ETH_USB_SERIAL|USB_SERIAL_CONSOLE) ETH_USB_SERIAL="$val" ;;
        ETH_MODE) ETH_MODE="$val" ;;
        ETH_IP) ETH_IP="$val" ;;
        ETH_NETMASK) ETH_NETMASK="$val" ;;
        ETH_GATEWAY) ETH_GATEWAY="$val" ;;
        ETH_DNS) ETH_DNS="$val" ;;
      esac
    done < "$CONF"
  fi
}

get_udc_interrupts() {
  grep -iE 'ath_udc|udc|ar7240_usb' /proc/interrupts 2>/dev/null | awk '{print $2}' | tr -d ' \r\n'
}

# Check if a PC is connected to USB gadget mode
# A USB Host sends SOF tokens every 1ms and bus resets / setup packets.
# A wall charger or unconnected cable generates 0 USB host interrupts.
is_pc_connected() {
  # 1. Sample interrupt counter over 1 second
  c1=$(get_udc_interrupts)
  if [ -n "$c1" ]; then
    sleep 1
    c2=$(get_udc_interrupts)
    if [ -n "$c2" ] && [ "$c2" -gt "$((c1 + 15))" ]; then
      log "PC host detected via USB interrupts ($c1 -> $c2)"
      return 0
    fi
  fi

  # 2. Check sysfs state if available (configured or addressed by a host)
  for s in /sys/devices/platform/ath_udc*/state /sys/class/udc/*/state; do
    if [ -f "$s" ]; then
      st=$(cat "$s" 2>/dev/null)
      case "$st" in
        *configured*|*addressed*)
          log "PC host detected via sysfs state ($st)"
          return 0
          ;;
      esac
    fi
  done

  return 1
}

find_eth_interface() {
  # Ethernet on Harmony Hub is strictly USB-based.
  # Ignore internal SoC eth0 (ag71xx platform device without physical port).
  for ifpath in /sys/class/net/*; do
    [ -d "$ifpath" ] || continue
    ifname=$(basename "$ifpath")
    case "$ifname" in
      eth*|usb*)
        if [ "$ifname" != "wifi0" ] && [ "$ifname" != "ath0" ] && [ "$ifname" != "ath1" ] && [ "$ifname" != "lo" ]; then
          if [ -e "$ifpath/device" ]; then
            devtarget=$(readlink "$ifpath/device" 2>/dev/null)
            case "$devtarget" in
              *usb*|*ehci*)
                echo "$ifname"
                return 0
                ;;
            esac
          fi
        fi
        ;;
    esac
  done
  return 1
}

ensure_wifi() {
  log "Ensuring Wi-Fi connection (ath0)..."
  if ! ifconfig ath0 >/dev/null 2>&1; then
    /sbin/wlanconfig ath0 create wlandev wifi0 wlanmode sta 2>/dev/null || true
  fi
  if ! ps | grep '[w]pa_supplicant' >/dev/null 2>&1; then
    /usr/sbin/wpa_supplicant -s -iath0 -c "$WPA_CONF" >/dev/null 2>&1 &
    sleep 2
  fi
  ifconfig ath0 up 2>/dev/null || true
  
  # Check if ath0 already has IP
  wifi_ip=$(ifconfig ath0 2>/dev/null | grep 'inet addr' | awk -F: '{print $2}' | awk '{print $1}')
  if [ -z "$wifi_ip" ]; then
    log "Requesting DHCP lease on ath0..."
    udhcpc -i ath0 -n -t 5 -T 3 >/dev/null 2>&1 || true
    wifi_ip=$(ifconfig ath0 2>/dev/null | grep 'inet addr' | awk -F: '{print $2}' | awk '{print $1}')
  fi

  if [ -n "$wifi_ip" ]; then
    log "Wi-Fi connected successfully: ath0 has IP $wifi_ip"
    echo "ath0:wifi:$wifi_ip" > "$STATE_FILE"
    return 0
  else
    log "Wi-Fi failed to obtain an IP lease"
    return 1
  fi
}

start_eth() {
  log "Switching USB to host mode and loading drivers..."
  if [ -x /mnt/data/usb_eth/start_usb_eth.sh ]; then
    /mnt/data/usb_eth/start_usb_eth.sh >> "$LOG" 2>&1
  elif [ -x /data/codex/bin/start_usb_eth.sh ]; then
    /data/codex/bin/start_usb_eth.sh >> "$LOG" 2>&1
  elif [ -x /usr/sbin/start_usb_eth.sh ]; then
    /usr/sbin/start_usb_eth.sh >> "$LOG" 2>&1
  else
    log "Error: start_usb_eth.sh script not found!"
    return 1
  fi

  # Wait up to 8s for Ethernet interface to appear
  eth_if=""
  for i in 1 2 3 4 5 6 7 8; do
    eth_if=$(find_eth_interface)
    [ -n "$eth_if" ] && break
    sleep 1
  done

  if [ -z "$eth_if" ]; then
    log "No USB Ethernet adapter found on USB bus"
    return 1
  fi

  log "Found USB Ethernet interface: $eth_if"
  ifconfig "$eth_if" up >> "$LOG" 2>&1

  # Check link/carrier (up to 5s)
  carrier_ok=0
  for i in 1 2 3 4 5; do
    cfile="/sys/class/net/$eth_if/carrier"
    if [ -f "$cfile" ] && [ "$(cat "$cfile" 2>/dev/null)" = "1" ]; then
      carrier_ok=1
      break
    fi
    sleep 1
  done

  if [ "$carrier_ok" != "1" ]; then
    log "Ethernet link down / cable unplugged on $eth_if"
    return 2
  fi

  # Configure IP
  if [ "$ETH_MODE" = "static" ] && [ -n "$ETH_IP" ]; then
    log "Configuring static IP $ETH_IP on $eth_if..."
    mask_arg=""
    [ -n "$ETH_NETMASK" ] && mask_arg="netmask $ETH_NETMASK"
    ifconfig "$eth_if" "$ETH_IP" $mask_arg up >> "$LOG" 2>&1
    if [ -n "$ETH_GATEWAY" ]; then
      route del default 2>/dev/null || true
      route add default gw "$ETH_GATEWAY" dev "$eth_if" >> "$LOG" 2>&1
    fi
    if [ -n "$ETH_DNS" ]; then
      echo "nameserver $ETH_DNS" > /etc/resolv.conf
    fi
  else
    log "Requesting DHCP lease on $eth_if..."
    udhcpc -i "$eth_if" -n -q -t 5 -T 3 >> "$LOG" 2>&1
  fi

  eth_ip=$(ifconfig "$eth_if" 2>/dev/null | grep 'inet addr' | awk -F: '{print $2}' | awk '{print $1}')
  if [ -n "$eth_ip" ]; then
    log "Ethernet connected successfully: $eth_if has IP $eth_ip"
    echo "$eth_if:ethernet:$eth_ip" > "$STATE_FILE"
    # Ensure default route prioritizes Ethernet over ath0
    route del default dev ath0 2>/dev/null || true
    return 0
  else
    log "DHCP failed to obtain IP on $eth_if"
    return 3
  fi
}

start_usb_serial() {
  log "Activating USB Serial Console (/dev/ttyGS0)..."
  # Stop gadgetfs/usbhid if running
  killall -9 usbgadget usbhid 2>/dev/null || true
  umount /dev/gadget 2>/dev/null || true
  rmmod gadgetfs 2>/dev/null || true

  # Ensure controller platform device is active
  if [ -x /data/codex/bin/register_ehci ]; then
    /data/codex/bin/register_ehci >> "$LOG" 2>&1 || true
  fi

  # Load g_serial module
  if ! lsmod | grep -q g_serial; then
    if [ -f /data/codex/modules/g_serial.ko ]; then
      insmod /data/codex/modules/g_serial.ko >> "$LOG" 2>&1 || true
    fi
  fi

  # Create device node if missing
  if [ ! -c /dev/ttyGS0 ]; then
    if [ -x /data/codex/bin/mknod ]; then
      /data/codex/bin/mknod /dev/ttyGS0 c 254 0 >> "$LOG" 2>&1 || true
    fi
  fi

  # Start shell on ttyGS0 if not already running
  if [ -c /dev/ttyGS0 ]; then
    if ! ps | grep '[s]h -l' | grep -q 'ttyGS0'; then
      ( while true; do /bin/sh -l </dev/ttyGS0 >/dev/ttyGS0 2>&1; sleep 1; done ) &
      log "Started root login shell loop on /dev/ttyGS0"
    fi
  fi
}

stop_usb_serial() {
  log "Stopping USB Serial Console..."
  killall -9 sh 2>/dev/null || true
  rmmod g_serial 2>/dev/null || true
}

stop_eth() {
  log "Stopping Ethernet and restoring USB gadget mode..."
  if [ -x /mnt/data/usb_eth/stop_usb_eth.sh ]; then
    /mnt/data/usb_eth/stop_usb_eth.sh >> "$LOG" 2>&1
  elif [ -x /data/codex/bin/stop_usb_eth.sh ]; then
    /data/codex/bin/stop_usb_eth.sh >> "$LOG" 2>&1
  elif [ -x /usr/sbin/stop_usb_eth.sh ]; then
    /usr/sbin/stop_usb_eth.sh >> "$LOG" 2>&1
  fi
  if [ "$ETH_USB_SERIAL" = "1" ]; then
    start_usb_serial
  fi
}

# Background watchdog: monitors Ethernet link and handles Wi-Fi failover / recovery
run_monitor() {
  while true; do
    sleep 8
    load_config
    [ "$ETH_ENABLED" = "1" ] || break

    cur_eth=$(find_eth_interface)
    if [ -n "$cur_eth" ]; then
      cfile="/sys/class/net/$cur_eth/carrier"
      carrier="0"
      [ -f "$cfile" ] && [ "$(cat "$cfile" 2>/dev/null)" = "1" ] && carrier="1"
      
      cur_state=$(cat "$STATE_FILE" 2>/dev/null)
      case "$cur_state" in
        *:ethernet:*)
          if [ "$carrier" != "1" ]; then
            log "Ethernet carrier lost on $cur_eth!"
            if [ "$ETH_FALLBACK_WIFI" = "1" ]; then
              log "Triggering Wi-Fi fallback..."
              ensure_wifi
              echo "ath0:wifi-fallback:" > "$STATE_FILE"
            fi
          fi
          ;;
        *:wifi*:*)
          if [ "$carrier" = "1" ]; then
            log "Ethernet carrier restored on $cur_eth! Re-connecting Ethernet..."
            if start_eth; then
              log "Switched back to primary Ethernet connection"
            fi
          fi
          ;;
      esac
    else
      # No ethernet interface found, check if fallback needed
      cur_state=$(cat "$STATE_FILE" 2>/dev/null)
      case "$cur_state" in
        *:ethernet:*)
          log "Ethernet interface disappeared!"
          if [ "$ETH_FALLBACK_WIFI" = "1" ]; then
            ensure_wifi
            echo "ath0:wifi-fallback:" > "$STATE_FILE"
          fi
          ;;
      esac
    fi
  done
}

# Boot procedure with 15-second USB Gadget safety delay
do_boot() {
  log "Boot initialization started..."
  load_config

  if [ "$ETH_ENABLED" != "1" ]; then
    log "Ethernet is disabled in configuration. Keeping USB gadget and Wi-Fi."
    if [ "$ETH_USB_SERIAL" = "1" ]; then
      start_usb_serial
    fi
    ensure_wifi
    exit 0
  fi

  log "Ethernet is enabled. USB gadget active on boot; waiting 15s for PC connection..."
  pc_detected=0
  for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
    if is_pc_connected; then
      pc_detected=1
      break
    fi
    sleep 1
  done

  if [ "$pc_detected" = "1" ]; then
    log "PC host connected to USB gadget! Aborting switch to Ethernet to preserve PC sync."
    if [ "$ETH_USB_SERIAL" = "1" ]; then
      start_usb_serial
    fi
    if [ "$ETH_FALLBACK_WIFI" = "1" ]; then
      log "Ensuring Wi-Fi is active while connected to PC..."
      ensure_wifi
    fi
    exit 0
  fi

  log "No PC connected within 15 seconds. Unloading USB gadget and starting Ethernet..."
  if start_eth; then
    run_monitor &
  else
    log "Ethernet connection failed!"
    if [ "$ETH_FALLBACK_WIFI" = "1" ]; then
      log "Fallback to Wi-Fi enabled. Connecting Wi-Fi..."
      ensure_wifi
      echo "ath0:wifi-fallback:" > "$STATE_FILE"
      run_monitor &
    else
      log "Fallback to Wi-Fi is disabled. Hub remaining offline."
    fi
  fi
}

do_apply() {
  log "Applying network configuration live..."
  load_config

  # Kill existing monitor loop if running
  killall network_manager.sh 2>/dev/null || true

  if [ "$ETH_ENABLED" = "1" ]; then
    log "Enabling Ethernet..."
    stop_usb_serial
    if start_eth; then
      run_monitor &
    else
      log "Ethernet start failed."
      if [ "$ETH_FALLBACK_WIFI" = "1" ]; then
        ensure_wifi
        run_monitor &
      fi
    fi
  else
    log "Disabling Ethernet. Restoring USB gadget and Wi-Fi..."
    stop_eth
    if [ "$ETH_USB_SERIAL" = "1" ]; then
      start_usb_serial
    else
      stop_usb_serial
    fi
    ensure_wifi
  fi
}

case "$1" in
  boot)
    do_boot
    ;;
  apply)
    do_apply
    ;;
  start_eth)
    load_config
    start_eth
    ;;
  stop_eth)
    stop_eth
    ;;
  start_serial)
    start_usb_serial
    ;;
  stop_serial)
    stop_usb_serial
    ;;
  ensure_wifi)
    ensure_wifi
    ;;
  monitor)
    run_monitor
    ;;
  status)
    load_config
    echo "ETH_ENABLED=$ETH_ENABLED"
    echo "ETH_FALLBACK_WIFI=$ETH_FALLBACK_WIFI"
    echo "ETH_USB_SERIAL=$ETH_USB_SERIAL"
    echo "ETH_MODE=$ETH_MODE"
    cat "$STATE_FILE" 2>/dev/null
    ;;
  *)
    echo "Usage: $0 {boot|apply|start_eth|stop_eth|start_serial|stop_serial|ensure_wifi|monitor|status}"
    exit 1
    ;;
esac
