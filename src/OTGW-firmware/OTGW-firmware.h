/* 
***************************************************************************  
**  Program  : OTGW-firmware.h
**  Version  : v2.0.0-alpha.61
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

#ifndef OTGW_FIRMWARE_H
#define OTGW_FIRMWARE_H

#include <Arduino.h>
#include <AceTime.h>
// #include <TimeLib.h>

// DEBUGGING: Uncomment the next line to disable WebSocket functionality
// #define DISABLE_WEBSOCKET

#include <SimpleTelnet.h>       // https://github.com/RvdB/SimpleTelnet — unified multi-client telnet (replaces TelnetStream + ESPTelnet)
extern SimpleTelnet<1> debugTelnet;   // defined in networkStuff.ino
#include "Wire.h"
#include "safeTimers.h"

// Per-component type headers (ADR-079). SAT is the first component extracted.
// SATtypes.h bundles both the runtime-state struct (SATRuntimeSection) and
// the settings struct (SATSection) in one file because their defaults and
// enums are tightly coupled; see SATtypes.h preamble for the rationale.
#include "SATtypes.h"
#include "boards.h"             // Board-specific pin maps and feature flags (HAS_PIC, HAS_DIRECT_OT)
// OTDirecttypes.h must follow boards.h because its contents are gated on HAS_DIRECT_OT (ADR-079).
#include "OTDirecttypes.h"
#include "OTGWLogMacros.h"
#if HAS_PIC
#include <OTGWSerial.h>         // Schelte Bron's Serial class - it upgrades and more
#endif
#include "OTGW-Core.h"          // Core code for this firmware
#include <OneWire.h>            // required for Dallas sensor library
#include <DallasTemperature.h>  // Miles Burton's - Arduino Dallas library

// Legacy pin aliases — map old names to boards.h constants so existing code
// (and any user forks) keeps compiling without search-and-replace churn.
#define I2CSCL  PIN_I2C_SCL
#define I2CSDA  PIN_I2C_SDA
#define BUTTON  PIN_BUTTON
#define LED1    PIN_LED1
#define LED2    PIN_LED2

#if HAS_PIC
#define PICRST  PIN_PIC_RST
#define PICFIRMWARE "/gateway.hex"
OTGWSerial OTGWSerial(PICRST, LED2);
void fwupgradestart(const char *hexfile);
void handlePendingUpgrade();
#endif

String checkforupdatepic(String filename);

#if HAS_DIRECT_OT
enum class OpenThermResponseStatus : byte;

// OTDirectRequestOrigin moved to OTDirecttypes.h (ADR-079/TASK-326)

// OT-direct forward declarations (defined in OTDirect.ino)
void initOTDirect();
void loopOTDirect();
void handleOTDirectBridgeStream();
void handleOTDirectCommand(const char* buf, int len);
void sendOTDirectOverridesJSON();
void sendPICSerial(const char* buf, int len);
// TASK-183: PI room compensation
float getFlowTemp();
// TASK-184: flame ratio metrics
uint8_t getFlameRatioDuty();
float   getFlameRatioFreq();
// TASK-185: MQTT-sourced room temp/setpoint routing
void otdMqttSetRoomTemp(float tempC);
void otdMqttSetRoomSetpoint(float tempC);
#endif

void blinkLEDnow();
void blinkLEDnow(uint8_t);
void setLed(uint8_t, uint8_t);

//Defaults and macro definitions
#define _HOSTNAME       "OTGW"
#define SETTINGS_FILE         "/settings.ini"
#define NTP_DEFAULT_TIMEZONE "Europe/Amsterdam"
#define NTP_HOST_DEFAULT "pool.ntp.org"
#define NTP_RESYNC_TIME 1800 //seconds = every 30 minutes
#define HOME_ASSISTANT_DISCOVERY_PREFIX   "homeassistant"  // Home Assistant discovery prefix
#define CMSG_SIZE  512   // General-purpose scratch buffer (webhook, REST API, JSON, MQTT topic render).
                         // All known users need ≤512 bytes.  MQTT autoconfig reads templates
                         // directly from PROGMEM pools (no RAM staging needed on ESP8266).
#define OT_TOPIC_LEN 50  // Shared MQTT topic scratch for OT print_* functions.
#define JSON_BUFF_MAX   1024
#define JSON_ENTRY_BUF   256  // max bytes for a single serialized JSON entry/object
// Replace CSTR macro with overloads to handle both String and char*
// Includes null pointer protection to prevent crashes
inline const char* CSTR(const String& x) { 
  const char* ptr = x.c_str(); 
  return ptr ? ptr : ""; 
}
inline const char* CSTR(const char* x) { return x ? x : ""; }
inline const char* CSTR(char* x) { return x ? x : ""; }

#define CONLINEOFFLINE(x) (x?"online":"offline")
#define CBOOLEAN(x) (x?"true":"false")
#define CONOFF(x) (x?"On":"Off")
#define CCONOFF(x) (x?"ON":"OFF")
#define CBINARY(x) (x?"1":"0")
#define EVALBOOLEAN(x) (strcasecmp(x,"true")==0||strcasecmp(x,"on")==0||strcasecmp(x,"1")==0)
#define ETX ((uint8_t)0x04)

// Per-component type headers (ADR-079 / TASK-326 AC3). Included after the
// #define block above because MQTTtypes.h references HOME_ASSISTANT_DISCOVERY_PREFIX
// and NTPtypes.h references NTP_DEFAULT_TIMEZONE / NTP_HOST_DEFAULT.
#include "Devicetypes.h"
#include "Hardwaretypes.h"
#include "Networktypes.h"
#include "PICtypes.h"
#include "OTBustypes.h"
// MQTTtypes.h merged into MQTTstuff.h (ADR-081). Include MQTTstuff.h here so
// the state/settings structs are visible before OTGWState / OTGWSettings
// reference them. Later .ino files that include MQTTstuff.h get a no-op via
// #pragma once.
#include "MQTTstuff.h"
#include "Flashtypes.h"
// debugStuff.h (formerly Debugtypes.h slot): ADR-081 merged DebugSection
// into debugStuff.h, so we include the whole stuff.h here -- early enough
// that DebugSection is defined before OTGWState references it (line ~255).
// The later #include "debugStuff.h" near the bottom becomes a no-op via
// #pragma once.
#include "debugStuff.h"
#include "Uptimetypes.h"
#include "NTPtypes.h"
#include "Sensorstypes.h"
#include "S0types.h"
#include "Outputstypes.h"
#include "Webhooktypes.h"
#include "UItypes.h"


// Forward declarations for heap monitoring (defined in helperStuff.ino)
enum HeapHealthLevel {
  HEAP_HEALTHY,
  HEAP_LOW,
  HEAP_WARNING,
  HEAP_CRITICAL
};
// Restore-side deadband above HEAP_LOW_THRESHOLD (5120). Used by the discovery
// drip (loopMQTTDiscovery in MQTTstuff.ino) on the ESP8266 path to decide when
// slow-mode may revert to normal-mode. Schmitt-trigger: enter at HEAP_LOW
// (freeHeap<5120), exit only when freeHeap has climbed +1KB above that boundary.
// Sized to cover one discovery alloc footprint (~1KB transient incl. broker-side
// TX buffers) so a single Status-burst + WS pong overlap does not immediately
// tip back across the entry threshold (TASK-553 / TASK-555). Declared here (not
// in helperStuff.ino) because MQTTstuff.ino is concatenated before helperStuff.ino
// in the Arduino sketch build, so a #define in helperStuff would not be visible
// to MQTTstuff. ESP32 path uses a different helper with platform-scaled values.
#define HEAP_LOW_RESTORE_THRESHOLD 6144  // bytes (HEAP_LOW_THRESHOLD + 1024)
HeapHealthLevel getHeapHealth();
uint8_t getHeapFragmentation();
bool canSendWebSocket();
bool canPublishMQTT();
void logHeapStats();
void emergencyHeapRecovery();
// Status-frame burst quiesce (TASK-342): suppress MQTT discovery drip during
// Status sub-topic fanout so allocation peaks do not stack.
// Post-burst cooldown (TASK-347): hold drip for STATUS_BURST_COOLDOWN_MS after
// an endStatusBurst() that had real MQTT traffic, to let lwIP pbufs drain.
void beginStatusBurst();
void endStatusBurst();
bool isStatusBurstActive();
bool dripDueWithinMs(uint32_t windowMs);  // true if drip fires within windowMs ms or is overdue
// isDripDeferred() is internal to MQTTstuff.ino (TASK-362) — single caller in loopMQTTDiscovery.
void incrementStatusBurstPublishCount(); // called by status publishers on each real MQTT send
bool updateLittleFSStatus(const char *probePath = nullptr);
bool updateLittleFSStatus(const __FlashStringHelper *probePath);
bool readLatestCrashLog(char* summary, size_t summarySize, char* details, size_t detailsSize);

//prototype
// ADR-104 (2.0.0 sibling of dev's ADR-076): return true iff the publish reached
// MQTTclient.endPublish() success. Bit/byte/normal slot helpers use the return
// value (or the mqttSendSuccessCount counter for the multi-publish normal-msgId
// path) to commit or discard pending throttle-slot updates so a heap-throttled
// early-return cannot leave a stale pending that the next unrelated publish
// silently commits.
bool sendMQTTData(const char*, const char*, const bool = false);
bool sendMQTTData(const __FlashStringHelper*, const char*, const bool = false);
bool sendMQTTData(const __FlashStringHelper*, const __FlashStringHelper*, const bool = false);
// Monotonic counter — sendMQTTData() increments after a confirmed
// MQTTclient.endPublish() success. OTPublishGate callers use it to detect
// whether *any* downstream publish landed for the current msgId frame, so the
// matching mqttPendingSlot is committed only when the frame actually emitted
// at least one MQTT message. (ADR-104 Decision item 7 / dev TASK-644.)
extern uint32_t mqttSendSuccessCount;
// PIC subtree helper -- prepends kPicSubtreePrefix so the otgw-pic/ subtree
// name has a single source of truth (ADR-065). Used by TASK-390 migrations.
void sendMQTTDataPic(const __FlashStringHelper* label, const char* value);
void sendMQTTDataPic(const __FlashStringHelper* label, const __FlashStringHelper* value);
void publishToSourceTopic(const char*, const char*, byte);
void loopMQTTDiscovery();
// ADR-106: topic-naming-mode cleanup helpers (defined in MQTTstuff.ino).
void armTopicCleanupOnLegacyToggle(bool newUseLegacy);
void runTopicCleanupStep();
void setMQTTConfigPending(const uint8_t MSGid);
void markAllMQTTConfigPending();
const char *messageIDToString(OTLibMessageID message_id);
void addCommandToQueue(const char* ,  int , const bool = false, const int16_t = 1000);
void sendMQTTheapdiag();
// TASK-692 / TASK-693 port (dev TASK-686 / TASK-688): OT-bus support-map
// accessors. State lives at file scope in OTGW-Core.ino; MQTT publisher
// lives in MQTTstuff.ino; LittleFS persistence lives in OTGW-Core.ino.
bool isBoilerMsgIdUnsupportedRead(uint8_t id);
bool isBoilerMsgIdUnsupportedWrite(uint8_t id);
bool isBoilerMsgIdAckedRead(uint8_t id);
bool isBoilerMsgIdAckedWrite(uint8_t id);
bool isThermostatMsgIdSentRead(uint8_t id);
bool isThermostatMsgIdSentWrite(uint8_t id);
bool getBoilerUnsupportedDirty();
void clearBoilerUnsupportedDirty();
void publishBoilerUnsupportedMsgids();    // MQTT retained CSV ("14W,16W,24R,...")
void loadOtSupportFiles();                 // read /ot-thermo.json and /ot-boiler.json at boot
void saveOtSupportFilesIfDirty();          // 15-min debounced atomic write
void doMqttDisconnect();                 // graceful disconnect for reboot path (MQTTclient is file-static)
void doWebSocketClose();                 // close all WS clients before reboot (webSocket not extern'd in any header)
void doRestart(const char* reason);      // canonical reboot path: flushSettings + prepareForReboot + ESP.restart
// MQTT discovery verification (ADR-062, TASK-349): state machine lives in
// mqtt_discovery_verify.cpp as of TASK-363; public API in that file's header.
// startDiscoveryVerification() / isDiscoveryVerificationActive() are
// re-declared here so that callers already transitively including OTGW-firmware.h
// keep compiling; prefer including mqtt_discovery_verify.h directly for new
// callers. endDiscoveryVerification() remains file-static inside the .cpp.
bool     startDiscoveryVerification();
bool     isDiscoveryVerificationActive();
uint16_t countPendingDiscoveryIds();
void     incPublishedTopicCount();    // called by streaming helpers in MQTTHaDiscovery.cpp (ADR-044 shim)
void sendLogToWebSocket(const char* logMessage);

// Forward declarations for functions defined in later .ino files
// (Arduino auto-prototype generation can fail for these)
enum class GPIOConflictCaller : uint8_t {
  Sensor,
  S0,
  Output,
};

enum class StatusMessage : uint8_t {
  None,
  LittleFSMismatch,
  PSModeActive,
};

void readSettings(bool show);
void writeSettings(bool show);
void updateSetting(const char *field, const char *newValue);
bool checkGPIOConflict(int pin, GPIOConflictCaller caller);
const __FlashStringHelper* getStatusMessageText();
void escapeJsonStringTo(const char* src, char* dest, size_t destSize);
void GetVersion(const char* hexfile, char* version, size_t destSize);
void startWebSocket();
void handleWebSocket();
void handleDebug();
void startPICStream();
void stopPICStream();
void applyLegacyPort25238Setting();
void handleOTGWstream();
void testWebhook(bool testOn);
void evalWebhook();
bool checkHttpAuth();  // HTTP Basic Auth guard (ADR-056; defined in restAPI.ino)
extern bool picSettingsCycleActive;  // PIC settings readout cycle flag (OTGW-Core.ino)

// SAT Weather forward declarations — defined in SATweather.ino
void weatherLoop();
void weatherFetch();
void weatherSendStatusJSON();
void weatherPublishMQTT();

// SAT BLE forward declarations — defined in SATble.ino (ESP32 only)
#if defined(ESP32)
void satBLEInit();
void satBLELoop();
void satBLEUpdateState();
float satBLEGetTemperature();
float satBLEGetHumidity();
void satBLEPublishMQTT();
void satBLESendStatusJSON();

// TASK-488 / TASK-492: BLE HA-discovery + per-MAC state-topic helpers
// (defined in MQTTstuff.ino). Caller (satBLEPublishMQTT) wires these in once
// per first-seen MAC (discovery) and on every publish cycle (state) — the
// publish cadence is iBleInterval, the radio scan itself is continuous since
// TASK-494. One-shot discovery via per-slot bDiscoveryPublished flag.
// satBLEMacToCompact is the canonical "AA:BB:..:FF" -> "aabb..ff" helper used
// by both the caller in SATble.ino and internally in MQTTstuff.ino.
// 12 hex chars + NUL — fixed by the BLE MAC layout, not a tunable.
static constexpr size_t BLE_MAC_COMPACT_SIZE = 13;
void satBLEMacToCompact(const char* macWithColons, char* out, size_t outSize);
void satBLEPublishStateTopics(const char* macCompact, float temp, float hum, uint8_t bat, int8_t rssi);
// TASK-493 (1A-H1): returns true only when all 4 retained discovery configs
// were successfully published; caller MUST gate per-MAC `bDiscoveryPublished`
// on this so transient failures retry on the next iBleInterval cycle.
// TASK-508: optional `label` injects a user-friendly name into HA's
// `friendly_name` template. NULL or empty falls back to "BLE <mac>".
bool satBLEPublishHaDiscovery(const char* macCompact, const char* macWithColons,
                              const char* label = nullptr);
// TASK-508: wipe HA retained discovery configs for one MAC by publishing
// 4 zero-byte retained payloads. Called from Forget on the selected slot.
void satBLEUnpublishDiscovery(const char* macCompact);
// TASK-508: roster REST helpers (called from handleSAT in restAPI.ino).
// All run on the Arduino loop task; settings mutations route through
// updateSetting() so the existing 2-s flush debounce persists them.
void satBLERosterSendJSON();
bool satBLERosterSelect(const char* mac);
bool satBLERosterSetLabel(const char* mac, const char* label);
bool satBLERosterForget(const char* mac);
#endif

// SAT (Smart Autotune Thermostat) forward declarations — defined in SATcontrol.ino, SATpid.ino, SATcycles.ino
void initSAT();
void satControlLoop();
void satPublishMQTT();
bool satHandleExternalTemp(const char* value);
bool satHandleExternalOutdoor(const char* value);
bool satHandleTargetTemp(const char* value);
bool satHandleZoneRoomTemp(uint8_t zone, const char* value);
bool satHandleZoneSetpoint(uint8_t zone, const char* value);
void satHandleEnabled(const char* value);
void satHandleHeatingMode(const char* value);
void satDisable();
void satHandleControlMode(const char* value);
void satCycleOnFlameChange(bool flameOn);
void satSendStatusJSON();
uint32_t satCycleGetFlameOnStartMs();
uint32_t satCycleGetFlameOffStartMs();
bool    satCycleIsHourLimitReached();
uint8_t satCycleGetCyclesThisHour();
void satHCRSaveState();
void satHCRLoadState();
void satHCRReset();
void satHCRAddSample();
const char* satHeatingCurveRecommendation();
// Cycle window persistence and flush (Task #237, defined in SATcycles.ino / SATcontrol.ino)
void satSaveCycleWindow();
void satLoadCycleWindow();
void satFlushCycleWindow();
void satFlushShortLivedData();

// HardwareSection + NetworkSection + enums moved to Hardwaretypes.h / Networktypes.h (ADR-079/TASK-326 AC3)


// PIC / OTBus / MQTT runtime / Flash / Debug / Uptime / PicSettings state structs moved per ADR-079/TASK-326 AC3

// --- SAT enums + SATWindowRecord + SATZoneState + SATRuntimeSection moved to state_sat.h (ADR-079/TASK-326)

// Verify-pass outcome classification (TASK-361). Replaces the earlier hack of
// writing verifyReceivedCount=expected on heap-abort to suppress the false-
// missing republish, which also lied to telemetry. With this enum the heap-
// abort and disconnect paths can honestly report what happened while still
// suppressing republish only when the outcome is ABORTED_* (not when CLEAN).
enum class VerifyOutcome : uint8_t {
  UNKNOWN = 0,            // no verify completed yet
  CLEAN,                  // verify closed with receivedCount >= expected, no missing
  MISSING,                // verify closed with missingCount > 0, republish triggered
  ABORTED_HEAP,           // heap dropped below VERIFICATION_MIN_HEAP_ABORT during window
  ABORTED_DISCONNECT      // MQTT disconnected during window
};

struct DiscoverySection {                    // state.discovery — MQTT auto-discovery verify telemetry (ADR-062)
  uint32_t iLastVerifyEpoch         = 0;     // unix-epoch of last endVerify (0 = never)
  uint32_t iVerifyRunCount          = 0;     // lifetime verify-start counter
  uint32_t iRepublishTriggeredCount = 0;     // lifetime count where missing>0 → markAllMQTTConfigPending
  uint32_t iPublishedTopicCount     = 0;     // running counter incremented by stream helpers after endPublish
  uint16_t iLastMissingCount        = 0;     // last run: expected - received
  uint16_t iLastOrphanCount         = 0;     // last run: foreign-nodeId retained configs observed
  VerifyOutcome eLastOutcome        = VerifyOutcome::UNKNOWN;  // TASK-361: honest outcome label for last verify pass
  // Active-window indicator is exposed via isDiscoveryVerificationActive()
  // reading the MQTTstuff.ino static-local verifyActive flag — single source
  // of truth. Do not add a mirror bool here (was removed in TASK-362).
};

// NOTE: this struct is NOT authoritative for the retained otgw-firmware/stats/*
// MQTT topics. sendMQTTheapdiag() publishes 17 individual retained topics: 8
// sourced from this struct, 3 live values (ESP.getFreeHeap / getMaxFreeBlockSize
// / getHeapFragmentation), and 6 from state.discovery (verify_runs /
// republish_triggered / last_missing / last_orphan / published_topics /
// last_verify_epoch). Adding a field here does NOT automatically surface on MQTT
// — add a corresponding publishStatU32(F("otgw-firmware/stats/...")) call in
// sendMQTTheapdiag().
struct HeapDiagSection {                 // state.heapdiag — cumulative heap-pressure diagnostics (reset on reboot)
  uint32_t iWsDropsTotal            = 0; // lifetime WebSocket messages dropped due to heap pressure
  uint32_t iMqttDropsTotal          = 0; // lifetime MQTT messages dropped due to heap pressure
  uint32_t iEnteredLowCount         = 0; // transitions into HEAP_LOW tier (from HEALTHY)
  uint32_t iEnteredWarningCount     = 0; // transitions into HEAP_WARNING tier
  uint32_t iEnteredCriticalCount    = 0; // transitions into HEAP_CRITICAL tier
  uint32_t iDripActiveBurstSkipCount = 0; // drip ticks skipped DURING active Status-burst (TASK-342)
  uint32_t iDripCooldownSkipCount   = 0; // drip ticks skipped in post-burst cooldown window (TASK-347)
  uint32_t iDripSlowModeCount       = 0; // transitions to 10s slow-mode due to heap pressure
};

enum RestPerfTarget : uint8_t {
  REST_PERF_NONE = 0,
  REST_PERF_SAT_STATUS,
  REST_PERF_DEVICE_INFO,
  REST_PERF_SETTINGS
};

struct RestPerfSample {
  uint32_t iLastTotalMs     = 0;
  uint32_t iLastSendMs      = 0;
  uint32_t iLastRenderMs    = 0;
  uint32_t iLastChunkCount  = 0;
  uint32_t iMaxTotalMs      = 0;
  uint32_t iSampleCount     = 0;
};

struct RestPerfSection {
  RestPerfSample satStatus;
  RestPerfSample deviceInfo;
  RestPerfSample settings;
  RestPerfTarget eActiveTarget = REST_PERF_NONE;
  uint32_t       iActiveSendMs = 0;
  uint32_t       iActiveChunkCount = 0;
};


struct OTGWState {
  HardwareSection    hw;          // state.hw.eMode, state.hw.bOLEDPresent
  NetworkSection     net;         // state.net (always present; fields vary by platform/build)
  PICSection         pic;         // state.pic.bAvailable, state.pic.sFwversion
#if defined(HAS_DIRECT_OT) && HAS_DIRECT_OT
  OTDirectSection    otd;         // state.otd — OT-direct schedule/override stats (OTGW32)
#endif
  OTBusState       otBus;       // state.otBus.bOnline, state.otBus.bBoilerState
  MQTTRuntimeSection mqtt;        // state.mqtt.bConnected
  FlashSection       flash;       // state.flash.bESPactive, state.flash.iPICprogress
  DebugSection       debug;       // state.debug.bOTmsg, state.debug.bMQTT
  UptimeSection      uptime;      // state.uptime.iSeconds, state.uptime.iRebootCount
  HeapDiagSection    heapdiag;    // state.heapdiag.iMqttDropsTotal, ...
  RestPerfSection    restperf;    // state.restperf — last REST timing breakdown for profiled endpoints
  DiscoverySection   discovery;   // state.discovery.iPublishedTopicCount, ... (ADR-062)
  PicSettingsSection picSettings; // state.picSettings — PR=-polled settings from PIC
  SATRuntimeSection  sat;         // state.sat — SAT thermostat controller
  StatusMessage      statusMessage = StatusMessage::None;
  bool               bSetupComplete = false;
};

OTGWState state;

// Central PIC availability guard — returns true when a PIC is available.
// Set at boot by detectPIC() and can flip true at runtime if a PIC banner is received.
// All PIC-related operations (commands, queries, upgrades) check this before proceeding.
inline bool isPICEnabled() {
#if HAS_PIC
  return state.pic.bAvailable;
#else
  return false;  // compiled out on OTGW32
#endif
}

// Returns true when OT-direct hardware is active (OTGW32 with working OT bus).
inline bool isOTDirectEnabled() {
#if HAS_DIRECT_OT
  return state.hw.eMode == HW_MODE_OT_DIRECT;
#else
  return false;  // compiled out on PIC boards
#endif
}

inline bool hasOTCommandInterface() {
  return isPICEnabled() || isOTDirectEnabled();
}

// Returns a PROGMEM string describing the hardware mode for display/MQTT/REST.
inline const __FlashStringHelper* hardwareModeName() {
  switch (state.hw.eMode) {
    case HW_MODE_PIC:        return F("PIC");
    case HW_MODE_OT_DIRECT:  return F("OT-Direct");
    case HW_MODE_DEGRADED:   return F("Degraded");
    default:                 return F("Unknown");
  }
}

// Returns a PROGMEM string describing the active network transport.
inline const __FlashStringHelper* networkModeName() {
#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
  return (state.net.eMode == NET_ETHERNET) ? F("Ethernet") : F("WiFi");
#else
  return F("WiFi");
#endif
}

//===================[ Unified network helpers ]===================
// Single API for checking connectivity and retrieving IP/MAC regardless
// of whether WiFi or Ethernet is the active transport.
// On ESP8266 the Ethernet branches compile away entirely (HAS_ETH_CAPABLE=0).

inline bool isNetworkUp() {
#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
  if (state.net.eMode == NET_ETHERNET) return true;  // Ethernet: link was verified by loopEthernet()
#endif
#if defined(_VERSION_PRERELEASE)
  if (state.net.bAPFallback) return true;  // BETA: SoftAP counts as network-up for web/telnet
#endif
  return (WiFi.status() == WL_CONNECTED);
}

// Returns true only when a real WiFi or Ethernet connection is available.
// Use this (not isNetworkUp) to gate services that need internet/LAN: MQTT, NTP.
inline bool isWiFiConnected() {
#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
  if (state.net.eMode == NET_ETHERNET) return true;
#endif
  return (WiFi.status() == WL_CONNECTED);
}

// Forward declarations for Ethernet.ino helpers (defined after this header)
#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
String getEthernetIPString();
String getEthernetMACString();
#endif

inline String getActiveIP() {
#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
  if (state.net.eMode == NET_ETHERNET) return getEthernetIPString();
#endif
  return WiFi.localIP().toString();
}

inline String getActiveMAC() {
#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
  if (state.net.eMode == NET_ETHERNET) return getEthernetMACString();
#endif
  return WiFi.macAddress();
}

// Returns a PROGMEM string describing the board variant.
inline const __FlashStringHelper* boardName() {
#if defined(BOARD_NODOSHOP_ESP8266)
  return F("Nodoshop OTGW (ESP8266)");
#elif defined(BOARD_NODOSHOP_ESP32)
  return F("Nodoshop OTGW32 (ESP32-S3)");
#elif defined(BOARD_SEEGEL_OTTHING)
  return F("OT-Thing Seegel (ESP32-S3)");
#else
  return F("Unknown board");
#endif
}

#if HAS_PIC
inline bool isGatewayFirmware() { return strcmp_P(state.pic.sType, PSTR("gateway")) == 0; }
#else
inline bool isGatewayFirmware() { return false; }
#endif

//===================[ Persistent Settings — serialized to LittleFS (ADR-051) ]===================
// Sub-section structs for OTGWSettings — groups configuration by feature area.
// Hungarian prefixes: b=bool, s=string/char[], i=int/uint, f=float

// MQTTSettingsSection moved to MQTTstuff.h (ADR-079/TASK-326 AC3; merged per ADR-081)
// NOTE: settings.mqtt.bDiscoveryAutoVerify (ADR-062/TASK-349/351) is referenced by
// restAPI.ino, settingStuff.ino, and OTGW-firmware.ino. The field must be added to
// MQTTSettingsSection in MQTTstuff.h for the tree to compile — this is a cross-file
// concern owned by the MQTT implementer, not resolvable from this header.

// NTPSection moved to NTPtypes.h (ADR-079/TASK-326 AC3)

// SensorsSection moved to Sensorstypes.h (ADR-079/TASK-326 AC3)

// S0Section moved to S0types.h (ADR-079/TASK-326 AC3)

// OutputsSection moved to Outputstypes.h (ADR-079/TASK-326 AC3)

// WebhookSection moved to Webhooktypes.h (ADR-079/TASK-326 AC3)

// UISection moved to UItypes.h (ADR-079/TASK-326 AC3)

// PICBootSection moved to PICtypes.h (ADR-079/TASK-326 AC3)

//--- SAT (Smart Autotune Thermostat) settings ---
// Ported from SAT releases/thermo-nova (https://github.com/Alexwijn/SAT)
// with permission from the SAT authors.
// SATSection moved to settings_sat.h (ADR-079/TASK-326)

// OTDirectSettingsSection moved to OTDirecttypes.h (ADR-079/TASK-326)

// EthernetSection moved to Networktypes.h (ADR-079/TASK-326 AC3)


// DeviceSection moved to Devicetypes.h (ADR-079/TASK-326 AC3)

struct OTGWSettings {
  // Device-level fields (universal device identity)
  char sHostname[41] = _HOSTNAME;
  char sHTTPpasswd[41] = "";  // HTTP Basic Auth password (empty = no authentication required)
  bool bLEDblink     = true;
  bool bDarkTheme    = false;
  bool bMyDEBUG      = false;
  bool bNightlyRestart = false;  // scheduled daily restart for heap recovery
  uint8_t iRestartHour = 4;     // hour (0-23) for nightly restart

  // Named sub-sections — access as settings.mqtt.sBroker, settings.ntp.sTimezone, etc.
  DeviceSection       device;
  MQTTSettingsSection mqtt;
  NTPSection          ntp;
  SensorsSection      sensors;
  S0Section           s0;
  OutputsSection      outputs;
  WebhookSection      webhook;
  UISection           ui;
  PICBootSection     picBoot;
  SATSection          sat;
#if defined(HAS_DIRECT_OT) && HAS_DIRECT_OT
  OTDirectSettingsSection otd;
#endif
#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
  EthernetSection     eth;
#endif
};

OTGWSettings settings;

//===================[ Global variables — not part of settings or state ]===================
WiFiClient  wifiClient;
char        cMsg[CMSG_SIZE];
char        otTopic[OT_TOPIC_LEN];  // Shared MQTT topic scratch for OT print_* functions (sequential, not re-entrant)
char        lastReset[129] = "";
uint32_t    MQTTautoConfigMap[8] = { 0 };
uint32_t    MQTTautoCfgPendingMap[8] = { 0 };  // bitmap for async MQTT discovery drip
// Deferred settings write timer (Finding #23: coalesce flash writes)
uint32_t  timerFlushSettings_interval = 2000;  // 2 second debounce
uint32_t  timerFlushSettings_due = 0;          // initially not due
byte      timerFlushSettings_type = 0;         // SKIP_MISSED_TICKS

// Helper inline function to check if any firmware flash is in progress
inline bool isFlashing() {
  return state.flash.bESPactive || state.flash.bPICactive;
}

//Use acetime
using namespace ace_time;
static ExtendedZoneProcessor tzProcessor;
static const int CACHE_SIZE = 3;
static ExtendedZoneProcessorCache<CACHE_SIZE> zoneProcessorCache;
static ExtendedZoneManager timezoneManager(
  zonedbx::kZoneAndLinkRegistrySize,
  zonedbx::kZoneAndLinkRegistry,
  zoneProcessorCache);

const char *weekDayName[]  {  "Unknown", "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Unknown" };
const char *flashMode[]    { "QIO", "QOUT", "DIO", "DOUT", "Unknown" };

//===================[ Module-local variables with extern declarations ]===================
// Dallas sensor variables — definitions in sensors_ext.ino
byte      OTGWdallasdataid = 246;                // foney dataid for temp sensor autoconfigure
int       DallasrealDeviceCount = 0;             // Total temperature devices found on the bus
bool      bSensorsDetected = false;              // Runtime: true when sensors initialized this boot
#define   MAXDALLASDEVICES 16                    // maximum number of devices on the bus

// Define structure to store temperature device addresses found on bus with their latest tempC value
struct
{
  int id;
  DeviceAddress addr;
  float tempC;
  time_t lasttime;
} DallasrealDevice[MAXDALLASDEVICES];
// prototype to allow use in restAPI.ino
char* getDallasAddress(DeviceAddress deviceAddress);
// forward declarations — defined in sensors_ext.ino (concatenated after handleDebug.ino)
void pollSensors();
// forward declarations — defined in s0PulseCount.ino (concatenated after OTGW-firmware.ino)
void initS0Count();
void sendS0Counters();

// S0 Counter variables — definitions here, used across modules
uint16_t  OTGWs0pulseCount;                       // Number of S0 pulses in measurement interval
uint32_t  OTGWs0pulseCountTot = 0;                // Number of S0 pulses since start of measurement
float     OTGWs0powerkw = 0.0f ;                  // Calculated kW actual consumption
time_t    OTGWs0lasttime = 0;                     // Last time S0 counters have been read
byte      OTGWs0dataid = 245;                     // foney dataid for counter autoconfigure

// Heap & discovery statistics (TASK-346): faux dataid used to anchor HA discovery
// configs for the 17 retained otgw-firmware/stats/* topics. Not an OT message ID;
// MQTTautoCfgPendingMap tracks this entry like any other in markAllMQTTConfigPending().
byte      OTGWheapstatsid = 247;                  // foney dataid for heap-stats autoconfigure

// Diagnostic discovery pseudo-IDs (TASK-540 / TASK-541):
//   248 — otgw-firmware/{reboot_count,reboot_reason,version,hostname}
//   249 — otgw-pic/{version,deviceid,firmwaretype,designer,picavailable} (PIC-gated)
//   250 — otgw-pic/settings/* (15 PR=-polled topics, PIC-gated)
//   251 — 2.0.0-specific diagnostics: otgw32/flame_*, sat/ble_* health, sat/ch_pressure_status
//   252 — TASK-543 SAT control/PID/cycle/statistics primaries
//   253 — TASK-543 SAT BLE + pressure + weather primaries
//   254 — TASK-543 SAT binary primaries + flame status
//   255 — TASK-543 dynamic SAT zone discovery
//
// TASK-543 gating decision: publish discovery unconditionally on the dual-target
// 2.0.0 branch. Runtime/platform publish paths determine whether an entity has
// live data (for example ESP32-only weather/BLE fields on ESP8266 stay
// unavailable instead of disappearing from discovery).
// 244 — resetgateway button + gpioa/gpiob/leda-f select entities (PIC control;
//       discovery unconditional per the TASK-543 gating decision above)
byte      OTGWpiccontrolsid  = 244;
byte      OTGWfwinfoid       = 248;
byte      OTGWpicinfoid      = 249;
byte      OTGWpicsettingsid  = 250;
byte      OTGWdiag200id      = 251;
byte      OTGWsatcoreid      = 252;
byte      OTGWsatweatherid   = 253;
byte      OTGWsatbinaryid    = 254;
byte      OTGWsatzoneid      = 255;
uint8_t   satGetMaxZones();
bool      satShouldDiscoverZone(uint8_t zoneIndex);

// Forward declarations for helperStuff.ino functions called from template
// instantiations in OTGW-ModUpdateServer-impl.h (pulled in via networkStuff.h).
// Without these, GCC 4.8.2 (Arduino Core 2.7.4 toolchain) reports
// "'foo' was not declared in this scope" at template-instantiation time because
// the template uses these names before their actual definitions are parsed.
// GCC 10.3 (Core 3.1.2 toolchain) resolved this via ADL/2-phase lookup; 4.8.2
// does not. Keep these in sync with helperStuff.ino definitions.
void logBootSignature(const char *phase);          // one-line boot/runtime signature for field diagnostics
void requestDeferredReboot(const char *reason);    // mark reboot pending; actual reset fires from loop() on next tick
void performDeferredReboot();                      // called by loop() when g_rebootPending && !isFlashing()
bool isRebootPending();                            // true when a deferred reboot is queued
void rebootHeapWatermarkTick();                    // update min-free-heap watermark; called from loop()
uint32_t getMinFreeHeap();                         // read current heap watermark (wraps platformMinFreeHeap)
void maybeWarnFlashMismatch();                     // one-shot flash-config sanity check at boot

//Now load Debug & network library
#include "debugStuff.h"
#include "networkStuff.h"
#include "helperStuff.h"


    // That's all folks...

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

#endif // OTGW_FIRMWARE_H
