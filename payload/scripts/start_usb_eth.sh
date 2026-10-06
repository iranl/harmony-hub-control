#!/bin/sh
# USB Ethernet Host mode activator
MODULES_DIR="/data/codex/modules"
BIN_DIR="/data/codex/bin"

echo "[USB_ETH] Switching USB controller to Host mode..."
killall -9 usbgadget usbhid 2>/dev/null || true
umount /dev/gadget 2>/dev/null || true
rmmod gadgetfs 2>/dev/null || true
rmmod ath_udc 2>/dev/null || true

# Load stock host core + EHCI driver
insmod /lib/modules/2.6.31-g89d565c/kernel/drivers/usb/core/usbcore.ko 2>/dev/null || true
insmod /lib/modules/2.6.31-g89d565c/kernel/drivers/usb/host/ehci-hcd.ko 2>/dev/null || true

# Register dormant platform device ar7240-ehci.0 via sys_call_table ONLY if not already bound
if [ ! -d /sys/devices/platform/ar7240-ehci.0/driver ]; then
  if [ -x "$BIN_DIR/register_ehci" ]; then
    "$BIN_DIR/register_ehci"
  elif [ -x /usr/sbin/register_ehci ]; then
    /usr/sbin/register_ehci
  else
    echo "[USB_ETH] Error: register_ehci not found!" >&2
  fi
fi

# Detect plugged USB device and load ONLY its specific driver (never load cdc_ether with r8152)
for i in 1 2 3; do
  [ -d /sys/bus/usb/devices/1-1 ] && break
  sleep 1
done

if [ -f /sys/bus/usb/devices/1-1/idVendor ]; then
  vid=$(cat /sys/bus/usb/devices/1-1/idVendor 2>/dev/null)
  pid=$(cat /sys/bus/usb/devices/1-1/idProduct 2>/dev/null)
  echo "[USB_ETH] Detected USB device $vid:$pid"
  case "$vid:$pid" in
    0bda:8152|0bda:8153)
      echo "[USB_ETH] Loading Realtek native driver (r8152.ko)..."
      insmod "$MODULES_DIR/r8152.ko" 2>/dev/null || true
      ;;
    0b95:*|077b:2226|0846:1040)
      echo "[USB_ETH] Loading ASIX driver..."
      insmod "$MODULES_DIR/usbnet.ko" 2>/dev/null || true
      insmod "$MODULES_DIR/asix.ko" 2>/dev/null || true
      ;;
    0424:*)
      echo "[USB_ETH] Loading SMSC driver..."
      insmod "$MODULES_DIR/usbnet.ko" 2>/dev/null || true
      insmod "$MODULES_DIR/smsc95xx.ko" 2>/dev/null || true
      ;;
    0a46:*)
      echo "[USB_ETH] Loading DM9601 driver..."
      insmod "$MODULES_DIR/usbnet.ko" 2>/dev/null || true
      insmod "$MODULES_DIR/dm9601.ko" 2>/dev/null || true
      ;;
    0bda:8150)
      echo "[USB_ETH] Loading RTL8150 driver..."
      insmod "$MODULES_DIR/rtl8150.ko" 2>/dev/null || true
      ;;
    *)
      echo "[USB_ETH] Loading generic CDC Ethernet driver..."
      insmod "$MODULES_DIR/usbnet.ko" 2>/dev/null || true
      insmod "$MODULES_DIR/cdc_ether.ko" 2>/dev/null || true
      ;;
  esac
else
  echo "[USB_ETH] No USB device detected yet on port 1-1"
fi

echo "[USB_ETH] Done! Active USB drivers:"
lsmod | grep -e r8152 -e asix -e cdc_ether -e smsc95xx -e dm9601 -e rtl8150 -e usbnet -e ehci_hcd
