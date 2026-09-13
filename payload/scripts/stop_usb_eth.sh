#!/bin/sh
# Revert USB Ethernet back to stock gadget mode
if [ -d /mnt/data/usb_eth ]; then
  DIR="/mnt/data/usb_eth"
elif [ -d /data/codex/usb_eth ]; then
  DIR="/data/codex/usb_eth"
else
  DIR="$(cd "$(dirname "$0")" && pwd)"
fi

echo "[USB_ETH] Unloading network modules..."
for mod in r8152 smsc95xx dm9601 rtl8150 rndis_host cdc_subset zaurus asix cdc_ether usbnet; do
  rmmod "$mod" 2>/dev/null
done

echo "[USB_ETH] Restoring stock USB gadget mode..."
insmod /lib/modules/2.6.31-g89d565c/kernel/drivers/usb/gadget/ath_udc.ko 2>/dev/null
insmod /lib/modules/2.6.31-g89d565c/kernel/drivers/usb/gadget/gadgetfs.ko 2>/dev/null
/usr/bin/usbhid &
echo "[USB_ETH] Restored to stock state."
