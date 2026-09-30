<#
.SYNOPSIS
    Synchronizes updated Lua apps and configurations from repo to MEOWKit SD Card.
.DESCRIPTION
    Auto-detects the MEOWKit SD card drive (default K:\, or probes removable drives)
    and syncs all updated .lua / .wasm apps, wifi.cfg, and media folders.
#>
param(
    [string]$TargetDrive = ""
)

Write-Host "=================================================" -ForegroundColor Cyan
Write-Host "   MEOWKIT S3 // SD CARD APPLICATION SYNC        " -ForegroundColor Cyan
Write-Host "=================================================" -ForegroundColor Cyan

$repoSdPath = Join-Path $PSScriptRoot "..\sd files"
if (-not (Test-Path $repoSdPath)) {
    Write-Error "Repo sd files path not found: $repoSdPath"
    exit 1
}

# Auto-detect target drive if not provided
if ([string]::IsNullOrWhiteSpace($TargetDrive)) {
    if (Test-Path "K:\") {
        $TargetDrive = "K:"
    } else {
        $vols = Get-Volume | Where-Object { $_.DriveType -eq 'Removable' -and $_.DriveLetter }
        foreach ($v in $vols) {
            $letter = "$($v.DriveLetter):"
            if (Test-Path "$letter\apps" -or (Test-Path "$letter\badusb")) {
                $TargetDrive = $letter
                break
            }
        }
    }
}

if ([string]::IsNullOrWhiteSpace($TargetDrive) -or (-not (Test-Path "$TargetDrive\"))) {
    Write-Warning "MEOWKit MicroSD card is not currently mounted."
    Write-Host "  -> To mount: Connect MEOWKit via USB-C, navigate to Settings/USB MSC, or insert the SD card directly into your PC." -ForegroundColor Yellow
    Write-Host "  -> Once connected, run: .\tools\sync_sd_apps.ps1" -ForegroundColor Yellow
    exit 0
}

Write-Host "[+] Target Drive Identified : $TargetDrive\" -ForegroundColor Green

# 1. Sync all apps
$destApps = "$TargetDrive\apps"
if (-not (Test-Path $destApps)) { New-Item -ItemType Directory -Path $destApps -Force | Out-Null }

Get-ChildItem -Path "$repoSdPath\apps\*" | ForEach-Object {
    Copy-Item -Path $_.FullName -Destination $destApps -Force
    Write-Host "  [SYNC] -> /apps/$($_.Name)" -ForegroundColor DarkCyan
}

# 2. Sync wifi.cfg (Safeguard: Never overwrite existing credentials on SD card)
$wifiDest = "$TargetDrive\wifi.cfg"
if (-not (Test-Path $wifiDest)) {
    $wifiSrc = "$repoSdPath\wifi.cfg"
    if (-not (Test-Path $wifiSrc)) { $wifiSrc = "$repoSdPath\wifi.cfg.example" }
    if (Test-Path $wifiSrc) {
        Copy-Item -Path $wifiSrc -Destination $wifiDest -Force
        Write-Host "  [INIT] -> /wifi.cfg (template deployed)" -ForegroundColor Green
    }
} else {
    Write-Host "  [PROTECT] -> /wifi.cfg (existing user credentials preserved)" -ForegroundColor Gray
}

# 3. Ensure recordings and data folders exist
$extraDirs = @("recordings", "logs", "pcaps", "badusb", "infrared")
foreach ($dir in $extraDirs) {
    $fullDir = "$TargetDrive\$dir"
    if (-not (Test-Path $fullDir)) {
        New-Item -ItemType Directory -Path $fullDir -Force | Out-Null
        Write-Host "  [INIT] -> /$dir/" -ForegroundColor Gray
    }
}

Write-Host "`n[SUCCESS] MEOWKit MicroSD card fully synchronized with all bugfixes and optimizations!" -ForegroundColor Green
