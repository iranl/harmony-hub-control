# Logitech Harmony Elite Remote Hacking & Support

## Serial Console & U-Boot (UART)
Baud rate: `115200 8N1` on serial pad.

To prevent kernel console suspend when remote idles:
```
printenv bootargs
setenv bootargs ${bootargs} no_console_suspend
saveenv
boot
```

## USB Ethernet & Dropbear SSH Setup
Architecture: ARMv5TE little-endian (`armv5tejl`, uClibc-0.9.30.1).

### Remote Configuration
1. Installed Dropbear static binary (`v2022.82`) in `/data/dropbear` (symlinked to `/usr/sbin/dropbear`).
2. Installed `dropbearkey` in `/data/dropbearkey`.
3. Installed `scp` in `/data/scp` (symlinked to `/usr/bin/scp`).
4. SSH keys generated in `/etc/dropbear/`:
   - `dropbear_rsa_host_key`
   - `dropbear_ed25519_host_key`
5. User public key installed to `/home/root/.ssh/authorized_keys`.
6. Dropbear starts automatically on boot via `/etc/init.d/rcS.local`:
```sh
/usr/bin/usbeth start
/data/dropbear -R -B
```

### Host Connection (PC)
- **USB Ethernet Interface**: `192.168.2.2 / 24`
- **Remote IP**: `192.168.2.1`
- **SSH Access**:
  ```sh
  ssh elite
  # or
  ssh -i ~/.ssh/harmony_owner_ed25519 root@192.168.2.1
  ```
- **SCP File Transfer** (requires legacy `-O` flag):
  ```sh
  scp -O file.bin elite:/data/
  ```

## UART-Free Root Vector (USB HID)
Lessons from Harmony Hub root (`harmony-hub-root`):
1. **USB Interface**: Elite exposes USB HID (`046d:c129`) managed by `/usr/bin/usbhid` and forwarded to HAL.
2. **Exploit Primitive**: Path traversal in `harmony.log?put` (`../etc/tdeenable`) writes the developer marker `/etc/tdeenable`.
3. **Staging**: Unlocks `connect.jsonfiletransfer` (production check disabled by `tdeEnable`).
4. **Provisioning**: Static ARM binaries from [`elite/bin`](file:///c:/Users/YR/Documents/GitHub/harmony-hub-control/elite/bin) (`dropbear`, `dropbearkey`, `scp`) are staged in `/data/`, and `/etc/init.d/rcS.local` is modified to start USB Ethernet and Dropbear on next boot.