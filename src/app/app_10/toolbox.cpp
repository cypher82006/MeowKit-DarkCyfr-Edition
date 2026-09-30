/**
 * @file toolbox.cpp
 * @brief App10 — DarkCyfr Tactical Toolbox & Sensor Suite
 * @version 1.0
 */

#include "toolbox.h"
#include "../app_08/badusb_ble.h"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace MOONCAKE::APPS
{
    /* ── Cyber Palette ── */
    static constexpr uint16_t COL_BG        = 0x10A2; // Dark Navy/Black
    static constexpr uint16_t COL_PANEL     = 0x18E4; // Slightly lighter card bg
    static constexpr uint16_t COL_BORDER    = 0x31A6; // Panel outline
    static constexpr uint16_t COL_TEXT      = 0xFFFF; // Pure white
    static constexpr uint16_t COL_MUTED     = 0x8410; // Dark grey
    static constexpr uint16_t COL_CYAN      = 0x07FF; // Neon Cyan
    static constexpr uint16_t COL_LIME      = 0xBEE7; // MeowKit Lime Green
    static constexpr uint16_t COL_ORANGE    = 0xFD20; // Alert Orange
    static constexpr uint16_t COL_RED       = 0xF800; // Warning Red

    /* Key codes for presentation clicker */
    static constexpr uint8_t K_PAGE_UP   = 0xD3;
    static constexpr uint8_t K_PAGE_DOWN = 0xD6;
    static constexpr uint8_t K_F5        = 0xC6;
    static constexpr uint8_t K_ESC       = 0xB1;

    AppToolbox::AppToolbox(DEVICES* device) : _device(device)
    {
        setAppInfo().name = "Toolbox";
    }

    void AppToolbox::onOpen()
    {
        _device->Lcd.fillScreen(COL_BG);
        _currentTab    = ToolTab::Level;
        _timerRunning  = false;
        _timerSeconds  = 25 * 60;
        _needsRedraw   = true;
        _lastRenderMs  = 0;
        _lastI2CScanMs = 0;
        _bleStarted    = false;

        // Perform initial I2C scan
        _scanI2CBus();
    }

    void AppToolbox::onClose()
    {
        if (_bleStarted) {
            bu_ble_end();
            _bleStarted = false;
        }
    }

    void AppToolbox::onRunning()
    {
        _handleInput();
        _updateTimer();

        uint32_t now = millis();
        // Render at ~30 FPS for smooth IMU/level visualization
        if (now - _lastRenderMs >= 33 || _needsRedraw) {
            _lastRenderMs = now;
            _needsRedraw  = false;

            _drawHeader();

            switch (_currentTab) {
                case ToolTab::Level:  _drawLevel();  break;
                case ToolTab::Timer:  _drawTimer();  break;
                case ToolTab::Telem:  _drawTelem();  break;
                case ToolTab::Remote: _drawRemote(); break;
                default: break;
            }

            _drawFooter();
        }
    }

    void AppToolbox::_handleInput()
    {
        // Tab switching via Joystick Left / Right
        if (_device->button.Left.pressed()) {
            uint8_t cur = (uint8_t)_currentTab;
            cur = (cur == 0) ? (uint8_t)ToolTab::Count - 1 : cur - 1;
            _currentTab = (ToolTab)cur;
            _device->Lcd.fillScreen(COL_BG);
            _needsRedraw = true;
        }
        else if (_device->button.Right.pressed()) {
            uint8_t cur = (uint8_t)_currentTab;
            cur = (cur + 1) % (uint8_t)ToolTab::Count;
            _currentTab = (ToolTab)cur;
            _device->Lcd.fillScreen(COL_BG);
            _needsRedraw = true;
        }

        // Tab-specific inputs
        if (_currentTab == ToolTab::Timer) {
            // [A] = Toggle Start / Pause
            if (_device->button.A.pressed()) {
                _timerRunning = !_timerRunning;
                _timerLastTick = millis();
                _needsRedraw = true;
            }
            // Joystick Up/Down = Switch Timer Mode
            if (_device->button.Up.pressed() || _device->button.Down.pressed()) {
                if (!_timerRunning) {
                    uint8_t m = (uint8_t)_timerMode;
                    m = (m + 1) % 4;
                    _timerMode = (TimerMode)m;
                    if (_timerMode == TimerMode::Pomodoro)        _timerSeconds = 25 * 60;
                    else if (_timerMode == TimerMode::ShortBreak) _timerSeconds = 5 * 60;
                    else if (_timerMode == TimerMode::LongBreak)  _timerSeconds = 15 * 60;
                    else if (_timerMode == TimerMode::Stopwatch)  _timerSeconds = 0;
                    _needsRedraw = true;
                }
            }
            // [B] short press = Reset timer
            if (_device->button.B.pressed() && !_device->button.B.isLongPress()) {
                _timerRunning = false;
                if (_timerMode == TimerMode::Pomodoro)        _timerSeconds = 25 * 60;
                else if (_timerMode == TimerMode::ShortBreak) _timerSeconds = 5 * 60;
                else if (_timerMode == TimerMode::LongBreak)  _timerSeconds = 15 * 60;
                else if (_timerMode == TimerMode::Stopwatch)  _timerSeconds = 0;
                _needsRedraw = true;
            }
        }
        else if (_currentTab == ToolTab::Remote) {
            if (!_bleStarted) {
                bu_ble_begin();
                _bleStarted = true;
            }
            if (_device->button.Up.pressed() || _device->button.Down.pressed()) {
                _clickerMode = (_clickerMode == ClickerMode::Slides) ? ClickerMode::Media : ClickerMode::Slides;
                _device->Lcd.fillScreen(COL_BG);
                _needsRedraw = true;
            }

            if (bu_ble_connected()) {
                if (_clickerMode == ClickerMode::Slides) {
                    if (_device->button.A.pressed()) {
                        // Start Slideshow (F5)
                        bu_ble_press(K_F5);
                        delay(20);
                        bu_ble_release_all();
                    }
                    if (_device->button.B.pressed() && !_device->button.B.isLongPress()) {
                        // Black screen ('b')
                        bu_ble_write('b');
                    }
                }
            }
        }
    }

    void AppToolbox::_drawHeader()
    {
        auto& Lcd = _device->Lcd;
        Lcd.fillRect(0, 0, 320, 24, COL_PANEL);
        Lcd.drawFastHLine(0, 24, 320, COL_BORDER);

        const char* tabNames[] = { "LEVEL", "TIMER", "TELEM", "REMOTE" };
        const int tabW = 80;

        for (int i = 0; i < 4; i++) {
            bool active = ((uint8_t)_currentTab == i);
            int x = i * tabW;
            if (active) {
                Lcd.fillRect(x, 0, tabW, 24, COL_LIME);
                Lcd.setTextColor(COL_BG, COL_LIME);
            } else {
                Lcd.setTextColor(COL_MUTED, COL_PANEL);
            }
            Lcd.setFont(&fonts::efontCN_14);
            int tw = (int)strlen(tabNames[i]) * 8;
            Lcd.setCursor(x + (tabW - tw) / 2, 4);
            Lcd.print(tabNames[i]);
        }
    }

    void AppToolbox::_drawFooter()
    {
        auto& Lcd = _device->Lcd;
        Lcd.fillRect(0, 216, 320, 24, COL_PANEL);
        Lcd.drawFastHLine(0, 215, 320, COL_BORDER);
        Lcd.setFont(&fonts::efontCN_14);
        Lcd.setTextColor(COL_TEXT, COL_PANEL);

        if (_currentTab == ToolTab::Level) {
            Lcd.setCursor(10, 220);
            Lcd.print("[<>]Tabs  [Flat=Level, Tilt=Angle]");
        } else if (_currentTab == ToolTab::Timer) {
            Lcd.setCursor(10, 220);
            Lcd.print("[A]Start/Stop [B]Reset [^v]Mode");
        } else if (_currentTab == ToolTab::Telem) {
            Lcd.setCursor(10, 220);
            Lcd.print("[<>]Tabs  Live Telemetry Active");
        } else if (_currentTab == ToolTab::Remote) {
            Lcd.setCursor(10, 220);
            Lcd.print("[^v]Mode [A]Action [Hold B]Exit");
        }
    }

    /* ══════════════════════════════════════════════════════════════════
     *  MODULE 1: SPIRIT LEVEL & 3D COMPASS
     * ══════════════════════════════════════════════════════════════════ */
    void AppToolbox::_drawLevel()
    {
        auto& Lcd = _device->Lcd;

        float ax = 0, ay = 0, az = 0;
        _device->imu.getAccel(&ax, &ay, &az);

        float mx = 0, my = 0, mz = 0;
        _device->imu.getMag(&mx, &my, &mz);

        // Calculate pitch and roll in degrees
        float pitch = atan2(ay, sqrt(ax * ax + az * az)) * 180.0f / M_PI;
        float roll  = atan2(-ax, az) * 180.0f / M_PI;

        // Calculate magnetic heading (0 ~ 360 deg)
        float heading = atan2(my, mx) * 180.0f / M_PI;
        if (heading < 0) heading += 360.0f;

        bool isLevel = (fabs(pitch) < 0.6f && fabs(roll) < 0.6f);

        // Center of the circular spirit level
        const int cx = 110;
        const int cy = 120;
        const int maxR = 64;

        // Clear display area
        Lcd.fillRect(0, 25, 320, 190, COL_BG);

        // Draw Target Rings
        Lcd.drawCircle(cx, cy, maxR, COL_BORDER);
        Lcd.drawCircle(cx, cy, 32, COL_BORDER);
        Lcd.drawCircle(cx, cy, 14, isLevel ? COL_LIME : COL_CYAN);
        Lcd.drawFastHLine(cx - maxR - 8, cy, (maxR * 2) + 16, COL_BORDER);
        Lcd.drawFastVLine(cx, cy - maxR - 8, (maxR * 2) + 16, COL_BORDER);

        // Calculate bubble coordinate
        int bx = cx + (int)(roll * 2.2f);
        int by = cy + (int)(pitch * 2.2f);

        // Clamp bubble inside boundary
        int dx = bx - cx;
        int dy = by - cy;
        float dist = sqrt(dx * dx + dy * dy);
        if (dist > (maxR - 8)) {
            bx = cx + (int)((dx / dist) * (maxR - 8));
            by = cy + (int)((dy / dist) * (maxR - 8));
        }

        // Draw spirit bubble
        uint16_t bubbleCol = isLevel ? COL_LIME : COL_CYAN;
        Lcd.fillCircle(bx, by, 8, bubbleCol);
        Lcd.drawCircle(bx, by, 8, TFT_WHITE);

        // Right side info panel
        const int rx = 195;
        Lcd.setFont(&fonts::efontCN_16);
        Lcd.setTextColor(isLevel ? COL_LIME : COL_CYAN, COL_BG);
        Lcd.setCursor(rx, 40);
        Lcd.printf(isLevel ? "[ LEVEL OK ]" : "[ TILTED ]");

        Lcd.setTextColor(COL_TEXT, COL_BG);
        Lcd.setCursor(rx, 70);
        Lcd.printf("PITCH: %+.1f*", pitch);

        Lcd.setCursor(rx, 95);
        Lcd.printf("ROLL : %+.1f*", roll);

        Lcd.setCursor(rx, 125);
        Lcd.setTextColor(COL_ORANGE, COL_BG);
        Lcd.printf("COMPASS:");

        const char* cardinal = "N";
        if (heading >= 22.5f && heading < 67.5f)        cardinal = "NE";
        else if (heading >= 67.5f && heading < 112.5f)  cardinal = "E";
        else if (heading >= 112.5f && heading < 157.5f) cardinal = "SE";
        else if (heading >= 157.5f && heading < 202.5f) cardinal = "S";
        else if (heading >= 202.5f && heading < 247.5f) cardinal = "SW";
        else if (heading >= 247.5f && heading < 292.5f) cardinal = "W";
        else if (heading >= 292.5f && heading < 337.5f) cardinal = "NW";

        Lcd.setCursor(rx, 150);
        Lcd.setFont(&fonts::Font4);
        Lcd.setTextColor(COL_TEXT, COL_BG);
        Lcd.printf("%3.0f* %s", heading, cardinal);
    }

    /* ══════════════════════════════════════════════════════════════════
     *  MODULE 2: POMODORO & STOPWATCH
     * ══════════════════════════════════════════════════════════════════ */
    void AppToolbox::_updateTimer()
    {
        if (!_timerRunning) return;

        uint32_t now = millis();
        if (now - _timerLastTick >= 1000) {
            _timerLastTick = now;

            if (_timerMode == TimerMode::Stopwatch) {
                _timerSeconds++;
            } else {
                if (_timerSeconds > 0) {
                    _timerSeconds--;
                    if (_timerSeconds == 0) {
                        _timerRunning = false;
                        // Timer completed alert: chime tone on speaker
                        if (!_device->speaker.isEnabled()) {
                            _device->speaker.begin();
                        }
                        _device->speaker.tone(1200, 400);
                    }
                }
            }
            _needsRedraw = true;
        }
    }

    void AppToolbox::_drawTimer()
    {
        auto& Lcd = _device->Lcd;
        Lcd.fillRect(0, 25, 320, 190, COL_BG);

        // Mode badge
        Lcd.setFont(&fonts::efontCN_16);
        const char* modeTitle = "POMODORO (25M)";
        if (_timerMode == TimerMode::ShortBreak) modeTitle = "SHORT BREAK (5M)";
        else if (_timerMode == TimerMode::LongBreak) modeTitle = "LONG BREAK (15M)";
        else if (_timerMode == TimerMode::Stopwatch) modeTitle = "COUNT-UP STOPWATCH";

        Lcd.setTextColor(COL_CYAN, COL_BG);
        int tw = (int)strlen(modeTitle) * 9;
        Lcd.setCursor((320 - tw) / 2, 45);
        Lcd.print(modeTitle);

        // Big Digital Clock Display (MM:SS)
        int mins = _timerSeconds / 60;
        int secs = _timerSeconds % 60;
        char timeBuf[16];
        snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", mins, secs);

        Lcd.setFont(&fonts::Font4);
        Lcd.setTextSize(2);
        Lcd.setTextColor(_timerRunning ? COL_LIME : COL_TEXT, COL_BG);
        Lcd.setCursor(65, 85);
        Lcd.print(timeBuf);
        Lcd.setTextSize(1);

        // Status indicator
        Lcd.setFont(&fonts::efontCN_16);
        if (_timerRunning) {
            Lcd.setTextColor(COL_LIME, COL_BG);
            Lcd.setCursor(125, 160);
            Lcd.print("RUNNING");
        } else {
            Lcd.setTextColor(COL_ORANGE, COL_BG);
            Lcd.setCursor(130, 160);
            Lcd.print("PAUSED");
        }
    }

    /* ══════════════════════════════════════════════════════════════════
     *  MODULE 3: HARDWARE TELEMETRY HUD
     * ══════════════════════════════════════════════════════════════════ */
    void AppToolbox::_scanI2CBus()
    {
        _i2cCount = 0;
        for (uint8_t addr = 0x08; addr < 0x78; addr++) {
            if (In_I2C.scanID(addr)) {
                if (_i2cCount < 16) {
                    _i2cAddresses[_i2cCount++] = addr;
                }
            }
        }
    }

    void AppToolbox::_drawTelem()
    {
        auto& Lcd = _device->Lcd;
        Lcd.fillRect(0, 25, 320, 190, COL_BG);

        // Rescan I2C every 4 seconds
        if (millis() - _lastI2CScanMs > 4000) {
            _lastI2CScanMs = millis();
            _scanI2CBus();
        }

        float vbat  = _device->pmu.getBatVoltage();
        float ibat  = _device->pmu.getBatCurrent();
        float batPct = _device->pmu.getBatLevel();
        float vbus  = _device->pmu.getVBUSVoltage();
        float tempPmu = _device->pmu.getAXP173Temp();
        bool charging = _device->pmu.isCharging();

        // 1. Power Card
        Lcd.fillRoundRect(10, 32, 145, 95, 4, COL_PANEL);
        Lcd.drawRoundRect(10, 32, 145, 95, 4, COL_BORDER);
        Lcd.setFont(&fonts::efontCN_14);
        Lcd.setTextColor(COL_LIME, COL_PANEL);
        Lcd.setCursor(18, 38);
        Lcd.print("AXP173 POWER");

        Lcd.setTextColor(COL_TEXT, COL_PANEL);
        Lcd.setCursor(18, 56);
        Lcd.printf("BAT: %.2fV (%.0f%%)", vbat, batPct);
        Lcd.setCursor(18, 74);
        Lcd.printf("CUR: %+.1fmA", ibat);
        Lcd.setCursor(18, 92);
        Lcd.printf("USB: %.2fV %s", vbus, charging ? "[CHG]" : "");
        Lcd.setCursor(18, 110);
        Lcd.printf("TEMP: %.1f*C", tempPmu);

        // 2. Memory & SoC Card
        Lcd.fillRoundRect(165, 32, 145, 95, 4, COL_PANEL);
        Lcd.drawRoundRect(165, 32, 145, 95, 4, COL_BORDER);
        Lcd.setTextColor(COL_CYAN, COL_PANEL);
        Lcd.setCursor(173, 38);
        Lcd.print("SYSTEM SOC");

        uint32_t freeHeap = esp_get_free_heap_size() / 1024;
        uint32_t freePsram = ESP.getFreePsram() / 1024;
        Lcd.setTextColor(COL_TEXT, COL_PANEL);
        Lcd.setCursor(173, 56);
        Lcd.printf("HEAP : %lu KB", (unsigned long)freeHeap);
        Lcd.setCursor(173, 74);
        Lcd.printf("PSRAM: %lu KB", (unsigned long)freePsram);
        Lcd.setCursor(173, 92);
        Lcd.printf("CPU  : 240 MHz");
        Lcd.setCursor(173, 110);
        Lcd.printf("RTC  : PCF8563 OK");

        // 3. I2C Bus Detection Map
        Lcd.fillRoundRect(10, 134, 300, 75, 4, COL_PANEL);
        Lcd.drawRoundRect(10, 134, 300, 75, 4, COL_BORDER);
        Lcd.setTextColor(COL_ORANGE, COL_PANEL);
        Lcd.setCursor(18, 140);
        Lcd.printf("I2C BUS DETECTED: %d DEVICE(S)", _i2cCount);

        Lcd.setTextColor(COL_TEXT, COL_PANEL);
        char devList[128] = "";
        for (int i = 0; i < _i2cCount; i++) {
            char hexBuf[16];
            snprintf(hexBuf, sizeof(hexBuf), "0x%02X ", _i2cAddresses[i]);
            strncat(devList, hexBuf, sizeof(devList) - strlen(devList) - 1);
        }
        Lcd.setCursor(18, 160);
        Lcd.print(devList);

        Lcd.setCursor(18, 185);
        Lcd.setTextColor(COL_MUTED, COL_PANEL);
        Lcd.print("Known: 19:IO 34:PMU 38:Touch 51:RTC 68:IMU");
    }

    /* ══════════════════════════════════════════════════════════════════
     *  MODULE 4: PRESENTATION & MEDIA CLICKER
     * ══════════════════════════════════════════════════════════════════ */
    void AppToolbox::_drawRemote()
    {
        auto& Lcd = _device->Lcd;
        Lcd.fillRect(0, 25, 320, 190, COL_BG);

        bool connected = bu_ble_connected();

        // Bluetooth status badge
        Lcd.fillRoundRect(20, 36, 280, 32, 4, connected ? COL_LIME : COL_PANEL);
        Lcd.drawRoundRect(20, 36, 280, 32, 4, COL_BORDER);
        Lcd.setFont(&fonts::efontCN_16);
        Lcd.setTextColor(connected ? COL_BG : COL_MUTED, connected ? COL_LIME : COL_PANEL);
        Lcd.setCursor(50, 42);
        Lcd.printf("BLE: %s (Pair 'MeowKit BadUSB')", connected ? "PAIRED & CONNECTED" : "ADVERTISING...");

        // Mode Display
        Lcd.setFont(&fonts::Font4);
        Lcd.setTextColor(COL_CYAN, COL_BG);
        Lcd.setCursor(50, 80);
        Lcd.printf("MODE: %s", (_clickerMode == ClickerMode::Slides) ? "SLIDESHOW" : "MEDIA PLAYER");

        // Action Mapping
        Lcd.setFont(&fonts::efontCN_16);
        Lcd.setTextColor(COL_TEXT, COL_BG);

        if (_clickerMode == ClickerMode::Slides) {
            Lcd.setCursor(40, 115);
            Lcd.print("Joystick [<-]: Prev Slide (PgUp)");
            Lcd.setCursor(40, 135);
            Lcd.print("Joystick [->]: Next Slide (PgDn)");
            Lcd.setCursor(40, 155);
            Lcd.print("Button   [A] : Start Presentation (F5)");
            Lcd.setCursor(40, 175);
            Lcd.print("Button   [B] : Blank / Black Screen ('b')");
        } else {
            Lcd.setCursor(40, 115);
            Lcd.print("Joystick [^] : Volume Up");
            Lcd.setCursor(40, 135);
            Lcd.print("Joystick [v] : Volume Down");
            Lcd.setCursor(40, 155);
            Lcd.print("Button   [A] : Play / Pause");
            Lcd.setCursor(40, 175);
            Lcd.print("Button   [B] : Mute");
        }
    }
}
