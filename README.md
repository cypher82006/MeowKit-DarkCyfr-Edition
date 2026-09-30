# ⚡ MeowKit-DarkCyfr-Edition (Custom ESP32-S3 Firmware)

```
███╗   ███╗███████╗ ██████╗ ██╗    ██╗██╗  ██╗██╗████████╗
████╗ ████║██╔════╝██╔═══██╗██║    ██║██║ ██╔╝██║╚══██╔══╝
██╔████╔██║█████╗  ██║   ██║██║ █╗ ██║█████╔╝ ██║   ██║   
██║╚██╔╝██║██╔══╝  ██║   ██║██║███╗██║██╔═██╗ ██║   ██║   
██║ ╚═╝ ██║███████╗╚██████╔╝╚███╔███╔╝██║  ██╗██║   ██║   
╚═╝     ╚═╝╚══════╝ ╚═════╝  ╚══╝╚══╝ ╚═╝  ╚═╝╚═╝   ╚═╝   
       ── DARKCYFR CUSTOM BUILD // V1.2.0-CYBER ──
```

Custom, hardened, high-performance firmware for the **MEOWKit ESP32-S3 Handheld Cyber Multitool**. Built for offensive security research, hardware hacking, network recon, and persistent field operations.

---

## 🛠️ Hardware Specification

* **SoC:** Espressif ESP32-S3-WROOM-1-N16R8 (Dual-Core Xtensa LX7 @ 240 MHz)
* **Storage / Memory:** 16 MB QIO Flash · 8 MB Octal OPI PSRAM · 512 KB SRAM
* **Display:** 2.4" 320×240 IPS Color LCD driven via LovyanGFX (40 MHz SPI DMA)
* **Storage Interface:** MicroSD slot mounted via native SDMMC (1-bit high-speed mode)
* **Audio Subsystem:** Everest ES7210 24-bit/16-bit I2S ADC codec + Dual Microphone Array + I2S Speaker
* **Power Management:** X-Powers AXP2101 PMU with I2C battery voltage, percentage, and charging telemetry
* **Sensors:** QMI8658 6-Axis Inertial Measurement Unit (Accelerometer + Gyroscope + Magnetometer + Temperature)
* **Radios:** 2.4 GHz 802.11 b/g/n Wi-Fi (Promiscuous capable) · Bluetooth 5.0 LE
* **Transceivers:** Infrared TX/RX LED arrays + WS2812 Digital RGB Status LED

---

## 🔥 DarkCyfr Edition Core Enhancements

### 1. Deterministic PSRAM Partitioning & Zero-Fragmentation Heap
Standard firmware architectures allocate runtime memory (Lua states, script text buffers, display canvasses) from the ESP32-S3's internal 8-bit SRAM. With FreeRTOS tasks and LVGL active, free internal SRAM quickly degrades below 80 KB, causing severe heap fragmentation where contiguous DMA blocks drop below 4 KB and trigger kernel panics.

* **PSRAM Lua VM Isolation (`lua_psram_alloc`):**
  100% of the Lua 5.4.7 VM heap, string pools, global symbol tables, closures, and userdata are allocated directly inside the **8 MB Octal PSRAM** (`MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT`). Internal SRAM consumption by Lua is literally **zero bytes**.
* **Instant Text Buffer Reclaim:**
  SD script buffers are loaded directly into PSRAM and explicitly freed immediately after bytecode compilation (`luaL_loadstring`), preventing multi-kilobyte script files from idling in memory.
* **Deterministic Radio & Hardware Teardown (`release_hardware_drivers`):**
  Exiting any application (via `[Hold B]`, normal exit, or script completion) completely stops and deallocates:
  * Wi-Fi STA buffers (`WiFi.mode(WIFI_OFF)` + `esp_wifi_stop()`) — reclaims ~50 KB internal DMA SRAM.
  * BLE controller and Bluedroid host memory (`BLEDevice::deinit(true)`) — reclaims ~35 KB internal SRAM.
  * UDP sockets, active I2S recording tasks, and status LEDs.
* **Double-Sweep Garbage Collection:**
  Forces two full `lua_gc(L, LUA_GCCOLLECT, 0)` sweeps before closing Lua states to guarantee metatable finalizers flush cleanly.
* **Real-Time Memory HUD & Telemetry:**
  The SD Apps browser footer features a real-time memory monitor:
  ```
  [^v]Sel [A]Run [B]Exit | SRAM:94K(max 86K) PSRAM:7850K
  ```
  Serial telemetry tracks heap recovery after every single application lifecycle.

### 2. Native 802.11 Promiscuous Sniffer & Hidden SSID Decloaker
* Added native C++ promiscuous packet handler `meow.wifi_decloak(bssid, channel, timeout_ms)`.
* Listens on target 802.11 channels for probe response frames and association requests to resolve hidden networks (empty broadcast beacons) into verified plain-text SSIDs automatically.

### 3. Wasm3 WebAssembly Engine Hardening
* Sandboxed memory allocation pool preventing WebAssembly runtime panics from exceeding hardware SRAM limits.
* Recompiled Rust WASM toolchain to run with `--initial-memory=65536` (1 page / 64 KB) and `-zstack-size=4096`, preventing the default 1 MB stack panic.
* Trap handling converts runtime exceptions into non-crashing UI alerts.

### 4. Hardware Driver Fixes
* **AXP2101 PMU Telemetry:** Direct ADC register queries for accurate battery voltage, charge state, and battery percentage (`meow.bat()`).
* **ES7210 Codec Pipeline:** Repaired I2S clock routing, DMA FIFO configurations, and automated `/recordings/` folder creation (`meow.record_wav`, `meow.mic_level`).
* **LovyanGFX Differential Reticle Rendering:** Replaced full-frame SPI blanking with differential scanline caching for smooth, tear-free 30+ FPS graphics.

---

## 💻 Lua 5.4 Host Bindings (`meow.*`)

| API Function | Parameters | Description |
|---|---|---|
| `meow.clear(col)` | `col` *(opt)* | Clear LCD screen to background or specified RGB565 color. |
| `meow.text(x, y, str, col)` | `x, y, str, col` | Render text at `(x, y)` using embedded Japanese/Unicode fonts. |
| `meow.rect(x, y, w, h, col, fill)` | `x, y, w, h, col, fill` | Draw bordered or filled rectangle. |
| `meow.circle(x, y, r, col, fill)` | `x, y, r, col, fill` | Draw bordered or filled circle. |
| `meow.btn(id)` | `"A"`, `"B"`, `"UP"`, etc. | Non-blocking tactile button and joystick input query. |
| `meow.imu()` | *none* | Returns 3-axis accelerometer data `(ax, ay, az)` in Gs. |
| `meow.gyro()` | *none* | Returns 3-axis gyroscope data `(gx, gy, gz)` in dps. |
| `meow.mag()` | *none* | Returns 3-axis magnetometer data `(mx, my, mz)` in uT. |
| `meow.temp()` | *none* | Returns internal IMU temperature in Celsius. |
| `meow.bat()` | *none* | Returns `(voltage, percent, is_charging, vbus_connected)`. |
| `meow.tone(freq, ms)` | `freq, ms` | Play acoustic frequency tone on internal speaker. |
| `meow.led(r, g, b)` | `r, g, b` (0..255) | Set onboard WS2812 RGB LED color. |
| `meow.wifi_scan()` | *none* | Returns array of detected Wi-Fi APs with SSID, BSSID, RSSI, Ch, Auth. |
| `meow.wifi_connect(ssid, pass)` | `ssid, pass` | Connect to 802.11 b/g/n wireless access point. |
| `meow.wifi_decloak(bssid, ch, ms)` | `bssid, ch, ms` | Promiscuous sniffer to decloak hidden SSID name. |
| `meow.mem()` | *none* | Returns `(sram_free, sram_largest_block, psram_free)`. |
| `meow.gc()` | *none* | Force full Lua garbage collection sweep and task yield. |
| `meow.ble_airtag(enable)` | `bool` | Toggle Apple FindMy / AirTag beacon emulation. |
| `meow.record_wav(path, sec)` | `path, sec` | Record audio from dual microphone array into 16kHz WAV. |
| `meow.mic_level()` | *none* | Returns instantaneous RMS microphone amplitude (0..100). |
| `meow.read_file(path)` | `path` | Read text/binary content from MicroSD file as string. |
| `meow.write_file(path, data)` | `path, data` | Write content to MicroSD file (overwriting existing). |
| `meow.append_file(path, data)` | `path, data` | Append string data directly to MicroSD file. |

---

## ⚡ Building & Flashing

### Requirements
* [PlatformIO Core](https://platformio.org/) (`python -m pip install -U platformio`)
* USB-C data cable connected to MEOWKit S3

### Build Firmware
```powershell
# In PowerShell (UTF-8 encoding enabled)
$env:PYTHONIOENCODING="utf-8"
python -m platformio run
```

### Flash to Device
Put device in bootloader mode (or connect while powered on) and upload:
```powershell
python -m platformio run --target upload --upload-port COM5
```

---

## 📂 MicroSD Payload Arsenal

All companion apps, wardriving utilities, BadUSB attack scripts, and compiled WebAssembly binaries are maintained in the companion repository:
👉 **[MeowKit-SD-Payloads](https://github.com/cypher82006/MeowKit-SD-Payloads)**

---

## 🏴‍☠️ Author & License

* **Lead Architect:** DarkCyfr (Navy Veteran · Cyber Security Operations)
* **Upstream Base:** WhitecliffTech / Mingo MeowKit-S3
* **License:** MIT / Apache 2.0 (respecting upstream submodules and components)
