param(
    [Alias("Host")]
    [string]$HubHost,
    [string]$KeyPath,
    [int]$Port = 22,
    [string]$SshUser = "root",
    [string]$BackupPath,
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
    return @(
        "-p", [string]$Port,
        "-i", $KeyPath,
        "-o", "IdentitiesOnly=yes",
        "-o", "BatchMode=yes",
        "-o", "StrictHostKeyChecking=accept-new",
        "$SshUser@$HubHost"
    )
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
        catch {}
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

function Split-RemoteDir([string]$Path) {
    $i = $Path.LastIndexOf("/")
    if ($i -le 0) { return "/" }
    return $Path.Substring(0, $i)
}

function Upload-Bytes([string]$LocalPath, [string]$RemotePath, [string]$Mode) {
    if (-not (Test-Path -LiteralPath $LocalPath)) { throw "missing local file: $LocalPath" }
    $localMd5 = (Get-FileHash -Algorithm MD5 -LiteralPath $LocalPath).Hash.ToLowerInvariant()

    # Check if remote file already matches to avoid unnecessary flash writes
    $check = (Invoke-Remote "test -f $(Remote-Quote $RemotePath) && md5sum $(Remote-Quote $RemotePath) 2>/dev/null || true" $null 15000).Trim()
    if ($check) {
        $parts = $check -split "\s+"
        if ($parts[0] -eq $localMd5) {
            Invoke-Remote "chmod $Mode $(Remote-Quote $RemotePath)" $null 5000 | Out-Null
            Info "$RemotePath up-to-date (md5=$($localMd5.Substring(0,8))...), skipping write"
            return
        }
    }

    $bytes = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $LocalPath))
    $dir = Split-RemoteDir $RemotePath
    $tmp = "$RemotePath.tmp-restore-$PID"
    $cmd = "mkdir -p $(Remote-Quote $dir) && cat > $(Remote-Quote $tmp) && mv $(Remote-Quote $tmp) $(Remote-Quote $RemotePath) && chmod $Mode $(Remote-Quote $RemotePath)"
    Invoke-Remote $cmd $bytes ([Math]::Max(90000, 45000 + [int]($bytes.Length / 12000))) | Out-Null
    Info "$RemotePath restored (bytes=$($bytes.Length) md5=$localMd5)"
}

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path

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

Step "Selecting backup archive"
$backupDir = Join-Path $ScriptRoot "backups"
if (-not $BackupPath) {
    if (Test-Path -LiteralPath $backupDir) {
        $found = @(Get-ChildItem -LiteralPath $backupDir -File |
            Where-Object { $_.Name -match "^hub_backup_.*\.(zip|tar)$" } |
            Sort-Object LastWriteTime -Descending)
        if ($found.Count -gt 0) {
            $BackupPath = $found[0].FullName
            Info "selected latest backup: $($found[0].Name) ($($found[0].Length) bytes)"
        }
    }
}

if (-not $BackupPath) {
    $BackupPath = Prompt-IfMissing $BackupPath "Path to backup file (.zip, .tar, or directory)" -Required
}

$BackupPath = (Resolve-Path -LiteralPath $BackupPath).Path
if (-not (Test-Path -LiteralPath $BackupPath)) {
    throw "Backup path not found: $BackupPath"
}

Step "Extracting backup"
$tempDir = Join-Path ([System.IO.Path]::GetTempPath()) ("harmony_restore_" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $tempDir -Force | Out-Null

try {
    if ($BackupPath -match "\.zip$") {
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        [System.IO.Compression.ZipFile]::ExtractToDirectory($BackupPath, $tempDir)
    }
    elseif ($BackupPath -match "\.tar$") {
        $hasTar = [bool](Get-Command tar.exe -ErrorAction SilentlyContinue)
        if ($hasTar) {
            & tar.exe -xf $BackupPath -C $tempDir
        }
        else {
            throw "tar.exe not found on system to extract $BackupPath"
        }
    }
    elseif (Test-Path -LiteralPath $BackupPath -PathType Container) {
        Copy-Item -Path (Join-Path $BackupPath "*") -Destination $tempDir -Recurse -Force
    }
    else {
        throw "Unsupported backup format: $BackupPath"
    }

    $files = @(Get-ChildItem -LiteralPath $tempDir -Recurse -File)
    if ($files.Count -eq 0) {
        throw "No files found in backup archive: $BackupPath"
    }
    Info "extracted $($files.Count) files from backup"

    # Known legacy flat-filename mapping for older backups
    $legacyMap = @{
        "etc_init.d_rcS.local"                                    = "/etc/init.d/rcS.local"
        "_etc_init.d_rcS.local"                                   = "/etc/init.d/rcS.local"
        "etc_init.d_rcS"                                          = "/etc/init.d/rcS"
        "_etc_init.d_rcS"                                         = "/etc/init.d/rcS"
        "opt_luaworks_tasks_connectserver_netservicestarter.lua"  = "/opt/luaworks/tasks/connectserver/netservicestarter.lua"
        "_opt_luaworks_tasks_connectserver_netservicestarter.lua" = "/opt/luaworks/tasks/connectserver/netservicestarter.lua"
        "usr_sbin_dropbear"                                       = "/usr/sbin/dropbear"
        "_usr_sbin_dropbear"                                      = "/usr/sbin/dropbear"
        "usr_sbin_dropbearkey"                                    = "/usr/sbin/dropbearkey"
        "_usr_sbin_dropbearkey"                                   = "/usr/sbin/dropbearkey"
        "data_codex_hub_id"                                       = "/data/codex/hub_id"
        "_data_codex_hub_id"                                      = "/data/codex/hub_id"
        "data_codexmqtt_config.json"                              = "/data/codexmqtt/config.json"
        "_data_codexmqtt_config.json"                             = "/data/codexmqtt/config.json"
        "data_codex_init.sh"                                      = "/data/codex/init.sh"
        "_data_codex_init.sh"                                     = "/data/codex/init.sh"
        "data_codex_recovery_ap.sh"                               = "/data/codex/recovery_ap.sh"
        "_data_codex_recovery_ap.sh"                              = "/data/codex/recovery_ap.sh"
    }

    Step "Stopping active services on hub"
    Invoke-Remote "killall -9 luaworks luadraws lua netmonitor codex_webui codex_daemon codex_btstack 2>/dev/null || true" $null 15000 | Out-Null

    Step "Restoring files to hub"
    $restoredCount = 0
    foreach ($file in $files) {
        $rel = $file.FullName.Substring($tempDir.Length).TrimStart("\", "/") -replace "\\", "/"
        if ($legacyMap.ContainsKey($rel)) {
            $remoteTarget = $legacyMap[$rel]
        }
        else {
            $remoteTarget = "/" + $rel
        }

        # Determine target file mode
        $mode = "644"
        if ($remoteTarget -match "^/(usr/sbin|data/codex/bin)" -or $remoteTarget -match "\.(sh|local)$") {
            $mode = "755"
        }
        elseif ($remoteTarget -match "config\.json$|key$|\.key$") {
            $mode = "600"
        }

        Upload-Bytes $file.FullName $remoteTarget $mode
        $restoredCount += 1
    }

    Step "Applying post-restore permissions and restarting services"
    $postRestore = "mkdir -p /data/codex/bin /data/codex/modules /etc/dropbear /data/codexmqtt; " +
        "if [ -f /data/codex/bin/dropbearmulti ]; then " +
        "ln -sf dropbearmulti /data/codex/bin/dropbear 2>/dev/null || true; " +
        "ln -sf dropbearmulti /data/codex/bin/dropbearkey 2>/dev/null || true; " +
        "fi; " +
        "chmod 755 /data/codex/bin/* /data/codex/*.sh /usr/sbin/dropbear* /etc/init.d/rcS* 2>/dev/null || true; " +
        "chmod 644 /data/codex/modules/*.ko 2>/dev/null || true; " +
        "chmod 600 /data/codexmqtt/config.json /etc/dropbear/*key* 2>/dev/null || true; " +
        "/bin/busybox sync 2>/dev/null || true; " +
        "if ! ps | grep '[d]ropbear' >/dev/null 2>&1; then /usr/sbin/dropbear -R -p 22 2>/dev/null || true; fi; " +
        "if [ -x /data/codex/bin/codex_webui ]; then /data/codex/bin/codex_webui 8080 >> /tmp/codex-init.log 2>&1 & fi; " +
        "if [ -x /data/codex/bin/codex_daemon ]; then /data/codex/bin/codex_daemon 8089 >> /tmp/codex-init.log 2>&1 & fi; " +
        "if [ -x /data/codex/bin/codex_btstack ]; then /data/codex/bin/codex_btstack >> /tmp/codex-init.log 2>&1 & fi; " +
        "sleep 1; " +
        "ps | grep '[c]odex_webui' || true; ps | grep '[c]odex_daemon' || true; ps | grep '[c]odex_btstack' || true; ps | grep '[d]ropbear' || true"

    $running = Invoke-Remote $postRestore $null 90000
    Write-Host $running.Trim()

    Step "Done"
    Info "Successfully restored $restoredCount files to $HubHost"
    Info "Web UI: http://$HubHost`:8080/"
    Info "Hub services restarted."
}
finally {
    if (Test-Path -LiteralPath $tempDir) {
        Remove-Item -LiteralPath $tempDir -Recurse -Force -ErrorAction SilentlyContinue
    }
}
