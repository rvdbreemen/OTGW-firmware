/* 
***************************************************************************  
**  Program  : OTGW-firmware.h
**  Version  : v2.0.0-beta
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

// OTDirectRequestOrigin moved to OTDirecttypes.h (ADR-079/TASK-326)

// OT-direct forward declarations (defined in OTDirect.ino)
void initOTDirect();
void loopOTDirect();
void handleOTDirectCommand(const char* buf, int len);
void sendOTDirectOverridesJSON();
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
#include "MQTTtypes.h"
#include "Flashtypes.h"
#include "Debugtypes.h"
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
HeapHealthLevel getHeapHealth();
bool canSendWebSocket();
bool canPublishMQTT();
void logHeapStats();
void emergencyHeapRecovery();
bool updateLittleFSStatus(const char *probePath = nullptr);
bool updateLittleFSStatus(const __FlashStringHelper *probePath);
bool readLatestCrashLog(char* summary, size_t summarySize, char* details, size_t detailsSize);

//prototype
void sendMQTTData(const char*, const char*, const bool = false);
void sendMQTTData(const __FlashStringHelper*, const char*, const bool = false);
void sendMQTTData(const __FlashStringHelper*, const __FlashStringHelper*, const bool = false);
void publishToSourceTopic(const char*, const char*, byte);
void loopMQTTDiscovery();
void setMQTTConfigPending(const uint8_t MSGid);
void markAllMQTTConfigPending();
const char *messageIDToString(OTLibMessageID message_id);
void addCommandToQueue(const char* ,  int , const bool = false, const int16_t = 1000);
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

// MQTTSettingsSection moved to MQTTtypes.h (ADR-079/TASK-326 AC3)

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

//Now load Debug & network library
#include "Debug.h"
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
