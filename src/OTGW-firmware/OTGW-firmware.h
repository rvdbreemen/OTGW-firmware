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
int getOTDirectOverridesJSON(char* buf, size_t bufSize);
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
// OT-direct operating modes (gateway perspective)
enum OTDirectMode : uint8_t {
  OTD_MODE_GATEWAY  = 1,   // Full gateway: scheduler + thermostat forwarding + overrides (default)
  OTD_MODE_MONITOR  = 2,   // Transparent: forward all frames unmodified, log everything
  OTD_MODE_BYPASS   = 0,   // Thermostat direct to boiler via relay, OT-direct inactive
  OTD_MODE_MASTER   = 3,   // Sole OT master: scheduler only, no thermostat expected
  OTD_MODE_LOOPBACK = 4,   // Internal test: simulated boiler responses, no hardware needed
};

struct OTDirectSection {       // state.otd — OT-direct (OTGW32) runtime status
  uint8_t  iScheduleTotal    = 0;   // total schedule entries
  uint8_t  iScheduleActive   = 0;   // entries not disabled by boiler
  uint8_t  iScheduleDisabled = 0;   // entries disabled (UNKNOWN_DATA_ID)
  uint8_t  iOverrideCount    = 0;   // number of active write overrides
  bool     bBypassActive     = false; // true = thermostat direct to boiler (relay)
  bool     bStepUpEnabled    = false; // 24V step-up converter on
  bool     bMonitorMode      = false; // true = transparent pass-through, no overrides applied
  OTDirectMode eMode         = OTD_MODE_GATEWAY; // current operating mode
  bool     bMasterMode       = false;    // true = standalone master, no thermostat
  bool     bThermostatConnected = false; // thermostat recently seen (within timeout)
  bool     bSetbackActive    = false;    // thermostat disconnected → setback override engaged
  uint32_t iLastThermostatMs = 0;        // millis() of last thermostat frame received
};
#endif

struct OTBusState {          // state.otgw — OpenTherm protocol & bus state
  bool bOnline           = false;  // was bOTGWonline — serial link alive
  bool bPSmode           = false;  // was bPSmode — Print Summary mode (PS=1)
  bool bGatewayMode      = false;  // was bOTGWgatewaystate — true=gateway, false=monitor
  bool bGatewayModeKnown = false;  // was bOTGWgatewaystateKnown
  bool bBoilerState      = false;  // was bOTGWboilerstate — CH/boiler active
  bool bThermostatState  = false;  // was bOTGWthermostatstate
};

struct MQTTRuntimeSection {    // state.mqtt -- MQTT broker connection state
  bool bConnected        = false;  // was statusMQTTconnection
  uint32_t iLastConnectedMs = 0;   // millis() when MQTT was last connected (for fallback detection)
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
// Manufacturer enum — indices must match satManufacturerTable[] in SATcontrol.ino
enum SATManufacturer : uint8_t {
  SAT_MFR_AUTO = 0,   // Auto-detect from OT MemberID (may be ambiguous)
  SAT_MFR_ATAG,       SAT_MFR_BAXI,      SAT_MFR_BROTGE,
  SAT_MFR_DEDIETRICH, SAT_MFR_FERROLI,   SAT_MFR_GEMINOX,
  SAT_MFR_IDEAL,      SAT_MFR_IMMERGAS,  SAT_MFR_INTERGAS,
  SAT_MFR_ITHO,       SAT_MFR_NEFIT,     SAT_MFR_RADIANT,
  SAT_MFR_REMEHA,     SAT_MFR_SIME,      SAT_MFR_VAILLANT,
  SAT_MFR_VIESSMANN,  SAT_MFR_WORCESTER, SAT_MFR_OTHER,
  SAT_MFR_COUNT
};
// Manufacturer quirk flags
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
  float fRawDerivative           = 0.0f;  // filtered derivative (for diagnostics)
  // Cycle tracking
  SATCycleClass eLastCycleClass  = SAT_CYCLE_NONE;
  uint32_t iCycleCount           = 0;
  float    fCycleMaxFlow         = 0.0f;
  float    fCycleOvershootSec    = 0.0f;
  float    fLastCycleDuration     = 0.0f;   // Duration of last completed cycle (sec)
  SATCycleKind eLastCycleKind    = SAT_CK_UNKNOWN; // Kind of last completed cycle
  float    fLastCycleFractionCH  = 0.0f;   // Fraction of last cycle that was CH (0-1)
  float    fLastCycleFractionDHW = 0.0f;   // Fraction of last cycle that was DHW (0-1)
  float    fDutyRatio            = 0.0f;   // EMA flame-on fraction
  float    fOvershootFraction    = 0.0f;   // EMA overshoot cycle fraction
  float    fUnderheatFraction    = 0.0f;   // EMA underheat cycle fraction
  // PWM state
  float fPwmDutyCycle            = 0.0f;
  bool  bPwmFlameRequested       = false;
  // Preset
  SATPreset eActivePreset        = SAT_PRESET_NONE;
  // Modulation control
  uint8_t iCurrentModulation     = 100;   // Last MM= value sent to boiler (0-100)
  // DHW state
  bool     bDhwActive               = false;  // DHW currently active (from OT status)
  // Modulation suppression
  bool     bModSuppressed           = false;
  uint32_t iModSuppressionSinceMs   = 0;
  // OPV calibration state
  SATCalibPhase eCalibPhase      = SAT_CALIB_IDLE;
  float    fCalibMaxTemp         = 0.0f;  // Maximum boiler temp observed during calibration
  float    fCalibStartTemp       = 0.0f;  // Boiler temp at calibration start
  uint32_t iCalibStartMs         = 0;     // millis() when calibration started
  uint16_t iCalibSamples         = 0;     // Number of temperature samples taken
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
  // Fallback mode
  bool     bFallbackActive       = false;
  SATFallbackReason eFallbackReason = SAT_FB_NONE;
  // Heating system detection
  uint8_t  iDetectedHeatingSystem = SAT_HSYS_RADIATORS; // auto-detected from OT MsgID 74
  // Manufacturer detection
  uint8_t  iDetectedManufacturer  = SAT_MFR_OTHER;      // auto-detected from OT MsgID 3 valueLB
  uint8_t  iSlaveMemberID        = 0;                   // raw slave MemberID code from MsgID 3
  // TRV valve detection (Task #29)
  bool     bValvesOpen            = true;   // default true: assume heat demand when no TRV data
  // Window detection
  bool     bWindowOpen            = false;
  uint32_t iWindowOpenSinceMs     = 0;
  float    fPreWindowTarget       = 0.0f;
  uint8_t  iPreWindowPreset       = 0;   // SATPreset before window opened
  // Pressure monitoring
  float    fSmoothedPressure      = 0.0f;
  float    fPressureDropRate      = 0.0f; // bar/hour (linear regression)
  bool     bPressureAlarm         = false;
  uint32_t iPressureAlarmSinceMs  = 0;
  bool     bPressureHealthy       = true;  // binary sensor: true=healthy
  // Modulation reliability
  bool     bModulationReliable    = true;
  uint8_t  iModChangeCount        = 0;   // changes observed in window
  // Heating curve recommendation + error statistics
  SATCurveRecommendation eCurveRecommendation = SAT_CR_INSUFFICIENT;
  float    fMeanError             = 0.0f;
  float    fErrorStdDev           = 0.0f;
  uint8_t  iErrorSampleCount      = 0;     // Number of samples in error ring buffer
  // Flame health (Task #70/#71)
  SATFlameStatus eFlameStatus     = SAT_FS_INSUFFICIENT_DATA;
  // OT setpoint sync
  bool     bSetpointMismatch      = false;
  uint32_t iMismatchSinceMs       = 0;
  // Weather data (Open-Meteo API)
  struct {
    float    fTemperature    = 0.0f;   // Current outdoor temperature (C)
    float    fHumidity       = 0.0f;   // Current relative humidity (%)
    float    fWindSpeed      = 0.0f;   // Current wind speed (km/h)
    bool     bValid          = false;  // true after first successful fetch
    uint32_t iLastUpdateMs   = 0;      // millis() of last successful fetch
    uint16_t iFetchErrors    = 0;      // consecutive or total fetch error count
  } weather;
  // Power and energy (Task #45)
  float    fCurrentPower          = 0.0f;   // Current power in kW (modulation * capacity)
  float    fEnergyTotal           = 0.0f;   // Cumulative energy in kWh
  uint32_t iEnergyLastMs          = 0;      // Last energy integration timestamp
  // Simulation (Task #37)
  float    fSimRoomTemp           = 20.0f;
  float    fSimFlowTemp           = 20.0f;
  float    fSimOutdoorTemp        = 5.0f;
  uint32_t iSimLastUpdateMs       = 0;
  bool     bSimWarmupDone         = false;
  // Thermal drop learning (Task #21)
  float    fEstimatedRoom         = 0.0f;   // Estimated room temp during fallback
  float    fLastKnownRoom         = 0.0f;   // Last valid room temp before fallback
  uint32_t iLastKnownRoomMs       = 0;      // When last valid room temp was recorded
  bool     bThermalModelValid     = false;   // True after sufficient learning data
  float    fThermalDropRate       = 0.0f;    // Current drop rate sample (C/hr per C delta)
  // Solar gain (Task #23)
  bool     bSolarGainActive       = false;
  float    fIndoorRiseRate        = 0.0f;  // Current indoor temp rise rate (C/hr)
  // Summer simmer (Task #24)
  bool     bSummerActive          = false;  // Summer mode currently suppressing heating
  float    fSummerHoursAbove      = 0.0f;   // Hours outdoor temp has been above threshold
  // Humidity and comfort (Task #28/#47)
  float    fHumidity              = 0.0f;   // Current indoor humidity %
  bool     bHumidityValid         = false;  // Humidity reading available
  uint32_t iHumidityLastMs        = 0;      // Last humidity update timestamp
  float    fComfortOffset         = 0.0f;   // Current comfort adjustment (C)
  // Multi-area room temperature (Task #25)
  float    fAreaTemp[4]           = {0};    // Temperature per area
  bool     bAreaValid[4]          = {false};// Area has valid reading
  uint32_t iAreaLastMs[4]         = {0};    // Last update per area
  // PID auto-tuning (Task #27)
  bool     bAutoTuneActive        = false;  // Currently running auto-tune analysis
  uint16_t iAutoTuneCycles        = 0;      // Cycles analyzed since last adjustment
  float    fAutoTuneScore         = 0.0f;   // Current performance score (-1 to +1)
#if defined(ESP32)
  // BLE temperature sensor (Task #20, ESP32 only)
  float    fBleTemp               = 0.0f;   // BLE sensor temperature (0.01C precision)
  float    fBleHumidity           = 0.0f;   // BLE sensor humidity %
  bool     bBleTempValid          = false;  // BLE reading available and non-stale
  uint32_t iBleTempLastMs         = 0;      // Last BLE temperature update timestamp
  uint8_t  iBleSensorCount        = 0;      // Number of active BLE sensors seen
  uint8_t  iBleBattery            = 0;      // Battery level of primary BLE sensor
  int8_t   iBleRssi               = 0;      // RSSI of primary BLE sensor
#endif
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

struct PICBootSection {            // PIC boot-time command injection
  bool bEnable        = false;
  char sCommands[129] = "";
};

//--- SAT (Smart Autotune Thermostat) settings ---
// Ported from SAT releases/thermo-nova (https://github.com/Alexwijn/SAT)
// with permission from the SAT authors.
struct SATSection {
  bool     bEnabled           = false;
  uint8_t  iHeatingSystem     = SAT_HSYS_AUTO; // SATHeatingSystem enum: auto/radiators/heat_pump/underfloor
  float    fTargetTemp        = 20.0f;  // Default room target °C
  float    fHeatingCurveCoeff = 1.5f;   // Heating curve coefficient
  float    fDeadband          = 0.25f;  // PID deadband °C
  uint16_t iControlInterval   = 30;     // Control loop interval (seconds)
  bool     bUseExternalTemp   = false;  // Prefer MQTT-pushed indoor temp over OT msg 24
  float    fPresetComfort     = 21.0f;  // Preset: Comfort
  float    fPresetEco         = 18.0f;  // Preset: Eco
  float    fPresetAway        = 15.0f;  // Preset: Away
  float    fPresetSleep       = 16.0f;  // Preset: Sleep
  float    fPresetActivity    = 10.0f;  // Preset: Activity (used by window detection)
  float    fPresetHome        = 18.0f;  // Preset: Home (standard at-home temperature)
  bool     bPwmAutoSwitch     = true;   // Auto-switch between PWM and continuous mode
  uint8_t  iMaxRelModulation  = 100;    // Max relative modulation 0-100% (MM= command)
  float    fOvpValue          = 0.0f;   // Overshoot Protection Value (0=not calibrated)
  bool     bOvpEnabled        = false;  // Use OPV for auto PWM switching
  float    fOvershootMargin   = 2.0f;   // Overshoot margin °C (cycle classification + auto-switch)
  float    fModSupDelay       = 20.0f;  // Modulation suppression delay (seconds)
  float    fModSupOffset      = 1.0f;   // Modulation suppression offset (°C below setpoint)
  float    fDhwSetpoint       = 0.0f;   // DHW setpoint (0=inactive, 30-60°C)
  bool     bDhwEnabled        = false;  // Enable DHW control in standalone/fallback mode
  bool     bPushSetpoint      = false;  // Push SAT target to thermostat display (TC= command)
  float    fFlameOffOffset    = 0.0f;   // Setpoint offset when flame off (anti-cycling hysteresis)
  bool     bWindowDetection   = false;  // Enable window open detection via MQTT
  uint16_t iWindowMinOpenSec  = 60;     // Minimum seconds window must stay open before action
  bool     bForcePWM          = false;  // Force PWM mode regardless of boiler modulation
  float    fFlowOffset        = 2.0f;   // Continuous mode: max setpoint drop from boiler temp
  float    fTargetTempStep    = 0.5f;   // Target temperature UI step size (0.1-1.0)
  float    fMinPressure       = 0.8f;   // Pressure alarm: minimum bar
  float    fMaxPressure       = 2.5f;   // Pressure alarm: maximum bar
  float    fMaxPressureDrop   = 0.3f;   // Pressure alarm: max bar/hour drop rate
  uint8_t  iManufacturer      = SAT_MFR_AUTO; // User-confirmed manufacturer (0=auto-detect)
  // Weather data (Open-Meteo API, Task #50)
  bool     bWeatherEnable     = false;  // Enable weather data fetching
  float    fWeatherLat        = 0.0f;   // Latitude (from browser geolocation or manual)
  float    fWeatherLon        = 0.0f;   // Longitude
  uint16_t iWeatherInterval   = 900;    // Poll interval in seconds (default 15 min, min 5 min)
  // Power/energy (Task #45)
  float    fBoilerCapacity    = 24.0f;  // Boiler capacity in kW (for power calculation)
  // Preset sync (Task #46) — broadcast preset changes to secondary entities
  bool     bPresetSync        = false;         // Sync preset to secondary entities via MQTT
  char     sPresetSyncTopic[65] = "";          // MQTT topic for preset sync
  // Simulation mode (Task #37) — test SAT without a real boiler
  bool     bSimulation        = false;  // Enable simulation mode
  float    fSimHeatRate       = 0.5f;   // Room heating rate C/min
  float    fSimCoolRate       = 0.1f;   // Room cooling rate C/min
  // Thermal drop learning (Task #21) — learned building thermal decay coefficient
  float    fThermalCoeff      = 0.05f;  // Learned thermal drop coefficient (C/hr per C delta)
  // Solar gain compensation (Task #23)
  bool     bSolarGainEnable   = false;  // Enable solar gain compensation
  float    fSolarMinRiseRate  = 0.5f;   // Minimum indoor rise rate (C/hr) to trigger
  float    fSolarSetpointOffset = 2.0f; // Setpoint reduction during solar gain (C)
  // Summer simmer (Task #24) — auto-disable heating when outdoor temp stays warm
  bool     bSummerSimmer      = false;  // Enable summer simmer auto-disable
  float    fSummerThreshold   = 18.0f;  // Outdoor temp threshold for summer mode (C)
  uint8_t  iSummerMinHours    = 6;      // Consecutive hours above threshold to trigger
  // Thermal comfort adjustment (Task #28/#47) — humidity-based setpoint correction
  bool     bComfortAdjust     = false;  // Enable thermal comfort (humidity) adjustment
  float    fComfortHumidity   = 50.0f;  // Reference humidity % (no adjustment at this level)
  float    fComfortMaxOffset  = 1.0f;   // Max target temp adjustment from humidity (C)
  // Multi-area room temperature (Task #25)
  bool     bMultiArea         = false;  // Enable multi-area weighted temperature
  uint8_t  iMultiAreaCount    = 0;      // Number of configured areas (0-4)
  float    fAreaWeight[4]     = {1.0f, 1.0f, 1.0f, 1.0f};  // Weight per area
  // PID auto-tuning (Task #27)
  bool     bAutoTune          = false;  // Enable automatic PID gains tuning
  float    fAutoTuneRate      = 0.02f;  // Adjustment rate per tuning cycle (2%)
#if defined(ESP32)
  // BLE temperature sensor (Task #20, ESP32 only)
  bool     bBleEnable         = false;         // Enable BLE temperature sensor scanning
  char     sBleMAC[18]        = "";            // Bind to specific sensor MAC (empty = accept all)
  uint16_t iBleInterval       = 30;            // Scan interval in seconds (10-300)
#endif
};

#if defined(HAS_DIRECT_OT) && HAS_DIRECT_OT
struct OTDirectSettingsSection {
  uint8_t  iMode              = 1;     // OTD_MODE_GATEWAY default, persisted across reboot
  bool     bAutoDetect        = true;  // Auto-detect thermostat presence at boot
  float    fSetbackTemp       = 16.0f; // Setback temp on thermostat disconnect (°C)
  uint8_t  iSetbackTimeout    = 30;    // Seconds before thermostat considered disconnected
  bool     bEnableSlave       = true;  // Enable slave interface in master mode
  bool     bSummerMode        = false; // SM= summer mode (bit5 of master status)
  bool     bFailSafe          = true;  // FS= fail-safe setback on thermostat disconnect
  uint16_t iMsgInterval       = 100;   // MI= minimum inter-message gap (ms, 100-2550)
};
#endif

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
