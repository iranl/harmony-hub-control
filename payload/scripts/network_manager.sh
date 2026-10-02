#!/bin/sh
# Codex Network Manager - Ethernet / USB Host & Wi-Fi supervisor
# Handles 15s USB gadget safety boot, Ethernet initialization, and Wi-Fi fallback.

PATH=/data/codex/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
CONF=/data/codex/ethernet.conf
WPA_CONF=/etc/wpa_supplicant.ath0.conf
[ -f "$WPA_CONF" ] || WPA_CONF=/etc/wpa_supplicant.conf

LOG=/tmp/codex-network.log
STATE_FILE=/tmp/codex_active_net_state

log() {
  echo "$(date '+%Y-%m-%d %H:%M:%S') [NET] $*" >> "$LOG"
}

# Load configuration with safe defaults
ETH_ENABLED=0
ETH_FALLBACK_WIFI=1
ETH_USB_SERIAL=1
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
        ETH_USB_SERIAL|USB_SERIAL_CONSOLE) ETH_USB_SERIAL=1 ;;
        ETH_MODE) ETH_MODE="$val" ;;
        ETH_IP) ETH_IP="$val" ;;
        ETH_NETMASK) ETH_NETMASK="$val" ;;
        ETH_GATEWAY) ETH_GATEWAY="$val" ;;
        ETH_DNS) ETH_DNS="$val" ;;
      esac
    done < "$CONF"
  fi
}

configure_arp_isolation() {
  # Prevent ARP flux and broadcast loops when both eth and wifi are on same subnet
  echo 1 > /proc/sys/net/ipv4/conf/all/arp_ignore 2>/dev/null || true
  echo 1 > /proc/sys/net/ipv4/conf/default/arp_ignore 2>/dev/null || true
  echo 2 > /proc/sys/net/ipv4/conf/all/arp_announce 2>/dev/null || true
  echo 2 > /proc/sys/net/ipv4/conf/default/arp_announce 2>/dev/null || true
  echo 2 > /proc/sys/net/ipv4/conf/all/rp_filter 2>/dev/null || true
  echo 2 > /proc/sys/net/ipv4/conf/default/rp_filter 2>/dev/null || true
}

find_eth_interface() {
  # Ethernet on Harmony Hub is strictly USB-based.
  # Ignore internal SoC eth0 and devboard eth1 (no physical ports).
  for ifpath in /sys/class/net/*; do
    [ -d "$ifpath" ] || continue
    ifname="${ifpath##*/}"
    case "$ifname" in
      eth*|usb*)
        if [ "$ifname" != "eth0" ] && [ "$ifname" != "eth1" ] && [ "$ifname" != "wifi0" ] && [ "$ifname" != "ath0" ] && [ "$ifname" != "lo" ]; then
          echo "$ifname"
          return 0
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
  if [ -x /data/codex/bin/start_usb_eth.sh ]; then
    /data/codex/bin/start_usb_eth.sh >> "$LOG" 2>&1
  elif [ -x /data/codex/start_usb_eth.sh ]; then
    /data/codex/start_usb_eth.sh >> "$LOG" 2>&1
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
  configure_arp_isolation
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
    configure_arp_isolation
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

stop_eth() {
  log "Stopping Ethernet..."
  if [ -x /data/codex/bin/stop_usb_eth.sh ]; then
    /data/codex/bin/stop_usb_eth.sh >> "$LOG" 2>&1
  elif [ -x /data/codex/stop_usb_eth.sh ]; then
    /data/codex/stop_usb_eth.sh >> "$LOG" 2>&1
  elif [ -x /usr/sbin/stop_usb_eth.sh ]; then
    /usr/sbin/stop_usb_eth.sh >> "$LOG" 2>&1
  fi
}

# Background watchdog: monitors Ethernet link and handles Wi-Fi failover / recovery
run_monitor() {
  echo "$$" > /var/run/codex_net_monitor.pid
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

do_boot() {
  log "Boot initialization started..."
  load_config

  if [ "$ETH_ENABLED" != "1" ]; then
    log "Ethernet is disabled in configuration. Keeping Wi-Fi active."
    ensure_wifi
    exit 0
  fi

  log "Ethernet is enabled. Waiting 15s for system boot to settle before switching USB..."
  sleep 15

  log "Starting Ethernet (USB Host mode)..."
  if start_eth; then
    run_monitor &
  else
    log "Ethernet start failed! Falling back to Wi-Fi..."
    stop_eth
    if [ "$ETH_FALLBACK_WIFI" = "1" ]; then
      log "Fallback to Wi-Fi enabled. Connecting Wi-Fi..."
      ensure_wifi
      echo "ath0:wifi-fallback:" > "$STATE_FILE"
      run_monitor &
    fi
  fi
}

do_apply() {
  log "Applying network configuration live..."
  load_config

  # Kill existing monitor loop if running
  if [ -f /var/run/codex_net_monitor.pid ]; then
    kill -9 "$(cat /var/run/codex_net_monitor.pid 2>/dev/null)" 2>/dev/null || true
    rm -f /var/run/codex_net_monitor.pid
  fi


  if [ "$ETH_ENABLED" = "1" ]; then
    log "Enabling Ethernet..."
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
    log "Disabling Ethernet..."
    stop_eth
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
