param(
    [Alias("Host")]
    [string]$HubHost,
    [string]$KeyPath,
    [int]$Port = 22,
    [string]$SshUser = "root",
    [string]$MqttBroker = "",
    [int]$MqttPort = 1883,
    [string]$MqttUser = "",
    [string]$MqttPassword = "",
    [string]$MqttBaseTopic = "harmony/hub",
    [string]$MqttDiscoveryPrefix = "homeassistant",
    [string]$MqttClientId = "harmony-local-mqtt",
    [switch]$MqttDisabled,
    [switch]$NoPrompt
)

$ErrorActionPreference = "Stop"

function Step($Text) {
    Write-Host ""
    Write-Host "== $Text ==" -ForegroundColor Cyan
}

function Info($Text) {
    Write-Host "  $Text"
}

function Prompt-IfMissing([string]$Value, [string]$Label, [switch]$Required) {
    if ($Value) { return $Value }
    if ($NoPrompt) {
        if ($Required) { throw "$Label is required" }
        return ""
    }
    $v = Read-Host $Label
    if ($Required -and -not $v) { throw "$Label is required" }
    return $v
}

function Resolve-DefaultKeyPath() {
    $userHome = $env:USERPROFILE
    if (-not $userHome) { $userHome = [Environment]::GetFolderPath("UserProfile") }
    if (-not $userHome) { return "" }
    $sshDir = Join-Path $userHome ".ssh"
    if (-not (Test-Path -LiteralPath $sshDir -PathType Container -ErrorAction SilentlyContinue)) { return "" }
    $keys = @(Get-ChildItem -LiteralPath $sshDir -File -Filter "harmony_owner_*" -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -notlike "*.pub" } |
        Sort-Object LastWriteTime -Descending)
    if ($keys.Count -lt 1) { return "" }
    return $keys[0].FullName
}

function Get-UniquePathList([string[]]$Paths) {
    $seen = @{}
    $out = @()
    foreach ($path in $Paths) {
        if (-not $path) { continue }
        if ($seen.ContainsKey($path)) { continue }
        $seen[$path] = $true
        $out += $path
    }
    return $out
}

function Quote-ProcessArg([string]$Arg) {
    if ($null -eq $Arg) { return '""' }
    if ($Arg.Length -eq 0) { return '""' }
    if ($Arg -notmatch '[\s"]') { return $Arg }
    $out = '"'
    $slashes = 0
    foreach ($ch in $Arg.ToCharArray()) {
        if ($ch -eq '\') {
            $slashes += 1
            continue
        }
        if ($ch -eq '"') {
            $out += ('\' * (($slashes * 2) + 1)) + '"'
            $slashes = 0
            continue
        }
        if ($slashes) {
            $out += ('\' * $slashes)
            $slashes = 0
        }
        $out += $ch
    }
    if ($slashes) { $out += ('\' * ($slashes * 2)) }
    return $out + '"'
}

function Remote-Quote([string]$Value) {
    return "'" + ($Value -replace "'", "'`"`"'`"'") + "'"
}

function Get-SshArgs() {
    $args = @(
        "-p", [string]$Port,
        "-i", $KeyPath,
        "-o", "IdentitiesOnly=yes",
        "-o", "BatchMode=yes",
        "-o", "StrictHostKeyChecking=accept-new",
        "$SshUser@$HubHost"
    )
    return $args
}

function Invoke-Remote([string]$Command, [byte[]]$InputBytes = $null, [int]$TimeoutMs = 90000) {
    $psi = [System.Diagnostics.ProcessStartInfo]::new()
    $psi.FileName = "ssh"
    $allArgs = (Get-SshArgs) + @($Command)
    $psi.Arguments = ($allArgs | ForEach-Object { Quote-ProcessArg $_ }) -join " "
    $psi.RedirectStandardInput = $true
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $p = [System.Diagnostics.Process]::Start($psi)
    if ($InputBytes) {
        try {
            $p.StandardInput.BaseStream.Write($InputBytes, 0, $InputBytes.Length)
        }
        catch {
            # Remote process closed stdin early (e.g. exit on error)
        }
    }
    try { $p.StandardInput.BaseStream.Close() } catch {}
    $stdoutTask = $p.StandardOutput.ReadToEndAsync()
    $stderrTask = $p.StandardError.ReadToEndAsync()
    if (-not $p.WaitForExit($TimeoutMs)) {
        try { $p.Kill() } catch {}
        throw "ssh timed out while running: $Command"
    }
    $stdout = $stdoutTask.GetAwaiter().GetResult()
    $stderr = $stderrTask.GetAwaiter().GetResult()
    if ($p.ExitCode -ne 0) {
        throw "ssh failed with exit $($p.ExitCode)`ncommand=$Command`nstdout=$stdout`nstderr=$stderr"
    }
    if ($stderr.Trim()) {
        Write-Host $stderr.Trim() -ForegroundColor DarkGray
    }
    return $stdout
}

function Invoke-RemoteBytes([string]$Command, [byte[]]$InputBytes = $null, [int]$TimeoutMs = 90000) {
    $psi = [System.Diagnostics.ProcessStartInfo]::new()
    $psi.FileName = "ssh"
    $allArgs = (Get-SshArgs) + @($Command)
    $psi.Arguments = ($allArgs | ForEach-Object { Quote-ProcessArg $_ }) -join " "
    $psi.RedirectStandardInput = $true
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $p = [System.Diagnostics.Process]::Start($psi)
    if ($InputBytes) {
        try {
            $p.StandardInput.BaseStream.Write($InputBytes, 0, $InputBytes.Length)
        }
        catch {}
    }
    try { $p.StandardInput.BaseStream.Close() } catch {}
    $ms = [System.IO.MemoryStream]::new()
    $copyTask = $p.StandardOutput.BaseStream.CopyToAsync($ms)
    $stderrTask = $p.StandardError.ReadToEndAsync()
    if (-not $p.WaitForExit($TimeoutMs)) {
        try { $p.Kill() } catch {}
        throw "ssh timed out while running: $Command"
    }
    [void]$copyTask.GetAwaiter().GetResult()
    $stderr = $stderrTask.GetAwaiter().GetResult()
    if ($p.ExitCode -ne 0) {
        throw "ssh failed with exit $($p.ExitCode)`ncommand=$Command`nstderr=$stderr"
    }
    if ($stderr.Trim()) {
        Write-Host $stderr.Trim() -ForegroundColor DarkGray
    }
    return $ms.ToArray()
}

function Test-TcpPort([string]$TargetHost, [int]$TargetPort, [int]$TimeoutMs = 1200) {
    $client = [System.Net.Sockets.TcpClient]::new()
    try {
        $async = $client.BeginConnect($TargetHost, $TargetPort, $null, $null)
        if (-not $async.AsyncWaitHandle.WaitOne($TimeoutMs, $false)) { return $false }
        $client.EndConnect($async)
        return $true
    }
    catch {
        return $false
    }
    finally {
        try { $client.Close() } catch {}
    }
}

function Wait-TcpPort([string]$TargetHost, [int]$TargetPort, [int]$Seconds, [string]$Label) {
    $deadline = [DateTime]::UtcNow.AddSeconds($Seconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        if (Test-TcpPort $TargetHost $TargetPort 1200) {
            Info "$Label is reachable on port $TargetPort"
            return
        }
        Start-Sleep -Seconds 2
    }
    throw "$Label did not become reachable on port $TargetPort within $Seconds seconds"
}

function Split-RemoteDir([string]$Path) {
    $i = $Path.LastIndexOf("/")
    if ($i -le 0) { return "/" }
    return $Path.Substring(0, $i)
}

function Upload-Bytes([string]$LocalPath, [string]$RemotePath, [string]$Mode) {
    if (-not (Test-Path -LiteralPath $LocalPath)) { throw "missing local file: $LocalPath" }
    $localMd5 = (Get-FileHash -Algorithm MD5 -LiteralPath $LocalPath).Hash.ToLowerInvariant()

    # Check if remote file already matches to avoid flash wear
    $check = (Invoke-Remote "test -f $(Remote-Quote $RemotePath) && md5sum $(Remote-Quote $RemotePath) 2>/dev/null || true" $null 15000).Trim()
    if ($check) {
        $parts = $check -split "\s+"
        if ($parts[0] -eq $localMd5) {
            Invoke-Remote "chmod $Mode $(Remote-Quote $RemotePath)" $null 5000 | Out-Null
            Info "$RemotePath up-to-date (md5=$($localMd5.Substring(0,8))...), skipping flash write"
            return
        }
    }

    $bytes = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $LocalPath))
    $dir = Split-RemoteDir $RemotePath
    $tmp = "$RemotePath.tmp-handoff-$PID"
    $cmd = "mkdir -p $(Remote-Quote $dir) && cat > $(Remote-Quote $tmp) && mv $(Remote-Quote $tmp) $(Remote-Quote $RemotePath) && chmod $Mode $(Remote-Quote $RemotePath)"
    Invoke-Remote $cmd $bytes ([Math]::Max(90000, 45000 + [int]($bytes.Length / 12000))) | Out-Null
    Info "$RemotePath written (bytes=$($bytes.Length) md5=$localMd5)"
}

function Upload-Text([string]$Text, [string]$RemotePath, [string]$Mode) {
    $bytes = [Text.Encoding]::UTF8.GetBytes($Text)
    $localMd5 = [System.BitConverter]::ToString([System.Security.Cryptography.MD5]::Create().ComputeHash($bytes)).Replace("-", "").ToLowerInvariant()

    $check = (Invoke-Remote "test -f $(Remote-Quote $RemotePath) && md5sum $(Remote-Quote $RemotePath) 2>/dev/null || true" $null 15000).Trim()
    if ($check) {
        $parts = $check -split "\s+"
        if ($parts[0] -eq $localMd5) {
            Invoke-Remote "chmod $Mode $(Remote-Quote $RemotePath)" $null 5000 | Out-Null
            Info "$RemotePath up-to-date, skipping flash write"
            return
        }
    }

    $dir = Split-RemoteDir $RemotePath
    $tmp = "$RemotePath.tmp-handoff-$PID"
    $cmd = "mkdir -p $(Remote-Quote $dir) && cat > $(Remote-Quote $tmp) && mv $(Remote-Quote $tmp) $(Remote-Quote $RemotePath) && chmod $Mode $(Remote-Quote $RemotePath)"
    Invoke-Remote $cmd $bytes 90000 | Out-Null
    if ($RemotePath -match "pass|authorized_keys") {
        Info "$RemotePath written (bytes=$($bytes.Length) md5=<hidden>)"
    }
    else {
        Info "$RemotePath written (bytes=$($bytes.Length) md5=$localMd5)"
    }
}

function Build-MqttConfig() {
    $enabled = (-not $MqttDisabled) -and [bool]$MqttBroker
    $cfg = [ordered]@{
        enabled         = $enabled
        name            = "Harmony Hub"
        clientId        = $MqttClientId
        baseTopic       = $MqttBaseTopic.Trim("/")
        discoveryPrefix = $MqttDiscoveryPrefix.Trim("/")
        haDiscovery     = $true
        pollSeconds     = 10
        keepAlive       = 60
        broker          = [ordered]@{
            host     = $MqttBroker
            port     = $MqttPort
            username = $MqttUser
            password = $MqttPassword
        }
    }
    return ($cfg | ConvertTo-Json -Compress -Depth 8) + "`n"
}

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$Payload = Join-Path $ScriptRoot "payload"

$HubHost = Prompt-IfMissing $HubHost "Harmony hub IP address" -Required
$defaultKeyPath = Resolve-DefaultKeyPath
if (-not $KeyPath -and $defaultKeyPath -and (Test-Path -LiteralPath $defaultKeyPath)) {
    $KeyPath = $defaultKeyPath
    Info "using SSH key $KeyPath"
}
$KeyPath = Prompt-IfMissing $KeyPath "SSH private key path for root login" -Required
$KeyPath = (Resolve-Path -LiteralPath $KeyPath).Path
if (-not (Test-Path -LiteralPath $KeyPath)) { throw "SSH key not found: $KeyPath" }

Step "Checking connection"
if (-not (Test-TcpPort $HubHost $Port 3000)) {
    throw "Cannot connect to $HubHost on port $Port`: port unreachable"
}
$identity = Invoke-Remote "id; uname -a" $null 30000
Write-Host $identity.Trim()

$hasExistingMqtt = ((Invoke-Remote "test -s /data/codexmqtt/config.json && echo 1 || echo 0" $null 15000).Trim() -eq "1")
$keepExistingMqtt = $false
if ($hasExistingMqtt) {
    $brokerHint = ""
    try {
        $rawMqtt = (Invoke-Remote "cat /data/codexmqtt/config.json 2>/dev/null || true" $null 15000).Trim()
        if ($rawMqtt) {
            $parsed = $rawMqtt | ConvertFrom-Json
            if ($parsed.broker -and $parsed.broker.host) { $brokerHint = $parsed.broker.host }
        }
    }
    catch {}

    if (-not $NoPrompt -and -not $MqttBroker -and -not $MqttDisabled) {
        $promptMsg = if ($brokerHint) { "Existing MQTT configuration found on hub (broker: $brokerHint). Keep current configuration? [Y/n]" } else { "Existing MQTT configuration found on hub. Keep current configuration? [Y/n]" }
        $ans = Read-Host $promptMsg
        if (-not $ans -or $ans -match "^(y|yes)$") {
            $keepExistingMqtt = $true
            Info "keeping existing MQTT configuration"
        }
    }
    elseif ($NoPrompt -and -not $MqttBroker -and -not $MqttDisabled) {
        $keepExistingMqtt = $true
        Info "keeping existing MQTT configuration"
    }
}

if (-not $keepExistingMqtt) {
    if (-not $NoPrompt -and -not $MqttBroker -and -not $MqttDisabled) {
        $MqttBroker = Read-Host "MQTT broker host/IP (blank to disable MQTT for now)"
        if (-not $MqttBroker) { $MqttDisabled = $true }
    }
    if (-not $NoPrompt -and $MqttBroker) {
        if (-not $MqttUser) { $MqttUser = Read-Host "MQTT username (blank if none)" }
        if (-not $MqttPassword) { $MqttPassword = Read-Host "MQTT password (blank if none)" }
    }
}


Step "Streaming remote backup to local host"
$backupDir = Join-Path $ScriptRoot "backups"
if (-not (Test-Path -LiteralPath $backupDir)) {
    New-Item -ItemType Directory -Path $backupDir -Force | Out-Null
}
$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$backupFolder = Join-Path $backupDir "hub_backup_$timestamp"
New-Item -ItemType Directory -Path $backupFolder -Force | Out-Null

$candidatePaths = @(
    # Boot & runtime scripts
    "/etc/init.d/rcS.local",
    "/etc/init.d/rcS",
    "/data/codex/init.sh",
    "/data/codex/recovery_ap.sh",
    "/usr/sbin/dropbear",
    "/usr/sbin/dropbearkey",

    # Active binaries
    "/data/codex/bin/codex_webui",
    "/data/codex/bin/codex_daemon",
    "/data/codex/bin/codex_btstack",
    "/data/codex/bin/codex_portal",
    "/data/codex/bin/codex_dhcpd",
    "/data/codex/bin/codex_sntp",
    "/data/codex/bin/dropbearmulti",
    "/data/codex/bin/register_ehci",
    "/data/codex/bin/mknod",
    "/data/codex/modules/g_serial.ko",
    "/mnt/data/usb_eth/register_ehci",
    "/mnt/data/usb_eth/start_usb_eth.sh",
    "/mnt/data/usb_eth/stop_usb_eth.sh",
    "/data/codex/usb_eth/start_usb_eth.sh",
    "/data/codex/usb_eth/stop_usb_eth.sh",

    # Core system & network config
    "/etc/tdeenable",
    "/etc/nowatchdog",
    "/etc/version",
    "/etc/wpa_supplicant.conf",
    "/home/root/.ssh/authorized_keys",
    "/etc/dropbear/authorized_keys",
    "/etc/dropbear/dropbear_rsa_host_key",
    "/etc/dropbear/dropbear_dss_host_key",
    "/etc/dropbear/dropbear_ecdsa_host_key",

    # Codex configs & state
    "/data/codexmqtt/config.json",
    "/data/codex/bt_remote_map.json",
    "/data/codex/bt_remote_mqtt",
    "/data/codex/bt_remote_target",
    "/data/codex/bt-devices.json",
    "/data/codex/bthid_target",
    "/data/codex/webui_auth.conf",
    "/data/codex/debug_logging.conf",
    "/data/codex/current_activity",

    # Harmony resources & IR database
    "/data/resources/DeviceList.json",
    "/data/resources/ActivityList.json",
    "/data/resources/FunctionList.json",
    "/data/resources/ProtocolList.json"
)

# Dynamically discover additional JSON / config files under /data
$dynamicCmd = "find /data/resources /data/codex /data/codexmqtt -maxdepth 2 -type f \( -name '*.json' -o -name '*.conf' \) 2>/dev/null || true"
$discovered = @((Invoke-Remote $dynamicCmd $null 15000) -split "`n" | ForEach-Object { $_.Trim() } | Where-Object { $_ })
$allCandidates = Get-UniquePathList ($candidatePaths + $discovered)

$checkScript = "for f in " + (($allCandidates | ForEach-Object { Remote-Quote $_ }) -join " ") + "; do test -f `"`$f`" && echo `"`$f`"; done"
$existingFiles = @((Invoke-Remote $checkScript $null 30000) -split "`n" | ForEach-Object { $_.Trim() } | Where-Object { $_ })
Info "found $($existingFiles.Count) files on hub to backup (binaries, scripts, and configuration)"

$hasLocalTar = [bool](Get-Command tar.exe -ErrorAction SilentlyContinue)
$tempTar = Join-Path $backupFolder "stream.tar"
$tarRemote = "tar -cf - " + (($existingFiles | ForEach-Object { Remote-Quote $_ }) -join " ") + " 2>/dev/null"
$tarSuccess = $false

if ($hasLocalTar -and $existingFiles.Count -gt 0) {
    try {
        $tarBytes = Invoke-RemoteBytes $tarRemote $null 90000
        if ($tarBytes -and $tarBytes.Length -ge 1024) {
            [System.IO.File]::WriteAllBytes($tempTar, $tarBytes)
            & tar.exe -xf $tempTar -C $backupFolder 2>$null
            Remove-Item -LiteralPath $tempTar -Force -ErrorAction SilentlyContinue
            $tarSuccess = $true
        }
    }
    catch {
        $tarSuccess = $false
    }
}

if (-not $tarSuccess) {
    foreach ($rpath in $existingFiles) {
        try {
            $bytes = Invoke-RemoteBytes "cat $(Remote-Quote $rpath)" $null 30000
            $relPath = $rpath.TrimStart("/").Replace("/", [System.IO.Path]::DirectorySeparatorChar)
            $localDest = Join-Path $backupFolder $relPath
            $parentDir = Split-Path -Parent $localDest
            if (-not (Test-Path -LiteralPath $parentDir)) {
                New-Item -ItemType Directory -Path $parentDir -Force | Out-Null
            }
            [System.IO.File]::WriteAllBytes($localDest, $bytes)
        }
        catch {
            Info "warning: could not stream $rpath`: $($_.Exception.Message)"
        }
    }
}

$backupZip = Join-Path $backupDir "hub_backup_$timestamp.zip"
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::CreateFromDirectory($backupFolder, $backupZip)
Remove-Item -LiteralPath $backupFolder -Recurse -Force
$backupTar = $backupZip
Info "backup saved locally: $backupZip ($((Get-Item $backupZip).Length) bytes, $($existingFiles.Count) files)"

# Clean up legacy flash backups and unused test artifacts on hub
Invoke-Remote "rm -rf /data/codex-backups /data/codex/bin/*.tmp-handoff* /data/*.tmp-handoff* /cache/*.log /cache/bin /data/codex/cloud_blocker.conf /data/codex/bt_backend.conf /opt/luaworks/tasks/connectserver/netservicestarter.lua /data/codex/bin/pair_b25.sh /data/codex/bin/do_pair.sh /data/codex/bin/test_ble_diag /data/codex/bin/test_smp /data/codex/bin/test_hci_sniff /data/codex/bin/codex_ir_send 2>/dev/null || true" $null 15000 | Out-Null

Step "Uploading binaries"
if (Test-Path -LiteralPath (Join-Path $Payload "bin\codex_daemon")) {
    Upload-Bytes (Join-Path $Payload "bin\codex_daemon") "/data/codex/bin/codex_daemon" "755"
}
Upload-Bytes (Join-Path $Payload "bin\dropbearmulti") "/data/codex/bin/dropbearmulti" "755"
Upload-Bytes (Join-Path $Payload "bin\codex_dhcpd") "/data/codex/bin/codex_dhcpd" "755"
if (Test-Path -LiteralPath (Join-Path $Payload "bin\codex_btstack")) {
    Upload-Bytes (Join-Path $Payload "bin\codex_btstack") "/data/codex/bin/codex_btstack" "755"
}
if (Test-Path -LiteralPath (Join-Path $Payload "bin\codex_sntp")) {
    Upload-Bytes (Join-Path $Payload "bin\codex_sntp") "/data/codex/bin/codex_sntp" "755"
}

Upload-Bytes (Join-Path $Payload "bin\codex_portal") "/data/codex/bin/codex_portal" "755"
Upload-Bytes (Join-Path $Payload "bin\codex_webui") "/data/codex/bin/codex_webui" "755"
if (Test-Path -LiteralPath (Join-Path $Payload "bin\register_ehci")) {
    Upload-Bytes (Join-Path $Payload "bin\register_ehci") "/data/codex/bin/register_ehci" "755"
}
if (Test-Path -LiteralPath (Join-Path $Payload "bin\mknod")) {
    Upload-Bytes (Join-Path $Payload "bin\mknod") "/data/codex/bin/mknod" "755"
}
Upload-Bytes (Join-Path $Payload "scripts\dropbear") "/usr/sbin/dropbear" "755"
Upload-Bytes (Join-Path $Payload "scripts\dropbearkey") "/usr/sbin/dropbearkey" "755"

Step "Uploading runtime files"
Upload-Bytes (Join-Path $Payload "scripts\init.sh") "/data/codex/init.sh" "755"
Upload-Bytes (Join-Path $Payload "scripts\bt_reconnect.sh") "/data/codex/bt_reconnect.sh" "755"
Upload-Bytes (Join-Path $Payload "scripts\recovery_ap.sh") "/data/codex/recovery_ap.sh" "755"
Upload-Bytes (Join-Path $Payload "scripts\rcS.local") "/etc/init.d/rcS.local" "755"
if (Test-Path -LiteralPath (Join-Path $Payload "scripts\network_manager.sh")) {
    Upload-Bytes (Join-Path $Payload "scripts\network_manager.sh") "/data/codex/network_manager.sh" "755"
}
if (Test-Path -LiteralPath (Join-Path $Payload "scripts\start_usb_eth.sh")) {
    Upload-Bytes (Join-Path $Payload "scripts\start_usb_eth.sh") "/mnt/data/usb_eth/start_usb_eth.sh" "755"
}
if (Test-Path -LiteralPath (Join-Path $Payload "scripts\stop_usb_eth.sh")) {
    Upload-Bytes (Join-Path $Payload "scripts\stop_usb_eth.sh") "/mnt/data/usb_eth/stop_usb_eth.sh" "755"
}

$modulesDir = Join-Path $Payload "modules"
if (Test-Path -LiteralPath $modulesDir -PathType Container) {
    Step "Uploading kernel modules"
    Get-ChildItem -LiteralPath $modulesDir -Filter "*.ko" | Sort-Object Name | ForEach-Object {
        Upload-Bytes $_.FullName "/mnt/data/usb_eth/$($_.Name)" "644"
        Upload-Bytes $_.FullName "/data/codex/modules/$($_.Name)" "644"
    }
}

Step "Uploading configuration"
Upload-Text "1`n" "/etc/tdeenable" "644"
Upload-Text "1`n" "/etc/nowatchdog" "644"
if ($keepExistingMqtt) {
    Info "keeping existing /data/codexmqtt/config.json"
}
else {
    Upload-Text (Build-MqttConfig) "/data/codexmqtt/config.json" "600"
}

Step "Post-install permissions and startup"
$post = "rm -rf /pkg/codexmqtt /data/codex/cloud_blocker.conf /data/codex/bt_backend.conf /opt/luaworks/tasks/connectserver/netservicestarter.lua /data/codex/bin/codex_hbus /data/codex/bin/codex_hal_ltcp /data/codex/bin/codex_bthid_keyboard /data/codex/bin/codex_bthid_remote /data/codex/bin/switch_bt.sh /data/codex/switch_bt.sh 2>/dev/null || true; " +
"mkdir -p /mnt/data/usb_eth /data/codex/bin /data/codex/modules /etc/dropbear /home/root/.ssh /data/codexmqtt; " +
"ln -sf /data/codex/bin/register_ehci /mnt/data/usb_eth/register_ehci 2>/dev/null || true; " +
"ln -sf /mnt/data/usb_eth /data/codex/usb_eth 2>/dev/null || true; " +
"ln -sf /mnt/data/usb_eth/start_usb_eth.sh /data/codex/bin/start_usb_eth.sh 2>/dev/null || true; " +
"ln -sf /mnt/data/usb_eth/stop_usb_eth.sh /data/codex/bin/stop_usb_eth.sh 2>/dev/null || true; " +
"ln -sf /mnt/data/usb_eth/start_usb_eth.sh /usr/sbin/start_usb_eth.sh 2>/dev/null || true; " +
"ln -sf /mnt/data/usb_eth/stop_usb_eth.sh /usr/sbin/stop_usb_eth.sh 2>/dev/null || true; " +
"ln -sf /data/codex/network_manager.sh /data/codex/bin/network_manager.sh 2>/dev/null || true; " +
"ln -sf /data/codex/network_manager.sh /usr/sbin/network_manager.sh 2>/dev/null || true; " +
"chmod 755 /mnt/data/usb_eth/*.sh /mnt/data/usb_eth/register_ehci /data/codex/bin/register_ehci /data/codex/bin/mknod 2>/dev/null || true; " +
"chmod 644 /data/codex/modules/*.ko /mnt/data/usb_eth/*.ko 2>/dev/null || true; " +
"ln -sf dropbearmulti /data/codex/bin/dropbear; " +
"ln -sf dropbearmulti /data/codex/bin/dropbearkey; " +
"chmod 755 /data/codex/bin/dropbearmulti /data/codex/bin/codex_dhcpd /data/codex/bin/codex_btstack /data/codex/bin/codex_sntp /data/codex/bin/codex_portal /data/codex/bin/codex_webui /data/codex/bin/codex_daemon /data/codex/bin/mknod /data/codex/init.sh /data/codex/bt_reconnect.sh /data/codex/recovery_ap.sh /data/codex/network_manager.sh /usr/sbin/dropbear /usr/sbin/dropbearkey /etc/init.d/rcS.local 2>/dev/null || true; " +
"chmod 600 /data/codexmqtt/config.json 2>/dev/null || true; " +
"chmod -x /usr/sbin/bluetoothd 2>/dev/null || true; " +
"/bin/busybox sync 2>/dev/null || true"
Invoke-Remote $post $null 60000 | Out-Null

$start = "killall -9 luaworks luadraws lua netmonitor codex_webui codex_daemon codex_btstack codex_bthid_remote codex_bthid_keyboard bluetoothd 2>/dev/null || true; " +
"chmod -x /opt/luaworks/luaworks 2>/dev/null || true; " +
"chmod -x /usr/sbin/bluetoothd 2>/dev/null || true; " +
"if ! ps | grep '[d]ropbear' >/dev/null 2>&1; then /usr/sbin/dropbear -R -p 22; fi; " +
"if [ -x /data/codex/bin/codex_webui ]; then " +
"/data/codex/bin/codex_webui 8080 >> /tmp/codex-init.log 2>&1 & " +
"fi; " +
"if [ -x /data/codex/bin/codex_daemon ]; then " +
"/data/codex/bin/codex_daemon 8089 >> /tmp/codex-init.log 2>&1 & " +
"fi; " +
"if [ -x /data/codex/bin/codex_btstack ]; then " +
"/data/codex/bin/codex_btstack >> /tmp/codex-init.log 2>&1 & " +
"fi; " +
"sleep 1; " +
"ps | grep '[c]odex_webui' || true; ps | grep '[c]odex_daemon' || true; ps | grep '[c]odex_btstack' || true; ps | grep '[d]ropbear' || true"
$running = Invoke-Remote $start $null 90000
Write-Host $running.Trim()

Step "Verifying binary checksums"
$expected = [ordered]@{
    "/data/codex/bin/dropbearmulti" = (Get-FileHash -Algorithm MD5 -LiteralPath (Join-Path $Payload "bin\dropbearmulti")).Hash.ToLowerInvariant()
    "/data/codex/bin/codex_dhcpd"   = (Get-FileHash -Algorithm MD5 -LiteralPath (Join-Path $Payload "bin\codex_dhcpd")).Hash.ToLowerInvariant()
    "/data/codex/bin/codex_portal"  = (Get-FileHash -Algorithm MD5 -LiteralPath (Join-Path $Payload "bin\codex_portal")).Hash.ToLowerInvariant()
    "/data/codex/bin/codex_webui"   = (Get-FileHash -Algorithm MD5 -LiteralPath (Join-Path $Payload "bin\codex_webui")).Hash.ToLowerInvariant()
}
if (Test-Path -LiteralPath (Join-Path $Payload "bin\codex_btstack")) {
    $expected["/data/codex/bin/codex_btstack"] = (Get-FileHash -Algorithm MD5 -LiteralPath (Join-Path $Payload "bin\codex_btstack")).Hash.ToLowerInvariant()
}
if (Test-Path -LiteralPath (Join-Path $Payload "bin\codex_sntp")) {
    $expected["/data/codex/bin/codex_sntp"] = (Get-FileHash -Algorithm MD5 -LiteralPath (Join-Path $Payload "bin\codex_sntp")).Hash.ToLowerInvariant()
}
if (Test-Path -LiteralPath (Join-Path $Payload "bin\codex_daemon")) {
    $expected["/data/codex/bin/codex_daemon"] = (Get-FileHash -Algorithm MD5 -LiteralPath (Join-Path $Payload "bin\codex_daemon")).Hash.ToLowerInvariant()
}
if (Test-Path -LiteralPath (Join-Path $Payload "bin\register_ehci")) {
    $expected["/data/codex/bin/register_ehci"] = (Get-FileHash -Algorithm MD5 -LiteralPath (Join-Path $Payload "bin\register_ehci")).Hash.ToLowerInvariant()
}
if (Test-Path -LiteralPath (Join-Path $Payload "bin\mknod")) {
    $expected["/data/codex/bin/mknod"] = (Get-FileHash -Algorithm MD5 -LiteralPath (Join-Path $Payload "bin\mknod")).Hash.ToLowerInvariant()
}
if (Test-Path -LiteralPath (Join-Path $Payload "modules\g_serial.ko")) {
    $expected["/data/codex/modules/g_serial.ko"] = (Get-FileHash -Algorithm MD5 -LiteralPath (Join-Path $Payload "modules\g_serial.ko")).Hash.ToLowerInvariant()
}

$paths = ($expected.Keys | ForEach-Object { Remote-Quote $_ }) -join " "
$verify = Invoke-Remote "md5sum $paths" $null 45000
Write-Host $verify.Trim()
foreach ($line in ($verify -split "`n")) {
    $parts = $line.Trim() -split "\s+"
    if ($parts.Count -ge 2 -and $expected.Contains($parts[1]) -and $expected[$parts[1]] -ne $parts[0]) {
        throw "checksum mismatch for $($parts[1]): expected $($expected[$parts[1]]) got $($parts[0])"
    }
}

Step "Done"
Info "Web UI: http://$HubHost`:8080/"
Info "Web UI authentication: disabled"
Info "Backup saved locally: $backupTar"