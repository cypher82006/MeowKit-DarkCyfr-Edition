## 📋 Description of Changes
<!-- Provide a clear, concise summary of the changes introduced in this PR. Explain the problem it solves or the feature it implements. -->

## 🎯 Subsystems Affected
- [ ] Core Kernel / FreeRTOS Tasks
- [ ] App 11 Engine (Lua 5.4 / Wasm3 Host Bindings)
- [ ] Display & Graphics (LovyanGFX / ST7789V / NV3041A)
- [ ] Audio Stack (ES7210 / MAX98357A / I2S)
- [ ] Sensors & Power (AXP2101 / QMI8658)
- [ ] Radio / Wireless (802.11 / BLE / Promiscuous Sniffing)
- [ ] Storage & File I/O (SD Card / LittleFS / SPIFFS)
- [ ] Documentation / README / Pinouts

## 🧪 Verification & Hardware Testing
<!-- Describe the tests performed on physical ESP32-S3 hardware. -->
- [ ] Clean compilation with zero compiler warnings under PlatformIO (`pio run`)
- [ ] Flashed and tested on physical MEOWKit ESP32-S3 hardware
- [ ] Memory verified: No internal SRAM leaks detected via `esp_get_free_heap_size()`
- [ ] Checked for screen flicker / tearing during LCD frame updates
- [ ] Holding `[B]` button cleanly terminates app and frees all allocated PSRAM/SRAM

## 📸 Screenshots / Serial Monitor Logs (If applicable)
<!-- Attach photos of LCD output or copy-paste relevant serial monitor logs -->
```
```

## 🔒 Security & Code Quality Checklist
- [ ] Adheres to the [Code of Conduct](CODE_OF_CONDUCT.md)
- [ ] Follows [Contributing Guidelines](CONTRIBUTING.md)
- [ ] No hardcoded passwords, personal Wi-Fi SSIDs, or private tokens
- [ ] Breaking changes (if any) are explicitly documented above
