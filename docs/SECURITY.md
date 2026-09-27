# Security Notes

This project changes a Harmony Hub into a local-control appliance. Treat it as trusted-LAN equipment.

## Current Policy

- HTTP authentication can be configured using the WebUI.
- The web UI should only be reachable from a trusted local network.
- SSH access is managed by the already-rooted hub state.
- MQTT credentials are provided at install time or from the web UI and must not be committed.

## Never Commit

- SSH private keys or `authorized_keys`
- MQTT passwords
- Home Assistant long-lived tokens
- Real hub backups
- Firmware dumps
- Rooting tools or vulnerability writeups
- Local IPs if they reveal a private deployment you do not want shared

## Before Sharing

Run a quick scan from the repo root:

```powershell
rg -n "PRIVATE KEY|BEGIN OPENSSH|password|authorized_keys|token|secret|credential" .
```

Review any hits before pushing or zipping the repository.

The installer intentionally writes `/etc/tdeenable` because the post-root runtime expects the local service mode to remain enabled. That is runtime setup, not a rooting method.
