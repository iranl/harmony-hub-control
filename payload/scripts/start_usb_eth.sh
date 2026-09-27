#!/bin/sh
# USB Ethernet Host mode activator
if [ -d /mnt/data/usb_eth ]; then
  DIR="/mnt/data/usb_eth"
elif [ -d /data/codex/usb_eth ]; then
  DIR="/data/codex/usb_eth"
else
  DIR="$(cd "$(dirname "$0")" && pwd)"
fi

echo "[USB_ETH] Switching USB controller to Host mode..."
killall usbhid 2>/dev/null
rmmod gadgetfs 2>/dev/null
rmmod ath_udc 2>/dev/null

# Load stock host core + EHCI driver
insmod /lib/modules/2.6.31-g89d565c/kernel/drivers/usb/core/usbcore.ko 2>/dev/null
insmod /lib/modules/2.6.31-g89d565c/kernel/drivers/usb/host/ehci-hcd.ko 2>/dev/null

# Register dormant platform device ar7240-ehci.0 via sys_call_table
if [ -x "$DIR/register_ehci" ]; then
  "$DIR/register_ehci"
elif [ -x /data/codex/bin/register_ehci ]; then
  /data/codex/bin/register_ehci
elif [ -x /usr/sbin/register_ehci ]; then
  /usr/sbin/register_ehci
else
  echo "[USB_ETH] Error: register_ehci not found!" >&2
fi

# Load network core + NIC drivers
for mod in usbnet asix cdc_ether r8152 smsc95xx dm9601 rtl8150 rndis_host cdc_subset zaurus; do
  if [ -f "$DIR/$mod.ko" ]; then
    insmod "$DIR/$mod.ko" 2>/dev/null
  elif [ -f "$DIR/modules/$mod.ko" ]; then
    insmod "$DIR/modules/$mod.ko" 2>/dev/null
  elif [ -f "/mnt/data/usb_eth/$mod.ko" ]; then
    insmod "/mnt/data/usb_eth/$mod.ko" 2>/dev/null
  fi
done

echo "[USB_ETH] Done! Active USB drivers:"
lsmod | grep -E "r8152|asix|cdc_ether|smsc95xx|dm9601|rtl8150|usbnet|ehci_hcd"
