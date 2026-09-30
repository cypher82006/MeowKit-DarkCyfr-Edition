/**
 * @file ui_sd_bridge.cpp
 * @brief SD_MMC ↔ C UI bridge implementation.
 */
#include "ui_sd_bridge.h"

#include <Arduino.h>
#include <SD_MMC.h>
#include <string.h>

extern "C" int ui_sd_present(void)
{
    /* Actively probe — SD_MMC.cardType() is a cached value that does not
     * notice hot removals. Opening "/" forces a bus transaction. */
    File root = SD_MMC.open("/");
    if (!root) {
        /* Open failed: card may have been removed then reinserted.
         * Try to remount with the same parameters used at boot. */
        SD_MMC.end();
        if (!SD_MMC.begin("/sdcard", true, false, 10000)) return 0;
        root = SD_MMC.open("/");
        if (!root) return 0;
    }
    bool ok = root.isDirectory();
    root.close();
    return ok ? 1 : 0;
}

extern "C" uint64_t ui_sd_total_bytes(void)
{
    if (!ui_sd_present()) return 0;
    return SD_MMC.cardSize();
}

extern "C" uint64_t ui_sd_used_bytes(void)
{
    /* Returning 0 safely avoids SD_MMC.usedBytes() / f_getfree() which scans
     * the entire FAT32 cluster table synchronously and triggers a TWDT reboot. */
    return 0;
}

extern "C" int ui_sd_list_dir(const char* path, ui_sd_visit_cb cb, void* user)
{
    if (!path || !cb) return -1;

    File dir = SD_MMC.open(path);
    if (!dir) return -1;
    if (!dir.isDirectory()) { dir.close(); return -1; }

    File f = dir.openNextFile();
    while (f) {
        const char* full = f.name();
        const char* slash = strrchr(full, '/');
        const char* base = slash ? slash + 1 : full;
        cb(base, f.isDirectory() ? 1 : 0, user);

        File next = dir.openNextFile();
        f.close();
        f = next;
    }
    dir.close();
    return 0;
}
