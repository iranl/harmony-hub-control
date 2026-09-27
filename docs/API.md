# Local Control API

The web UI intentionally has no HTTP authentication. Run it only on a trusted LAN or behind your own access controls.

All write endpoints accept `application/x-www-form-urlencoded` bodies. JSON responses use `ok: true` on success and `ok: false` with `error` on failure.

## Inventory

```powershell
Invoke-RestMethod "http://<hub-ip>:8080/api/inventory"
```

Returns hub limits, configured devices, command names, keycode/raw flags, and local command counts.

List runnable commands for one device:

```powershell
Invoke-RestMethod "http://<hub-ip>:8080/api/device-commands?deviceId=<device-id>"
```

## IR Control

Send one saved command:

```powershell
Invoke-RestMethod "http://<hub-ip>:8080/api/ir-send" -Method Post -Body @{
  deviceId = "<device-id>"
  command  = "PowerOff"
}
```

Send a cancellable batch:

```powershell
Invoke-RestMethod "http://<hub-ip>:8080/api/ir-batch-send" -Method Post -Body @{
  deviceId = "<device-id>"
  commands = "PowerOff`nInputHdmi1"
  delayMs  = "80"
  dryRun   = "0"
  runId    = "example-run-1"
}
```

The response includes `sent`, `attempted`, `skipped`, `failed`, `elapsedMs`, and `lastReply`. `failed` is incremented when the hub rejects a stored command, so large sweeps can keep going while still showing unsupported names clearly.

Cancel a running batch:

```powershell
Invoke-RestMethod "http://<hub-ip>:8080/api/ir-cancel" -Method Post -Body @{
  runId = "example-run-1"
}
```

Create or reuse the temporary IR sweep target:

```powershell
Invoke-RestMethod "http://<hub-ip>:8080/api/ir-lab-target" -Method Post
```

Import commands from database-converted lines:

```powershell
Invoke-RestMethod "http://<hub-ip>:8080/api/irdb-import" -Method Post -Body @{
  deviceId = "<device-id>"
  payload  = "PowerOff|G:Toshiba 32 Bit:(0xE0E040BF)(Repeat)():3"
}
```

Raw timing imports use:

```text
CommandName|raw|F9470P20D0S1068...
```

## IR Learning

Capture from a remote:

```powershell
Invoke-RestMethod "http://<hub-ip>:8080/api/capture" -Method Post -Body @{
  timeout = "8"
}
```

Test a learned signal before saving:

```powershell
Invoke-RestMethod "http://<hub-ip>:8080/api/ir-test-learned" -Method Post -Body @{
  deviceId = "<device-id>"
  name     = "PowerToggle"
  mode     = "raw"
  raw      = "F9470P20D0S1068..."
}
```

## Bluetooth HID

Make the hub discoverable and pairable as a keyboard:

```powershell
Invoke-RestMethod "http://<hub-ip>:8080/api/bt-call" -Method Post -Body @{
  action = "pairing_on"
  type   = "btkeyboard"
  name   = "Harmony Keyboard"
}
```

Check adapter and connection state:

```powershell
Invoke-RestMethod "http://<hub-ip>:8080/api/bt-call" -Method Post -Body @{
  action = "adapter_status"
}
```

Check the FIFO keyboard runtime:

```powershell
Invoke-RestMethod "http://<hub-ip>:8080/api/bt-text-status"
```

Send exact text through the keyboard FIFO runtime:

```powershell
Invoke-RestMethod "http://<hub-ip>:8080/api/bt-text" -Method Post -Body @{
  text = "Hello World`n"
}
```

The unified Bluetooth daemon `/data/codex/bin/codex_btstack` runs automatically and handles Classic Bluetooth HID keyboard emulation and BLE remote connectivity. The runtime reads `/tmp/bthid_input`, sends exact press/release reports for ASCII text, and uses the paired target saved by the Bluetooth controls.

Send a named key or shortcut through the low-level HID report path:

```powershell
Invoke-RestMethod "http://<hub-ip>:8080/api/bt-call" -Method Post -Body @{
  action = "report"
  type   = "btkeyboard"
  code   = "ctrl+l"
}
```

Send multiple named keys. Each key is encoded as a press and release pair:

```powershell
Invoke-RestMethod "http://<hub-ip>:8080/api/bt-call" -Method Post -Body @{
  action = "reportseq"
  type   = "btkeyboard"
  code   = "enter`nspace`nalt+f4"
  gapMs  = "35"
}
```

## Exports

```text
GET /export/bundle
GET /export/activities
GET /export/devices
GET /export/functions
GET /export/protocols
GET /export/mqtt
GET /export/wifi
GET /export/cloud
GET /export/bluetooth
GET /export/remote-mapping
```

Exports are for backups and debugging. Do not share files containing local network or credential material.

## Software Updates

The System page can update the local control stack from this repository. The browser fetches `payload/bin/MANIFEST.txt` and selected `codex_*` binaries, uploads them to the hub in chunks, then the hub verifies MD5 hashes from the manifest before installing. The default updater tries the GitHub contents API, raw GitHub, and jsDelivr mirrors so public updates still work when one browser fetch path is blocked.

Low-level SSH/dropbear files are intentionally not updated by the web UI.

```powershell
Invoke-RestMethod "http://<hub-ip>:8080/api/update-status"
```

The chunked update endpoints are:

```text
POST /api/update-begin
POST /api/update-chunk
POST /api/update-apply
```

The default public GitHub repository is read through GitHub's Contents API and works without a token. For a private GitHub repo or fork, paste a GitHub token into the System page update field. It is used only by the browser to read GitHub and is not sent to or stored on the hub. Change the raw base URL only when using a public mirror.

## Physical Bluetooth Remote

The unified daemon `/data/codex/bin/codex_btstack` connects to a paired BLE remote (e.g. Homatics B25 / RTL8762) over BLE GATT/HOGP, auto-encrypts the link using BTstack SM/LE-SC pairing, subscribes to HID report CCCDs, and decodes button presses from incoming notifications.
It maps buttons dynamically based on the hub's active activity context (or Off mode `-1`) using `/data/codex/bt_remote_map.json`, and dispatches IR, Bluetooth, or MQTT commands directly via hardware drivers (`hw_action.c`).

Endpoints:
- `GET /api/remote-mapping`: Returns daemon status, connected status, and the JSON mapping configuration.
- `POST /api/remote-mapping-save`: Saves mapping configuration JSON directly to `/data/codex/bt_remote_map.json`.
- `POST /api/remote-scan`: Initiates a non-blocking background BLE scan via `codex_btstack`.
- `GET /api/remote-scan-result`: Polls discovered Bluetooth devices and scan status.
- `POST /api/remote-pair`: Initiates pairing and encryption negotiation for the specified `addr`.
- `GET /api/remote-pair-status`: Polls pairing state and countdown timer.
