/*
***************************************************************************
**  Program  : platform_esp32.h
**  Version  : v1.4.0-beta
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
  // ESP32 doesn't expose flash chip ID the same way; return 0
  return 0;
}

inline uint8_t platformFlashChipMode() {
  return 0;  // Not directly available on ESP32 in the same enum
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

// RTC user memory — emulated via NVS Preferences on ESP32
inline bool platformRtcRead(uint32_t slot, uint32_t *data, size_t len) {
  Preferences prefs;
  if (!prefs.begin("otgw_rtc", true)) return false;  // read-only
  char key[12];
  snprintf(key, sizeof(key), "s%u", (unsigned)slot);
  size_t got = prefs.getBytes(key, data, len);
  prefs.end();
  return (got == len);
}

inline bool platformRtcWrite(uint32_t slot, const uint32_t *data, size_t len) {
  Preferences prefs;
  if (!prefs.begin("otgw_rtc", false)) return false;  // read-write
  char key[12];
  snprintf(key, sizeof(key), "s%u", (unsigned)slot);
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

// DHCP restart — use esp_netif API to restart only the DHCP client without
// dropping the WiFi association (mirrors ESP8266's wifi_station_dhcpc_stop/start).
inline void platformRestartDHCP() {
  esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if (netif) {
    esp_netif_dhcpc_stop(netif);
    esp_netif_dhcpc_start(netif);
  }
}

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
