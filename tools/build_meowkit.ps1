<#
.SYNOPSIS
    DarkCyfr Tactical Build Engine // Whitecliff MEOWKit ESP32-S3
.DESCRIPTION
    Wraps PlatformIO CLI to compile, build, flash, and monitor MEOWKit firmware.
.PARAMETER Action
    Build (default), Upload, Clean, Monitor, or Export
#>
param(
    [ValidateSet("Build", "Upload", "Clean", "Monitor", "Export")]
    [string]$Action = "Build",
    [string]$Port = ""
)

Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "   MEOWKIT ESP32-S3 // TACTICAL BUILD ENGINE            " -ForegroundColor Yellow
Write-Host "========================================================" -ForegroundColor Cyan

# Force UTF-8 encoding for Python / PlatformIO CLI output on Windows
$env:PYTHONIOENCODING = "utf-8"
$env:PYTHONUTF8 = "1"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

# 1. Resolve PlatformIO Executable
$PioCandidates = @(
    "pio",
    "platformio",
    "$env:USERPROFILE\AppData\Local\Packages\PythonSoftwareFoundation.Python.3.11_qbz5n2kfra8p0\LocalCache\local-packages\Python311\Scripts\pio.exe",
    "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe"
)

$PioPath = $null
foreach ($cand in $PioCandidates) {
    if (Get-Command $cand -ErrorAction SilentlyContinue) {
        $PioPath = $cand
        break
    }
    if (Test-Path $cand) {
        $PioPath = $cand
        break
    }
}

if (-not $PioPath) {
    Write-Error "[-] PlatformIO CLI (pio.exe) not found. Run: python -m pip install -U platformio"
    exit 1
}

Write-Host "[+] Using PlatformIO : $PioPath" -ForegroundColor Green
$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $ProjectRoot
Write-Host "[*] Project Directory: $ProjectRoot" -ForegroundColor Gray
Write-Host "[*] Target MCU       : ESP32-S3-WROOM-1-N16R8 (16MB Flash / 8MB OPI PSRAM)" -ForegroundColor Gray

# 2. Execute Requested Action
switch ($Action) {
    "Build" {
        Write-Host "`n[*] Starting firmware compilation..." -ForegroundColor Cyan
        & $PioPath run
        if ($LASTEXITCODE -eq 0) {
            Write-Host "`n[OK] SUCCESS: MEOWKit firmware compiled successfully!" -ForegroundColor Green
            $BinPath = Join-Path $ProjectRoot ".pio\build\esp32s3box\firmware.bin"
            if (Test-Path $BinPath) {
                $Bytes = (Get-Item $BinPath).Length
                $SizeKB = [math]::Round($Bytes / 1024, 1)
                Write-Host "[+] Staged Binary : $BinPath [$SizeKB KB]" -ForegroundColor Yellow
                
                # Copy to release folder
                $ReleaseDir = Join-Path $ProjectRoot "build"
                if (-not (Test-Path $ReleaseDir)) { 
                    New-Item -ItemType Directory -Path $ReleaseDir -Force | Out-Null 
                }
                Copy-Item $BinPath (Join-Path $ReleaseDir "meowkit_firmware.bin") -Force
                Write-Host "[+] Exported to   : $ReleaseDir\meowkit_firmware.bin" -ForegroundColor Green
            }
        } else {
            Write-Error "[-] Build failed with exit code $LASTEXITCODE."
            exit $LASTEXITCODE
        }
    }

    "Upload" {
        Write-Host "`n[*] Preparing MEOWKit for flashing..." -ForegroundColor Cyan
        if ($Port -ne "") {
            Write-Host "[*] Probing $Port for software bootloader trigger..." -ForegroundColor DarkCyan
            try {
                $sp = [System.IO.Ports.SerialPort]::new($Port, 115200)
                $sp.ReadTimeout = 500
                $sp.WriteTimeout = 500
                $sp.DtrEnable = $true
                $sp.RtsEnable = $true
                $sp.Open()
                $sp.WriteLine("BOOTLOADER")
                Start-Sleep -Milliseconds 300
                $sp.Close()
                Write-Host "[+] Software bootloader command sent to $Port." -ForegroundColor Green
                Start-Sleep -Milliseconds 1200
            } catch {
                Write-Host "[-] Note: Serial trigger bypassed (device may already be in ROM bootloader mode)." -ForegroundColor Yellow
            }
        }
        Write-Host "`n[*] Flashing firmware to MEOWKit..." -ForegroundColor Cyan
        $UploadArgs = @("run", "--target", "upload")
        if ($Port -ne "") {
            $UploadArgs += @("--upload-port", $Port)
        }
        & $PioPath @UploadArgs
    }

    "Clean" {
        Write-Host "`n[*] Cleaning build environment..." -ForegroundColor Yellow
        & $PioPath run --target clean
    }

    "Monitor" {
        Write-Host "`n[*] Opening serial monitor [115200 baud]..." -ForegroundColor Cyan
        $MonitorArgs = @("device", "monitor", "-b", "115200")
        if ($Port -ne "") {
            $MonitorArgs += @("--port", $Port)
        }
        & $PioPath @MonitorArgs
    }

    "Export" {
        $BinPath = Join-Path $ProjectRoot ".pio\build\esp32s3box\firmware.bin"
        if (Test-Path $BinPath) {
            $ReleaseDir = Join-Path $ProjectRoot "build"
            if (-not (Test-Path $ReleaseDir)) { 
                New-Item -ItemType Directory -Path $ReleaseDir -Force | Out-Null 
            }
            Copy-Item $BinPath (Join-Path $ReleaseDir "meowkit_firmware.bin") -Force
            Write-Host "[OK] Exported: $ReleaseDir\meowkit_firmware.bin" -ForegroundColor Green
        } else {
            Write-Warning "[-] Binary not found. Run build first."
        }
    }
}
