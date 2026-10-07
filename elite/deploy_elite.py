#!/usr/bin/env python3
"""Deploy Harmony Elite Custom Runtime over SSH.

Deploys binaries, recovery boot script, and custom UI runtime to Logitech
Harmony Elite at 192.168.2.1 (USB Ethernet).

Usage:
    python deploy_elite.py [--ip 192.168.2.1] [--key ~/.ssh/id_rsa]
"""

from __future__ import annotations

import argparse
import pathlib
import subprocess
import sys

SCRIPT_DIR = pathlib.Path(__file__).resolve().parent
BIN_DIR = SCRIPT_DIR / "bin"
RCS_LOCAL_FILE = SCRIPT_DIR / "rcS.local"


def run_cmd(cmd: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, capture_output=True, text=True)


def main() -> None:
    parser = argparse.ArgumentParser(description="Deploy codex_elite and recovery scripts to Harmony Elite")
    parser.add_argument("--ip", default="192.168.2.1", help="Elite IP address (default 192.168.2.1)")
    parser.add_argument("--user", default="root", help="SSH username (default root)")
    parser.add_argument("--key", type=pathlib.Path, help="SSH private key path")
    args = parser.parse_args()

    target = f"{args.user}@{args.ip}"
    ssh_opts = ["-o", "StrictHostKeyChecking=no", "-o", "UserKnownHostsFile=/dev/null"]
    if args.key and args.key.is_file():
        ssh_opts.extend(["-i", str(args.key)])

    print("=" * 60)
    print("  Harmony Elite Custom Runtime Deployment")
    print(f"  Target: {target}")
    print("=" * 60)

    # 1. Test SSH connectivity
    print("[*] Testing SSH connection...")
    probe = run_cmd(["ssh"] + ssh_opts + [target, "uname -a"])
    if probe.returncode != 0:
        print(f"[-] Cannot connect to {target} over SSH: {probe.stderr.strip()}")
        print("    Verify USB cable is connected and PC interface is set to 192.168.2.2/24.")
        sys.exit(1)
    print(f"[+] Remote system online: {probe.stdout.strip()}")

    # 2. Upload binaries using scp (with legacy -O option for older Dropbear)
    scp_opts = ["-O"] + ssh_opts
    print("[*] Uploading ARM binaries to /data/...")
    binaries = ["codex_elite", "codex_sync", "dropbear", "dropbearkey", "scp", "codex_rf_query", "codex_rf_sniff"]
    for b in binaries:
        src = BIN_DIR / b
        if src.is_file():
            print(f"    -> {b}")
            res = run_cmd(["scp"] + scp_opts + [str(src), f"{target}:/data/{b}"])
            if res.returncode != 0:
                print(f"[-] Failed uploading {b}: {res.stderr.strip()}")

    # 3. Upload recovery boot script
    if RCS_LOCAL_FILE.is_file():
        print("[*] Uploading /etc/init.d/rcS.local (reboot counter & safe mode)...")
        res = run_cmd(["scp"] + scp_opts + [str(RCS_LOCAL_FILE), f"{target}:/etc/init.d/rcS.local"])
        if res.returncode != 0:
            print(f"[-] Failed uploading rcS.local: {res.stderr.strip()}")

    # 4. Set permissions and create symlinks
    print("[*] Finalizing remote permissions...")
    post = (
        "chmod 755 /data/codex_* /data/dropbear* /data/scp /etc/init.d/rcS.local 2>/dev/null || true; "
        "ln -sf /data/codex_sync /data/sync 2>/dev/null || true; "
        "ln -sf /data/codex_sync /usr/bin/sync 2>/dev/null || true; "
        "ln -sf /data/dropbear /usr/sbin/dropbear 2>/dev/null || true; "
        "killall -9 codex_elite 2>/dev/null || true; "
        "/data/codex_elite > /tmp/codex_elite.log 2>&1 & "
        "sleep 1; pidof codex_elite || echo 'offline'"
    )
    final_res = run_cmd(["ssh"] + ssh_opts + [target, post])
    print(f"    codex_elite PID: {final_res.stdout.strip()}")

    print()
    print("[+] Deployment to Harmony Elite completed successfully!")


if __name__ == "__main__":
    main()
