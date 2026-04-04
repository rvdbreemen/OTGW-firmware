/* 
***************************************************************************  
**  Program  : OTGW-firmware.h
**  Version  : v1.4.0-beta
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

#include <TelnetStream.h>       // https://github.com/jandrassy/TelnetStream/commit/1294a9ee5cc9b1f7e51005091e351d60c8cddecf
#include "Wire.h"
#include "safeTimers.h"
#include "boards.h"             // Board-specific pin maps and feature flags (HAS_PIC, HAS_DIRECT_OT)
#if HAS_PIC
#include <OTGWSerial.h>         // Bron Schelte's Serial class - it upgrades and more
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

enum OTDirectRequestOrigin : uint8_t {
  OT_DIRECT_ORIGIN_GATEWAY = 0,
  OT_DIRECT_ORIGIN_THERMOSTAT
};

// OT-direct forward declarations (defined in OTDirect.ino)
void initOTDirect();
void loopOTDirect();
void handleOTDirectCommand(const char* buf, int len);
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
                         // All known users need ≤512 bytes.  MQTT autoconfig lines (≤900 bytes)
                         // use sLine[SLINE_SIZE] instead, so cMsg stays at its original size.
#define SLINE_SIZE 1200  // MQTT autoconfig line buffer.  mqttha.cfg lines reach ~900 bytes max.
                         // Exclusively owned by MQTTstuff.ino; guarded by mqttAutoConfigInProgress.
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
HeapHealthLevel getHeapHealth();
bool canSendWebSocket();
bool canPublishMQTT();
void logHeapStats();
void emergencyHeapRecovery();
void resetMQTTBufferSize();
bool updateLittleFSStatus(const char *probePath = nullptr);
bool updateLittleFSStatus(const __FlashStringHelper *probePath);
bool readLatestCrashLog(char* summary, size_t summarySize, char* details, size_t detailsSize);

//prototype
void sendMQTTData(const char*, const char*, const bool = false);
void sendMQTTData(const __FlashStringHelper*, const char*, const bool = false);
void sendMQTTData(const __FlashStringHelper*, const __FlashStringHelper*, const bool = false);
void publishToSourceTopic(const char*, const char*, byte);
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
void writeSettings(bool show);
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

// SAT (Smart Autotune Thermostat) forward declarations — defined in SATcontrol.ino, SATpid.ino, SATcycles.ino
void initSAT();
void satControlLoop();
void satPublishMQTT();
bool satHandleExternalTemp(const char* value);
bool satHandleExternalOutdoor(const char* value);
bool satHandleTargetTemp(const char* value);
void satHandleEnabled(const char* value);
void satDisable();
void satHandleControlMode(const char* value);
void satCycleOnFlameChange(bool flameOn);
void satSendStatusJSON();
uint32_t satCycleGetFlameOnStartMs();
uint32_t satCycleGetFlameOffStartMs();

//===================[ Hardware Mode — detected at boot ]===================
enum OTGWHardwareMode : uint8_t {
  HW_MODE_UNKNOWN   = 0,   // Not yet detected
  HW_MODE_PIC       = 1,   // PIC16F co-processor on UART (traditional OTGW)
  HW_MODE_OT_DIRECT = 2,   // Direct GPIO OpenTherm via opentherm_library (OTGW32)
  HW_MODE_DEGRADED  = 3,   // Hardware detected but non-functional (OT bus dead, PIC missing)
};

//===================[ Network Transport — WiFi vs Ethernet ]===================
enum OTGWNetworkMode : uint8_t {
  NET_WIFI     = 0,   // Using WiFi (default)
  NET_ETHERNET = 1,   // Using wired Ethernet (W5500)
};

struct HardwareSection {       // state.hw — detected hardware capabilities
  OTGWHardwareMode eMode       = HW_MODE_UNKNOWN;
#if defined(HAS_OLED_CAPABLE) && HAS_OLED_CAPABLE
  bool bOLEDPresent            = false;
#endif
#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
  bool bEthernetPresent        = false;
#endif
};

#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
struct NetworkSection {        // state.net — active network transport state
  OTGWNetworkMode eMode        = NET_WIFI;
  bool bEthernetLink           = false;   // physical link up on W5500
};
#endif

//===================[ Runtime State — transient, never persisted (ADR-051) ]===================
// Sub-section structs for OTGWState — groups runtime state by system component.
// Hungarian prefixes: b=bool, s=string/char[], i=int/uint, f=float

struct PICSection {            // state.pic — PIC microcontroller identity/status
  bool bAvailable     = false;           // was bPICavailable
  char sFwversion[32] = "no pic found";  // was sPICfwversion
  char sDeviceid[32]  = "no pic found";  // was sPICdeviceid
  char sType[32]      = "no pic found";  // was sPICtype
};

#if defined(HAS_DIRECT_OT) && HAS_DIRECT_OT
struct OTDirectSection {       // state.otd — OT-direct (OTGW32) runtime status
  uint8_t  iScheduleTotal    = 0;   // total schedule entries
  uint8_t  iScheduleActive   = 0;   // entries not disabled by boiler
  uint8_t  iScheduleDisabled = 0;   // entries disabled (UNKNOWN_DATA_ID)
  uint8_t  iOverrideCount    = 0;   // number of active write overrides
  bool     bBypassActive     = false; // true = thermostat direct to boiler (relay)
  bool     bStepUpEnabled    = false; // 24V step-up converter on
  bool     bMonitorMode      = false; // true = transparent pass-through, no overrides applied
};
#endif

struct OTGWProtocol {          // state.otgw — OpenTherm protocol & bus state
  bool bOnline           = false;  // was bOTGWonline — serial link alive
  bool bPSmode           = false;  // was bPSmode — Print Summary mode (PS=1)
  bool bGatewayMode      = false;  // was bOTGWgatewaystate — true=gateway, false=monitor
  bool bGatewayModeKnown = false;  // was bOTGWgatewaystateKnown
  bool bBoilerState      = false;  // was bOTGWboilerstate — CH/boiler active
  bool bThermostatState  = false;  // was bOTGWthermostatstate
};

struct MQTTRuntimeSection {    // state.mqtt — MQTT broker connection state
  bool bConnected        = false;  // was statusMQTTconnection
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
  bool     bMQTT                  = false;  // was bDebugMQTT — MQTT publish/receive trace
  bool     bSensors               = false;  // was bDebugSensors — Dallas sensor scan trace
  bool     bSensorSim             = false;  // was bDebugSensorSimulation
  bool     bOTGWSimulation        = false;  // was bDebugOTGWSimulation
  uint32_t iOTGWSimulationIntervalMs = 750;
  uint32_t iOTGWSimulationNextDueMs  = 0;
};

struct UptimeSection {         // state.uptime — System longevity counters
  uint32_t iSeconds      = 0;  // was upTimeSeconds
  uint32_t iRebootCount  = 0;  // was rebootCount
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

//--- SAT runtime enums and state ---
enum SATControlMode : uint8_t { SAT_MODE_OFF = 0, SAT_MODE_CONTINUOUS, SAT_MODE_PWM };
enum SATCycleClass  : uint8_t {
  SAT_CYCLE_NONE = 0, SAT_CYCLE_GOOD, SAT_CYCLE_OVERSHOOT,
  SAT_CYCLE_UNDERHEAT, SAT_CYCLE_SHORT, SAT_CYCLE_UNCERTAIN
};
enum SATBoilerStatus : uint8_t {
  SAT_BS_OFF = 0, SAT_BS_IDLE, SAT_BS_PREHEATING, SAT_BS_AT_SETPOINT,
  SAT_BS_MODULATING_UP, SAT_BS_MODULATING_DOWN, SAT_BS_IGNITION_SURGE,
  SAT_BS_STALLED_IGNITION, SAT_BS_ANTI_CYCLING, SAT_BS_PUMP_STARTING,
  SAT_BS_WAITING_FLAME, SAT_BS_OVERSHOOT_COOLING, SAT_BS_POST_CYCLE,
  SAT_BS_HEATING, SAT_BS_COOLING
};

struct SATRuntimeSection {         // state.sat — SAT thermostat controller state
  bool            bActive        = false;
  SATControlMode  eControlMode   = SAT_MODE_OFF;
  SATBoilerStatus eBoilerStatus  = SAT_BS_OFF;
  // Heating curve + PID
  float fHeatingCurveValue       = 0.0f;
  float fPidOutput               = 0.0f;  // = curve + P + I + D
  float fPidP                    = 0.0f;
  float fPidI                    = 0.0f;
  float fPidD                    = 0.0f;
  float fFinalSetpoint           = 0.0f;
  float fError                   = 0.0f;  // target - room temp
  float fKp                      = 0.0f;
  float fKi                      = 0.0f;
  float fKd                      = 0.0f;
  // Cycle tracking
  SATCycleClass eLastCycleClass  = SAT_CYCLE_NONE;
  uint32_t iCycleCount           = 0;
  float    fCycleMaxFlow         = 0.0f;
  float    fCycleOvershootSec    = 0.0f;
  // PWM state
  float fPwmDutyCycle            = 0.0f;
  bool  bPwmFlameRequested       = false;
  // External inputs (MQTT/REST overrides)
  float fExternalTemp            = 0.0f;
  float fExternalOutdoor         = 0.0f;
  bool  bExternalTempValid       = false;
  bool  bExternalOutdoorValid    = false;
  uint32_t iLastControlMs        = 0;
  // Safety / fallback
  uint32_t iExternalTempLastMs   = 0;   // millis() when external indoor temp was last received
  uint32_t iExternalOutdoorLastMs = 0;  // millis() when external outdoor temp was last received
  bool     bSafetyTripped        = false; // true if safety forced satDisable()
};

struct OTGWState {
  HardwareSection    hw;          // state.hw.eMode, state.hw.bOLEDPresent
#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
  NetworkSection     net;         // state.net.eMode, state.net.bEthernetLink
#endif
  PICSection         pic;         // state.pic.bAvailable, state.pic.sFwversion
#if defined(HAS_DIRECT_OT) && HAS_DIRECT_OT
  OTDirectSection    otd;         // state.otd — OT-direct schedule/override stats (OTGW32)
#endif
  OTGWProtocol       otgw;        // state.otgw.bOnline, state.otgw.bBoilerState
  MQTTRuntimeSection mqtt;        // state.mqtt.bConnected
  FlashSection       flash;       // state.flash.bESPactive, state.flash.iPICprogress
  DebugSection       debug;       // state.debug.bOTmsg, state.debug.bMQTT
  UptimeSection      uptime;      // state.uptime.iSeconds, state.uptime.iRebootCount
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
  return (WiFi.status() == WL_CONNECTED);
}

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
#elif defined(BOARD_NODOSHOP_OTGW32)
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

//--- SAT (Smart Autotune Thermostat) settings ---
// Ported from SAT releases/thermo-nova (https://github.com/Alexwijn/SAT)
// with permission from the SAT authors.
struct SATSection {
  bool     bEnabled           = false;
  uint8_t  iHeatingSystem     = 0;      // 0=radiator, 1=underfloor
  float    fTargetTemp        = 20.0f;  // Default room target °C
  float    fHeatingCurveCoeff = 1.5f;   // Heating curve coefficient
  float    fDeadband          = 0.25f;  // PID deadband °C
  uint16_t iControlInterval   = 30;     // Control loop interval (seconds)
  bool     bUseExternalTemp   = false;  // Prefer MQTT-pushed indoor temp over OT msg 24
  float    fPresetComfort     = 21.0f;  // Preset: Comfort
  float    fPresetEco         = 18.0f;  // Preset: Eco
  float    fPresetAway        = 15.0f;  // Preset: Away
  bool     bPwmAutoSwitch     = true;   // Auto-switch between PWM and continuous mode
};

#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
struct EthernetSection {
  bool bStaticIP       = false;             // false=DHCP (default), true=use static IP
  char sIPaddress[16]  = "0.0.0.0";        // Static IP address
  char sGateway[16]    = "0.0.0.0";        // Gateway
  char sSubnet[16]     = "255.255.255.0";  // Subnet mask
  char sDNS[16]        = "0.0.0.0";        // DNS server (0.0.0.0 = use gateway)
};
#endif

struct OTGWSettings {
  // Device-level fields (universal device identity)
  char sHostname[41] = _HOSTNAME;
  char sHTTPpasswd[41] = "";  // HTTP Basic Auth password (empty = no authentication required)
  bool bLEDblink     = true;
  bool bDarkTheme    = false;
  bool bMyDEBUG      = false;

  // Named sub-sections — access as settings.mqtt.sBroker, settings.ntp.sTimezone, etc.
  MQTTSettingsSection mqtt;
  NTPSection          ntp;
  SensorsSection      sensors;
  S0Section           s0;
  OutputsSection      outputs;
  WebhookSection      webhook;
  UISection           ui;
  OTGWBootSection     otgw;
  SATSection          sat;
#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
  EthernetSection     eth;
#endif
};

OTGWSettings settings;

//===================[ Global variables — not part of settings or state ]===================
WiFiClient  wifiClient;
char        cMsg[CMSG_SIZE];
char        sLine[SLINE_SIZE];  // MQTT autoconfig line scratch (MQTTstuff.ino, guarded by mqttAutoConfigInProgress)
char        otTopic[OT_TOPIC_LEN];  // Shared MQTT topic scratch for OT print_* functions (sequential, not re-entrant)
char        lastReset[129] = "";
uint32_t    MQTTautoConfigMap[8] = { 0 };
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
