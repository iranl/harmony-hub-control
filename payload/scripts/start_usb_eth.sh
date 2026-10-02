#!/bin/sh
# USB Ethernet Host mode activator
MODULES_DIR="/data/codex/modules"
BIN_DIR="/data/codex/bin"

echo "[USB_ETH] Switching USB controller to Host mode..."
killall usbhid 2>/dev/null
rmmod gadgetfs 2>/dev/null
rmmod ath_udc 2>/dev/null

# 1. Load USB core
insmod /lib/modules/2.6.31-g89d565c/kernel/drivers/usb/core/usbcore.ko 2>/dev/null

# 2. Register vendor NIC drivers before host controller starts enumeration
for mod in r8152 usbnet asix smsc95xx dm9601 rtl8150; do
  if [ -f "$MODULES_DIR/$mod.ko" ]; then
    insmod "$MODULES_DIR/$mod.ko" 2>/dev/null
  fi
done

# 3. Load EHCI host controller driver
insmod /lib/modules/2.6.31-g89d565c/kernel/drivers/usb/host/ehci-hcd.ko 2>/dev/null

# Register dormant platform device ar7240-ehci.0 via sys_call_table if needed
if [ ! -d /sys/devices/platform/ar7240-ehci.0/driver ]; then
  if [ -x "$BIN_DIR/register_ehci" ]; then
    "$BIN_DIR/register_ehci"
  elif [ -x /usr/sbin/register_ehci ]; then
    /usr/sbin/register_ehci
  else
    echo "[USB_ETH] Error: register_ehci not found!" >&2
  fi
fi

# Wait for USB bus enumeration to settle
sleep 2

# Check if connected device is Realtek RTL8152/8153 (vendor 0bda)
is_r8152=0
for udev in /sys/bus/usb/devices/*; do
  if [ -f "$udev/idVendor" ] && [ -f "$udev/idProduct" ]; then
    vid=$(cat "$udev/idVendor" 2>/dev/null)
    pid=$(cat "$udev/idProduct" 2>/dev/null)
    if [ "$vid" = "0bda" ] && { [ "$pid" = "8152" ] || [ "$pid" = "8153" ]; }; then
      is_r8152=1
      break
    fi
  fi
done

# Load generic CDC / RNDIS drivers only if not RTL8152 (prevents config 1 vs 2 workqueue deadlock)
if [ "$is_r8152" != "1" ]; then
  for mod in cdc_ether rndis_host cdc_subset zaurus; do
    if [ -f "$MODULES_DIR/$mod.ko" ]; then
      insmod "$MODULES_DIR/$mod.ko" 2>/dev/null
    fi
  done
fi

# Bring up any detected USB network interface
for dev in /sys/class/net/eth* /sys/class/net/usb*; do
  [ -d "$dev" ] || continue
  name="${dev##*/}"
  if [ "$name" != "eth0" ] && [ "$name" != "eth1" ]; then
    ifconfig "$name" up 2>/dev/null || true
  fi
done

echo "[USB_ETH] Done! Active USB drivers:"
lsmod | grep -e r8152 -e asix -e cdc_ether -e smsc95xx -e dm9601 -e rtl8150 -e usbnet -e ehci_hcd
