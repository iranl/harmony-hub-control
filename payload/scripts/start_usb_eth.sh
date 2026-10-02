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
insmod /lib/modules/2.6.31-g89d565c/kernel/drivers/usb/core/usbcore.ko 2>/dev/null
insmod /lib/modules/2.6.31-g89d565c/kernel/drivers/usb/host/ehci-hcd.ko 2>/dev/null

# Register dormant platform device ar7240-ehci.0 via sys_call_table
if [ -x "$BIN_DIR/register_ehci" ]; then
  "$BIN_DIR/register_ehci"
elif [ -x /usr/sbin/register_ehci ]; then
  /usr/sbin/register_ehci
else
  echo "[USB_ETH] Error: register_ehci not found!" >&2
fi

# Load network core + NIC drivers
for mod in usbnet asix cdc_ether r8152 smsc95xx dm9601 rtl8150 rndis_host cdc_subset zaurus; do
  if [ -f "$MODULES_DIR/$mod.ko" ]; then
    insmod "$MODULES_DIR/$mod.ko" 2>/dev/null
  fi
done

echo "[USB_ETH] Done! Active USB drivers:"
lsmod | grep -e r8152 -e asix -e cdc_ether -e smsc95xx -e dm9601 -e rtl8150 -e usbnet -e ehci_hcd
