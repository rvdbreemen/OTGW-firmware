/*
***************************************************************************
**  Program  : platform_esp8266.h
**  Version  : v1.4.0-beta
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  ESP8266-specific includes, type aliases, and shim functions.
**  Included only by platform.h — never include this file directly.
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
*/

#ifndef PLATFORM_ESP8266_H
#define PLATFORM_ESP8266_H

// ---- Core ESP8266 libraries ----------------------------------------------
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266LLMNR.h>

extern "C" {
  #include "user_interface.h"   // wifi_station_dhcpc_stop/start, system_get_rst_info
}

// ---- Platform name -------------------------------------------------------
#define PLATFORM_NAME "ESP8266"

// ---- Feature flags -------------------------------------------------------
#define HAS_LLMNR           1    // ESP8266 has LLMNR support
#define MDNS_NEEDS_UPDATE   1    // ESP8266 mDNS requires MDNS.update() in loop

// ---- Unified type aliases ------------------------------------------------
using OTGWWebServer = ESP8266WebServer;

// ---- FSInfo (ESP32 uses a different struct) ------------------------------
// ESP8266 LittleFS.info(FSInfo&) is the native API — no shim needed.
// On ESP32, see platform_esp32.h for the compatibility wrapper.

// ---- Platform shim functions ---------------------------------------------

// Chip identity
inline uint32_t platformChipId() {
  return ESP.getChipId();
}

// Reset reason as a C string (copies into caller-supplied buffer)
inline void platformResetReason(char *buf, size_t len) {
  strlcpy(buf, ESP.getResetReason().c_str(), len);
}

// Heap information
inline uint32_t platformFreeHeap() {
  return ESP.getFreeHeap();
}

inline uint32_t platformMaxFreeBlock() {
  return ESP.getMaxFreeBlockSize();
}

// Flash information
inline uint32_t platformFlashChipRealSize() {
  return ESP.getFlashChipRealSize();
}

inline uint32_t platformFlashChipSize() {
  return ESP.getFlashChipSize();
}

inline uint32_t platformFlashChipSpeed() {
  return ESP.getFlashChipSpeed();
}

inline uint32_t platformFlashChipId() {
  return ESP.getFlashChipId();
}

inline uint8_t platformFlashChipMode() {
  return ESP.getFlashChipMode();   // returns FlashMode_t (enum)
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
  return RANDOM_REG32;
}

// RTC user memory (ESP8266-specific, emulated on ESP32 via Preferences)
inline bool platformRtcRead(uint32_t slot, uint32_t *data, size_t len) {
  return ESP.rtcUserMemoryRead(slot, data, len);
}

inline bool platformRtcWrite(uint32_t slot, const uint32_t *data, size_t len) {
  return ESP.rtcUserMemoryWrite(slot, const_cast<uint32_t*>(data), len);
}

// Reset info (ESP8266 SDK rst_info)
inline bool platformIsExternalReset() {
  rst_info *resetInfo = ESP.getResetInfoPtr();
  return (resetInfo != nullptr) && (resetInfo->reason == REASON_EXT_SYS_RST);
}

// DHCP restart
inline void platformRestartDHCP() {
  wifi_station_dhcpc_stop();
  wifi_station_dhcpc_start();
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

#endif // PLATFORM_ESP8266_H
