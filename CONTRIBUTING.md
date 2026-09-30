# 🛠️ Contributing to MeowKit-DarkCyfr-Edition

Welcome, Operator. We welcome contributions, hardware optimizations, peripheral drivers, and subsystem improvements to the **MeowKit-DarkCyfr-Edition** firmware for the ESP32-S3.

To maintain rock-solid field reliability, zero screen tearing, and strict memory safety, please follow these guidelines when submitting issues or code.

---

## 🧭 Code of Conduct

All contributors are expected to uphold the standards outlined in our [Code of Conduct](CODE_OF_CONDUCT.md). Respectful collaboration, rigorous testing, and ethical security practices are strictly required.

---

## 🏗️ Hardware Architecture & Targets

* **MCU:** ESP32-S3-WROOM-1 (Dual-core Xtensa LX7 @ 240 MHz)
* **Flash:** 16 MB QIO Flash (`partitions_16mb.csv`)
* **PSRAM:** 8 MB Octal SPI PSRAM
* **Display:** 320x240 LCD (ST7789V / NV3041A with LovyanGFX double-buffering)
* **Audio:** ES7210 I2S ADC (Dual Mics) + MAX98357A I2S DAC (Piezo / Speaker)
* **Sensors:** QMI8658 6-Axis IMU (I2C)
* **PMIC:** AXP2101 Power Management IC (I2C)
* **Storage:** MicroSD over 1-bit / 4-bit SPI / SDMMC

---

## ⚡ Development Setup

### Prerequisites
1. [Visual Studio Code](https://code.visualstudio.com/) with the **PlatformIO IDE** extension.
2. ESP-IDF / Arduino-ESP32 framework (automatically managed by PlatformIO).
3. Git CLI.

### Building Firmware
```bash
# Clone the repository
git clone https://github.com/cypher82006/MeowKit-DarkCyfr-Edition.git
cd MeowKit-DarkCyfr-Edition

# Build default environment
pio run -e esp32-s3-devkitc-1

# Build and flash via USB Serial/JTAG
pio run -e esp32-s3-devkitc-1 -t upload

# Open Serial Monitor (115200 baud)
pio device monitor -b 115200
```

---

## 🧠 Memory Discipline & Coding Standards

Because the ESP32-S3 shares memory between Wi-Fi buffers, Bluetooth stacks, audio queues, and display framebuffers, adherence to memory discipline is paramount:

### 1. PSRAM First Allocation
* **Always** place large dynamic buffers, file caches, and execution contexts in **PSRAM**:
  ```cpp
  void* buf = ps_malloc(size);
  // or
  void* buf = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
  ```
* Reserve **Internal SRAM** (`MALLOC_CAP_INTERNAL`) strictly for DMA descriptors, interrupt handlers, and time-critical rendering routines.

### 2. App 11 (Dynamic Scripting Engine) Rules
* Any new Lua binding added to `src/app/app_11/app_loader.cpp` must:
  * Check argument counts and types using `luaL_check*()`.
  * Return integer error codes or throw descriptive Lua errors with `luaL_error()`.
  * Guarantee deterministic resource cleanup if the user terminates the app (holding the `[B]` button).
* Any new Wasm3 host symbol must be registered in both the environment table and documented in `README.md`.

### 3. Display & UI Rendering
* Always draw into the double-buffered LovyanGFX sprite or direct DMA memory.
* Never call long blocking delays (`delay()` or `vTaskDelay()`) inside UI rendering loops. Yield to FreeRTOS tasks cleanly.

---

## 🔀 Git Workflow & Pull Requests

1. **Fork the Repo:** Create a topic branch from `main`:
   ```bash
   git checkout -b feat/axp2101-fast-charge
   ```
2. **Commit Conventions:** Follow Conventional Commits:
   * `feat:` New features, hardware drivers, or API bindings.
   * `fix:` Bug fixes, memory leak resolutions, or crash prevention.
   * `docs:` Documentation, pinout tables, or guides.
   * `perf:` Execution speed optimizations or memory footprint reductions.
   * `refactor:` Code restructuring without logic alterations.
3. **Verify Build:** Confirm clean compilation with zero compiler warnings under PlatformIO:
   ```bash
   pio run
   ```
4. **Submit PR:** Use our [Pull Request Template](.github/pull_request_template.md) and detail the testing performed on physical hardware.

---

## 🐛 Reporting Bugs & Requesting Features

* To report a bug, open an issue using the [Bug Report Template](.github/ISSUE_TEMPLATE/bug_report.md). Include serial monitor logs, firmware commit hash, and hardware revision.
* To propose a new driver, peripheral, or architectural upgrade, submit a [Feature Proposal](.github/ISSUE_TEMPLATE/feature_request.md).
