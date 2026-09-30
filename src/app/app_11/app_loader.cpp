/**
 * @file app_loader.cpp
 * @brief App11 — Dynamic SD App Loader (Lua 5.4 + WASM Wasm3 Engine)
 * @version 1.0
 */

#include "app_loader.h"
#include <SD_MMC.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <WiFi.h>
#include <esp_wifi.h>
#include <HTTPClient.h>
#include <WiFiUDP.h>
#include "../../bsp/config.h"
#include "../../bsp/i2c/I2C_Class.hpp"
#include "../../bsp/audio/ES7210_Class.hpp"
#include <driver/i2s.h>
#include <BLEDevice.h>
#include <esp_gap_ble_api.h>
#include <esp_heap_caps.h>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include "wasm3.h"
#include "m3_env.h"

namespace MOONCAKE::APPS
{
    static DEVICES* s_dev = nullptr;

    /* ── Cyber Palette ── */
    static constexpr uint16_t COL_BG     = 0x10A2;
    static constexpr uint16_t COL_PANEL  = 0x18E4;
    static constexpr uint16_t COL_BORDER = 0x31A6;
    static constexpr uint16_t COL_TEXT   = 0xFFFF;
    static constexpr uint16_t COL_MUTED  = 0x8410;
    static constexpr uint16_t COL_CYAN   = 0x07FF;
    static constexpr uint16_t COL_LIME   = 0xBEE7;
    static constexpr uint16_t COL_ORANGE = 0xFD20;
    static constexpr uint16_t COL_RED    = 0xF800;

    /* ══════════════════════════════════════════════════════════════════
     *  Lua Host Bindings (meow.*)
     * ══════════════════════════════════════════════════════════════════ */
    static int l_clear(lua_State* L) {
        uint16_t col = (uint16_t)luaL_optinteger(L, 1, COL_BG);
        if (s_dev) s_dev->Lcd.fillScreen(col);
        return 0;
    }

    static int l_text(lua_State* L) {
        if (!s_dev) return 0;
        int x = (int)std::round(luaL_checknumber(L, 1));
        int y = (int)std::round(luaL_checknumber(L, 2));
        const char* str = luaL_checkstring(L, 3);
        uint16_t col = (uint16_t)luaL_optinteger(L, 4, COL_TEXT);
        s_dev->Lcd.setFont(&fonts::efontCN_16);
        s_dev->Lcd.setTextColor(col);
        s_dev->Lcd.setCursor(x, y);
        s_dev->Lcd.print(str);
        return 0;
    }

    static int l_rect(lua_State* L) {
        if (!s_dev) return 0;
        int x = (int)std::round(luaL_checknumber(L, 1));
        int y = (int)std::round(luaL_checknumber(L, 2));
        int w = (int)std::round(luaL_checknumber(L, 3));
        int h = (int)std::round(luaL_checknumber(L, 4));
        uint16_t col = (uint16_t)luaL_optinteger(L, 5, COL_CYAN);
        bool fill = lua_toboolean(L, 6);
        if (fill) s_dev->Lcd.fillRect(x, y, w, h, col);
        else      s_dev->Lcd.drawRect(x, y, w, h, col);
        return 0;
    }

    static int l_circle(lua_State* L) {
        if (!s_dev) return 0;
        int x = (int)std::round(luaL_checknumber(L, 1));
        int y = (int)std::round(luaL_checknumber(L, 2));
        int r = (int)std::round(luaL_checknumber(L, 3));
        uint16_t col = (uint16_t)luaL_optinteger(L, 4, COL_CYAN);
        bool fill = lua_toboolean(L, 5);
        if (fill) s_dev->Lcd.fillCircle(x, y, r, col);
        else      s_dev->Lcd.drawCircle(x, y, r, col);
        return 0;
    }

    static int l_btn(lua_State* L) {
        bool pressed = false;
        if (lua_isinteger(L, 1)) {
            int b = (int)lua_tointeger(L, 1);
            switch (b) {
                case 0: pressed = (digitalRead(HAL_PIN_BTN_A) == LOW); break;
                case 1: pressed = (digitalRead(HAL_PIN_BTN_B) == LOW); break;
                case 2: pressed = (digitalRead(HAL_PIN_JOY_UP) == LOW); break;
                case 3: pressed = (digitalRead(HAL_PIN_JOY_DOWN) == LOW); break;
                case 4: pressed = (digitalRead(HAL_PIN_JOY_LEFT) == LOW); break;
                case 5: pressed = (digitalRead(HAL_PIN_JOY_RIGHT) == LOW); break;
                default: break;
            }
        } else if (lua_isstring(L, 1)) {
            const char* b = lua_tostring(L, 1);
            if (strcasecmp(b, "A") == 0)          pressed = (digitalRead(HAL_PIN_BTN_A) == LOW);
            else if (strcasecmp(b, "B") == 0)     pressed = (digitalRead(HAL_PIN_BTN_B) == LOW);
            else if (strcasecmp(b, "UP") == 0)    pressed = (digitalRead(HAL_PIN_JOY_UP) == LOW);
            else if (strcasecmp(b, "DOWN") == 0)  pressed = (digitalRead(HAL_PIN_JOY_DOWN) == LOW);
            else if (strcasecmp(b, "LEFT") == 0)  pressed = (digitalRead(HAL_PIN_JOY_LEFT) == LOW);
            else if (strcasecmp(b, "RIGHT") == 0) pressed = (digitalRead(HAL_PIN_JOY_RIGHT) == LOW);
        }
        lua_pushboolean(L, pressed);
        return 1;
    }

    static int l_imu(lua_State* L) {
        float ax = 0, ay = 0, az = 0;
        if (s_dev) s_dev->imu.getAccel(&ax, &ay, &az);
        lua_pushnumber(L, ax);
        lua_pushnumber(L, ay);
        lua_pushnumber(L, az);
        return 3;
    }

    static int l_gyro(lua_State* L) {
        float gx = 0, gy = 0, gz = 0;
        if (s_dev) s_dev->imu.getGyro(&gx, &gy, &gz);
        lua_pushnumber(L, gx);
        lua_pushnumber(L, gy);
        lua_pushnumber(L, gz);
        return 3;
    }

    static int l_mag(lua_State* L) {
        float mx = 0, my = 0, mz = 0;
        if (s_dev) s_dev->imu.getMag(&mx, &my, &mz);
        lua_pushnumber(L, mx);
        lua_pushnumber(L, my);
        lua_pushnumber(L, mz);
        return 3;
    }

    static int l_temp(lua_State* L) {
        float t = 0;
        if (s_dev) s_dev->imu.getTemp(&t);
        lua_pushnumber(L, t);
        return 1;
    }

    static int l_bat(lua_State* L) {
        float v = s_dev ? s_dev->pmu.getBatVoltage() : 0.0f;
        float pct = s_dev ? s_dev->pmu.getBatLevel() : 0.0f;
        bool charging = s_dev ? s_dev->pmu.isCharging() : false;
        bool vbus = s_dev ? s_dev->pmu.isVBUSExist() : false;
        lua_pushnumber(L, v);
        lua_pushnumber(L, pct);
        lua_pushboolean(L, charging);
        lua_pushboolean(L, vbus);
        return 4;
    }

    static int l_tone(lua_State* L) {
        if (!s_dev) return 0;
        int freq = (int)luaL_checkinteger(L, 1);
        int dur  = (int)luaL_checkinteger(L, 2);
        if (!s_dev->speaker.isEnabled()) s_dev->speaker.begin();
        s_dev->speaker.tone(freq, dur);
        return 0;
    }

    static int l_millis(lua_State* L) {
        lua_pushinteger(L, (lua_Integer)millis());
        return 1;
    }

    static int l_delay(lua_State* L) {
        int ms = (int)luaL_checkinteger(L, 1);
        delay(ms);
        return 0;
    }

    /* ── LED (WS2812B RGB) ── */
    static int l_led(lua_State* L) {
        if (!s_dev) return 0;
        int r = (int)luaL_optinteger(L, 1, 0);
        int g = (int)luaL_optinteger(L, 2, 0);
        int b = (int)luaL_optinteger(L, 3, 0);
        if (r <= 0 && g <= 0 && b <= 0) s_dev->led.off();
        else s_dev->led.setColor((uint8_t)constrain(r, 0, 255), (uint8_t)constrain(g, 0, 255), (uint8_t)constrain(b, 0, 255));
        return 0;
    }

    /* ── Capacitive Touch Panel ── */
    static int l_touch(lua_State* L) {
        if (!s_dev) { lua_pushboolean(L, false); lua_pushinteger(L, -1); lua_pushinteger(L, -1); return 3; }
        bool touched = s_dev->ctp.isTouched();
        int x = -1, y = -1;
        if (touched) s_dev->ctp.getPos(x, y);
        lua_pushboolean(L, touched);
        lua_pushinteger(L, x);
        lua_pushinteger(L, y);
        return 3;
    }

    /* ── WiFi & Network ── */
    static int l_wifi_connect(lua_State* L) {
        const char* ssid = luaL_checkstring(L, 1);
        const char* pass = luaL_optstring(L, 2, "");
        if (!s_dev) { lua_pushboolean(L, false); return 1; }
        s_dev->wifi.begin();
        bool ok = s_dev->wifi.connect(ssid, pass, 8000);
        lua_pushboolean(L, ok);
        return 1;
    }

    static int l_wifi_status(lua_State* L) {
        bool connected = (WiFi.status() == WL_CONNECTED);
        lua_newtable(L);
        lua_pushboolean(L, connected);
        lua_setfield(L, -2, "connected");
        lua_pushstring(L, connected ? WiFi.localIP().toString().c_str() : "0.0.0.0");
        lua_setfield(L, -2, "ip");
        lua_pushinteger(L, connected ? WiFi.RSSI() : 0);
        lua_setfield(L, -2, "rssi");
        lua_pushstring(L, WiFi.SSID().c_str());
        lua_setfield(L, -2, "ssid");
        return 1;
    }

    static int l_wifi_scan(lua_State* L) {
        if (digitalRead(HAL_PIN_BTN_B) == LOW) {
            lua_newtable(L);
            return 1;
        }
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        delay(20);
        int n = WiFi.scanNetworks(false, true);
        lua_newtable(L);
        if (n > 0) {
            for (int i = 0; i < n; i++) {
                lua_newtable(L);
                lua_pushstring(L, WiFi.SSID(i).c_str());
                lua_setfield(L, -2, "ssid");
                lua_pushstring(L, WiFi.BSSIDstr(i).c_str());
                lua_setfield(L, -2, "bssid");
                lua_pushinteger(L, WiFi.RSSI(i));
                lua_setfield(L, -2, "rssi");
                lua_pushinteger(L, WiFi.channel(i));
                lua_setfield(L, -2, "channel");
                lua_pushboolean(L, WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
                lua_setfield(L, -2, "encrypted");
                lua_pushboolean(L, WiFi.SSID(i).length() == 0);
                lua_setfield(L, -2, "hidden");
                lua_rawseti(L, -2, i + 1);
            }
        }
        WiFi.scanDelete();
        return 1;
    }

    /* ── Tactical Wi-Fi Decloaker (Promiscuous 802.11 Sniffer + Directed Prober) ── */
    static char s_decloaked_ssid[33] = "";
    static uint8_t s_target_bssid[6] = {0};
    static volatile bool s_decloak_found = false;

    static void IRAM_ATTR s_promisc_decloak_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
        if (type != WIFI_PKT_MGMT || s_decloak_found) return;
        const wifi_promiscuous_pkt_t* pkt = (const wifi_promiscuous_pkt_t*)buf;
        const uint8_t* payload = pkt->payload;
        uint16_t len = pkt->rx_ctrl.sig_len;
        if (len < 24) return;

        uint8_t frame_ctrl = payload[0];
        uint8_t subtype = (frame_ctrl >> 4) & 0x0F;
        // Subtype 5 = Probe Response, 0 = Assoc Req, 2 = Reassoc Req
        if (subtype != 5 && subtype != 0 && subtype != 2) return;

        const uint8_t* bssid = &payload[16];
        if (memcmp(bssid, s_target_bssid, 6) != 0) return;

        size_t tag_offset = (subtype == 5) ? 36 : ((subtype == 0) ? 28 : 34);
        while (tag_offset + 2 <= len) {
            uint8_t tag_id = payload[tag_offset];
            uint8_t tag_len = payload[tag_offset + 1];
            if (tag_id == 0) { // SSID tag
                if (tag_len > 0 && tag_len <= 32) {
                    memcpy(s_decloaked_ssid, &payload[tag_offset + 2], tag_len);
                    s_decloaked_ssid[tag_len] = '\0';
                    s_decloak_found = true;
                }
                break;
            }
            tag_offset += 2 + tag_len;
        }
    }

    static int l_wifi_decloak(lua_State* L) {
        const char* bssid_str = luaL_checkstring(L, 1);
        int ch = (int)luaL_optinteger(L, 2, 1);
        int timeout_ms = (int)luaL_optinteger(L, 3, 1200);

        unsigned int b[6] = {0};
        if (sscanf(bssid_str, "%x:%x:%x:%x:%x:%x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) != 6) {
            lua_pushnil(L);
            lua_pushstring(L, "Invalid BSSID");
            return 2;
        }
        for (int i = 0; i < 6; i++) s_target_bssid[i] = (uint8_t)b[i];

        memset(s_decloaked_ssid, 0, sizeof(s_decloaked_ssid));
        s_decloak_found = false;

        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
        esp_wifi_set_promiscuous_rx_cb(&s_promisc_decloak_cb);
        esp_wifi_set_promiscuous(true);

        uint32_t start = millis();
        while ((millis() - start < (uint32_t)timeout_ms) && !s_decloak_found) {
            if (digitalRead(HAL_PIN_BTN_B) == LOW) break;
            delay(10);
        }

        esp_wifi_set_promiscuous(false);

        if (s_decloak_found && strlen(s_decloaked_ssid) > 0) {
            lua_pushstring(L, s_decloaked_ssid);
            return 1;
        }

        lua_pushnil(L);
        return 1;
    }

    static int l_http_get(lua_State* L) {
        const char* url = luaL_checkstring(L, 1);
        int timeout_ms = (int)luaL_optinteger(L, 2, 4000);
        HTTPClient http;
        http.setTimeout(timeout_ms);
        if (!http.begin(url)) {
            lua_pushnil(L);
            lua_pushstring(L, "Invalid URL");
            return 2;
        }
        int code = http.GET();
        if (code > 0) {
            String payload = http.getString();
            lua_pushinteger(L, code);
            lua_pushstring(L, payload.c_str());
            http.end();
            return 2;
        }
        http.end();
        lua_pushnil(L);
        lua_pushstring(L, "Connection Failed");
        return 2;
    }

    /* ── UDP for P2P / Walkie ── */
    static WiFiUDP s_udp;
    static bool s_udp_active = false;

    static int l_udp_bind(lua_State* L) {
        int port = (int)luaL_checkinteger(L, 1);
        if (s_udp_active) s_udp.stop();
        s_udp_active = (s_udp.begin((uint16_t)port) == 1);
        lua_pushboolean(L, s_udp_active);
        return 1;
    }

    static int l_udp_send(lua_State* L) {
        const char* ip = luaL_checkstring(L, 1);
        int port = (int)luaL_checkinteger(L, 2);
        const char* data = luaL_checkstring(L, 3);
        s_udp.beginPacket(ip, (uint16_t)port);
        s_udp.write((const uint8_t*)data, strlen(data));
        bool ok = (s_udp.endPacket() == 1);
        lua_pushboolean(L, ok);
        return 1;
    }

    static int l_udp_recv(lua_State* L) {
        int sz = s_udp.parsePacket();
        if (sz > 0) {
            std::vector<char> buf(sz + 1);
            int len = s_udp.read(buf.data(), sz);
            buf[len] = '\0';
            lua_pushstring(L, buf.data());
            lua_pushstring(L, s_udp.remoteIP().toString().c_str());
            lua_pushinteger(L, s_udp.remotePort());
            return 3;
        }
        return 0;
    }

    /* ── Hardware I2C Scanner ── */
    static int l_i2c_scan(lua_State* L) {
        bool res[128] = {false};
        In_I2C.scanID(res, 100000);
        lua_newtable(L);
        int count = 1;
        for (int i = 8; i < 120; i++) {
            if (res[i]) {
                char hex[10];
                snprintf(hex, sizeof(hex), "0x%02X", i);
                lua_pushstring(L, hex);
                lua_rawseti(L, -2, count++);
            }
        }
        return 1;
    }

    /* ── GPIO & ADC ── */
    static int l_pin_mode(lua_State* L) {
        int pin = (int)luaL_checkinteger(L, 1);
        const char* mode = luaL_optstring(L, 2, "in");
        if (strcasecmp(mode, "out") == 0) pinMode(pin, OUTPUT);
        else if (strcasecmp(mode, "pullup") == 0) pinMode(pin, INPUT_PULLUP);
        else pinMode(pin, INPUT);
        return 0;
    }

    static int l_pin_read(lua_State* L) {
        int pin = (int)luaL_checkinteger(L, 1);
        lua_pushinteger(L, digitalRead(pin));
        return 1;
    }

    static int l_pin_write(lua_State* L) {
        int pin = (int)luaL_checkinteger(L, 1);
        int val = (int)luaL_checkinteger(L, 2);
        digitalWrite(pin, val ? HIGH : LOW);
        return 0;
    }

    static int l_adc_read(lua_State* L) {
        int pin = (int)luaL_checkinteger(L, 1);
        lua_pushinteger(L, analogRead(pin));
        return 1;
    }

    /* ── File I/O ── */
    static int l_read_file(lua_State* L) {
        const char* path = luaL_checkstring(L, 1);
        File f = SD_MMC.open(path, FILE_READ);
        if (!f) return 0;
        size_t sz = f.size();
        std::vector<char> buf(sz + 1);
        f.read((uint8_t*)buf.data(), sz);
        buf[sz] = '\0';
        f.close();
        lua_pushstring(L, buf.data());
        return 1;
    }

    static int l_write_file(lua_State* L) {
        const char* path = luaL_checkstring(L, 1);
        const char* data = luaL_checkstring(L, 2);
        File f = SD_MMC.open(path, FILE_WRITE);
        if (!f) { lua_pushboolean(L, false); return 1; }
        f.write((const uint8_t*)data, strlen(data));
        f.close();
        lua_pushboolean(L, true);
        return 1;
    }

    /* ── BLE AirTag / FindMy Beacon ── */
    static bool s_ble_airtag_active = false;
    static esp_ble_adv_params_t s_airtag_adv_params = {
        .adv_int_min        = 0x0100,
        .adv_int_max        = 0x0100,
        .adv_type           = ADV_TYPE_NONCONN_IND,
        .own_addr_type      = BLE_ADDR_TYPE_RANDOM,
        .peer_addr          = {0,0,0,0,0,0},
        .peer_addr_type     = BLE_ADDR_TYPE_PUBLIC,
        .channel_map        = ADV_CHNL_ALL,
        .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
    };

    static void _airtag_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param) {
        (void)event; (void)param;
    }

    static void stop_ble_airtag() {
        if (s_ble_airtag_active) {
            esp_ble_gap_stop_advertising();
            s_ble_airtag_active = false;
        }
    }

    /* ── High-Efficiency Memory & Driver Management ── */
    static void* lua_psram_alloc(void* ud, void* ptr, size_t osize, size_t nsize) {
        (void)ud; (void)osize;
        if (nsize == 0) {
            free(ptr);
            return nullptr;
        }
        if (psramFound()) {
            void* p = heap_caps_realloc(ptr, nsize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (p) return p;
        }
        return realloc(ptr, nsize);
    }

    static void release_hardware_drivers() {
        if (s_udp_active) {
            s_udp.stop();
            s_udp_active = false;
        }
        stop_ble_airtag();
        if (BLEDevice::getInitialized()) {
            BLEDevice::deinit(true);
        }
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        esp_wifi_stop();
        if (s_dev) s_dev->led.off();
    }

    static int l_mem(lua_State* L) {
        uint32_t int_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        uint32_t int_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        uint32_t ps_free = psramFound() ? heap_caps_get_free_size(MALLOC_CAP_SPIRAM) : 0;
        lua_pushinteger(L, (lua_Integer)int_free);
        lua_pushinteger(L, (lua_Integer)int_largest);
        lua_pushinteger(L, (lua_Integer)ps_free);
        return 3;
    }

    static int l_gc(lua_State* L) {
        lua_gc(L, LUA_GCCOLLECT, 0);
        return 0;
    }

    static void start_ble_airtag() {
        if (!s_ble_airtag_active) {
            if (!BLEDevice::getInitialized()) {
                BLEDevice::init("MeowKit");
            }
            esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P21);
            esp_ble_gap_register_callback(_airtag_gap_cb);
            esp_ble_gap_start_advertising(&s_airtag_adv_params);
            s_ble_airtag_active = true;
        }

        uint8_t pkt[32];
        uint8_t i = 0;
        pkt[i++] = 0x1E;
        pkt[i++] = 0xFF;
        pkt[i++] = 0x4C;
        pkt[i++] = 0x00;
        pkt[i++] = 0x12;
        pkt[i++] = 0x19;
        pkt[i++] = 0x10;
        for (int k = 0; k < 22; k++) {
            pkt[i++] = (uint8_t)(esp_random() & 0xFF);
        }
        pkt[i++] = 0x00;
        pkt[i++] = 0xEC;

        esp_bd_addr_t rand_addr;
        for (int k = 0; k < 6; k++) rand_addr[k] = (uint8_t)(esp_random() & 0xFF);
        rand_addr[0] |= 0xC0;
        esp_ble_gap_set_rand_addr(rand_addr);
        esp_ble_gap_config_adv_data_raw(pkt, i);
    }

    static int l_ble_airtag(lua_State* L) {
        bool enable = lua_toboolean(L, 1);
        if (enable) {
            start_ble_airtag();
            lua_pushboolean(L, true);
        } else {
            stop_ble_airtag();
            lua_pushboolean(L, false);
        }
        return 1;
    }

    /* ── Audio Recorder / Wiretap ── */
    static int l_record_wav(lua_State* L) {
        const char* path = luaL_checkstring(L, 1);
        int seconds = (int)luaL_optinteger(L, 2, 5);
        if (seconds <= 0) seconds = 1;
        if (seconds > 120) seconds = 120;

        ES7210_Class es7210(ES7210_I2C_ADDR, &In_I2C);
        bool codec_ok = es7210.begin(16000, ES7210_BIT_16, ES7210_FMT_I2S, ES7210_SIGNAL_I2S);
        if (!codec_ok) {
            lua_pushboolean(L, false);
            lua_pushstring(L, "ES7210 init failed");
            return 2;
        }
        es7210.selectMic(ES7210_MIC1 | ES7210_MIC2);
        es7210.setGain(ES7210_GAIN_30DB);
        es7210.start();

        i2s_config_t i2s_cfg = {};
        i2s_cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
        i2s_cfg.sample_rate = 16000;
        i2s_cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
        i2s_cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
        i2s_cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
        i2s_cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
        i2s_cfg.dma_buf_count = 4;
        i2s_cfg.dma_buf_len = 512;
        i2s_cfg.use_apll = false;

        esp_err_t err = i2s_driver_install(I2S_NUM_1, &i2s_cfg, 0, nullptr);
        if (err != ESP_OK) {
            es7210.stop();
            es7210.end();
            lua_pushboolean(L, false);
            lua_pushstring(L, "I2S install failed");
            return 2;
        }

        i2s_pin_config_t pins = {};
        pins.mck_io_num   = HAL_PIN_I2S_MCLK;
        pins.bck_io_num   = HAL_PIN_I2S_BCLK;
        pins.ws_io_num    = HAL_PIN_I2S_WS;
        pins.data_out_num = I2S_PIN_NO_CHANGE;
        pins.data_in_num  = HAL_PIN_I2S_DIN;
        i2s_set_pin(I2S_NUM_1, &pins);
        i2s_zero_dma_buffer(I2S_NUM_1);
        i2s_start(I2S_NUM_1);

        const char* lastSlash = strrchr(path, '/');
        if (lastSlash && lastSlash != path) {
            std::string parentDir(path, lastSlash - path);
            if (!SD_MMC.exists(parentDir.c_str())) {
                SD_MMC.mkdir(parentDir.c_str());
            }
        }

        File f = SD_MMC.open(path, FILE_WRITE);
        if (!f) {
            i2s_stop(I2S_NUM_1);
            i2s_driver_uninstall(I2S_NUM_1);
            es7210.stop();
            es7210.end();
            lua_pushboolean(L, false);
            lua_pushstring(L, "Failed to create SD file");
            return 2;
        }

        uint32_t sampleRate = 16000;
        uint16_t numChannels = 1;
        uint16_t bitsPerSample = 16;
        uint32_t byteRate = sampleRate * numChannels * (bitsPerSample / 8);
        uint16_t blockAlign = numChannels * (bitsPerSample / 8);
        uint32_t totalSamples = sampleRate * seconds;
        uint32_t subChunk2Size = totalSamples * (bitsPerSample / 8);
        uint32_t chunkSize = 36 + subChunk2Size;

        uint8_t wav_hdr[44];
        memcpy(&wav_hdr[0], "RIFF", 4);
        memcpy(&wav_hdr[4], &chunkSize, 4);
        memcpy(&wav_hdr[8], "WAVE", 4);
        memcpy(&wav_hdr[12], "fmt ", 4);
        uint32_t subChunk1Size = 16;
        memcpy(&wav_hdr[16], &subChunk1Size, 4);
        uint16_t audioFormat = 1;
        memcpy(&wav_hdr[20], &audioFormat, 2);
        memcpy(&wav_hdr[22], &numChannels, 2);
        memcpy(&wav_hdr[24], &sampleRate, 4);
        memcpy(&wav_hdr[28], &byteRate, 4);
        memcpy(&wav_hdr[32], &blockAlign, 2);
        memcpy(&wav_hdr[34], &bitsPerSample, 2);
        memcpy(&wav_hdr[36], "data", 4);
        memcpy(&wav_hdr[40], &subChunk2Size, 4);

        f.write(wav_hdr, 44);

        constexpr size_t kChunk = 512;
        int16_t stereo_buf[kChunk * 2];
        int16_t mono_buf[kChunk];

        uint32_t samplesWritten = 0;
        while (samplesWritten < totalSamples) {
            if (digitalRead(HAL_PIN_BTN_B) == LOW) {
                break;
            }

            size_t toRead = std::min((uint32_t)kChunk, totalSamples - samplesWritten);
            size_t bytesRead = 0;
            i2s_read(I2S_NUM_1, stereo_buf, toRead * 2 * sizeof(int16_t), &bytesRead, pdMS_TO_TICKS(100));

            size_t samples = bytesRead / (2 * sizeof(int16_t));
            for (size_t k = 0; k < samples; k++) {
                mono_buf[k] = stereo_buf[k * 2];
            }
            if (samples > 0) {
                f.write((const uint8_t*)mono_buf, samples * sizeof(int16_t));
                samplesWritten += samples;
            }
        }

        if (samplesWritten != totalSamples) {
            subChunk2Size = samplesWritten * (bitsPerSample / 8);
            chunkSize = 36 + subChunk2Size;
            f.seek(4);
            f.write((const uint8_t*)&chunkSize, 4);
            f.seek(40);
            f.write((const uint8_t*)&subChunk2Size, 4);
        }

        f.flush();
        f.close();

        i2s_stop(I2S_NUM_1);
        i2s_driver_uninstall(I2S_NUM_1);
        es7210.stop();
        es7210.end();

        lua_pushboolean(L, true);
        lua_pushinteger(L, (lua_Integer)samplesWritten);
        return 2;
    }

    static int l_mic_level(lua_State* L) {
        ES7210_Class es7210(ES7210_I2C_ADDR, &In_I2C);
        if (!es7210.begin(16000, ES7210_BIT_16, ES7210_FMT_I2S, ES7210_SIGNAL_I2S)) {
            lua_pushinteger(L, 0);
            return 1;
        }
        es7210.selectMic(ES7210_MIC1 | ES7210_MIC2);
        es7210.setGain(ES7210_GAIN_30DB);
        es7210.start();

        i2s_config_t i2s_cfg = {};
        i2s_cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
        i2s_cfg.sample_rate = 16000;
        i2s_cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
        i2s_cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
        i2s_cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
        i2s_cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
        i2s_cfg.dma_buf_count = 2;
        i2s_cfg.dma_buf_len = 256;
        i2s_cfg.use_apll = false;

        int level = 0;
        if (i2s_driver_install(I2S_NUM_1, &i2s_cfg, 0, nullptr) == ESP_OK) {
            i2s_pin_config_t pins = {};
            pins.mck_io_num   = HAL_PIN_I2S_MCLK;
            pins.bck_io_num   = HAL_PIN_I2S_BCLK;
            pins.ws_io_num    = HAL_PIN_I2S_WS;
            pins.data_out_num = I2S_PIN_NO_CHANGE;
            pins.data_in_num  = HAL_PIN_I2S_DIN;
            i2s_set_pin(I2S_NUM_1, &pins);
            i2s_zero_dma_buffer(I2S_NUM_1);
            i2s_start(I2S_NUM_1);

            int16_t sbuf[256];
            size_t bread = 0;
            i2s_read(I2S_NUM_1, sbuf, sizeof(sbuf), &bread, pdMS_TO_TICKS(50));

            int32_t max_val = 0;
            size_t samples = bread / sizeof(int16_t);
            for (size_t i = 0; i < samples; i++) {
                int32_t v = std::abs((int32_t)sbuf[i]);
                if (v > max_val) max_val = v;
            }

            i2s_stop(I2S_NUM_1);
            i2s_driver_uninstall(I2S_NUM_1);
            level = (max_val * 100) / 32767;
        }

        es7210.stop();
        es7210.end();
        lua_pushinteger(L, constrain(level, 0, 100));
        return 1;
    }

    static void register_lua_bindings(lua_State* L) {
        lua_newtable(L);
        lua_pushcfunction(L, l_clear);        lua_setfield(L, -2, "clear");
        lua_pushcfunction(L, l_text);         lua_setfield(L, -2, "text");
        lua_pushcfunction(L, l_rect);         lua_setfield(L, -2, "rect");
        lua_pushcfunction(L, l_circle);       lua_setfield(L, -2, "circle");
        lua_pushcfunction(L, l_btn);          lua_setfield(L, -2, "btn");
        lua_pushcfunction(L, l_imu);          lua_setfield(L, -2, "imu");
        lua_pushcfunction(L, l_gyro);         lua_setfield(L, -2, "gyro");
        lua_pushcfunction(L, l_mag);          lua_setfield(L, -2, "mag");
        lua_pushcfunction(L, l_temp);         lua_setfield(L, -2, "temp");
        lua_pushcfunction(L, l_bat);          lua_setfield(L, -2, "bat");
        lua_pushcfunction(L, l_tone);         lua_setfield(L, -2, "tone");
        lua_pushcfunction(L, l_millis);       lua_setfield(L, -2, "millis");
        lua_pushcfunction(L, l_delay);        lua_setfield(L, -2, "delay");
        lua_pushcfunction(L, l_led);          lua_setfield(L, -2, "led");
        lua_pushcfunction(L, l_touch);        lua_setfield(L, -2, "touch");
        lua_pushcfunction(L, l_wifi_connect); lua_setfield(L, -2, "wifi_connect");
        lua_pushcfunction(L, l_wifi_status);  lua_setfield(L, -2, "wifi_status");
        lua_pushcfunction(L, l_wifi_scan);    lua_setfield(L, -2, "wifi_scan");
        lua_pushcfunction(L, l_wifi_decloak); lua_setfield(L, -2, "wifi_decloak");
        lua_pushcfunction(L, l_http_get);     lua_setfield(L, -2, "http_get");
        lua_pushcfunction(L, l_udp_bind);     lua_setfield(L, -2, "udp_bind");
        lua_pushcfunction(L, l_udp_send);     lua_setfield(L, -2, "udp_send");
        lua_pushcfunction(L, l_udp_recv);     lua_setfield(L, -2, "udp_recv");
        lua_pushcfunction(L, l_i2c_scan);     lua_setfield(L, -2, "i2c_scan");
        lua_pushcfunction(L, l_pin_mode);     lua_setfield(L, -2, "pin_mode");
        lua_pushcfunction(L, l_pin_read);     lua_setfield(L, -2, "pin_read");
        lua_pushcfunction(L, l_pin_write);    lua_setfield(L, -2, "pin_write");
        lua_pushcfunction(L, l_adc_read);     lua_setfield(L, -2, "adc_read");
        lua_pushcfunction(L, l_read_file);    lua_setfield(L, -2, "read_file");
        lua_pushcfunction(L, l_write_file);   lua_setfield(L, -2, "write_file");
        lua_pushcfunction(L, l_ble_airtag);   lua_setfield(L, -2, "ble_airtag");
        lua_pushcfunction(L, l_record_wav);   lua_setfield(L, -2, "record_wav");
        lua_pushcfunction(L, l_mic_level);    lua_setfield(L, -2, "mic_level");
        lua_pushcfunction(L, l_mem);          lua_setfield(L, -2, "mem");
        lua_pushcfunction(L, l_gc);           lua_setfield(L, -2, "gc");
        lua_setglobal(L, "meow");
    }

    /* ══════════════════════════════════════════════════════════════════
     *  Wasm3 Host Bindings (env.*)
     * ══════════════════════════════════════════════════════════════════ */
    m3ApiRawFunction(m3_meow_clear) {
        m3ApiGetArg(uint32_t, col);
        if (s_dev) s_dev->Lcd.fillScreen((uint16_t)col);
        m3ApiSuccess();
    }

    m3ApiRawFunction(m3_meow_btn_a) {
        m3ApiReturnType(uint32_t);
        bool pressed = s_dev && (s_dev->button.A.pressed() || s_dev->button.A.state() == Button_Class::PRESSED);
        m3ApiReturn(pressed ? 1 : 0);
    }

    m3ApiRawFunction(m3_meow_btn_b) {
        m3ApiReturnType(uint32_t);
        bool pressed = s_dev && (s_dev->button.B.pressed() || s_dev->button.B.state() == Button_Class::PRESSED);
        m3ApiReturn(pressed ? 1 : 0);
    }

    m3ApiRawFunction(m3_meow_tone) {
        m3ApiGetArg(uint32_t, freq);
        m3ApiGetArg(uint32_t, dur);
        if (s_dev) {
            if (!s_dev->speaker.isEnabled()) s_dev->speaker.begin();
            s_dev->speaker.tone(freq, dur);
        }
        m3ApiSuccess();
    }

    m3ApiRawFunction(m3_meow_millis) {
        m3ApiReturnType(uint32_t);
        m3ApiReturn((uint32_t)millis());
    }

    m3ApiRawFunction(m3_meow_delay) {
        m3ApiGetArg(uint32_t, ms);
        delay(ms);
        m3ApiSuccess();
    }

    m3ApiRawFunction(m3_meow_pixel) {
        m3ApiGetArg(int32_t, x);
        m3ApiGetArg(int32_t, y);
        m3ApiGetArg(uint32_t, col);
        if (s_dev) s_dev->Lcd.drawPixel(x, y, (uint16_t)col);
        m3ApiSuccess();
    }

    m3ApiRawFunction(m3_meow_rect) {
        m3ApiGetArg(int32_t, x);
        m3ApiGetArg(int32_t, y);
        m3ApiGetArg(int32_t, w);
        m3ApiGetArg(int32_t, h);
        m3ApiGetArg(uint32_t, col);
        m3ApiGetArg(uint32_t, fill);
        if (s_dev) {
            if (fill) s_dev->Lcd.fillRect(x, y, w, h, (uint16_t)col);
            else      s_dev->Lcd.drawRect(x, y, w, h, (uint16_t)col);
        }
        m3ApiSuccess();
    }

    m3ApiRawFunction(m3_meow_circle) {
        m3ApiGetArg(int32_t, x);
        m3ApiGetArg(int32_t, y);
        m3ApiGetArg(int32_t, r);
        m3ApiGetArg(uint32_t, col);
        m3ApiGetArg(uint32_t, fill);
        if (s_dev) {
            if (fill) s_dev->Lcd.fillCircle(x, y, r, (uint16_t)col);
            else      s_dev->Lcd.drawCircle(x, y, r, (uint16_t)col);
        }
        m3ApiSuccess();
    }

    m3ApiRawFunction(m3_meow_btn) {
        m3ApiReturnType(uint32_t);
        m3ApiGetArg(uint32_t, btn_id);
        bool pressed = false;
        switch (btn_id) {
            case 0: pressed = (digitalRead(HAL_PIN_BTN_A) == LOW); break;
            case 1: pressed = (digitalRead(HAL_PIN_BTN_B) == LOW); break;
            case 2: pressed = (digitalRead(HAL_PIN_JOY_UP) == LOW); break;
            case 3: pressed = (digitalRead(HAL_PIN_JOY_DOWN) == LOW); break;
            case 4: pressed = (digitalRead(HAL_PIN_JOY_LEFT) == LOW); break;
            case 5: pressed = (digitalRead(HAL_PIN_JOY_RIGHT) == LOW); break;
            default: break;
        }
        m3ApiReturn(pressed ? 1 : 0);
    }

    m3ApiRawFunction(m3_meow_text) {
        m3ApiGetArg(int32_t, x);
        m3ApiGetArg(int32_t, y);
        m3ApiGetArgMem(const char*, str);
        m3ApiGetArg(uint32_t, col);
        if (s_dev && str) {
            s_dev->Lcd.setFont(&fonts::efontCN_16);
            s_dev->Lcd.setTextColor((uint16_t)col);
            s_dev->Lcd.setCursor(x, y);
            s_dev->Lcd.print(str);
        }
        m3ApiSuccess();
    }

    static bool link_wasm3_bindings(IM3Module module) {
        m3_LinkRawFunction(module, "env", "meow_clear",  "v(i)",      &m3_meow_clear);
        m3_LinkRawFunction(module, "env", "meow_text",   "v(iiii)",   &m3_meow_text);
        m3_LinkRawFunction(module, "env", "meow_pixel",  "v(iii)",    &m3_meow_pixel);
        m3_LinkRawFunction(module, "env", "meow_rect",   "v(iiiiii)", &m3_meow_rect);
        m3_LinkRawFunction(module, "env", "meow_circle", "v(iiiii)",  &m3_meow_circle);
        m3_LinkRawFunction(module, "env", "meow_btn",    "i(i)",      &m3_meow_btn);
        m3_LinkRawFunction(module, "env", "meow_btn_a",  "i()",       &m3_meow_btn_a);
        m3_LinkRawFunction(module, "env", "meow_btn_b",  "i()",       &m3_meow_btn_b);
        m3_LinkRawFunction(module, "env", "meow_tone",   "v(ii)",     &m3_meow_tone);
        m3_LinkRawFunction(module, "env", "meow_millis", "i()",       &m3_meow_millis);
        m3_LinkRawFunction(module, "env", "meow_delay",  "v(i)",      &m3_meow_delay);
        return true;
    }

    /* ══════════════════════════════════════════════════════════════════
     *  AppLoader Implementation
     * ══════════════════════════════════════════════════════════════════ */
    AppLoader::AppLoader(DEVICES* device) : _device(device)
    {
        setAppInfo().name = "SD Apps";
        s_dev = device;
    }

    void AppLoader::onOpen()
    {
        s_dev = _device;
        _isRunningApp = false;
        _selectedIdx = 0;
        _scrollOffset = 0;
        _appList.clear();

        _scanSdApps();
        _drawBrowser();
    }

    void AppLoader::onClose()
    {
        _isRunningApp = false;
        release_hardware_drivers();
    }

    void AppLoader::onRunning()
    {
        if (!_isRunningApp) {
            _handleBrowserInput();
        }
    }

    void AppLoader::_scanSdApps()
    {
        _appList.clear();
        File dir = SD_MMC.open("/apps");
        if (!dir || !dir.isDirectory()) {
            if (dir) dir.close();
            return;
        }

        File file = dir.openNextFile();
        while (file) {
            std::string name = file.name();
            // Basename extraction
            size_t slash = name.find_last_of('/');
            std::string base = (slash != std::string::npos) ? name.substr(slash + 1) : name;

            if (!file.isDirectory()) {
                if (base.length() > 4 && base.substr(base.length() - 4) == ".lua") {
                    _appList.push_back({base, file.path(), AppFileType::Lua, file.size()});
                }
                else if (base.length() > 5 && base.substr(base.length() - 5) == ".wasm") {
                    _appList.push_back({base, file.path(), AppFileType::Wasm, file.size()});
                }
            } else {
                // Check if directory contains main.lua or main.wasm
                std::string luaPath = std::string(file.path()) + "/main.lua";
                std::string wasmPath = std::string(file.path()) + "/main.wasm";
                if (SD_MMC.exists(luaPath.c_str())) {
                    _appList.push_back({base + " [pkg]", luaPath, AppFileType::Lua, 0});
                } else if (SD_MMC.exists(wasmPath.c_str())) {
                    _appList.push_back({base + " [pkg]", wasmPath, AppFileType::Wasm, 0});
                }
            }

            File next = dir.openNextFile();
            file.close();
            file = next;
        }
        dir.close();
    }

    void AppLoader::_drawBrowser()
    {
        auto& Lcd = _device->Lcd;
        Lcd.fillScreen(COL_BG);

        // Header
        Lcd.fillRect(0, 0, 320, 26, COL_PANEL);
        Lcd.drawFastHLine(0, 26, 320, COL_BORDER);
        Lcd.setFont(&fonts::efontCN_16);
        Lcd.setTextColor(COL_LIME, COL_PANEL);
        Lcd.setCursor(10, 4);
        Lcd.printf("DYNAMIC SD APPS (/apps/) [%d Found]", (int)_appList.size());

        // Footer with dynamic Memory Health Monitor HUD
        Lcd.fillRect(0, 216, 320, 24, COL_PANEL);
        Lcd.drawFastHLine(0, 215, 320, COL_BORDER);
        Lcd.setFont(&fonts::Font0);
        Lcd.setTextColor(COL_TEXT, COL_PANEL);
        Lcd.setCursor(6, 223);
        uint32_t freeSram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) / 1024;
        uint32_t maxBlock = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) / 1024;
        uint32_t freePsram = (psramFound() ? heap_caps_get_free_size(MALLOC_CAP_SPIRAM) : 0) / 1024;
        Lcd.printf("[^v]Sel [A]Run [B]Exit | SRAM:%uK(max %uK) PSRAM:%uK", freeSram, maxBlock, freePsram);

        if (_appList.empty()) {
            Lcd.setFont(&fonts::efontCN_16);
            Lcd.setTextColor(COL_ORANGE, COL_BG);
            Lcd.setCursor(40, 80);
            Lcd.print("No .lua or .wasm apps found!");
            Lcd.setTextColor(COL_MUTED, COL_BG);
            Lcd.setCursor(40, 110);
            Lcd.print("Drop apps into K:\\apps\\ on your SD");
            Lcd.setCursor(40, 135);
            Lcd.print("Supports: .lua scripts / .wasm bins");
            return;
        }

        const int startY = 34;
        const int rowH   = 34;
        const int visibleRows = 5;

        for (int i = 0; i < visibleRows && (i + _scrollOffset) < (int)_appList.size(); i++) {
            int idx = i + _scrollOffset;
            bool sel = (idx == _selectedIdx);
            int y = startY + (i * rowH);

            if (sel) {
                Lcd.fillRoundRect(8, y, 304, rowH - 4, 4, COL_LIME);
                Lcd.setTextColor(COL_BG, COL_LIME);
            } else {
                Lcd.fillRoundRect(8, y, 304, rowH - 4, 4, COL_PANEL);
                Lcd.drawRoundRect(8, y, 304, rowH - 4, 4, COL_BORDER);
                Lcd.setTextColor(COL_TEXT, COL_PANEL);
            }

            // Tag badge
            Lcd.setFont(&fonts::efontCN_14);
            const char* tag = (_appList[idx].type == AppFileType::Lua) ? "[LUA]" : "[WASM]";
            uint16_t tagCol = (_appList[idx].type == AppFileType::Lua) ? (sel ? COL_BG : COL_CYAN) : (sel ? COL_BG : COL_ORANGE);
            Lcd.setTextColor(tagCol);
            Lcd.setCursor(16, y + 6);
            Lcd.print(tag);

            // Filename
            Lcd.setTextColor(sel ? COL_BG : COL_TEXT);
            Lcd.setCursor(75, y + 6);
            Lcd.print(_appList[idx].filename.c_str());

            // Size
            if (_appList[idx].fileSize > 0) {
                Lcd.setFont(&fonts::Font0);
                Lcd.setCursor(260, y + 10);
                Lcd.printf("%u KB", (unsigned)(_appList[idx].fileSize / 1024));
            }
        }
    }

    void AppLoader::_handleBrowserInput()
    {
        if (_appList.empty()) return;

        bool changed = false;
        if (_device->button.Up.pressed()) {
            if (_selectedIdx > 0) {
                _selectedIdx--;
                if (_selectedIdx < _scrollOffset) _scrollOffset = _selectedIdx;
                changed = true;
            }
        }
        else if (_device->button.Down.pressed()) {
            if (_selectedIdx < (int)_appList.size() - 1) {
                _selectedIdx++;
                if (_selectedIdx >= _scrollOffset + 5) _scrollOffset = _selectedIdx - 4;
                changed = true;
            }
        }

        if (changed) {
            _drawBrowser();
            delay(100);
        }

        if (_device->button.A.pressed()) {
            const auto& item = _appList[_selectedIdx];
            _runningFile = item.filename;
            if (item.type == AppFileType::Lua) {
                _launchLua(item.path.c_str());
            } else if (item.type == AppFileType::Wasm) {
                _launchWasm(item.path.c_str());
            }
        }
    }

    /* ══════════════════════════════════════════════════════════════════
     *  Execution: Lua 5.4.7
     * ══════════════════════════════════════════════════════════════════ */
    bool AppLoader::_launchLua(const char* fullpath)
    {
        _isRunningApp = true;
        _device->Lcd.fillScreen(COL_BG);

        File f = SD_MMC.open(fullpath, FILE_READ);
        if (!f) {
            _drawError("Failed to open Lua file!");
            _isRunningApp = false;
            return false;
        }

        size_t size = f.size();
        char* buffer = (char*)(psramFound() ? ps_malloc(size + 1) : malloc(size + 1));
        if (!buffer) {
            f.close();
            _drawError("OOM: Script Buffer Fail");
            _isRunningApp = false;
            return false;
        }
        f.read((uint8_t*)buffer, size);
        buffer[size] = '\0';
        f.close();

        // Instantiate Lua VM strictly in 8MB Octal PSRAM
        lua_State* L = lua_newstate(lua_psram_alloc, nullptr);
        if (!L) {
            free(buffer);
            _drawError("Lua OOM - Out of memory!");
            _isRunningApp = false;
            return false;
        }

        luaL_openlibs(L);
        register_lua_bindings(L);

        // Load script into bytecode and immediately release raw text buffer
        int status = luaL_loadstring(L, buffer);
        free(buffer);
        if (status != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            _drawError(err ? err : "Lua Syntax Error");
            lua_close(L);
            release_hardware_drivers();
            _isRunningApp = false;
            return false;
        }

        // Run top-level script (initializes variables and defines on_loop)
        status = lua_pcall(L, 0, 0, 0);
        if (status != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            _drawError(err ? err : "Lua Exec Error");
            lua_close(L);
            release_hardware_drivers();
            _isRunningApp = false;
            return false;
        }

        // Event loop: calls on_loop() function in Lua repeatedly if defined
        bool hasLoop = false;
        lua_getglobal(L, "on_loop");
        if (lua_isfunction(L, -1)) hasLoop = true;
        lua_pop(L, 1);

        uint32_t b_held_start = 0;
        while (_isRunningApp) {
            if (_device) _device->button.update();

            // Direct hardware check on Button B (GPIO 4, active-low)
            if (digitalRead(HAL_PIN_BTN_B) == LOW) {
                if (b_held_start == 0) {
                    b_held_start = millis();
                } else if (millis() - b_held_start > 400) { // 400ms hold = Exit
                    if (_device) {
                        if (!_device->speaker.isEnabled()) _device->speaker.begin();
                        _device->speaker.tone(1200, 60);
                    }
                    delay(80);
                    break;
                }
            } else {
                b_held_start = 0;
            }

            if (hasLoop) {
                lua_getglobal(L, "on_loop");
                if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
                    const char* err = lua_tostring(L, -1);
                    _drawError(err ? err : "Runtime error in on_loop()");
                    break;
                }
            } else {
                // One-shot script completed
                delay(100);
            }
            delay(10);
        }

        // Double sweep GC to flush userdata and finalizers
        lua_gc(L, LUA_GCCOLLECT, 0);
        lua_gc(L, LUA_GCCOLLECT, 0);
        lua_close(L);
        release_hardware_drivers();
        _isRunningApp = false;
        _drawBrowser();
        return true;
    }

    /* ══════════════════════════════════════════════════════════════════
     *  Execution: WASM (Wasm3)
     * ══════════════════════════════════════════════════════════════════ */
    bool AppLoader::_launchWasm(const char* fullpath)
    {
        _isRunningApp = true;
        _device->Lcd.fillScreen(COL_BG);

        File f = SD_MMC.open(fullpath, FILE_READ);
        if (!f) {
            _drawError("Failed to open WASM binary!");
            _isRunningApp = false;
            return false;
        }

        size_t size = f.size();
        uint8_t* wasmBytes = nullptr;
        if (psramFound()) {
            wasmBytes = (uint8_t*)ps_malloc(size);
        }
        if (!wasmBytes) {
            wasmBytes = (uint8_t*)malloc(size);
        }
        if (!wasmBytes) {
            f.close();
            _drawError("Failed to allocate RAM for WASM!");
            _isRunningApp = false;
            return false;
        }
        f.read(wasmBytes, size);
        f.close();

        IM3Environment env = m3_NewEnvironment();
        if (!env) {
            free(wasmBytes);
            _drawError("Failed to create Wasm3 Env!");
            _isRunningApp = false;
            return false;
        }

        IM3Runtime runtime = m3_NewRuntime(env, 16 * 1024, NULL);
        if (!runtime) {
            m3_FreeEnvironment(env);
            free(wasmBytes);
            _drawError("Failed to create Wasm3 Runtime!");
            _isRunningApp = false;
            return false;
        }

        IM3Module module;
        M3Result result = m3_ParseModule(env, &module, wasmBytes, size);
        if (result) {
            _drawError(result);
            m3_FreeRuntime(runtime);
            m3_FreeEnvironment(env);
            free(wasmBytes);
            _isRunningApp = false;
            return false;
        }

        result = m3_LoadModule(runtime, module);
        if (result) {
            _drawError(result);
            m3_FreeRuntime(runtime);
            m3_FreeEnvironment(env);
            free(wasmBytes);
            _isRunningApp = false;
            return false;
        }

        link_wasm3_bindings(module);

        IM3Function fMain;
        result = m3_FindFunction(&fMain, runtime, "main");
        if (!result && fMain) {
            M3Result resMain = m3_CallV(fMain);
            if (resMain) {
                _drawError(resMain);
                m3_FreeRuntime(runtime);
                m3_FreeEnvironment(env);
                free(wasmBytes);
                _isRunningApp = false;
                _drawBrowser();
                return false;
            }
        }

        IM3Function fLoop;
        result = m3_FindFunction(&fLoop, runtime, "on_loop");
        if (!result && fLoop) {
            uint32_t b_held_start = 0;
            while (_isRunningApp) {
                if (_device) _device->button.update();
                if (digitalRead(HAL_PIN_BTN_B) == LOW) {
                    if (b_held_start == 0) b_held_start = millis();
                    else if (millis() - b_held_start > 400) {
                        if (_device) {
                            if (!_device->speaker.isEnabled()) _device->speaker.begin();
                            _device->speaker.tone(1200, 60);
                        }
                        delay(80);
                        break;
                    }
                } else {
                    b_held_start = 0;
                }

                M3Result resLoop = m3_CallV(fLoop);
                if (resLoop) {
                    _drawError(resLoop);
                    break;
                }
                delay(10);
            }
        }

        m3_FreeRuntime(runtime);
        m3_FreeEnvironment(env);
        free(wasmBytes);
        release_hardware_drivers();

        _isRunningApp = false;
        _drawBrowser();
        return true;
    }

    void AppLoader::_drawError(const char* msg)
    {
        auto& Lcd = _device->Lcd;
        Lcd.fillScreen(COL_BG);
        Lcd.fillRoundRect(20, 50, 280, 140, 6, COL_PANEL);
        Lcd.drawRoundRect(20, 50, 280, 140, 6, COL_RED);

        Lcd.setFont(&fonts::efontCN_16);
        Lcd.setTextColor(COL_RED, COL_PANEL);
        Lcd.setCursor(40, 65);
        Lcd.print("[ RUNTIME ERROR ]");

        Lcd.setTextColor(COL_TEXT, COL_PANEL);
        Lcd.setCursor(35, 95);
        Lcd.print(msg);

        Lcd.setTextColor(COL_MUTED, COL_PANEL);
        Lcd.setCursor(40, 155);
        Lcd.print("Press [B] to return");

        while (!_device->button.B.pressed() && !_device->button.A.pressed()) {
            delay(50);
        }
    }
}
