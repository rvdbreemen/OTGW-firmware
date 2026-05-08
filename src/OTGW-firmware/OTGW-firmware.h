/* 
***************************************************************************  
**  Program  : OTGW-firmware.h
**  Version  : v1.5.1-beta.3
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
void sendMQTTData(const char*, const char*, const bool = false);
void sendMQTTData(const __FlashStringHelper*, const char*, const bool = false);
void sendMQTTData(const __FlashStringHelper*, const __FlashStringHelper*, const bool = false);
// PIC subtree helper -- prepends kPicSubtreePrefix so the otgw-pic/ subtree
// name has a single source of truth (ADR-065). Used by TASK-390 migrations.
void sendMQTTDataPic(const __FlashStringHelper* label, const char* value);
void sendMQTTDataPic(const __FlashStringHelper* label, const __FlashStringHelper* value);
void publishToSourceTopic(const char*, const char*, byte);
void loopMQTTDiscovery();
void sendMQTTheapdiag();
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
#if defined(ENABLE_SAT)
// Alias used by SAT subsystem (name harmonised with OTGW32 branch)
#define addCommandToQueue addOTWGcmdtoqueue
#endif
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

#if defined(ENABLE_SAT)
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
void satSaveCycleWindow();
void satLoadCycleWindow();
void satFlushCycleWindow();
void satFlushShortLivedData();
// SAT Weather forward declarations — defined in SATweather.ino
void weatherLoop();
void weatherFetch();
void weatherSendStatusJSON();
void weatherPublishMQTT();
#endif // ENABLE_SAT

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
  char sError[129]       = "";     // was errorupgrade
  char sPICfile[65]      = "";     // was currentPICFlashFile
  int  iPICprogress      = 0;      // was currentPICFlashProgress — percent 0-100
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
#if defined(ENABLE_SAT)
  bool     bSAT                   = true;   // SAT control loop + cycle + HCR trace (default on)
#endif
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

struct PicSettingsSection {    // state.picSettings — settings polled from PIC via PR= commands
  // Source: Schelte Bron's OTGW firmware documentation (https://otgw.tclcode.com/firmware.html)
  // PR=A (About/version) is handled by getpicfwversion(); PR=M (mode) by queryOTGWgatewaymode().
  // All other PR= reports are polled on-demand by queryNextPICsetting(), one per 3s tick.

  // --- Active settings (most useful for HA integration) ---
  char sSetpointOverride[16]  = "";  // PR=O: setpoint override ("T20.5" TT active, "C20.5" TC active, "N" none)
  char sSetback[16]           = "";  // PR=S: setback temperature (SB command value, e.g. "15.0")
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

#if defined(ENABLE_SAT)
//--- SAT runtime enums and state ---
enum SATHeatingSystem : uint8_t {
  SAT_HSYS_AUTO       = 0,  // Auto-detect from OT MsgID 3 system_type bit
  SAT_HSYS_RADIATORS  = 1,  // Gas boiler + radiators (default if auto-detect fails)
  SAT_HSYS_HEAT_PUMP  = 2,  // Heat pump (hybrid or standalone)
  SAT_HSYS_UNDERFLOOR = 3   // Underfloor heating
};
enum SATControlMode : uint8_t { SAT_MODE_OFF = 0, SAT_MODE_CONTINUOUS, SAT_MODE_PWM };
enum SATCalibPhase  : uint8_t {
  SAT_CALIB_IDLE = 0, SAT_CALIB_STARTING, SAT_CALIB_WARMING,
  SAT_CALIB_MEASURING, SAT_CALIB_COOLDOWN, SAT_CALIB_DONE, SAT_CALIB_FAILED
};
enum SATPreset : uint8_t {
  SAT_PRESET_NONE = 0, SAT_PRESET_AWAY, SAT_PRESET_ECO,
  SAT_PRESET_COMFORT, SAT_PRESET_SLEEP, SAT_PRESET_ACTIVITY,
  SAT_PRESET_HOME
};
enum SATFallbackReason : uint8_t {
  SAT_FB_NONE = 0, SAT_FB_THERMOSTAT_LOST, SAT_FB_MQTT_LOST
};
enum SATCycleClass  : uint8_t {
  SAT_CYCLE_NONE = 0, SAT_CYCLE_GOOD, SAT_CYCLE_OVERSHOOT,
  SAT_CYCLE_UNDERHEAT, SAT_CYCLE_SHORT, SAT_CYCLE_UNCERTAIN
};
enum SATCycleKind : uint8_t {
  SAT_CK_UNKNOWN = 0, SAT_CK_CH, SAT_CK_DHW, SAT_CK_MIXED
};
enum SATCyclePhase : uint8_t {
  SAT_CP_IDLE = 0, SAT_CP_STARTUP, SAT_CP_STEADY, SAT_CP_COOLDOWN
};
enum SATCurveRecommendation : uint8_t {
  SAT_CR_INSUFFICIENT = 0, SAT_CR_INCREASE, SAT_CR_DECREASE, SAT_CR_HOLD
};
enum SATManufacturer : uint8_t {
  SAT_MFR_AUTO = 0,
  SAT_MFR_ATAG,       SAT_MFR_BAXI,      SAT_MFR_BROTGE,
  SAT_MFR_DEDIETRICH, SAT_MFR_FERROLI,   SAT_MFR_GEMINOX,
  SAT_MFR_IDEAL,      SAT_MFR_IMMERGAS,  SAT_MFR_INTERGAS,
  SAT_MFR_ITHO,       SAT_MFR_NEFIT,     SAT_MFR_RADIANT,
  SAT_MFR_REMEHA,     SAT_MFR_SIME,      SAT_MFR_VAILLANT,
  SAT_MFR_VIESSMANN,  SAT_MFR_WORCESTER, SAT_MFR_OTHER,
  SAT_MFR_COUNT
};
#define SAT_QUIRK_MIN_MOD_10     0x01  // Geminox: minimum modulation 10%
#define SAT_QUIRK_IMMERGAS_TP    0x02  // Immergas: extra TP=11:12 command, cap 80%
#define SAT_QUIRK_NO_REL_MOD     0x04  // Ideal/Intergas/Geminox/Nefit: no relative modulation support
#define SAT_QUIRK_MI_500_BOOT    0x08  // Ideal/Intergas/Nefit: send MI=500 on boot
enum SATFlameStatus : uint8_t {
  SAT_FS_INSUFFICIENT_DATA = 0, SAT_FS_HEALTHY, SAT_FS_IDLE_OK,
  SAT_FS_STUCK_ON, SAT_FS_STUCK_OFF, SAT_FS_PWM_SHORT, SAT_FS_SHORT_CYCLING
};
enum SATBoilerStatus : uint8_t {
  SAT_BS_OFF = 0, SAT_BS_IDLE, SAT_BS_PREHEATING, SAT_BS_AT_SETPOINT,
  SAT_BS_MODULATING_UP, SAT_BS_MODULATING_DOWN, SAT_BS_IGNITION_SURGE,
  SAT_BS_STALLED_IGNITION, SAT_BS_ANTI_CYCLING, SAT_BS_PUMP_STARTING,
  SAT_BS_WAITING_FLAME, SAT_BS_OVERSHOOT_COOLING, SAT_BS_POST_CYCLE,
  SAT_BS_HEATING, SAT_BS_COOLING
};

// ESP8266: 30 slots (~2h at 4-min avg cycle)
#define SAT_WIN4H_SIZE 30

struct SATWindowRecord {
  uint32_t endMs;
  uint32_t onDurationMs;
  uint32_t offDurationMs;
  float    p90FlowTemp;
  float    avgFlowRetDelta;
  uint8_t  eClass;
};

struct SATZoneState {
  float    fRoomTemp     = 0.0f;
  float    fSetpoint     = 0.0f;
  float    fPidOutput    = 0.0f;
  float    fPidIntegral  = 0.0f;
  float    fPrevError    = 0.0f;
  uint32_t iLastUpdateMs = 0;
  bool     bRoomValid    = false;
  bool     bSpValid      = false;
};

struct SATRuntimeSection {         // state.sat — SAT thermostat controller state
  bool            bActive        = false;
  SATControlMode  eControlMode   = SAT_MODE_OFF;
  SATBoilerStatus eBoilerStatus  = SAT_BS_OFF;
  float fHeatingCurveValue       = 0.0f;
  float fPidOutput               = 0.0f;
  float fPidP                    = 0.0f;
  float fPidI                    = 0.0f;
  float fPidD                    = 0.0f;
  float fFinalSetpoint           = 0.0f;
  float fError                   = 0.0f;
  float fKp                      = 0.0f;
  float fKi                      = 0.0f;
  float fKd                      = 0.0f;
  float fRawDerivative           = 0.0f;
  SATCycleClass eLastCycleClass  = SAT_CYCLE_NONE;
  uint32_t iCycleCount           = 0;
  uint8_t  iCyclesThisHour       = 0;
  float    fCycleMaxFlow         = 0.0f;
  float    fCycleOvershootSec    = 0.0f;
  float    fLastCycleDuration    = 0.0f;
  SATCycleKind eLastCycleKind    = SAT_CK_UNKNOWN;
  float    fLastCycleFractionCH  = 0.0f;
  float    fLastCycleFractionDHW = 0.0f;
  float    fDutyRatio            = 0.0f;
  float    fOvershootFraction    = 0.0f;
  float    fUnderheatFraction    = 0.0f;
  uint8_t  i4hCycles             = 0;
  float    f4hAvgOnSec           = 0.0f;
  float    f4hAvgOffSec          = 0.0f;
  float    f4hAvgFlow            = 0.0f;
  float    f4hDutyRatio          = 0.0f;
  float    f4hOvershootFraction  = 0.0f;
  float    f4hUnderheatFraction  = 0.0f;
  float    f4hFlowRetDeltaP50    = 0.0f;
  float    f4hFlowRetDeltaP90    = 0.0f;
  float fPwmDutyCycle            = 0.0f;
  bool  bPwmFlameRequested       = false;
  SATPreset eActivePreset        = SAT_PRESET_NONE;
  uint8_t iCurrentModulation     = 100;
  bool     bDhwActive            = false;
  bool     bModSuppressed        = false;
  uint32_t iModSuppressionSinceMs = 0;
  SATCalibPhase eCalibPhase      = SAT_CALIB_IDLE;
  float    fCalibMaxTemp         = 0.0f;
  float    fCalibStartTemp       = 0.0f;
  uint32_t iCalibStartMs         = 0;
  uint16_t iCalibSamples         = 0;
  float fExternalTemp            = 0.0f;
  float fExternalOutdoor         = 0.0f;
  bool  bExternalTempValid       = false;
  bool  bExternalOutdoorValid    = false;
  uint32_t iLastControlMs        = 0;
  uint32_t iExternalTempLastMs   = 0;
  uint32_t iExternalOutdoorLastMs = 0;
  bool     bSafetyTripped        = false;
  bool     bFallbackActive       = false;
  SATFallbackReason eFallbackReason = SAT_FB_NONE;
  uint8_t  iDetectedHeatingSystem = SAT_HSYS_RADIATORS;
  uint8_t  iDetectedManufacturer  = SAT_MFR_OTHER;
  uint8_t  iSlaveMemberID        = 0;
  bool     bValvesOpen           = true;
  bool     bWindowOpen           = false;
  uint32_t iWindowOpenSinceMs    = 0;
  float    fPreWindowTarget      = 0.0f;
  uint8_t  iPreWindowPreset      = 0;
  float    fPreCustomTemp        = 0.0f;
  float    fPreActivityTemp      = 0.0f;
  float    fSmoothedPressure     = 0.0f;
  float    fPressureDropRate     = 0.0f;
  bool     bPressureAlarm        = false;
  uint32_t iPressureAlarmSinceMs = 0;
  bool     bPressureHealthy      = true;
  float    fLastPressure         = 0.0f;
  uint32_t iLastPressureMs       = 0;
  uint32_t iLastSeenPressureMs   = 0;
  float    fBoilerPressure       = 0.0f;
  char     sPressureStatus[8]    = "ok";
  bool     bModulationReliable   = true;
  uint8_t  iModChangeCount       = 0;
  SATCurveRecommendation eCurveRecommendation = SAT_CR_INSUFFICIENT;
  float    fMeanError            = 0.0f;
  float    fErrorStdDev          = 0.0f;
  uint8_t  iErrorSampleCount     = 0;
  char     sHeatCurveRec[13]     = "insufficient";
  SATFlameStatus eFlameStatus    = SAT_FS_INSUFFICIENT_DATA;
  bool     bSetpointMismatch     = false;
  uint32_t iMismatchSinceMs      = 0;
  struct {
    float    fTemperature  = 0.0f;
    float    fHumidity     = 0.0f;
    float    fWindSpeed    = 0.0f;
    bool     bValid        = false;
    uint32_t iLastUpdateMs = 0;
    uint16_t iFetchErrors  = 0;
  } weather;
  float    fCurrentPower         = 0.0f;
  float    fEnergyTotal          = 0.0f;
  uint32_t iEnergyLastMs         = 0;
  float    fEnergyEstimatedKWh   = 0.0f;
  float    fEstEnergyLastSavedKWh = 0.0f;
  uint32_t iEstEnergyLastMs      = 0;
  float    fSimRoomTemp          = 20.0f;
  float    fSimFlowTemp          = 20.0f;
  float    fSimOutdoorTemp       = 5.0f;
  uint32_t iSimLastUpdateMs      = 0;
  bool     bSimWarmupDone        = false;
  float    fEstimatedRoom        = 0.0f;
  float    fLastKnownRoom        = 0.0f;
  uint32_t iLastKnownRoomMs      = 0;
  bool     bThermalModelValid    = false;
  float    fThermalDropRate      = 0.0f;
  bool     bSolarGainActive      = false;
  float    fIndoorRiseRate       = 0.0f;
  float    fSunElevation         = 0.0f;
  bool     bSunElevationValid    = false;
  uint32_t iSunElevLastMs        = 0;
  bool     bSummerActive         = false;
  float    fSummerHoursAbove     = 0.0f;
  float    fHumidity             = 0.0f;
  bool     bHumidityValid        = false;
  uint32_t iHumidityLastMs       = 0;
  float    fComfortOffset        = 0.0f;
  float    fAreaTemp[4]          = {0};
  bool     bAreaValid[4]         = {false};
  uint32_t iAreaLastMs[4]        = {0};
  bool     bAutoTuneActive       = false;
  uint16_t iAutoTuneCycles       = 0;
  float    fAutoTuneScore        = 0.0f;
};
#endif // ENABLE_SAT

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
#if defined(ENABLE_SAT)
  SATRuntimeSection  sat;         // state.sat — SAT thermostat controller
#endif
  StatusMessage      statusMessage = StatusMessage::None;
  bool               bSetupComplete = false;
};

OTGWState state;

// Central PIC availability guard — returns true when a PIC is available.
// Set at boot by detectPIC() and can flip true at runtime if a PIC banner is received.
// All PIC-related operations (commands, queries, upgrades) check this before proceeding.
inline bool isPICEnabled() { return state.pic.bAvailable; }
inline bool isGatewayFirmware() { return strcmp_P(state.pic.sType, PSTR("gateway")) == 0; }
#if defined(ENABLE_SAT)
// On ESP8266 dev branch the only OT command interface is the PIC serial link.
inline bool hasOTCommandInterface() { return isPICEnabled(); }
#endif

//===================[ Persistent Settings — serialized to LittleFS (ADR-051) ]===================
// Sub-section structs for OTGWSettings — groups configuration by feature area.
// Hungarian prefixes: b=bool, s=string/char[], i=int/uint, f=float

struct MQTTSettingsSection {
  bool    bEnable          = true;
  bool    bSecure          = false;
  char    sBroker[65]      = "homeassistant.local";
  int16_t iBrokerPort      = 1883;
  char    sUser[41]        = "";
  char    sPasswd[41]      = "";
  char    sHaprefix[41]    = HOME_ASSISTANT_DISCOVERY_PREFIX;
  bool    bHaRebootDetect  = true;
  char    sTopTopic[41]    = "OTGW";
  char    sUniqueid[41]    = "";  // Initialized in readSettings
  bool    bOTmessage       = false;
  uint16_t iInterval       = 0;   // MQTT publish interval in seconds (0 = publish every message)
  bool    bSeparateSources = false; // ADR-040: publish source-specific topics
  bool    bLegacyPort25238Enabled = false; // Opt-in otmonitor TCP stream for legacy clients
  bool    bDiscoveryAutoVerify = true;  // ADR-062: daily auto-heal of retained discovery configs (TASK-351 wires the trigger; TASK-349 ships the field only)
};

struct NTPSection {
  bool bEnable        = true;
  char sTimezone[65]  = NTP_DEFAULT_TIMEZONE;
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
  int  iGraphTimeWindow = 60;      // Default to 1 Hour (60 minutes)
};

struct OTGWBootSection {            // PIC boot-time command injection
  bool bEnable        = false;
  char sCommands[129] = "";
};

#if defined(ENABLE_SAT)
//--- SAT (Smart Autotune Thermostat) settings ---
struct SATSection {
  bool     bEnabled           = false;
  uint8_t  iHeatingSystem     = SAT_HSYS_AUTO;
  float    fTargetTemp        = 20.0f;
  float    fHeatingCurveCoeff = 1.5f;
  float    fDeadband          = 0.1f;
  uint16_t iControlInterval   = 30;
  bool     bUseExternalTemp   = false;
  float    fPresetComfort     = 21.0f;
  float    fPresetEco         = 18.0f;
  float    fPresetAway        = 15.0f;
  float    fPresetSleep       = 16.0f;
  float    fPresetActivity    = 10.0f;
  float    fPresetHome        = 18.0f;
  bool     bPwmAutoSwitch     = true;
  uint8_t  iMaxRelModulation  = 100;
  float    fOvpValue          = 0.0f;
  bool     bOvpEnabled        = false;
  float    fOvershootMargin   = 2.0f;
  float    fModSupDelay       = 20.0f;
  float    fModSupOffset      = 1.0f;
  float    fDhwSetpoint       = 0.0f;
  bool     bDhwEnabled        = false;
  bool     bPushSetpoint      = false;
  float    fFlameOffOffset    = 0.0f;
  bool     bWindowDetection   = false;
  uint16_t iWindowMinOpenSec  = 60;
  bool     bForcePWM          = false;
  float    fFlowOffset        = 2.0f;
  float    fTargetTempStep    = 0.5f;
  float    fMinPressure       = 0.8f;
  float    fMaxPressure       = 2.5f;
  float    fMaxPressureDrop   = 0.3f;
  uint8_t  iManufacturer      = SAT_MFR_AUTO;
  bool     bWeatherEnable     = false;
  float    fWeatherLat        = 0.0f;
  float    fWeatherLon        = 0.0f;
  uint16_t iWeatherInterval   = 900;
  float    fBoilerCapacity    = 24.0f;
  float    fBoilerRatedKW     = 0.0f;
  float    fBoilerEfficiency  = 0.92f;
  bool     bPresetSync        = false;
  char     sPresetSyncTopic[65] = "";
  bool     bSimulation        = false;
  float    fSimHeatRate       = 0.5f;
  float    fSimCoolRate       = 0.1f;
  float    fThermalCoeff      = 0.05f;
  bool     bSolarGainEnable   = false;
  float    fSolarMinRiseRate  = 0.5f;
  float    fSolarSetpointOffset = 2.0f;
  float    fSolarMinElevation = 12.0f;
  bool     bSummerSimmer      = false;
  float    fSummerThreshold   = 18.0f;
  uint8_t  iSummerMinHours    = 6;
  bool     bComfortAdjust     = false;
  float    fComfortHumidity   = 50.0f;
  float    fComfortMaxOffset  = 1.0f;
  bool     bMultiArea         = false;
  uint8_t  iMultiAreaCount    = 0;
  float    fAreaWeight[4]     = {1.0f, 1.0f, 1.0f, 1.0f};
  bool     bAutoTune          = false;
  float    fAutoTuneRate      = 0.02f;
  float    fMaxSetpoint        = 65.0f;
  uint32_t iSensorMaxAgeS     = 21600;
  bool     bErrorMonitoring   = false;
  float    fAutoGainsValue    = 2.0f;
  bool     bAutoGains         = true;
  float    fKpManual          = 5.0f;
  float    fKiManual          = 0.0005f;
  float    fKdManual          = 0.0f;
  bool     bThermalComfort    = false;
  uint16_t iHumidityTimeoutS  = 1800;
  uint8_t  iHeatingMode       = 0;
  uint8_t  iCyclesPerHour     = 3;
  float    fValveOffset       = 0.0f;
  bool     bSolarFreezeIntegral = true;
  uint16_t iSatFlushThresholdH = 24;
  uint8_t  iZoneCount          = 1;
  uint16_t iZoneTimeoutS       = 300;
};
#endif // ENABLE_SAT

// Hardware identity for HA device registry discovery.
// Defaults set per platform; user can override via settings.ini or web UI.
struct DeviceSection {
  char sManufacturer[32] = "NodoShop";
  char sModel[32]        = "OTGW";
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
  DeviceSection       device;
  MQTTSettingsSection mqtt;
  NTPSection          ntp;
  SensorsSection      sensors;
  S0Section           s0;
  OutputsSection      outputs;
  WebhookSection      webhook;
  UISection           ui;
  OTGWBootSection     otgw;
#if defined(ENABLE_SAT)
  SATSection          sat;
#endif
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
byte      OTGWfwinfoid       = 248;
byte      OTGWpicinfoid      = 249;
byte      OTGWpicsettingsid  = 250;

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
