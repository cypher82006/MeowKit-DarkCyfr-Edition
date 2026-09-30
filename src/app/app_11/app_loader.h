/**
 * @file app_loader.h
 * @brief App11 — Dynamic SD App Loader (Lua 5.4 + WASM Wasm3 Engine)
 * @version 1.0
 */
#pragma once

#include <mooncake.h>
#include "../../bsp/devices.h"
#include <vector>
#include <string>

namespace MOONCAKE::APPS
{
    enum class AppFileType {
        Unknown = 0,
        Lua     = 1,
        Wasm    = 2
    };

    struct SdAppEntry {
        std::string filename;
        std::string path;
        AppFileType type;
        size_t      fileSize;
    };

    class AppLoader : public mooncake::AppAbility {
    public:
        AppLoader(DEVICES* device);
        void onOpen() override;
        void onRunning() override;
        void onClose() override;

    private:
        DEVICES* _device = nullptr;
        bool     _isRunningApp = false;
        int      _selectedIdx = 0;
        int      _scrollOffset = 0;
        std::vector<SdAppEntry> _appList;
        std::string _runningFile;

        /* Scan & Navigation */
        void _scanSdApps();
        void _drawBrowser();
        void _handleBrowserInput();

        /* Execution */
        bool _launchLua(const char* fullpath);
        bool _launchWasm(const char* fullpath);
        void _drawError(const char* msg);
    };
}
