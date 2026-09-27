# Build Notes

The repository ships ready-to-install MIPS binaries in `payload/bin/`. Rebuild only when native source changes.

## Target

- CPU: MIPS, big-endian
- Userspace: uClibc-era embedded Linux
- Output: mostly static helper binaries

## Linux Build

The helper build script is written for a Debian/Kali-like Linux environment:

```sh
cd build
./build_harmony_tools_kali.sh
```

It downloads the Bootlin MIPS uClibc toolchain and Dropbear source into
`build/toolchains/` and `build/tmp/`, then writes fresh binaries to
`build/output/`.

After rebuilding:

1. Copy the required binaries from `build/output/` into `payload/bin/`.
2. Refresh `payload/bin/MANIFEST.txt`.
3. Deploy to a test hub with `install_webui.ps1` or `python3 install_webui.py`.
4. Confirm checksums and runtime behavior.

## Windows

Windows can deploy with PowerShell, and Linux/macOS can deploy with the Python installer. Use WSL, a Linux VM, or the existing build server for rebuilding MIPS binaries.

## Dropbear

`dropbearmulti` is included so the web UI package can keep SSH reachable on a rooted hub. The installer does not replace `authorized_keys`; it only uploads Dropbear binaries/wrappers and starts the service if needed.
