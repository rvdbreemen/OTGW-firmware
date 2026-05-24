/*
***************************************************************************
**  Program  : platform_esp32.h
**  Version  : v2.0.0-alpha.58
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  ESP32-specific includes, type aliases, and shim functions.
**  Included only by platform.h — never include this file directly.
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
*/

#ifndef PLATFORM_ESP32_H
#define PLATFORM_ESP32_H

// ---- Core ESP32 libraries ------------------------------------------------
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <Update.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <esp_system.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <esp_flash.h>

// ---- Platform name -------------------------------------------------------
#define PLATFORM_NAME "ESP32"

// ---- Feature flags -------------------------------------------------------
#define HAS_LLMNR           0    // ESP32 does not have LLMNR
#define MDNS_NEEDS_UPDATE   0    // ESP32 mDNS runs on a background task

// ---- Unified type aliases ------------------------------------------------
using OTGWWebServer = WebServer;

// ---- FSInfo compatibility ------------------------------------------------
// ESP8266 uses FSInfo struct natively. Define it here for ESP32.
struct FSInfo {
  size_t totalBytes;
  size_t usedBytes;
  size_t blockSize;
  size_t pageSize;
  size_t maxOpenFiles;
  size_t maxPathLength;
};

// ---- Platform functions --------------------------------------------------

// WiFi hostname
inline void platformSetHostname(const char *hostname) {
  WiFi.setHostname(hostname);
}

inline const char* platformGetHostname() {
  return WiFi.getHostname();
}

// LittleFS info (ESP32: construct from totalBytes/usedBytes)
inline bool platformFSInfo(FSInfo &info) {
  info.totalBytes    = LittleFS.totalBytes();
  info.usedBytes     = LittleFS.usedBytes();
  info.blockSize     = 0;
  info.pageSize      = 0;
  info.maxOpenFiles  = 10;
  info.maxPathLength = 255;
  return (info.totalBytes > 0);
}

// Core/SDK version
inline const char* platformCoreVersion() {
  return ESP.getSdkVersion();
}

// SDK version
inline const char* platformSdkVersion() {
  return ESP.getSdkVersion();
}

// CPU frequency
inline uint32_t platformCpuFreqMHz() {
  return ESP.getCpuFreqMHz();
}

// MAC address
inline void platformGetMacAddress(uint8_t *mac) {
  esp_efuse_mac_get_default(mac);
}

// Chip identity (derive a 32-bit ID from the 48-bit MAC)
inline uint32_t platformChipId() {
  uint64_t mac = ESP.getEfuseMac();
  // XOR upper and lower 24 bits for a compact unique ID
  uint32_t low  = (uint32_t)(mac & 0xFFFFFF);
  uint32_t high = (uint32_t)((mac >> 24) & 0xFFFFFF);
  return low ^ high;
}

// Reset reason as a C string
inline void platformResetReason(char *buf, size_t len) {
  esp_reset_reason_t reason = esp_reset_reason();
  const char *str;
  switch (reason) {
    case ESP_RST_POWERON:  str = "Power on";        break;
    case ESP_RST_EXT:      str = "External reset";  break;
    case ESP_RST_SW:       str = "Software reset";   break;
    case ESP_RST_PANIC:    str = "Exception/panic";  break;
    case ESP_RST_INT_WDT:  str = "Interrupt watchdog"; break;
    case ESP_RST_TASK_WDT: str = "Task watchdog";   break;
    case ESP_RST_WDT:      str = "Other watchdog";  break;
    case ESP_RST_DEEPSLEEP: str = "Deep sleep wake"; break;
    case ESP_RST_BROWNOUT: str = "Brownout";         break;
    case ESP_RST_SDIO:     str = "SDIO reset";       break;
    default:               str = "Unknown";          break;
  }
  strlcpy(buf, str, len);
}

// Heap information
inline uint32_t platformFreeHeap() {
  return ESP.getFreeHeap();
}

inline uint32_t platformMaxFreeBlock() {
  return ESP.getMaxAllocHeap();
}

// Heap fragmentation as percentage (0 = none, 100 = worst). ESP32 has no
// native fragmentation API, so we compute it the same way ESP8266 core does:
// 100 - (maxBlock * 100 / totalFree). 0 when heap is empty.
inline uint8_t platformHeapFragmentation() {
  const uint32_t freeHeap = ESP.getFreeHeap();
  if (freeHeap == 0) return 0;
  const uint32_t maxBlk = ESP.getMaxAllocHeap();
  return (uint8_t)(100U - ((maxBlk * 100U) / freeHeap));
}

// Minimum observed free heap since boot. ESP32 tracks this natively in the
// heap allocator via heap_caps_get_minimum_free_size — no user-space
// watermark needed.
inline uint32_t platformMinFreeHeap() {
  return ESP.getMinFreeHeap();
}

// No-op on ESP32: the heap allocator tracks min-free internally.
inline void platformUpdateMinFreeHeap() {
  // intentionally empty
}

// Exception cause from the last reset. ESP32 does not expose a direct
// numeric exccause via esp_reset_reason(); instead the reset_reason enum
// conveys the class of fault. We map panic/watchdog to non-zero sentinel
// values so the boot signature still flags abnormal reboots.
inline uint32_t platformExccause() {
  switch (esp_reset_reason()) {
    case ESP_RST_PANIC:    return 1U;  // software panic / exception
    case ESP_RST_INT_WDT:  return 2U;  // interrupt watchdog
    case ESP_RST_TASK_WDT: return 3U;  // task watchdog
    case ESP_RST_WDT:      return 4U;  // other watchdog
    case ESP_RST_BROWNOUT: return 5U;  // brownout
    default:               return 0U;  // clean boot / power-on / external / SW
  }
}

// Flash information
inline uint32_t platformFlashChipRealSize() {
  return ESP.getFlashChipSize();   // ESP32 has no "real" vs "configured" distinction
}

inline uint32_t platformFlashChipSize() {
  return ESP.getFlashChipSize();
}

inline uint32_t platformFlashChipSpeed() {
  return ESP.getFlashChipSpeed();
}

inline uint32_t platformFlashChipId() {
  // Read JEDEC flash chip ID via ESP-IDF esp_flash API (ESP32 / ESP32-S3)
  uint32_t id = 0;
  esp_flash_read_id(NULL, &id);  // NULL = default/main flash chip
  return id;
}

inline uint8_t platformFlashChipMode() {
  return 4;  // Not available on ESP32; return index for "Unknown" in flashMode[]
}

inline uint32_t platformSketchSize() {
  return ESP.getSketchSize();
}

inline uint32_t platformFreeSketchSpace() {
  return ESP.getFreeSketchSpace();
}

// Restart
inline void platformRestart() {
  ESP.restart();
}

// Hardware random
inline uint32_t platformHardwareRandom() {
  return esp_random();
}

// RTC user memory — emulated via NVS Preferences on ESP32.
// WARNING: Unlike ESP8266 RTC memory (battery-backed SRAM, unlimited writes),
// NVS on ESP32 is flash-backed with finite erase/write cycles (10K-100K per
// sector). Only call platformRtcWrite() infrequently (boot, reset, deep sleep).
// Never use in a loop or on a timer. For high-frequency volatile storage on
// ESP32, consider RTC_DATA_ATTR variables instead.
inline bool platformRtcRead(uint32_t slot, uint32_t *data, size_t len) {
  Preferences prefs;
  if (!prefs.begin("otgw_rtc", true)) return false;  // read-only
  char key[12];
  snprintf_P(key, sizeof(key), PSTR("s%u"), (unsigned)slot);
  size_t got = prefs.getBytes(key, data, len);
  prefs.end();
  return (got == len);
}

inline bool platformRtcWrite(uint32_t slot, const uint32_t *data, size_t len) {
  Preferences prefs;
  if (!prefs.begin("otgw_rtc", false)) return false;  // read-write
  char key[12];
  snprintf_P(key, sizeof(key), PSTR("s%u"), (unsigned)slot);
  prefs.putBytes(key, data, len);
  prefs.end();
  return true;
}

// Reset info
inline bool platformIsExternalReset() {
  return (esp_reset_reason() == ESP_RST_EXT);
}

inline uint32_t platformResetCode() {
  return (uint32_t)esp_reset_reason();
}

// Register dump — not available on ESP32 via esp_reset_reason() API
inline void platformResetRegisterDump(char *buf, size_t bufLen) {
  buf[0] = '\0';
}

// Exception cause — not available on ESP32 via esp_reset_reason() API
inline void platformResetExceptionInfo(char *buf, size_t bufLen) {
  buf[0] = '\0';
}

// platformRestartDHCP() removed in TASK-432 port. Although the ESP32
// esp_netif_dhcpc_stop/start API differs from ESP8266's SDK calls, the
// underlying ownership-takeover concern still applies: once user code
// manages DHCP explicitly, autonomous SDK reassociation may not restart
// it correctly. The ESP8266 fix removed the call sites in networkStuff.ino;
// this header drops the function for symmetry across platforms.

// Serial error checks (ESP32 HardwareSerial does not expose overrun/rx error)
inline bool platformSerialHasOverrun(HardwareSerial &serial) {
  (void)serial;
  return false;
}

inline bool platformSerialHasRxError(HardwareSerial &serial) {
  (void)serial;
  return false;
}

/***************************************************************************
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to permit
* persons to whom the Software is furnished to do so, subject to the
* following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
* THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
****************************************************************************
*/

#endif // PLATFORM_ESP32_H
