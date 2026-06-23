/* 
***************************************************************************  
**  Program  : OTGW-firmware.h
**  Version  : v1.7.0-beta.34
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

#ifndef OTGW_FIRMWARE_H
#define OTGW_FIRMWARE_H

#include <Arduino.h>

// strlcpy_P: copy a PROGMEM string into a RAM buffer, return source length.
// Standard strlcpy semantics: copies at most (size-1) chars, always NUL-terminates.
// Provided here because ESP8266 Arduino 3.x newlib does not expose strlcpy_P;
// the #ifndef guard keeps it from conflicting if a future core adds it back.
#ifndef strlcpy_P
inline size_t strlcpy_P(char *dst, PGM_P src, size_t size) {
  size_t srcLen = strlen_P(src);
  if (size > 0) {
    size_t n = (srcLen < size - 1) ? srcLen : (size - 1);
    memcpy_P(dst, src, n);
    dst[n] = '\0';
  }
  return srcLen;
}
#endif

#include <AceTime.h>
// #include <TimeLib.h>

// DEBUGGING: Uncomment the next line to disable WebSocket functionality
// #define DISABLE_WEBSOCKET

#include <SimpleTelnet.h>       // https://github.com/RvdB/SimpleTelnet — unified multi-client telnet (replaces TelnetStream + ESPTelnet)
extern SimpleTelnet<1> debugTelnet;   // defined in networkStuff.ino
#include "Wire.h"
#include "safeTimers.h"
#include <OTGWSerial.h>         // Schelte Bron's Serial class - it upgrades and more
#include "OTGW-Core.h"          // Core code for this firmware 
#include <OneWire.h>            // required for Dallas sensor library
#include <DallasTemperature.h>  // Miles Burton's - Arduino Dallas library

//OTGW Nodoshop hardware definitions
#define I2CSCL D1
#define I2CSDA D2
#define BUTTON D3
#define PICRST D5

#define LED1 D4
#define LED2 D0

#define PICFIRMWARE "/gateway.hex"

OTGWSerial OTGWSerial(PICRST, LED2);
void fwupgradestart(const char *hexfile);
void handlePendingUpgrade();

void blinkLEDnow();
void blinkLEDnow(uint8_t);
void setLed(int8_t, uint8_t);

//Defaults and macro definitions
#define _HOSTNAME       "OTGW"
#define SETTINGS_FILE         "/settings.ini"
#define NTP_DEFAULT_TIMEZONE "Europe/Amsterdam"
#define NTP_HOST_DEFAULT "pool.ntp.org"
#define NTP_RESYNC_TIME 1800 //seconds = every 30 minutes
#define HOME_ASSISTANT_DISCOVERY_PREFIX   "homeassistant"  // Home Assistant discovery prefix
#define MQTT_DEFAULT_PUBLISH_INTERVAL_SEC 60
#define CMSG_SIZE  512   // General-purpose scratch buffer (webhook, REST API, JSON, MQTT topic render).
                         // All known users need ≤512 bytes.  MQTT autoconfig reads templates
                         // directly from PROGMEM pools (no RAM staging needed on ESP8266).
#define OT_TOPIC_LEN 40  // Shared MQTT topic scratch; only ever holds an OT label (messageIDToString -> OTlookupitem.label[40], <=39 chars) (was 50)
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

// Forward declarations for heap monitoring (defined in helperStuff.ino)
enum HeapHealthLevel {
  HEAP_HEALTHY,
  HEAP_LOW,
  HEAP_WARNING,
  HEAP_CRITICAL
};
// Restore-side deadband above HEAP_LOW_THRESHOLD (5120). Used by the discovery
// drip (loopMQTTDiscovery in MQTTstuff.ino) to decide when slow-mode may revert
// to normal-mode. Schmitt-trigger pattern: enter slow-mode at HEAP_LOW
// (freeHeap<5120), exit slow-mode only when freeHeap has climbed +1KB above
// that boundary. Sized to cover one discovery alloc footprint (~1KB transient
// incl. broker-side TX buffers) so a single Status-burst + WS pong overlap
// does not immediately tip the heap back across the entry threshold (TASK-553).
// Declared here (not in helperStuff.ino) because MQTTstuff.ino is concatenated
// before helperStuff.ino in the Arduino sketch build, so a #define in
// helperStuff would not be visible to MQTTstuff.
#define HEAP_LOW_RESTORE_THRESHOLD 6144  // bytes (HEAP_LOW_THRESHOLD + 1024)
// Minimum largest-contiguous-block required before attempting an MQTT/WS publish
// or a PubSubClient/lwIP allocation. umm_malloc has no compaction: free heap can
// look adequate while the largest block is too small for the next contiguous
// alloc, which then returns NULL and (unchecked) faults as a StoreProhibited.
// Gating on maxBlock converts that crash into a logged graceful skip. Matched to
// HEAP_FRAG_PROMOTE_MAXBLOCK (1536) for coherence with the tier-promote rule.
// Declared here (not helperStuff.ino) so MQTTstuff.ino, concatenated earlier, sees it.
#define MQTT_PUBLISH_MIN_MAXBLOCK 1536   // bytes
// HTTP serving needs a bigger contiguous block than MQTT: ESP8266WebServer's
// streamFile() path (BufferedStreamDataSource::get_buffer in the core) does an
// UNCHECKED `new uint8_t[~1460]` per TCP segment; on Core 2.7.4 -fno-exceptions
// that returns NULL under fragmentation and the following memcpy/readBytes faults
// (StoreProhibited, ROM memcpy 0x4000df64, excvaddr=0 — confirmed root cause).
// Gate the HTTP-serve path above that ~1460 cliff with margin for mid-request dips.
// Field calibration: beta.6 served 521 requests fine at maxBlock floor 1944, so the
// gate sits just above the cliff (2048), not so high it refuses healthy serving.
#define HTTP_SERVE_MIN_MAXBLOCK 2048     // bytes
HeapHealthLevel getHeapHealth();
uint8_t getHeapFragmentation();
bool canSendWebSocket();
bool canPublishMQTT();
bool canServeHttp();
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
// ADR-076: return true iff the publish reached MQTTclient.endPublish() success.
// Bit/byte/normal slot helpers use the return value (or the
// mqttSendSuccessCount counter for the multi-publish normal-msgId path) to
// commit or discard pending throttle-slot updates so a heap-throttled
// early-return cannot leave a stale pending that the next unrelated publish
// silently commits.
bool sendMQTTData(const char*, const char*, const bool = false);
bool sendMQTTData(const __FlashStringHelper*, const char*, const bool = false);
bool sendMQTTData(const __FlashStringHelper*, const __FlashStringHelper*, const bool = false);
// Monotonic counter — sendMQTTData() increments after a confirmed
// MQTTclient.endPublish() success. Callers that gate-and-publish under an
// OTPublishGate use it to detect whether *any* downstream publish landed for
// the current msgId frame, so the matching mqttPendingSlot is committed only
// when the frame actually emitted at least one MQTT message. (ADR-076 pattern,
// extended to mqttPendingSlot in TASK-644.)
extern uint32_t mqttSendSuccessCount;
// PIC subtree helper -- prepends kPicSubtreePrefix so the otgw-pic/ subtree
// name has a single source of truth (ADR-065). Used by TASK-390 migrations.
void sendMQTTDataPic(const __FlashStringHelper* label, const char* value);
void sendMQTTDataPic(const __FlashStringHelper* label, const __FlashStringHelper* value);
void publishToSourceTopic(const char*, const char*, byte);
void loopMQTTDiscovery();
void sendMQTTheapdiag();
// TASK-686 / TASK-688: support-map accessors (state lives in OTGW-Core.ino at file scope).
bool isBoilerMsgIdUnsupportedRead(uint8_t id);
bool isBoilerMsgIdUnsupportedWrite(uint8_t id);
bool isBoilerMsgIdAckedRead(uint8_t id);
bool isBoilerMsgIdAckedWrite(uint8_t id);
bool isThermostatMsgIdSentRead(uint8_t id);
bool isThermostatMsgIdSentWrite(uint8_t id);
bool getBoilerUnsupportedDirty();
void clearBoilerUnsupportedDirty();
void publishBoilerUnsupportedMsgids();     // TASK-686: MQTT retained CSV ("14W,16W,24R,...")
void loadOtSupportFiles();                  // TASK-688: read /ot-thermo.json and /ot-boiler.json at boot
void saveOtSupportFilesIfDirty();           // TASK-688: 15-min debounced atomic write
void doMqttDisconnect();                 // graceful disconnect for reboot path (MQTTclient is file-static)
void doWebSocketClose();                 // close all WS clients before reboot (webSocket not extern'd in any header)
void doRestart(const char* reason);      // canonical reboot path: flushSettings + prepareForReboot + ESP.restart
void logBootSignature(const char *phase); // one-line boot/runtime signature for field diagnostics (TASK-395)
// Reboot-process instrumentation helpers (TASK-396)
void requestDeferredReboot(const char *reason); // mark reboot pending; actual reset fires from loop() on next tick
void performDeferredReboot();                   // called by loop() when g_rebootPending && !isFlashing()
bool isRebootPending();                         // true when a deferred reboot is queued
void rebootHeapWatermarkTick();                 // update g_minFreeHeap; called from loop()
uint32_t getMinFreeHeap();                      // read current heap watermark
void maybeWarnFlashMismatch();                  // one-shot flash-config sanity check at boot
// MQTT discovery verification (ADR-062, TASK-349): state machine lives in
// mqtt_discovery_verify.cpp as of TASK-363; public API in that file's header.
// startDiscoveryVerification() / isDiscoveryVerificationActive() are
// re-declared here so that callers already transitively including OTGW-firmware.h
// keep compiling; prefer including mqtt_discovery_verify.h directly for new
// callers. endDiscoveryVerification() remains file-static inside the .cpp.
bool     startDiscoveryVerification();
bool     isDiscoveryVerificationActive();
uint16_t countPendingDiscoveryIds();
void     incPublishedTopicCount();    // called by streaming helpers in mqtt_configuratie.cpp (ADR-044 shim)
void addOTWGcmdtoqueue(const char* ,  int , const bool = false, const int16_t = 1000);
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
bool writeSettings(bool show);
void updateSetting(const char *field, const char *newValue);
bool checkGPIOConflict(int pin, GPIOConflictCaller caller);
const __FlashStringHelper* getStatusMessageText();
void escapeJsonStringTo(const char* src, char* dest, size_t destSize);
void GetVersion(const char* hexfile, char* version, size_t destSize);
void startWebSocket();
void handleWebSocket();
void testWebhook(bool testOn);
void evalWebhook();
bool checkHttpAuth();  // HTTP Basic Auth guard (ADR-054; defined in restAPI.ino)
extern bool picSettingsCycleActive;  // PIC settings readout cycle flag (OTGW-Core.ino)


//===================[ Runtime State — transient, never persisted (ADR-051) ]===================
// Sub-section structs for OTGWState — groups runtime state by system component.
// Hungarian prefixes: b=bool, s=string/char[], i=int/uint, f=float

struct PICSection {            // state.pic — PIC microcontroller identity/status
  bool bAvailable     = false;           // was bPICavailable
  char sFwversion[32] = "no pic found";  // was sPICfwversion
  char sDeviceid[32]  = "no pic found";  // was sPICdeviceid
  char sType[32]      = "no pic found";  // was sPICtype
};

struct OTGWProtocol {          // state.otgw — OpenTherm protocol & bus state
  bool bOnline           = false;  // was bOTGWonline — serial link alive
  bool bPSmode           = false;  // was bPSmode — Print Summary mode (PS=1)
  bool bGatewayMode      = false;  // was bOTGWgatewaystate — true=gateway, false=monitor
  bool bGatewayModeKnown = false;  // was bOTGWgatewaystateKnown
  bool bBoilerState      = false;  // was bOTGWboilerstate — CH/boiler active
  bool bThermostatState  = false;  // was bOTGWthermostatstate
};

struct MQTTRuntimeSection {    // state.mqtt — MQTT broker connection state
  bool     bConnected       = false;  // was statusMQTTconnection
  uint32_t iLastConnectedMs = 0;      // millis() of last confirmed live connection; 0 = never connected
};

struct FlashSection {          // state.flash — Firmware upgrade operations
  bool bESPactive        = false;  // was isESPFlashing
  bool bPICactive        = false;  // was isPICFlashing
  char sError[48]        = "";     // PIC-flash error msg; longest literal 40 chars, snprintf_P(sizeof) truncates safe (was 129)
  char sPICfile[65]      = "";     // was currentPICFlashFile
  int8_t iPICprogress    = 0;      // PIC flash percent (-1 error .. 100); int8 ample (was int)
};

struct DebugSection {          // state.debug — Runtime diagnostic output flags
  bool     bOTmsg                 = true;   // was bDebugOTmsg — OpenTherm message trace
  bool     bRestAPI               = false;  // was bDebugRestAPI — REST API request trace
  bool     bMQTT                  = false;  // was bDebugMQTT — MQTT communication trace (connect/send/receive)
  bool     bMQTTGate              = false;  // MQTT interval gating decisions (high volume)
  bool     bSensors               = false;  // was bDebugSensors — Dallas sensor scan trace
  bool     bNTP                   = true;   // NTP time sync telemetry (on by default for diagnostics)
  bool     bSensorSim             = false;  // was bDebugSensorSimulation
  bool     bOTGWSimulation        = false;  // was bDebugOTGWSimulation
  uint32_t iOTGWSimulationIntervalMs = 750;
  uint32_t iOTGWSimulationNextDueMs  = 0;
};

struct UptimeSection {         // state.uptime — System longevity counters
  uint32_t iSeconds      = 0;  // was upTimeSeconds
  uint32_t iRebootCount  = 0;  // was rebootCount
};

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
  uint16_t iVerifyRunCount          = 0;     // per-boot verify-start counter (display-only; u16 wrap cosmetic)
  uint16_t iRepublishTriggeredCount = 0;     // per-boot republish-trigger counter (display-only; u16 wrap cosmetic)
  uint16_t iPublishedTopicCount     = 0;     // topics published; verify already casts (uint16_t) so u16 is behavior-identical
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
struct HeapDiagSection {                 // state.heapdiag — per-boot heap-pressure diagnostics (reset on reboot; display-only, u16 wrap cosmetic)
  uint16_t iWsDropsTotal            = 0; // per-boot WebSocket messages dropped due to heap pressure
  uint16_t iMqttDropsTotal          = 0; // per-boot MQTT messages dropped due to heap pressure
  uint16_t iMqttMaxBlockSkips       = 0; // MQTT publishes skipped by the maxBlock pre-flight gate (fragmentation guard)
  uint16_t iWsMaxBlockSkips         = 0; // WebSocket sends skipped by the maxBlock pre-flight gate (fragmentation guard)
  uint16_t iHttpFragSkips           = 0; // httpServer.handleClient() ticks skipped by the maxBlock gate (HTTP-load fragmentation guard)
  uint16_t iEnteredLowCount         = 0; // transitions into HEAP_LOW tier (from HEALTHY)
  uint16_t iEnteredWarningCount     = 0; // transitions into HEAP_WARNING tier
  uint16_t iEnteredCriticalCount    = 0; // transitions into HEAP_CRITICAL tier
  uint16_t iDripActiveBurstSkipCount = 0; // drip ticks skipped DURING active Status-burst (TASK-342)
  uint16_t iDripCooldownSkipCount   = 0; // drip ticks skipped in post-burst cooldown window (TASK-347)
  uint16_t iDripSlowModeCount       = 0; // transitions to 10s slow-mode due to heap pressure
};

struct PicSettingsSection {    // state.picSettings — settings polled from PIC via PR= commands
  // Source: Schelte Bron's OTGW firmware documentation (https://otgw.tclcode.com/firmware.html)
  // PR=A (About/version) is handled by getpicfwversion(); PR=M (mode) by queryOTGWgatewaymode().
  // All other PR= reports are polled on-demand by queryNextPICsetting(), one per 3s tick.

  // --- Active settings (most useful for HA integration) ---
  char sSetpointOverride[8]   = "";  // PR=O: setpoint override ("T20.5"/"C20.5"/"N", max ~6); strlcpy(sizeof) bounds (was 16)
  char sSetback[8]            = "";  // PR=S: setback temp (e.g. "15.0", max ~5); strlcpy(sizeof) bounds (was 16)
  char sDhwOverride[8]        = "";  // PR=W: DHW/hot-water override ("0"=off, "1"=on, "A"=auto)

  // --- Hardware configuration ---
  char sGpio[8]               = "";  // PR=G: GPIO A+B function codes (two digits, e.g. "05")
  char sGpioStates[8]         = "";  // PR=I: GPIO A+B current input states (two digits, e.g. "00")
  char sLed[8]                = "";  // PR=L: LED A–F function chars (six chars, e.g. "RFFTTT")
  char sTweaks[8]             = "";  // PR=T: tweaks (two chars: ignore_transitions + ovrd_high_byte)
  char sTempSensor[4]         = "";  // PR=D: external temp sensor function ("O"=outside, "R"=return; v5+ only)
  char sSmartPower[16]        = "";  // PR=P: smart power mode ("L"/"Low power", "M"/"Medium power", "H"/"High power", "N"/"Normal power")
  char sThermostatDetect[8]   = "";  // PR=R: thermostat detection setting

  // --- Diagnostics ---
  char sBuilddate[24]         = "";  // PR=B: firmware build date/time (e.g. "17:52 12-03-2023")
  char sClockMHz[8]           = "";  // PR=C: PIC clock speed in MHz (e.g. "4", "4 MHz")
  char sResetCause[4]         = "";  // PR=Q: last reset cause ("W"=watchdog, "B"=brownout, "P"=power-on)
  char sStandaloneInterval[8] = "";  // PR=N: message interval in standalone mode (seconds)
  char sVoltageRef[4]         = "";  // PR=V: voltage reference setting (numeric)
};


struct OTGWState {
  PICSection         pic;         // state.pic.bAvailable, state.pic.sFwversion
  OTGWProtocol       otgw;        // state.otgw.bOnline, state.otgw.bBoilerState
  MQTTRuntimeSection mqtt;        // state.mqtt.bConnected
  FlashSection       flash;       // state.flash.bESPactive, state.flash.iPICprogress
  DebugSection       debug;       // state.debug.bOTmsg, state.debug.bMQTT
  UptimeSection      uptime;      // state.uptime.iSeconds, state.uptime.iRebootCount
  HeapDiagSection    heapdiag;    // state.heapdiag.iMqttDropsTotal, ...
  DiscoverySection   discovery;   // state.discovery.iPublishedTopicCount, ... (ADR-062)
  PicSettingsSection picSettings; // state.picSettings — PR=-polled settings from PIC
  StatusMessage      statusMessage = StatusMessage::None;
  bool               bSetupComplete = false;
};

OTGWState state;

// Central PIC availability guard — returns true when a PIC is available.
// Set at boot by detectPIC() and can flip true at runtime if a PIC banner is received.
// All PIC-related operations (commands, queries, upgrades) check this before proceeding.
inline bool isPICEnabled() { return state.pic.bAvailable; }
inline bool isGatewayFirmware() { return strcmp_P(state.pic.sType, PSTR("gateway")) == 0; }

//===================[ Persistent Settings — serialized to LittleFS (ADR-051) ]===================
// Sub-section structs for OTGWSettings — groups configuration by feature area.
// Hungarian prefixes: b=bool, s=string/char[], i=int/uint, f=float

struct MQTTSettingsSection {
  bool    bEnable          = true;
  bool    bSecure          = false;
  char    sBroker[65]      = "homeassistant.local";
  uint16_t iBrokerPort     = 1883;   // TCP port 1..65535; was int16_t (overflowed >32767)
  char    sUser[41]        = "";
  char    sPasswd[41]      = "";
  char    sHaprefix[41]    = HOME_ASSISTANT_DISCOVERY_PREFIX;
  bool    bHaRebootDetect  = true;
  char    sTopTopic[41]    = "OTGW";
  char    sUniqueid[41]    = "";  // Initialized in readSettings
  bool    bOTmessage       = false;
  bool    bOnChangePublishing = true; // On-change publishing: publish on change, heartbeat unchanged values every iInterval (ADR-081)
  uint16_t iInterval       = MQTT_DEFAULT_PUBLISH_INTERVAL_SEC; // Heartbeat interval (s) when on-change active; 0 = legacy publish-every-message
  bool    bSeparateSources = false; // ADR-040: publish source-specific topics
  bool    bLegacyPort25238Enabled = false; // Opt-in otmonitor TCP stream for legacy clients
  bool    bDiscoveryAutoVerify = true;  // ADR-062: daily auto-heal of retained discovery configs (TASK-351 wires the trigger; TASK-349 ships the field only)
};

struct NTPSection {
  bool bEnable        = true;
  char sTimezone[48]  = NTP_DEFAULT_TIMEZONE;   // longest IANA tz id ~32 chars; 48 ample, text-serialized (was 65)
  char sHostname[65]  = NTP_HOST_DEFAULT;
  bool bSendtime      = false;
};

struct SensorsSection {             // Dallas DS18B20 external sensors
  bool    bEnabled       = false;
  bool    bLegacyFormat  = false;   // Default to false (new standard format)
  int8_t  iPin           = 10;     // GPIO 13 = D7, GPIO 10 = SDIO 3
  int16_t iInterval      = 20;     // Interval time to read out temp and send to MQ
};

struct S0Section {
  bool     bEnabled      = false;
  uint8_t  iPin          = 12;     // GPIO 12 = D6, preferred
  uint16_t iDebounceTime = 80;     // Depending on S0 switch
  uint16_t iPulsekw      = 1000;   // Most S0 counters have 1000 pulses per kW
  uint16_t iInterval     = 60;     // Suggested measurement reporting interval
};

struct OutputsSection {             // GPIO relay outputs
  bool   bEnabled    = false;
  int8_t iPin        = 16;
  int8_t iTriggerBit = 0;
};

struct WebhookSection {
  bool   bEnabled         = false;
  char   sURLon[101]      = "http://homeassistant.local:8123/api/webhook/otgw_boiler";
  char   sURLoff[101]     = "http://homeassistant.local:8123/api/webhook/otgw_boiler";
  int8_t iTriggerBit      = 1;     // Default: bit 1 = CH mode (slave: CH active)
  char   sPayload[201]    = "";    // Body template for HTTP POST; empty = HTTP GET
  char   sContentType[32] = "application/json";
};

struct UISection {
  bool bAutoScroll      = true;
  bool bShowTimestamp   = true;
  bool bCaptureMode     = false;
  bool bAutoScreenshot  = false;
  bool bAutoDownloadLog = false;
  bool bAutoExport      = false;
  int16_t iGraphTimeWindow = 60;   // 1..1440 min; int16 ample (was int)
};

struct OTGWBootSection {            // PIC boot-time command injection
  bool bEnable        = false;
  char sCommands[129] = "";
};


// Hardware identity for HA device registry discovery is FIXED for this ESP8266
// firmware (NodoShop OTGW) and lives in flash as kMfrVal/kModelVal in
// mqtt_configuratie.cpp — no editable/persisted DeviceSection (TASK-903).

struct WifiSection {              // settings.wifi — static IP (all empty = DHCP)
  char sStaticIp[16] = "";       // e.g. "192.168.1.100"  (empty = DHCP)
  char sSubnet[16]   = "";       // e.g. "255.255.255.0"
  char sGateway[16]  = "";       // e.g. "192.168.1.1"
  char sDns1[16]     = "";       // e.g. "8.8.8.8"
  char sDns2[16]     = "";       // e.g. "8.8.4.4"
};

struct OTGWSettings {
  // Device-level fields (universal device identity)
  char sHostname[41] = _HOSTNAME;
  char sHTTPpasswd[41] = "";  // HTTP Basic Auth password (empty = no authentication required)
  bool bLEDblink     = true;
  bool bDarkTheme    = false;
  bool bMyDEBUG      = false;
  bool bNightlyRestart = false;  // Scheduled nightly restart for heap recovery (opt-in)
  uint8_t iRestartHour = 4;     // Hour (0-23, local time) for nightly restart (default 04:00)

  // Named sub-sections — access as settings.mqtt.sBroker, settings.ntp.sTimezone, etc.
  WifiSection         wifi;
  MQTTSettingsSection mqtt;
  NTPSection          ntp;
  SensorsSection      sensors;
  S0Section           s0;
  OutputsSection      outputs;
  WebhookSection      webhook;
  UISection           ui;
  OTGWBootSection     otgw;
};

OTGWSettings settings;

//===================[ Global variables — not part of settings or state ]===================
WiFiClient  wifiClient;
char        cMsg[CMSG_SIZE];
char        otTopic[OT_TOPIC_LEN];  // Shared MQTT topic scratch for OT print_* functions (sequential, not re-entrant)
char        lastReset[129] = "";
uint32_t    MQTTautoConfigMap[8] = { 0 };       // "already published" bitmap (256 bits)
uint32_t    MQTTautoCfgPendingMap[8] = { 0 };  // "needs publishing" bitmap (256 bits)
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
static const char flashMode[][8] PROGMEM = { "QIO", "QOUT", "DIO", "DOUT", "Unknown" };  // flash, not RAM (was const char*[] in RODATA)

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

// Diagnostic discovery pseudo-IDs (TASK-540):
//   248 — otgw-firmware/{reboot_count,reboot_reason,version,hostname}
//   249 — otgw-pic/{version,deviceid,firmwaretype,designer,picavailable} (PIC-gated)
//   250 — otgw-pic/settings/* (15 PR=-polled topics, PIC-gated)
// PIC control discovery pseudo-ID:
//   251 — resetgateway button + gpioa/gpiob/leda-f select entities
byte      OTGWfwinfoid       = 248;
byte      OTGWpicinfoid      = 249;
byte      OTGWpicsettingsid  = 250;
byte      OTGWpiccontrolsid  = 251;

//Now load Debug & network library
#include "Debug.h"
#include "networkStuff.h"


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
