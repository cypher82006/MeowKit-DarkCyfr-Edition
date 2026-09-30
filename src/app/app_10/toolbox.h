/**
 * @file toolbox.h
 * @brief App10 — DarkCyfr Tactical Toolbox & Sensor Suite
 * @version 1.0
 */
#pragma once

#include <mooncake.h>
#include "../../bsp/devices.h"
#include <cstdint>

namespace MOONCAKE::APPS
{
    enum class ToolTab : uint8_t {
        Level   = 0,
        Timer   = 1,
        Telem   = 2,
        Remote  = 3,
        Count   = 4
    };

    class AppToolbox : public mooncake::AppAbility {
    public:
        AppToolbox(DEVICES* device);
        void onOpen() override;
        void onRunning() override;
        void onClose() override;

    private:
        DEVICES* _device = nullptr;
        ToolTab  _currentTab = ToolTab::Level;
        uint32_t _lastRenderMs = 0;
        bool     _needsRedraw = true;

        /* ── Navigation & Input ── */
        void _handleInput();
        void _drawHeader();
        void _drawFooter();

        /* ── Module 1: Spirit Level & 3D Compass ── */
        void _drawLevel();

        /* ── Module 2: Pomodoro & Stopwatch ── */
        enum class TimerMode : uint8_t { Pomodoro = 0, ShortBreak = 1, LongBreak = 2, Stopwatch = 3 };
        TimerMode _timerMode = TimerMode::Pomodoro;
        bool      _timerRunning = false;
        int32_t   _timerSeconds = 25 * 60;
        uint32_t  _timerLastTick = 0;
        void _drawTimer();
        void _updateTimer();

        /* ── Module 3: Hardware Telemetry HUD ── */
        uint32_t _lastI2CScanMs = 0;
        uint8_t  _i2cCount = 0;
        uint8_t  _i2cAddresses[16];
        void _scanI2CBus();
        void _drawTelem();

        /* ── Module 4: BLE Presentation & Media Clicker ── */
        enum class ClickerMode : uint8_t { Slides = 0, Media = 1 };
        ClickerMode _clickerMode = ClickerMode::Slides;
        bool        _bleStarted = false;
        void _drawRemote();
    };
}
