<#
.SYNOPSIS
    Auto-Flash Watcher for MEOWKit ESP32-S3
.DESCRIPTION
    Polls Windows serial ports and immediately flashes firmware when MEOWKit connects.
#>

Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "   MEOWKIT AUTO-FLASH WATCHER // STANDBY                " -ForegroundColor Yellow
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "[*] Plug your MEOWKit S3 into USB-C..." -ForegroundColor White
Write-Host "[*] (If it doesn't appear, hold BOOT/Key A while plugging in)" -ForegroundColor Gray

$InitialPorts = [System.IO.Ports.SerialPort]::GetPortNames()

while ($true) {
    $CurrentPorts = [System.IO.Ports.SerialPort]::GetPortNames()
    
    # Check if any port is available
    if ($CurrentPorts.Count -gt 0) {
        $TargetPort = $CurrentPorts[0]
        Write-Host "`n[+] DETECTED COM PORT: $TargetPort" -ForegroundColor Green
        Write-Host "[*] Initiating flash sequence on $TargetPort..." -ForegroundColor Cyan
        
        $ScriptPath = Join-Path $PSScriptRoot "build_meowkit.ps1"
        & $ScriptPath -Action Upload -Port $TargetPort
        break
    }
    
    Start-Sleep -Milliseconds 500
}
