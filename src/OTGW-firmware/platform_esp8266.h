/*
***************************************************************************
**  Program  : platform_esp8266.h
**  Version  : v2.0.0-alpha.129
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

// ---- PlatformIO / Windows include-order guard ----------------------------
// On Windows (case-insensitive FS), the framework's Stream.h may pull in
// this project's Debug.h instead of the framework's own debug.h, causing
// ESP8266WebServer.h (included below) to use DEBUGV before it is defined.
// Pre-defining DEBUGV as empty is safe: it matches the framework's own
// definition in non-debug builds (no DEBUG_ESP_PORT set).
#ifndef DEBUGV
#define DEBUGV(...)
#endif

// ---- Core ESP8266 libraries ----------------------------------------------
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266LLMNR.h>
#include <LittleFS.h>
#include <coredecls.h>          // crc32() — used by the settings no-op fingerprint shim

extern "C" {
  #include "user_interface.h"   // wifi_station_dhcpc_stop/start, system_get_rst_info
  int clock_gettime(clockid_t unused, struct timespec *tp);
}

// ---- Platform name -------------------------------------------------------
#define PLATFORM_NAME "ESP8266"

// ---- Feature flags -------------------------------------------------------
#define HAS_LLMNR           1    // ESP8266 has LLMNR support
#define MDNS_NEEDS_UPDATE   1    // ESP8266 mDNS requires MDNS.update() in loop

// ---- Unified type aliases ------------------------------------------------
using OTGWWebServer = ESP8266WebServer;

// ---- Platform functions --------------------------------------------------

// WiFi hostname
inline void platformSetHostname(const char *hostname) {
  WiFi.hostname(hostname);
}

inline const char* platformGetHostname() {
  static char _hn[64];
  strlcpy(_hn, WiFi.hostname().c_str(), sizeof(_hn));
  return _hn;
}

// LittleFS info (native on ESP8266)
inline bool platformFSInfo(FSInfo &info) {
  return LittleFS.info(info);
}

// Core/SDK version
inline const char* platformCoreVersion() {
  static char _ver[32];
  strlcpy(_ver, ESP.getCoreVersion().c_str(), sizeof(_ver));
  return _ver;
}

// MAC address
inline void platformGetMacAddress(uint8_t *mac) {
  WiFi.macAddress(mac);
}

// SDK version
inline const char* platformSdkVersion() {
  return ESP.getSdkVersion();
}

// CPU frequency
inline uint32_t platformCpuFreqMHz() {
  return ESP.getCpuFreqMHz();
}

// Chip identity
inline uint32_t platformChipId() {
  return ESP.getChipId();
}

// Reset reason as a C string (copies into caller-supplied buffer)
inline void platformResetReason(char *buf, size_t len) {
  strlcpy(buf, ESP.getResetReason().c_str(), len);
}

// WiFi scan: is the network at scan index `i` encrypted (not open)?
inline bool platformWiFiIsEncrypted(uint8_t i) {
  return WiFi.encryptionType(i) != ENC_TYPE_NONE;
}

// Put the MQTT WiFiClient in synchronous-write mode (TASK-743). On ESP8266 this
// eliminates the TCP_SND_BUF temporary copy (~1KB) — writes flush straight to
// lwIP. The ESP32 NetworkClient has no equivalent and already streams without
// that copy, so its shim is a no-op (see platform_esp32.h).
inline void platformWiFiClientSetSync(WiFiClient& c) {
  c.setSync(true);
}

// PIC-compatible reset-cause char for the OTDirect PR: Q= response. OTDirect is
// ESP32-only and not built here, so this is a no-op stub for link symmetry.
inline char platformGetResetReasonChar() {
  return 'P';
}

// NTP hostname guard for an ESP8266 SDK bug: configTime() resets the WiFi
// station hostname to "ESP-XXXXXX" on some SDK versions. Call before and after
// configTime() to save/restore the configured hostname. The corrected hostname
// is sent on the next DHCP exchange; no SDK DHCP calls here (a
// wifi_station_dhcpc_start while connected breaks setAutoReconnect DHCP on later
// reassociations — see TASK-432).
inline void platformNtpHostnameFix(const char *hostname) {
  platformSetHostname(hostname);
}

// Heap information
inline uint32_t platformFreeHeap() {
  return ESP.getFreeHeap();
}

inline uint32_t platformMaxFreeBlock() {
  return ESP.getMaxFreeBlockSize();
}

// Heap fragmentation as percentage (0 = none, 100 = worst). ESP8266 exposes
// this natively via ESP.getHeapFragmentation(); ESP32 computes it.
inline uint8_t platformHeapFragmentation() {
  return ESP.getHeapFragmentation();
}

// Minimum observed free heap since boot. ESP8266 has no native API for this,
// so we maintain our own watermark that the firmware updates each loop via
// platformUpdateMinFreeHeap(). Returns current free heap if no sample has
// been taken yet (sentinel 0xFFFFFFFF).
extern uint32_t g_platformMinFreeHeapWatermark;  // defined in helperStuff.ino

inline uint32_t platformMinFreeHeap() {
  return (g_platformMinFreeHeapWatermark == 0xFFFFFFFFUL)
           ? ESP.getFreeHeap()
           : g_platformMinFreeHeapWatermark;
}

inline void platformUpdateMinFreeHeap() {
  const uint32_t cur = ESP.getFreeHeap();
  if (cur < g_platformMinFreeHeapWatermark) g_platformMinFreeHeapWatermark = cur;
}

// Exception cause from the last reset. Returns 0 on normal/clean reboot;
// non-zero (typically 1..29) on crash, WDT, or unaligned access. Wraps the
// raw exccause field from ESP.getResetInfoPtr() which is ESP8266-specific.
inline uint32_t platformExccause() {
  rst_info *rst = ESP.getResetInfoPtr();
  return (rst != nullptr) ? (uint32_t)rst->exccause : 0U;
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

// Restart: ESP.reset() is a bootrom jump (0x40000080), equivalent to the
// physical reset pin. It wipes ALL SDK and lwIP state, sidestepping the
// Core 3.1.0 regression where ESP.restart() could leave WiFi SDK state in a
// half-associated condition across the soft-reset ("WiFi connected but
// services dead" symptom). Graceful peer cleanup (MQTT LWT, WS close frames,
// TCP FINs) is handled by the caller via prepareForReboot() beforehand.
// ESP.reset() never returns, so no safety-tail delay is required.
inline void platformRestart() {
  ESP.reset();
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

inline uint32_t platformResetCode() {
  struct rst_info *rtc_info = system_get_rst_info();
  return (rtc_info != nullptr) ? rtc_info->reason : (uint32_t)-1;
}

// Register dump for crash analysis (WDT, exception, soft WDT resets)
inline void platformResetRegisterDump(char *buf, size_t bufLen) {
  buf[0] = '\0';
  struct rst_info *rtc_info = system_get_rst_info();
  if (rtc_info == nullptr) return;
  if (rtc_info->reason == REASON_WDT_RST ||
      rtc_info->reason == REASON_EXCEPTION_RST ||
      rtc_info->reason == REASON_SOFT_WDT_RST) {
    snprintf_P(buf, bufLen,
      PSTR("ESP register contents: epc1=0x%08x, epc2=0x%08x, epc3=0x%08x, excvaddr=0x%08x, depc=0x%08x\r\n"),
      rtc_info->epc1, rtc_info->epc2, rtc_info->epc3, rtc_info->excvaddr, rtc_info->depc);
  }
}

// Exception cause description
inline void platformResetExceptionInfo(char *buf, size_t bufLen) {
  buf[0] = '\0';
  struct rst_info *rtc_info = system_get_rst_info();
  if (rtc_info == nullptr || rtc_info->reason != REASON_EXCEPTION_RST) return;
  switch (rtc_info->exccause) {
    case 0:   snprintf_P(buf, bufLen, PSTR("- Invalid command (0)")); break;
    case 6:   snprintf_P(buf, bufLen, PSTR("- Division by zero (6)")); break;
    case 9:   snprintf_P(buf, bufLen, PSTR("- Unaligned read/write operation addresses (9)")); break;
    case 28:  snprintf_P(buf, bufLen, PSTR("- Access to invalid address (28)")); break;
    case 29:  snprintf_P(buf, bufLen, PSTR("- Access to invalid address (29)")); break;
    default:  snprintf_P(buf, bufLen, PSTR("- Other (not specified) (%d)"), rtc_info->exccause); break;
  }
}

// platformRestartDHCP() removed in TASK-432 port. Calling
// wifi_station_dhcpc_stop/start while the station is connected takes DHCP
// ownership away from the SDK and breaks setAutoReconnect-driven DHCP on
// subsequent reassociations. See networkStuff.ino for the rationale.
// system_get_rst_info() above still requires user_interface.h, so the
// include stays.

// Serial error checks (ESP8266 HardwareSerial has these; ESP32 does not)
inline bool platformSerialHasOverrun(HardwareSerial &serial) {
  return serial.hasOverrun();
}

inline bool platformSerialHasRxError(HardwareSerial &serial) {
  return serial.hasRxError();
}

// ---- Settings no-op fingerprint (TASK-564) -------------------------------
// Capture a fingerprint of the settings blob, then test whether a later state
// is unchanged. ESP8266 uses the SDK crc32() sentinel (~4 B) rather than a full
// struct snapshot, because DRAM is scarce. Not re-entrant by contract:
// updateSetting() captures at entry and compares once before returning.
inline uint32_t &_platformSettingsNoopFp() { static uint32_t fp = 0; return fp; }

inline void platformSettingsNoopCapture(const void *data, size_t len) {
  _platformSettingsNoopFp() = crc32(data, len);
}

inline bool platformSettingsNoopUnchanged(const void *data, size_t len) {
  return crc32(data, len) == _platformSettingsNoopFp();
}

// ---- SAT-BLE no-op stubs (ESP-abstraction Tier 2, TASK-742) --------------
// The BLE room-sensor subsystem (SATble.ino and the BLE helpers in
// MQTTstuff.ino) is gated by HAS_SAT_BLE / defined(ESP32) and compiles to
// nothing on ESP8266 (no BLE radio). These inline no-ops let the shared call
// sites in SATcontrol.ino / OTGW-firmware.ino / networkStuff.ino /
// handleDebug.ino / restAPI.ino drop their #if defined(ESP32) guards. Getters
// return neutral values; the runtime state.sat BLE fields (bBleTempValid,
// iBleSensorCount, ...) stay zero on ESP8266, so every BLE-dependent code
// path is naturally inert. Default args (e.g. label=nullptr) live only in the
// forward declarations in OTGW-firmware.h, never repeated here.
inline void  satBLEInit() {}
inline void  satBLELoop() {}
inline void  satBLEUpdateState() {}
inline float satBLEGetTemperature() { return 0.0f; }
inline float satBLEGetHumidity() { return 0.0f; }
inline void  satBLEPublishMQTT() {}
inline void  satBLESendStatusJSON() {}
inline void  satBLEMacToCompact(const char* /*macWithColons*/, char* out, size_t outSize) { if (out && outSize) out[0] = '\0'; }
inline void  satBLEPublishStateTopics(const char* /*macCompact*/, float /*temp*/, float /*hum*/, uint8_t /*bat*/, int8_t /*rssi*/) {}
inline bool  satBLEPublishHaDiscovery(const char* /*macCompact*/, const char* /*macWithColons*/, const char* /*label*/) { return false; }
inline void  satBLEUnpublishDiscovery(const char* /*macCompact*/) {}
inline void  satBLERosterSendJSON() {}
inline bool  satBLERosterSelect(const char* /*mac*/) { return false; }
inline bool  satBLERosterSetLabel(const char* /*mac*/, const char* /*label*/) { return false; }
inline bool  satBLERosterForget(const char* /*mac*/) { return false; }

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
