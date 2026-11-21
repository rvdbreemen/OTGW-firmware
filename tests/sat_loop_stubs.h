// The stub layer of tests/test_sat_loop_integration.py: everything outside SAT that SATcontrol.ino / SATcycles.ino / SATpid.ino reach.
// Every "STUB" line says what the stub does NOT model. Included after the real type headers and before the real SAT sources.
#pragma once

// STUB: DebugT*/SATDebug* print nothing (the telnet console is not modelled)
#undef Debug
#undef Debugln
#undef Debugf
#undef DebugFlush
#undef DebugT
#undef DebugTln
#undef DebugTf
#define DebugTf(...) ((void)0)
#define DebugTln(...) ((void)0)
#define Debugf(...) ((void)0)
#define Debugln(...) ((void)0)
#define OTDebugTln(...) ((void)0)

// STUB: FreeRTOS critical sections are no-ops (one host thread; real mutual exclusion is not modelled)
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(m) ((void)(m))
#define portEXIT_CRITICAL(m) ((void)(m))

// STUB: JsonEmit swallows every call (the REST status JSON is not produced; it is not on the control path)
struct JsonEmit {
  template <class... A> void beginObject(A&&...) {}
  template <class... A> void endObject(A&&...) {}
  template <class... A> void beginArray(A&&...) {}
  template <class... A> void endArray(A&&...) {}
  template <class... A> void field(A&&...) {}
};
#include "SATmqttPublish.h"

// ---- MQTT publish: STUB: every publish helper only records that it was called; no broker, no heartbeat timing, no on-change shadow logic
static int g_mqttPublishes = 0;
static bool publishIfChangedF(const __FlashStringHelper*, float, SATShadowF&, float, uint8_t, bool) { g_mqttPublishes++; return false; }
static bool publishIfChangedI(const __FlashStringHelper*, int32_t, SATShadowI&, bool) { g_mqttPublishes++; return false; }
static bool publishIfChangedB(const __FlashStringHelper*, bool, SATShadowB&, bool) { g_mqttPublishes++; return false; }
static bool publishIfChangedS(const __FlashStringHelper*, const char*, SATShadowS&, bool) { g_mqttPublishes++; return false; }
static bool publishIfChangedBStr(const __FlashStringHelper*, bool, SATShadowB&, const char*, const char*, bool) { g_mqttPublishes++; return false; }
static bool publishIfChangedF(const char*, float, SATShadowF&, float, uint8_t, bool) { g_mqttPublishes++; return false; }
static bool publishIfChangedI(const char*, int32_t, SATShadowI&, bool) { g_mqttPublishes++; return false; }
static bool publishIfChangedB(const char*, bool, SATShadowB&, bool) { g_mqttPublishes++; return false; }
static bool publishIfChangedS(const char*, const char*, SATShadowS&, bool) { g_mqttPublishes++; return false; }
static bool publishJsonAttrIfChanged(const __FlashStringHelper*, const char*, uint32_t&, bool, bool) { g_mqttPublishes++; return false; }
static bool satPublishDeferred() { return false; }          // STUB: the connect-pace window never defers a publish
static void satPublishDeferredClear() {}
static bool mqttConnectPaceHold() { return false; }
static void sendMQTTData(const char*, const char*, const bool = false) { g_mqttPublishes++; }                       // STUB: no broker
static void sendMQTTData(const __FlashStringHelper*, const char*, const bool = false) { g_mqttPublishes++; }
static void sendMQTTData(const __FlashStringHelper*, const __FlashStringHelper*, const bool = false) { g_mqttPublishes++; }
static void weatherPublishMQTT() {}                          // STUB: SATweather.ino is not compiled
static void satBLEInit() {}                                  // STUB: SATble.ino is not compiled; no BLE sensor ever
static void satBLEPublishMQTT() {}
static bool satBLEFailoverActive() { return false; }
static void satBLESendStatusJSON(JsonEmit&, const SATRuntimeSection&, bool) {}
static void satPressureHealthPublish() {}                    // STUB: SATpressure.ino is not compiled; boiler pressure is never published or judged
static void satPressureHealthUpdate() {}

// ---- web/REST: STUB: nothing is served, REST perf counters are dropped
template <class F> static void restSendChunked(const char*, F) {}
static void restPerfBegin(RestPerfTarget) {}
static void restPerfCommit(RestPerfTarget, uint32_t) {}
static void sendApiBusy(const __FlashStringHelper*) {}
static uint32_t platformMaxFreeBlock() { return 100000; }    // STUB: heap is always plentiful
static int g_wsEvents = 0;
static void sendEventToWebSocket(char, const char*) { g_wsEvents++; }   // STUB: the WebSocket ring drains to a counter
static void sendEventToWebSocket_P(char, const char*) { g_wsEvents++; }
static void sendWebSocketJSON(const char*) { g_wsEvents++; }
static void feedWatchDog() {}                                // STUB: no watchdog

// ---- settings persistence: STUB: nothing is written; settings live in RAM only (updateSetting() ignores the change)
static int g_settingsDirty = 0;
static void settingsMarkDirty() { g_settingsDirty++; }
static bool writeSettings(bool) { g_settingsDirty++; return true; }
static void updateSetting(const char*, const char*) {}

// ---- filesystem: STUB: LittleFS is not mounted; every open fails, so nothing is loaded or persisted (state starts from defaults)
static bool LittleFSmounted = false;   // STUB: see above, nothing is mounted
struct File {
  explicit operator bool() const { return false; }
  size_t print(...) { return 0; } size_t println(...) { return 0; } size_t readBytes(char*, size_t) { return 0; } size_t write(...) { return 0; }
  int available() { return 0; } int read() { return -1; } size_t size() { return 0; } void close() {} void flush() {}
};
struct LittleFSstub {
  File open(const char*, const char* = "r", bool = false) { return File(); }
  bool exists(const char*) { return false; } bool remove(const char*) { return false; }
  bool mkdir(const char*) { return false; } bool rename(const char*, const char*) { return false; }
} static LittleFS;
// STUB (TASK-1989): the SAT state saves hand their file to the fsWriter queue; with LittleFS not mounted the real
// fsWriterSubmit() refuses (fsWriter.ino, the !LittleFSmounted && !bFsImageWriting check), so nothing is persisted.
static bool fsWriterSubmit(const char*, const char*, size_t) { return false; }
#define FPSTR(p) (p)   // STUB: PROGMEM and RAM are the same on the host
// STUB: dtostrf via sprintf (no ESP float-to-string quirks)
static char* dtostrf(float v, int, int p, char* b) { sprintf(b, "%.*f", p, v); return b; }

// ---- other firmware services the control loop asks: each a fixed answer
static bool g_ntp = true;       static bool isNTPtimeSet() { return g_ntp; }          // STUB: NTP never drifts, wall-clock time is not used
static bool isFlashing() { return false; }                                            // STUB: no flash update ever runs
static bool g_pic = true;       static bool isPICEnabled() { return g_pic; }          // PIC transport; the test can switch it
static bool isOTDirectEnabled() { return !g_pic; }   // STUB: OT-Direct is never selected by the driver
static bool hasOTCommandInterface() { return true; }   // STUB: a command interface is always present (the PIC); the no-interface 5-tick trip is not run
static bool otDirectBoilerPresent() { return false; }                                 // STUB: SAT simulation is off, no loopback boiler
static bool otRealBoilerSeenRecently() { return false; }   // STUB: only SAT simulation reads it
static bool g_unsup28 = false, g_unsup25 = false;                                     // STUB: the OT support map is two flags the test sets
static bool isBoilerMsgIdUnsupportedRead(uint8_t id) { return id == 28 ? g_unsup28 : (id == 25 ? g_unsup25 : false); }
static bool boilerMsgId28UnknownConfirmed() { return g_unsup28; }
static bool picBoardTretValid() { return false; }                                      // STUB: no PR=E board return sensor
static bool picBoardTretAbsent() { return true; }
// STUB: EVALBOOLEAN is the firmware macro verbatim (OTGW-firmware.h)
#define EVALBOOLEAN(x) (strcasecmp(x,"true")==0||strcasecmp(x,"on")==0||strcasecmp(x,"1")==0)
#include <memory>
#if defined(_MSC_VER)   // TASK-3020: POSIX hosts (g++/musl on the NAS) have localtime_r already
static inline struct tm* localtime_r(const time_t* t, struct tm* o) { localtime_s(o, t); return o; }   // STUB: host time zone, wall clock is not under test
#endif
void connProbe(uint8_t) {}              // STUB: MQTT connect diagnostics probe
void satTakeoverHeapTraceBegin() {}     // STUB: TASK-2019 SAT takeover heap trace (MQTTstuff.ino)
uint8_t g_satShadowEpoch = 0;           // STUB: the publish shadow epoch of SATmqttPublish.cpp (that file is not compiled)
static constexpr uint16_t TRACKED_TIME_UNSEEN = 0xFFFFU;   // the real sentinel of OTGW-Core.ino (TASK-1979 climate publish in SATcontrol)
static bool hvacHeartbeatDue(uint16_t) { return false; }   // STUB: the MQTT heartbeat timing of OTGW-Core.ino is not modelled (no republish on a timer)
static uint16_t currentTrackedSeconds() { return (uint16_t)((millis() / 1000UL) % 0xFFFFUL); }   // STUB: same shape as OTGW-Core.ino (seconds, never the 0xFFFF sentinel); no wrap behaviour tested
void beginStatusBurst() {}                // STUB: MQTT status-burst windowing (ADR-088) not modelled
void endStatusBurst() {}                  // STUB: idem
void incrementStatusBurstPublishCount() {}   // STUB: idem, no burst budget
