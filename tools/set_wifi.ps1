<#
.SYNOPSIS
    Pushes Wi-Fi credentials to MEOWKit S3 over USB serial or creates/updates wifi.cfg.
.PARAMETER Port
    Serial COM port (default: COM5)
.PARAMETER SSID
    Target Wi-Fi SSID
.PARAMETER Password
    Target Wi-Fi Password
#>
param(
    [string]$Port = "COM5",
    [string]$SSID = "YOUR_SSID",
    [string]$Password = "YOUR_PASSWORD"
)

Write-Host "=================================================" -ForegroundColor Cyan
Write-Host "   MEOWKIT S3 // TACTICAL WIFI PROVISIONER       " -ForegroundColor Cyan
Write-Host "=================================================" -ForegroundColor Cyan
Write-Host "[+] Target SSID : $SSID" -ForegroundColor Green
Write-Host "[+] Target Pass : $Password" -ForegroundColor Green
Write-Host "[+] Target Port : $Port" -ForegroundColor Yellow

# 1. Update local sd files/wifi.cfg as well
$cfgPath = Join-Path $PSScriptRoot "..\sd files\wifi.cfg"
if (Test-Path (Split-Path $cfgPath)) {
    Set-Content -Path $cfgPath -Value "SSID=$SSID`nPASSWORD=$Password" -Encoding utf8
    Write-Host "[+] Updated $cfgPath" -ForegroundColor DarkCyan
}

# 2. Transmit over Serial if port is active
$ports = [System.IO.Ports.SerialPort]::GetPortNames()
if ($ports -contains $Port) {
    Write-Host "[*] Opening $Port at 115200 baud..." -ForegroundColor Cyan
    try {
        $sp = New-Object System.IO.Ports.SerialPort $Port, 115200, None, 8, One
        $sp.DtrEnable = $true
        $sp.RtsEnable = $true
        $sp.ReadTimeout = 1000
        $sp.WriteTimeout = 1000
        $sp.Open()
        Start-Sleep -Milliseconds 400

        $cmd = "WIFI:${SSID}:${Password}`n"
        $sp.Write($cmd)
        Write-Host "[OK] Sent command: WIFI:$SSID:***" -ForegroundColor Green
        Start-Sleep -Milliseconds 600

        while ($sp.BytesToRead -gt 0) {
            $line = $sp.ReadLine()
            Write-Host "  [ESP32] $line" -ForegroundColor Gray
        }
        $sp.Close()
        Write-Host "[SUCCESS] Wi-Fi provisioned successfully over serial!" -ForegroundColor Green
    } catch {
        Write-Warning "Serial transmission error: $_"
    }
} else {
    Write-Warning "Port $Port not currently connected. Once plugged in, run: .\tools\set_wifi.ps1 -Port $Port"
}
