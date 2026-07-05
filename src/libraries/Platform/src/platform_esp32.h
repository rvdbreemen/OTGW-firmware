/*
***************************************************************************
**  Program  : platform_esp32.h
**  Version  : v2.0.0-alpha.137
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
#include <lwip/priv/tcp_priv.h>  // tcp_active_pcbs (platformTcpActivePcbCount)
#include <lwip/tcpip.h>          // LOCK_TCPIP_CORE/UNLOCK_TCPIP_CORE (platformTcpActivePcbCount)

// ---- ADR-123 async/FreeRTOS stack (TASK-865.4) ---------------------------
// Foundation headers for the 2.0.0 ESP32-S3 event-driven migration. Included
// here (the platform include aggregator) ungated so every ESP32 env compiles
// against them and PlatformIO's LDF pulls the libraries. The lib_deps pins live
// in [env:esp32].lib_deps (inherited by esp32-classic/esp32-combo via extends).
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>   // also declares AsyncWebSocket
#include <espMqttClient.h>

// ---- Platform name -------------------------------------------------------
#define PLATFORM_NAME "ESP32"

// ---- ADR-123 async-stack link-proof smoke (TASK-865.4) -------------------
// A bare #include only proves the headers compile, not that each library's
// compiled object actually LINKS. This function instantiates one class per
// async lib, which forces their out-of-line constructors (and therefore the
// libraries' .cpp objects) to be pulled in at link time — confirmable by
// grepping the .map for AsyncClient / AsyncWebSocket / espMqttClient.
//
// __attribute__((used)) defeats -flto dead-stripping: without it an uncalled
// static function (and its references) would be eliminated and prove nothing.
// The whole proof is gated on ASYNC_LINK_PROOF, set ONLY in [env:esp32]
// build_flags. esp32-classic/esp32-combo do not see that flag (they rebuild
// build_flags from the global [env]), so the smoke and its flash cost vanish
// there — keeping the ~98.4 %-full combo binary inside its 1.875 MB app slot.
// The function is never called: it exists purely so the linker resolves the
// symbols. ASYNC_LINK_PROOF is a feature flag, not a platform/board macro, so
// it does not trip the ESP-abstraction boundary check.
#if defined(ASYNC_LINK_PROOF)
// `inline` (not `static`) so the several .cpp TUs that include platform.h share
// ONE COMDAT-folded definition instead of each retaining its own copy.
inline __attribute__((used)) void platformAsyncLinkProof() {
  volatile void *sink;
  AsyncClient    asyncTcpProof;                 // AsyncTCP
  AsyncWebSocket asyncWsProof("/_linkproof");   // ESPAsyncWebServer (WebSocket)
  espMqttClient  mqttProof;                      // espMqttClient (plain, non-async)
  sink = &asyncTcpProof;
  sink = &asyncWsProof;
  sink = &mqttProof;
  (void)sink;
}
#endif

// ---- Feature flags -------------------------------------------------------
#define HAS_LLMNR           0    // ESP32 does not have LLMNR
#define MDNS_NEEDS_UPDATE   0    // ESP32 mDNS runs on a background task

// ---- Unified type aliases ------------------------------------------------
// (OTGWWebServer = WebServer removed in TASK-865.9: the HTTP server moved to
//  ESPAsyncWebServer. The sync WebServer type now survives only inside
//  OTGW-ModUpdateServer-esp32.h, which includes <WebServer.h> itself; seq11
//  ports OTA onto AsyncWebServer and retires that last consumer.)

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

// Disable WiFi modem power-save. ESP32 defaults to WIFI_PS_MIN_MODEM, which
// parks the radio between DTIM beacons and adds 100 ms-1 s of latency to every
// inbound packet (and drops buffered packets under burst) - the root cause of
// the "web page takes seconds to load, sometimes never" symptom on OTGW32.
// Costs a few mA extra; on a mains-powered gateway that is the right trade.
inline void platformWifiDisableSleep() {
  WiFi.setSleep(false);
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

// WiFi scan: is the network at scan index `i` encrypted (not open)?
inline bool platformWiFiIsEncrypted(uint8_t i) {
  return WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
}

// No-op: the ESP32 NetworkClient has no setSync() and already streams writes
// without the ESP8266 TCP_SND_BUF temporary copy (TASK-743). Stub for link
// symmetry so the caller stays unguarded.
inline void platformWiFiClientSetSync(WiFiClient&) {
}

// Reset cause as a PIC-compatible boot-code char for the OTDirect PR: Q=
// response (P=power-on, C=cold/SW-reset, W=watchdog, B=brownout, E=external).
// OTDirect (ESP32) is the only caller; the vocabulary mirrors the PIC's reset
// codes so downstream PR-parsing stays identical to the PIC path.
inline char platformGetResetReasonChar() {
  switch (esp_reset_reason()) {
    case ESP_RST_SW:        return 'C';  // Software reset -> Cold start
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:       return 'W';  // Watchdog
    case ESP_RST_BROWNOUT:  return 'B';  // Brownout
    case ESP_RST_EXT:       return 'E';  // External reset
    default:                return 'P';  // Power-on or unknown
  }
}

// NTP hostname guard. ESP32's configTime() does not touch the WiFi station
// hostname, so this is a no-op. Call it before and after configTime() so the
// shared startNTP() code path stays platform-neutral.
inline void platformNtpHostnameFix(const char *hostname) {
  (void)hostname;
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

// TASK-1017: count of active lwIP TCP PCBs (established/handshaking TCP
// connections), for load-test observability of connection-level pressure
// (TASK-879 class: PCB pool exhaustion under flood). tcp_active_pcbs is an
// internal lwIP linked list (lwip/priv/tcp_priv.h) normally only walked from
// the tcpip thread; this build has CONFIG_LWIP_TCPIP_CORE_LOCKING=y (verified
// in the arduino-esp32 sdkconfig), so LOCK_TCPIP_CORE()/UNLOCK_TCPIP_CORE()
// is a real mutex safe to take from any task (the documented pattern for
// touching lwIP internals from application code). Sample from the 1 Hz loop
// task only (helperStuff.ino sampleHeapWatermark) — never from a per-request
// hot path, since a live per-response walk would (a) break the REST chunked
// emit's determinism contract (byte-identical JSON across re-run windows,
// see DeviceInfoSnap) and (b) add lock contention to the async_tcp task.
inline uint16_t platformTcpActivePcbCount() {
  uint16_t count = 0;
  LOCK_TCPIP_CORE();
  for (struct tcp_pcb *pcb = tcp_active_pcbs; pcb != nullptr; pcb = pcb->next) {
    count++;
  }
  UNLOCK_TCPIP_CORE();
  return count;
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

// ---- Settings no-op fingerprint (TASK-564) -------------------------------
// ESP32 retains a full-struct byte snapshot (memcmp) rather than a CRC sentinel:
// DRAM is plentiful and a byte-exact compare avoids CRC collisions, and the
// ESP8266 SDK crc32() is not part of the ESP32 Arduino API. The buffer is
// allocated once (settings size is fixed) and reused. Not re-entrant by
// contract: updateSetting() captures at entry and compares once before return.
// Fail-safe: if allocation fails, "unchanged" returns false so the caller
// treats the settings as dirty and writes them.
struct _PlatformNoopSnap { uint8_t *buf; size_t len; };
inline _PlatformNoopSnap &_platformSettingsNoopSnap() {
  static _PlatformNoopSnap s{nullptr, 0};
  return s;
}

inline void platformSettingsNoopCapture(const void *data, size_t len) {
  _PlatformNoopSnap &s = _platformSettingsNoopSnap();
  if (s.len != len) {
    free(s.buf);
    s.buf = static_cast<uint8_t *>(malloc(len));
    s.len = s.buf ? len : 0;
  }
  if (s.buf) memcpy(s.buf, data, len);
}

inline bool platformSettingsNoopUnchanged(const void *data, size_t len) {
  _PlatformNoopSnap &s = _platformSettingsNoopSnap();
  return s.buf && s.len == len && memcmp(s.buf, data, len) == 0;
}

// ---- ADR-123 concurrency primitives (TASK-865.5) -------------------------
// FreeRTOS-backed mutex and value-copy queue shims. Application code uses the
// opaque PlatformMutex / PlatformQueue handles and the platformXxx() calls
// below, never the raw FreeRTOS API — so the OTGWState mutex and the OT-frame
// producer/consumer queue stay platform-neutral. On ESP32 these map directly
// onto the IDF FreeRTOS objects (freertos/FreeRTOS.h is pulled in by Arduino.h).
//
// Phase 1 (this task) creates ONE mutex and ONE queue in setup() (ADR-044) and
// runs the consumer cooperatively from loop(); seq6 lifts the producer into a
// FreeRTOS task. The shims already provide the real synchronisation so that
// lift needs no further primitive work.
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

using PlatformMutex = SemaphoreHandle_t;
using PlatformQueue = QueueHandle_t;

// Create a non-recursive mutex. Returns nullptr on allocation failure; callers
// must treat that as fatal-at-setup (a missing mutex means no cross-task
// protection). Non-recursive by design: a single call chain must never take it
// twice (would self-deadlock even single-threaded).
inline PlatformMutex platformMutexCreate() {
  return xSemaphoreCreateMutex();
}

// Take the mutex, blocking up to timeoutMs. timeoutMs == 0 means block forever
// (portMAX_DELAY). Returns true if acquired. A nullptr handle returns true
// (no-op lock) so a failed-create path degrades to "unprotected" rather than
// "always blocked".
inline bool platformMutexLock(PlatformMutex m, uint32_t timeoutMs = 0) {
  if (m == nullptr) return true;
  TickType_t ticks = (timeoutMs == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeoutMs);
  return xSemaphoreTake(m, ticks) == pdTRUE;
}

inline void platformMutexUnlock(PlatformMutex m) {
  if (m == nullptr) return;
  xSemaphoreGive(m);
}

// Create a value-copy queue of `length` items, each `itemSize` bytes. The queue
// copies by value on send/receive (no shared pointers, no mutex needed for the
// item flow). Returns nullptr on allocation failure.
inline PlatformQueue platformQueueCreate(size_t length, size_t itemSize) {
  return xQueueCreate((UBaseType_t)length, (UBaseType_t)itemSize);
}

// Non-blocking send (copy `item` into the queue). Returns false if the queue is
// full or the handle is null. Callers size the queue so it cannot fill in
// normal operation and treat a false result as a diagnostic drop, never a
// fall-back to direct processing (which would reorder the FIFO).
inline bool platformQueueSend(PlatformQueue q, const void *item) {
  if (q == nullptr) return false;
  return xQueueSend(q, item, 0) == pdTRUE;
}

// Non-blocking send to the FRONT of the queue (copy `item` ahead of every item
// already queued). Returns false if the queue is full or the handle is null.
// Used to restore FIFO order after a flow-control short write: the popped chunk
// is put back at the head so it is the next thing written, never reordered
// behind chunks that were enqueued after it (TASK-865.15 TX-ordering invariant).
inline bool platformQueueSendToFront(PlatformQueue q, const void *item) {
  if (q == nullptr) return false;
  return xQueueSendToFront(q, item, 0) == pdTRUE;
}

// Sentinel for platformQueueReceive: block indefinitely until an item arrives
// (FreeRTOS portMAX_DELAY). An event-consumer task that idles with zero CPU
// between events (e.g. the webhook sender task, TASK-865.13) passes this instead
// of a finite poll. pdMS_TO_TICKS(UINT32_MAX) would overflow to a garbage tick
// count, so the sentinel maps straight to portMAX_DELAY rather than converting.
#define PLATFORM_QUEUE_WAIT_FOREVER 0xFFFFFFFFUL

// Receive one item (copy out). timeoutMs == 0 polls without blocking;
// timeoutMs == PLATFORM_QUEUE_WAIT_FOREVER blocks indefinitely. Returns true if
// an item was dequeued.
inline bool platformQueueReceive(PlatformQueue q, void *item, uint32_t timeoutMs = 0) {
  if (q == nullptr) return false;
  TickType_t ticks;
  if (timeoutMs == 0) {
    ticks = 0;
  } else if (timeoutMs == PLATFORM_QUEUE_WAIT_FOREVER) {
    ticks = portMAX_DELAY;
  } else {
    ticks = pdMS_TO_TICKS(timeoutMs);
  }
  return xQueueReceive(q, item, ticks) == pdTRUE;
}

// ---- ADR-123 dedicated-task primitives (TASK-865.6) ----------------------
// The PIC UART moves onto its own FreeRTOS task pinned to the application core.
// Application code uses these shims and the opaque PlatformTask handle, never
// the raw xTaskCreatePinnedToCore / vTaskDelay symbols, so the dedicated-task
// machinery stays behind the platform abstraction (no raw FreeRTOS in *.ino).
using PlatformTask = TaskHandle_t;

// Pin the task to the application core. APP_CPU_NUM is core 1 on a dual-core
// ESP32-S3; the protocol core (0) is left for the WiFi/BT stack and AsyncTCP.
// stackBytes is in BYTES (xTaskCreate wants words for some ports but the IDF
// Arduino wrapper takes a byte count). priority 1 keeps it just above the idle
// task; the loop()/Arduino task runs at priority 1 too, so the PIC task does
// not pre-empt it indefinitely (both yield via the task delay below).
// Returns nullptr on creation failure.
inline PlatformTask platformTaskCreatePinned(void (*fn)(void *),
                                             const char *name,
                                             uint32_t stackBytes,
                                             void *arg,
                                             unsigned int priority) {
  TaskHandle_t h = nullptr;
  BaseType_t ok = xTaskCreatePinnedToCore(fn, name, stackBytes, arg,
                                          (UBaseType_t)priority, &h, APP_CPU_NUM);
  return (ok == pdPASS) ? h : nullptr;
}

// Block the calling task for ms milliseconds. This is the cooperative yield
// point inside the PIC task loop: it MUST block (vTaskDelay) between drains so
// it never busy-polls available() and starves the core / trips the TWDT. A 0 ms
// delay still yields one tick (vTaskDelay(0) is a no-op, so clamp to >=1 tick).
inline void platformTaskDelay(uint32_t ms) {
  TickType_t ticks = pdMS_TO_TICKS(ms);
  if (ticks == 0) ticks = 1;
  vTaskDelay(ticks);
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
