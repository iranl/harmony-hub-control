<#
.SYNOPSIS
    Deploy Harmony Elite custom binaries and recovery script over SSH.
.PARAMETER Ip
    Remote IP of the Elite remote (default 192.168.2.1).
.PARAMETER KeyPath
    Optional path to SSH private key.
#>
param(
    [string]$Ip = "192.168.2.1",
    [string]$KeyPath = ""
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$BinDir = Join-Path $ScriptDir "bin"
$RcsLocal = Join-Path $ScriptDir "rcS.local"
$Target = "root@$Ip"

$sshArgs = @("-o", "StrictHostKeyChecking=no", "-o", "UserKnownHostsFile=NUL")
if ($KeyPath -and (Test-Path $KeyPath)) {
    $sshArgs += @("-i", $KeyPath)
}

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "  Harmony Elite Deployment (SSH)" -ForegroundColor Cyan
Write-Host "  Target: $Target" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan

Write-Host "[*] Testing connection..." -ForegroundColor Yellow
$probe = & ssh @sshArgs $Target "uname -a" 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "[-] Could not connect to $Target over SSH: $probe" -ForegroundColor Red
    Write-Host "    Ensure USB Ethernet interface is set to 192.168.2.2/24."
    exit 1
}
Write-Host "[+] Remote: $probe" -ForegroundColor Green

$scpArgs = @("-O") + $sshArgs

$binaries = @("codex_elite", "codex_sync", "dropbear", "dropbearkey", "scp", "codex_rf_query", "codex_rf_sniff")
foreach ($b in $binaries) {
    $src = Join-Path $BinDir $b
    if (Test-Path $src) {
        Write-Host "    -> Uploading $b"
        & scp @scpArgs $src "${Target}:/data/$b" | Out-Null
    }
}

if (Test-Path $RcsLocal) {
    Write-Host "[*] Uploading /etc/init.d/rcS.local..." -ForegroundColor Yellow
    & scp @scpArgs $RcsLocal "${Target}:/etc/init.d/rcS.local" | Out-Null
}

Write-Host "[*] Configuring permissions and starting codex_elite..." -ForegroundColor Yellow
$postCmd = "chmod 755 /data/codex_* /data/dropbear* /data/scp /etc/init.d/rcS.local 2>/dev/null || true; " +
           "ln -sf /data/codex_sync /data/sync 2>/dev/null || true; " +
           "ln -sf /data/codex_sync /usr/bin/sync 2>/dev/null || true; " +
           "ln -sf /data/dropbear /usr/sbin/dropbear 2>/dev/null || true; " +
           "killall -9 codex_elite 2>/dev/null || true; " +
           "/data/codex_elite > /tmp/codex_elite.log 2>&1 & sleep 1; pidof codex_elite || echo 'offline'"

$res = & ssh @sshArgs $Target $postCmd
Write-Host "[+] Status: $res" -ForegroundColor Green
Write-Host "[+] Elite deployment complete!" -ForegroundColor Cyan
