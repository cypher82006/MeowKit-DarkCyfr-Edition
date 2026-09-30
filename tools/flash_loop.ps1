<#
.SYNOPSIS
    Continuous Flasher Loop for MEOWKit ESP32-S3
#>
$Esptool = "C:\Users\cyphe\.platformio\packages\tool-esptoolpy\esptool.py"
$BinPath = "C:\Users\cyphe\meowkit-workspace\.pio\build\esp32s3box\firmware.bin"
$Bootloader = "C:\Users\cyphe\meowkit-workspace\.pio\build\esp32s3box\bootloader.bin"
$Partitions = "C:\Users\cyphe\meowkit-workspace\.pio\build\esp32s3box\partitions.bin"

Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "   MEOWKIT RECOVERY & DOWNLOAD-MODE FLASHER             " -ForegroundColor Yellow
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "[*] HOLD Key [A] (BOOT), tap RESET, then release Key [A]..." -ForegroundColor White

while ($true) {
    Write-Host "`n[*] Probing COM6 for ESP32-S3 ROM Bootloader..." -ForegroundColor Gray
    
    # Try esptool write_flash
    & python $Esptool --chip esp32s3 --port COM6 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size 16MB 0x0 $Bootloader 0x8000 $Partitions 0x10000 $BinPath
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "`n========================================================" -ForegroundColor Green
        Write-Host "   [SUCCESS] MEOWKIT S3 FLASH COMPLETE!                " -ForegroundColor Green
        Write-Host "========================================================" -ForegroundColor Green
        break
    }
    
    Start-Sleep -Seconds 1
}
