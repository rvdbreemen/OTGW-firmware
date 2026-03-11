# OTGW-Firmware: C++ Code Review & Refactoring Plan

**Date:** 2026-03-01
**Version:** v1.3.0-beta
**Reviewer:** Senior C++ expert analysis
**Branch:** `claude/cpp-refactoring-analysis-hPxQM`

---

## Context

Systematic code review of the OTGW-firmware ESP8266 project, commissioned to find the worst issues, reduce complexity, and prioritize improvements by **impact vs refactoring risk**. The codebase is actively maintained with 42 ADRs documenting architectural decisions.

### User-Requested Focus Areas
1. Group global settings into structs with named sub-sections for readability
2. Scalable, centralized API routing approach replacing the monolithic if-else chain
3. WiFi reconnect as a non-blocking state machine
4. Webhook methods as a non-blocking state machine with retry

### Platform Constraints (Must Not Be Violated)
- **ESP8266**: ~40KB usable RAM, single-core, cooperative scheduling, Arduino single-TU compilation
- All `.ino` + `.h` files in one folder compile as one translation unit
- No STL, no dynamic allocation in hot paths, no HTTPS/TLS
- PROGMEM required for string literals (ADR-009), static buffers required (ADR-004)
- `safeTimers.h` `DECLARE_TIMER` / `DUE()` macros for all timing (ADR-007)

---

## Priority Scoring Method

**Priority = Impact (1-5) x (6 - Risk)**, where Risk 1=low, 5=high.

Higher score = better improvement-to-risk ratio. This ensures we prioritize changes that deliver the most improvement with the least risk of introducing regressions.

---

## PRIORITY 1 -- Score 20 | String Class in executeCommand() (ADR-004 VIOLATION)

**Files:** `src/OTGW-firmware/OTGW-Core.ino:265-434`
**ADR:** Violates ADR-004 (static buffer allocation), Violates ADR-009 (indirect)

### Problem

Three functions use the `String` class (heap-allocated) in protocol-critical paths:

```cpp
// Line 372: heap allocation on every call
String executeCommand(const String sCmd) {
    String line = OTGWSerial.readStringUntil('\n'); // heap alloc
    ...
    return _ret; // heap alloc
}

// Line 265: called at boot, returns heap String
String getpicfwversion() { ... }

// Line 308: calls executeCommand(), stores in String
bool queryOTGWgatewaymode() {
    String response = executeCommand("PR=M"); // heap alloc
}
```

`executeCommand()` is called from the command queue handler and from several places in the PIC firmware upgrade flow. Each call allocates/deallocates on ESP8266's 40KB heap, causing **fragmentation over time**. ADR-004 explicitly mandates `char[]` arrays.

### Fix

Replace all three functions with `char[]`-based equivalents:

```cpp
// New signature: caller provides output buffer
bool executeCommand(const char* sCmd, char* outBuf, size_t outSize);
void getpicfwversion(char* out, size_t outSize);
bool queryOTGWgatewaymode();  // output via bOTGWgatewaystate global (no change)
```

Use `OTGWSerial.readBytesUntil('\n', buf, size)` instead of `readStringUntil`. Update the ~6 callers of `executeCommand()` (all in OTGW-Core.ino).

**Lines changed:** ~80 in OTGW-Core.ino, ~20 caller-side adjustments
**New ADR:** ADR-049 -- String Class Prohibition in Protocol Paths (extends ADR-004)

---

## PRIORITY 2 -- Score 16 | API Route Dispatch Table (User Requested)

**Files:** `src/OTGW-firmware/restAPI.ino:118-404`, `src/OTGW-firmware/FSexplorer.ino:186-231`
**ADR:** Supports ADR-035 (RESTful compliance), ADR-002 (modular architecture)

### Problem

`processAPI()` (287 lines) is a monolithic nested if-else dispatcher. Adding one new endpoint requires modifying this function and understanding its nesting structure. Current structure:

```
processAPI()
  +-- words[1]=="api" -> words[2]=="v2" -> words[3]=={resource} -> words[4]=={sub}
      12 top-level resource branches, some with 8+ sub-branches
      Each branch manually checks GET/POST/OPTIONS
```

Additional problems:
- **Dual registration:** 3 routes bypass processAPI() via direct `httpServer.on()`: `/api/firmwarefilelist`, `/api/listfiles`, `/upload` (marked DEPRECATED)
- `onNotFound` falls through to processAPI() creating two code paths
- OPTIONS/CORS headers duplicated in multiple branches
- Deprecated v0/v1 handled inside processAPI, not at registration time

### Fix -- Route Dispatch Table Pattern

Extract resource handlers into separate named functions and use a struct-array dispatch table:

```cpp
// In restAPI.ino (top of file):
typedef void (*ApiResourceHandler)(const char words[][32], uint8_t wc, HTTPMethod method);

struct ApiRoute {
  PGM_P segment;
  ApiResourceHandler handler;
};

// Forward-declared resource handler functions:
static void handleHealth   (const char words[][32], uint8_t wc, HTTPMethod method);
static void handleSettings (const char words[][32], uint8_t wc, HTTPMethod method);
static void handleDevice   (const char words[][32], uint8_t wc, HTTPMethod method);
static void handleSensors  (const char words[][32], uint8_t wc, HTTPMethod method);
static void handleOtgw     (const char words[][32], uint8_t wc, HTTPMethod method);
static void handleFlash    (const char words[][32], uint8_t wc, HTTPMethod method);
static void handlePic      (const char words[][32], uint8_t wc, HTTPMethod method);
static void handleFirmware (const char words[][32], uint8_t wc, HTTPMethod method);
static void handleFilesys  (const char words[][32], uint8_t wc, HTTPMethod method);
static void handleWebhook  (const char words[][32], uint8_t wc, HTTPMethod method);

// Route table (DRAM, ~120 bytes for 10 entries - acceptable):
static const ApiRoute kV2Routes[] = {
  { PSTR("health"),     handleHealth },
  { PSTR("settings"),   handleSettings },
  { PSTR("device"),     handleDevice },
  { PSTR("sensors"),    handleSensors },
  { PSTR("otgw"),       handleOtgw },
  { PSTR("flash"),      handleFlash },
  { PSTR("pic"),        handlePic },
  { PSTR("firmware"),   handleFirmware },
  { PSTR("filesystem"), handleFilesys },
  { PSTR("webhook"),    handleWebhook },
  { nullptr, nullptr }  // sentinel
};

// processAPI() becomes:
void processAPI() {
  // 1. Parse URI into words[] tokens (existing logic, ~30 lines)
  // 2. Heap/URI-length guards (existing, ~20 lines)
  // 3. Common OPTIONS/CORS handler (extract to sendApiOptions())
  // 4. Dispatch: strcmp_P each kV2Routes[i].segment against words[3]
  //    -> call kV2Routes[i].handler(words, wc, method)
  // Total: ~60 lines instead of 287
}
```

**Centralize route registration** in `FSexplorer.ino:startWebserver()`:
- Remove `httpServer.on("/api/firmwarefilelist", ...)` (lines 218-219) -- deprecated
- Remove `httpServer.on("/api/listfiles", ...)` -- deprecated
- The `onNotFound` handler already falls through to `processAPI()` -- keep as-is
- Result: all API routing through `processAPI()`, no split registration

**Lines changed:** restAPI.ino restructure (~400 LOC); FSexplorer.ino -4 lines
**New ADR:** ADR-050 -- Centralized API Route Dispatch Table

---

## PRIORITY 3 -- Score 15 | msglastupdated[] -- Definition in Header

**File:** `src/OTGW-firmware/OTGW-Core.h:487`
**ADR:** Supports ADR-004 (definitions in .ino not .h)

### Problem

```cpp
time_t msglastupdated[256] = {0};  // defined in a header -- anti-pattern
```

**Note (verified):** On ESP8266, `time_t` is **4 bytes** (32-bit). The full 256-element range IS needed -- `OTdata.id` is a byte (0-255) and vendor-specific OT IDs (e.g., Remeha at 131-133) and unknown IDs can appear at any byte value. Capping to `OT_MSGID_MAX+1=134` would silently discard valid updates for high vendor IDs. **No RAM savings possible** by reducing the array size without breaking correctness.

The real issue is the **definition living in a header file**: `OTGW-Core.h` is included by `MQTTstuff.ino:16` as well as `OTGW-Core.ino`. The `#ifndef OTGWCore_h` include guard prevents double-definition, but the pattern is fragile.

### Fix

Move the definition to `OTGW-Core.ino` and add an `extern` declaration in the header:

```cpp
// OTGW-Core.h:487 -- replace definition with:
extern time_t msglastupdated[256];

// OTGW-Core.ino -- add near line 141 with other module globals:
time_t msglastupdated[256] = {0};
```

**Lines changed:** 3 lines (remove, add definition, add extern)
**No new ADR required** -- ADR-050 covers this pattern

---

## PRIORITY 4 -- Score 15 | MQTTAutoConfigBuffers -- 2800 Bytes Always in RAM

**File:** `src/OTGW-firmware/MQTTstuff.ino:42-51`
**ADR:** Supports ADR-004 and ADR-030 (heap conservation)

### Problem

```cpp
static MQTTAutoConfigBuffers mqttAutoConfigBuffers; // 1200+200+1200+200 = 2800 bytes
```

This struct is only used during HA auto-discovery (rare, user-triggered event), yet occupies **2800 bytes permanently** (~7% of usable RAM). The existing `MQTTAutoConfigSessionLock` already detects re-entry, so this was designed to prevent allocation but the trade-off is too costly.

### Fix

Gate the struct with lazy first-touch allocation (never freed -- acceptable for embedded, avoids repeated alloc/dealloc):

```cpp
static MQTTAutoConfigBuffers* pMqttAcBufs = nullptr;

// In doAutoConfigure(), before use:
if (!pMqttAcBufs) pMqttAcBufs = new MQTTAutoConfigBuffers();
if (!pMqttAcBufs) { /* OOM: log and return */ return; }
```

This trades permanent RAM cost for a one-time allocation only when actually used. If MQTT is disabled, the 2800 bytes are never allocated.

Alternatively, reduce buffer sizes: `line` 1200->512, `msg` 1200->512 = **~1400 bytes saved permanently**.

**Lines changed:** ~25 in MQTTstuff.ino
**No new ADR required**

---

## PRIORITY 5 -- Score 12 | Settings + State Objects: Two Encapsulating Structs (User Requested)

**File:** `src/OTGW-firmware/OTGW-firmware.h:112-264`, `settingStuff.ino`
**ADR:** New ADR-051 -- Settings and Runtime State Organization

### Problem

`OTGW-firmware.h` has two fundamentally different types of globals mixed together in a flat namespace with only comments as dividers:

1. **Persistent settings** (`setting*` variables, lines 167-264): serialized to LittleFS, restored on boot, represent user configuration. 62+ variables with no language-level grouping.
2. **Runtime state** (`bOTGW*`, `bPIC*`, `sPIC*`, `isESPFlashing`, etc., lines 112-164): derived from live system operation, never persisted, represent "what is the system doing right now".

Neither group can be passed as a unit, snapshot, or reset atomically. `settingStuff.ino` must enumerate every `setting*` variable individually to read/write LittleFS.

### Fix -- Two Encapsulating Objects: `settings` and `state`

#### Part A: `OTGWSettings settings` -- All Persistent Configuration

Define a two-level hierarchy: `OTGWSettings` contains named sub-section structs. One global `settings` instance replaces all flat `setting*` variables. Access becomes `settings.mqtt.sBroker`, `settings.ntp.sTimezone`, etc.

**Hungarian notation prefixes:** `b` (bool), `s` (string/char[]), `i` (int/uint), `f` (float)

```cpp
// In OTGW-firmware.h -- REPLACE all flat setting* variables with:

// -- Sub-section structs (POD, aggregate-initializable) --

struct MQTTSection {
  bool    bEnable          = true;
  bool    bSecure          = false;
  char    sBroker[65]      = "homeassistant.local";
  int16_t iBrokerPort      = 1883;
  char    sUser[41]        = "";
  char    sPasswd[41]      = "";
  char    sHaprefix[41]    = HOME_ASSISTANT_DISCOVERY_PREFIX;
  bool    bHaRebootDetect  = true;
  char    sTopTopic[41]    = "OTGW";
  char    sUniqueid[41]    = "";
  bool    bOTmessage       = false;
  bool    bSeparateSources = false;
};

struct NTPSection {
  bool bEnable        = true;
  char sTimezone[65]  = NTP_DEFAULT_TIMEZONE;
  char sHostname[65]  = NTP_HOST_DEFAULT;
  bool bSendtime      = false;
};

struct SensorsSection {             // Dallas DS18B20 external sensors
  bool    bEnabled       = false;
  bool    bLegacyFormat  = false;
  int8_t  iPin           = 10;
  int16_t iInterval      = 20;
};

struct S0Section {
  bool     bEnabled      = false;
  uint8_t  iPin          = 12;
  uint16_t iDebounceTime = 80;
  uint16_t iPulsekw      = 1000;
  uint16_t iInterval     = 60;
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
  int8_t iTriggerBit      = 1;
  char   sPayload[201]    = "";
  char   sContentType[32] = "application/json";
};

struct UISection {
  bool bAutoScroll      = true;
  bool bShowTimestamp   = true;
  bool bCaptureMode     = false;
  bool bAutoScreenshot  = false;
  bool bAutoDownloadLog = false;
  bool bAutoExport      = false;
  int  iGraphTimeWindow = 60;
};

struct OTGWBootSection {            // PIC boot-time command injection
  bool bEnable        = false;
  char sCommands[129] = "";
};

// -- Top-level encapsulating object --

struct OTGWSettings {
  // Device-level fields (no sub-section: universal device identity)
  char sHostname[41] = _HOSTNAME;
  bool bLEDblink     = true;
  bool bDarkTheme    = false;
  bool bMyDEBUG      = false;

  // Named sections
  MQTTSection    mqtt;
  NTPSection     ntp;
  SensorsSection sensors;
  S0Section      s0;
  OutputsSection outputs;
  WebhookSection webhook;
  UISection      ui;
  OTGWBootSection otgw;
};

// Single global settings instance
OTGWSettings settings;
```

**Why a single settings object is superior to separate `cfgMQTT`, `cfgNTP` etc.:**
- `settings = OTGWSettings{}` performs a factory reset of ALL settings in one statement
- `settings.mqtt = MQTTSection{}` resets only the MQTT category
- Functions can receive `OTGWSettings&` to operate on the full config atomically
- `memcpy(&backup, &settings, sizeof(settings))` snapshots entire configuration in one call
- `sizeof(OTGWSettings)` gives exact RAM footprint at compile time

#### Settings Rename Table (Full, Mechanical Find/Replace)

| Old name | New name |
|---|---|
| `settingHostname` | `settings.sHostname` |
| `settingLEDblink` | `settings.bLEDblink` |
| `settingDarkTheme` | `settings.bDarkTheme` |
| `settingMyDEBUG` | `settings.bMyDEBUG` |
| `settingMQTTenable` | `settings.mqtt.bEnable` |
| `settingMQTTbroker` | `settings.mqtt.sBroker` |
| `settingMQTTbrokerPort` | `settings.mqtt.iBrokerPort` |
| `settingMQTTuser` | `settings.mqtt.sUser` |
| `settingMQTTpasswd` | `settings.mqtt.sPasswd` |
| `settingMQTThaprefix` | `settings.mqtt.sHaprefix` |
| `settingMQTTharebootdetection` | `settings.mqtt.bHaRebootDetect` |
| `settingMQTTtopTopic` | `settings.mqtt.sTopTopic` |
| `settingMQTTuniqueid` | `settings.mqtt.sUniqueid` |
| `settingMQTTOTmessage` | `settings.mqtt.bOTmessage` |
| `settingMQTTSeparateSources` | `settings.mqtt.bSeparateSources` |
| `settingNTPenable` | `settings.ntp.bEnable` |
| `settingNTPtimezone` | `settings.ntp.sTimezone` |
| `settingNTPhostname` | `settings.ntp.sHostname` |
| `settingNTPsendtime` | `settings.ntp.bSendtime` |
| `settingGPIOSENSORSenabled` | `settings.sensors.bEnabled` |
| `settingGPIOSENSORSlegacyformat` | `settings.sensors.bLegacyFormat` |
| `settingGPIOSENSORSpin` | `settings.sensors.iPin` |
| `settingGPIOSENSORSinterval` | `settings.sensors.iInterval` |
| `settingS0COUNTERenabled` | `settings.s0.bEnabled` |
| `settingS0COUNTERpin` | `settings.s0.iPin` |
| `settingS0COUNTERdebouncetime` | `settings.s0.iDebounceTime` |
| `settingS0COUNTERpulsekw` | `settings.s0.iPulsekw` |
| `settingS0COUNTERinterval` | `settings.s0.iInterval` |
| `settingGPIOOUTPUTSenabled` | `settings.outputs.bEnabled` |
| `settingGPIOOUTPUTSpin` | `settings.outputs.iPin` |
| `settingGPIOOUTPUTStriggerBit` | `settings.outputs.iTriggerBit` |
| `settingWebhookEnabled` | `settings.webhook.bEnabled` |
| `settingWebhookURLon` | `settings.webhook.sURLon` |
| `settingWebhookURLoff` | `settings.webhook.sURLoff` |
| `settingWebhookTriggerBit` | `settings.webhook.iTriggerBit` |
| `settingWebhookPayload` | `settings.webhook.sPayload` |
| `settingWebhookContentType` | `settings.webhook.sContentType` |
| `settingUIAutoScroll` | `settings.ui.bAutoScroll` |
| `settingUIShowTimestamp` | `settings.ui.bShowTimestamp` |
| `settingUICaptureMode` | `settings.ui.bCaptureMode` |
| `settingUIAutoScreenshot` | `settings.ui.bAutoScreenshot` |
| `settingUIAutoDownloadLog` | `settings.ui.bAutoDownloadLog` |
| `settingUIAutoExport` | `settings.ui.bAutoExport` |
| `settingUIGraphTimeWindow` | `settings.ui.iGraphTimeWindow` |
| `settingOTGWcommandenable` | `settings.otgw.bEnable` |
| `settingOTGWcommands` | `settings.otgw.sCommands` |

`settingStuff.ino` update: `readSettings()`, `writeSettings()`, and `updateSetting()` use the new member paths. JSON string keys (e.g., `"MQTTbroker"`) remain identical -- no migration needed for existing LittleFS settings files.

#### Part B: `OTGWState state` -- All Runtime State

A parallel `state` struct replaces all `bOTGW*`, `bPIC*`, `sPIC*`, and flash-operation globals. These are never persisted. The struct makes their transient nature explicit and groups them by the system component they describe -- using the same two-level sub-structure pattern as settings:

```cpp
// In OTGW-firmware.h -- REPLACE runtime state flat globals with:

// -- Named sub-section state structs --

struct PICSection {            // state.pic
  bool bAvailable     = false;           // was bPICavailable
  char sFwversion[32] = "no pic found";  // was sPICfwversion
  char sDeviceid[32]  = "no pic found";  // was sPICdeviceid
  char sType[32]      = "no pic found";  // was sPICtype
};

struct OTGWProtocol {          // state.otgw
  bool bOnline           = true;   // was bOTGWonline
  bool bPSmode           = false;  // was bPSmode
  bool bGatewayMode      = false;  // was bOTGWgatewaystate
  bool bGatewayModeKnown = false;  // was bOTGWgatewaystateKnown
  bool bBoilerState      = false;  // was bOTGWboilerstate
  bool bThermostatState  = false;  // was bOTGWthermostatstate
};

struct MQTTRuntimeSection {    // state.mqtt
  bool bConnected        = false;  // was statusMQTTconnection
};

struct FlashSection {          // state.flash
  bool bESPactive        = false;  // was isESPFlashing
  bool bPICactive        = false;  // was isPICFlashing
  char sError[129]       = "";     // was errorupgrade
  char sPICfile[65]      = "";     // was currentPICFlashFile
  int  iPICprogress      = 0;      // was currentPICFlashProgress
};

struct DebugSection {          // state.debug
  bool bOTmsg     = true;   // was bDebugOTmsg
  bool bRestAPI   = false;  // was bDebugRestAPI
  bool bMQTT      = false;  // was bDebugMQTT
  bool bSensors   = false;  // was bDebugSensors
  bool bSensorSim = false;  // was bDebugSensorSimulation
};

struct UptimeSection {         // state.uptime
  uint32_t iSeconds      = 0;  // was upTimeSeconds
  uint32_t iRebootCount  = 0;  // was rebootCount
};

// -- Top-level runtime state object --

struct OTGWState {
  PICSection         pic;     // state.pic.bAvailable, state.pic.sFwversion
  OTGWProtocol       otgw;   // state.otgw.bOnline, state.otgw.bBoilerState
  MQTTRuntimeSection mqtt;   // state.mqtt.bConnected
  FlashSection       flash;  // state.flash.bESPactive, state.flash.iPICprogress
  DebugSection       debug;  // state.debug.bOTmsg, state.debug.bMQTT
  UptimeSection      uptime; // state.uptime.iSeconds, state.uptime.iRebootCount
};

// Single global runtime state instance
OTGWState state;
```

#### State Rename Table (Find/Replace)

| Old name | New name |
|---|---|
| `bPICavailable` | `state.pic.bAvailable` |
| `sPICfwversion` | `state.pic.sFwversion` |
| `sPICdeviceid` | `state.pic.sDeviceid` |
| `sPICtype` | `state.pic.sType` |
| `bOTGWonline` | `state.otgw.bOnline` |
| `bPSmode` | `state.otgw.bPSmode` |
| `bOTGWgatewaystate` | `state.otgw.bGatewayMode` |
| `bOTGWgatewaystateKnown` | `state.otgw.bGatewayModeKnown` |
| `bOTGWboilerstate` | `state.otgw.bBoilerState` |
| `bOTGWthermostatstate` | `state.otgw.bThermostatState` |
| `statusMQTTconnection` | `state.mqtt.bConnected` |
| `isESPFlashing` | `state.flash.bESPactive` |
| `isPICFlashing` | `state.flash.bPICactive` |
| `errorupgrade` | `state.flash.sError` |
| `currentPICFlashFile` | `state.flash.sPICfile` |
| `currentPICFlashProgress` | `state.flash.iPICprogress` |
| `bDebugOTmsg` | `state.debug.bOTmsg` |
| `bDebugRestAPI` | `state.debug.bRestAPI` |
| `bDebugMQTT` | `state.debug.bMQTT` |
| `bDebugSensors` | `state.debug.bSensors` |
| `bDebugSensorSimulation` | `state.debug.bSensorSim` |
| `upTimeSeconds` | `state.uptime.iSeconds` |
| `rebootCount` | `state.uptime.iRebootCount` |

#### Debug Macros Update

```cpp
// MQTTstuff.ino:
#define MQTTDebugTln(...) ({ if (state.debug.bMQTT)    DebugTln(__VA_ARGS__); })

// OTGW-Core.ino:
#define OTGWDebugTln(...) ({ if (state.debug.bOTmsg)   DebugTln(__VA_ARGS__); })

// restAPI.ino:
#define RESTDebugTln(...) ({ if (state.debug.bRestAPI) DebugTln(__VA_ARGS__); })
```

#### Inline Helpers Update

```cpp
inline bool isFlashing() {
  return state.flash.bESPactive || state.flash.bPICactive;
}
```

#### Variable Disposition: Module-Local vs Header-Declared

| Variable | Action | Destination |
|---|---|---|
| `DallasrealDevice[MAXDALLASDEVICES]` | MOVE definition to module | `sensors_ext.ino` |
| `DallasrealDeviceCount` | MOVE definition to module | `sensors_ext.ino` |
| `bSensorsDetected` | MOVE definition to module | `sensors_ext.ino` |
| `OTGWdallasdataid` | MOVE definition to module | `sensors_ext.ino` |
| `OTGWs0pulseCount` | MOVE definition to module | `s0PulseCount.ino` |
| `OTGWs0pulseCountTot` | MOVE definition to module | `s0PulseCount.ino` |
| `OTGWs0powerkw` | MOVE definition to module | `s0PulseCount.ino` |
| `OTGWs0lasttime` | MOVE definition to module | `s0PulseCount.ino` |
| `OTGWs0dataid` | MOVE definition to module | `s0PulseCount.ino` |
| `cMsg[CMSG_SIZE]` | KEEP flat in header | cross-module scratch buffer |
| `sMessage[257]` | KEEP flat in header | WebSocket message buffer |

**Forward declarations** for cross-module access: Arduino single-TU concatenates `.ino` files alphabetically. `restAPI.ino` (r) comes before `sensors_ext.ino` (s) and `s0PulseCount.ino` (s), so variables defined there are not visible to `restAPI.ino` without forward declarations. Add `extern` declarations in `OTGW-firmware.h`:

```cpp
// OTGW-firmware.h -- extern declarations (definitions live in the .ino files):
extern int       DallasrealDeviceCount;
extern bool      bSensorsDetected;
extern byte      OTGWdallasdataid;
extern uint16_t  OTGWs0pulseCount;
extern uint32_t  OTGWs0pulseCountTot;
extern float     OTGWs0powerkw;
extern time_t    OTGWs0lasttime;
extern byte      OTGWs0dataid;
```

#### Benefits Summary

| Aspect | Before | After |
|---|---|---|
| Find all settings | grep 62 names | `settings.*` |
| Factory reset settings | set 62 vars | `settings = OTGWSettings{}` |
| Reset MQTT settings only | set 12 vars | `settings.mqtt = MQTTSection{}` |
| Find all runtime state | grep 20+ names | `state.*` |
| Pass entire config to fn | impossible | `void fn(OTGWSettings& cfg)` |
| Distinguish config vs runtime | by name prefix only | by object: `settings` vs `state` |
| Check if PIC responding | `bPICavailable` | `state.pic.bAvailable` |
| Inspect OT protocol state | `bOTGWgatewaystate` | `state.otgw.bGatewayMode` |
| Reset all debug flags | set 5 bools | `state.debug = DebugSection{}` |
| Flash progress check | `isESPFlashing \|\| isPICFlashing` | `state.flash.bESPactive \|\| state.flash.bPICactive` |

**Risk mitigation:** Rename one sub-section per commit; each commit independently buildable.

**Lines changed:** ~250 settings substitutions + ~100 state substitutions + ~170 lines of struct definitions
**New ADR:** ADR-051 -- Settings and Runtime State Organization with Dual Encapsulating Objects

---

## PRIORITY 6 -- Score 12 | Command Queue -- Header Definition + Rename for Clarity

**File:** `src/OTGW-firmware/OTGW-Core.h:496-498`, `OTGW-Core.ino:~1751`
**ADR:** ADR-016 (OpenTherm Command Queue)

### Problem (Corrected)

The command queue is **not a bug** -- `handleOTGWqueue()` implements a fill-pointer pattern with left-shift deletion, not a circular buffer. When an entry is consumed or aged out, the remaining entries shift left and `cmdptr` decrements. Bounds guards prevent overflow. A full queue returns early with a debug log message. ADR-016 is correct.

The actual problem is: (1) `cmdqueue[]` and `cmdptr` are **defined** in the header (same anti-pattern as `msglastupdated[]`), and (2) `cmdptr` is a misleading name -- it is a fill count (current queue size), not a pointer.

### Fix

1. Move `cmdqueue[]` and `cmdptr` definitions to `OTGW-Core.ino`; add `extern` declarations in header
2. Rename `cmdptr` -> `cmdQueueSize` throughout for clarity
3. Add a comment documenting the fill-pointer + left-shift-delete pattern

```cpp
// OTGW-Core.h -- replace definitions with:
extern struct OT_cmd_t cmdqueue[CMDQUEUE_MAX];
extern int cmdQueueSize;  // number of active entries (0..CMDQUEUE_MAX)

// OTGW-Core.ino -- add near line 141:
struct OT_cmd_t cmdqueue[CMDQUEUE_MAX];
int cmdQueueSize = 0;  // fill-pointer: entries are 0..cmdQueueSize-1
                       // deletion: left-shift remaining entries, decrement cmdQueueSize
```

**Lines changed:** ~10 lines
**Update ADR-016** to document fill-pointer + left-shift-delete pattern explicitly

---

## PRIORITY 7 -- Score 10 | Duplicate Conditional Debug Macro Pattern

**Files:** `OTGW-Core.ino:14-20`, `MQTTstuff.ino:23-28`, `restAPI.ino:16-21`, `OTGW-firmware.ino:31-37`, (similar in `sensors_ext.ino`)
**ADR:** ADR-002 (modular architecture)

### Problem

The same 6-macro pattern is copy-pasted with a different flag variable and prefix:

```cpp
#define OTGWDebugTln(...) ({ if (bDebugOTmsg)  DebugTln(__VA_ARGS__); })
#define MQTTDebugTln(...) ({ if (bDebugMQTT)   DebugTln(__VA_ARGS__); })
#define RESTDebugTln(...) ({ if (bDebugRestAPI) DebugTln(__VA_ARGS__); })
```

Five separate definitions of the same pattern, creating maintenance risk if `DebugTln` signature changes.

### Fix

Add a parameterized macro factory to `Debug.h`:

```cpp
// Debug.h -- after existing macro definitions, add:
// Module-specific conditional debug macros -- use MODULE_DEBUG_MACROS(PREFIX, FLAG)
// Usage: MODULE_DEBUG_MACROS(MQTT, state.debug.bMQTT) -> MQTTDebugTln(), etc.
```

Or simply document the pattern in a comment and accept the duplication as intentional (Arduino single-TU means they don't conflict; the copies are per-file scoping).

**Lines changed:** 5-10 in Debug.h
**No new ADR required**

---

## PRIORITY 8 -- Score 6 | Static Local Variables for Throttle State

**Files:** `OTGW-Core.ino:290-293`, `helperStuff.ino` (dayChanged, minuteChanged etc.)
**ADR:** ADR-002, ADR-007 (timer-based scheduling)

### Problem

```cpp
bool queryOTGWgatewaymode() {
  static uint32_t lastGatewayModeQueryMs = 0;
  static bool cachedGatewayMode = false;
  static bool hasCachedGatewayMode = false;
  ...
}
```

Hidden per-function state cannot be tested or reset without a reboot.

### Fix

**Acceptable for production embedded code**; document via code comment that this is intentional single-thread-safe throttle state. Do not refactor -- the risk of introducing bugs exceeds the maintainability benefit given the project is single-threaded.

Add a `// DESIGN: single-threaded throttle state; not testable without reboot` comment.

**Lines changed:** 4-6 (comments only)
**No ADR change needed**

---

## PRIORITY 9 -- Score 12 | WiFi Reconnect State Machine (User Requested)

**File:** `src/OTGW-firmware/OTGW-firmware.ino:120-156`, `networkStuff.h`
**ADR:** ADR-007 (timer-based scheduling), ADR-010 (concurrent network services)

### Problem

`restartWifi()` (`OTGW-firmware.ino:126-156`) has a hidden `static int iTryRestarts = 0` counter and uses a **blocking `while` loop** with up to 30-second `delay(100)` + `feedWatchDog()` spins. During those 30 seconds: no OpenTherm data is processed, no MQTT published, no HTTP requests served -- a **30-second blackout** on a heating system controller.

Additional issues:
- `isConnected` (`networkStuff.h:118`) is set at boot and never updated during the main loop; `doBackgroundTasks()` uses `WiFi.status()` directly instead of this variable -- inconsistency
- Called only from `doTaskMinuteChanged()` -- means up to 60 seconds before a reconnect attempt begins
- No exponential backoff: every attempt is identical (30-second timeout x 15 attempts = up to 7.5 min of cumulative blocking)
- Hidden static `iTryRestarts` cannot be inspected or reset without a reboot

### Fix -- Non-Blocking WiFi Reconnect State Machine

Replace `restartWifi()` with a non-blocking state machine called from `doBackgroundTasks()`:

```cpp
// In OTGW-firmware.ino:
enum WifiState_t {
  WIFI_IDLE,         // connected, monitoring
  WIFI_DISCONNECTED, // just dropped -- start reconnect immediately
  WIFI_CONNECTING,   // waiting non-blocking for connection
  WIFI_RECONNECTED,  // just came back -- restart services
  WIFI_FAILED        // too many retries -- trigger reboot
};
WifiState_t wifiState = WIFI_IDLE;
int wifiRetryCount = 0;

// In doBackgroundTasks() -- called every loop iteration:
void loopWifi() {
  DECLARE_TIMER_SEC(timerWifiRetry, 5, CATCH_UP_MISSED_TICKS);
  switch (wifiState) {
    case WIFI_IDLE:
      if (WiFi.status() != WL_CONNECTED) {
        DebugTln(F("WiFi: connection lost, starting reconnect"));
        isConnected = false;
        wifiRetryCount = 0;
        wifiState = WIFI_DISCONNECTED;
      }
      break;

    case WIFI_DISCONNECTED:
      WiFi.begin();  // uses stored credentials
      RESTART_TIMER(timerWifiRetry);
      wifiState = WIFI_CONNECTING;
      break;

    case WIFI_CONNECTING:
      if (WiFi.status() == WL_CONNECTED) {
        wifiState = WIFI_RECONNECTED;
        wifiRetryCount = 0;
      } else if DUE(timerWifiRetry) {
        wifiRetryCount++;
        DebugTf(PSTR("WiFi: connect attempt %d failed\r\n"), wifiRetryCount);
        if (wifiRetryCount >= 15) {
          wifiState = WIFI_FAILED;
        } else {
          wifiState = WIFI_DISCONNECTED;  // retry
        }
      }
      break;

    case WIFI_RECONNECTED:
      isConnected = true;
      startTelnet();
      startOTGWstream();
      startMQTT();
      startWebSocket();
      wifiState = WIFI_IDLE;
      DebugTln(F("WiFi: reconnected, services restarted"));
      break;

    case WIFI_FAILED:
      doRestart(PSTR("WiFi: too many reconnect failures"));
      break;
  }
}
```

**Key improvements over current `restartWifi()`:**
- **Non-blocking:** no `delay()` in the reconnect path; main loop continues processing OT data
- `isConnected` kept accurate -- no more stale boolean
- State is inspectable via telnet debug or API
- Exponential backoff can be added easily by varying `RESTART_TIMER` interval per retry count
- `doTaskMinuteChanged()` call to `restartWifi()` is replaced by always-running `loopWifi()`

Remove `restartWifi()` and the `doTaskMinuteChanged()` call. Add `loopWifi()` call to `doBackgroundTasks()` unconditionally (before the WiFi-connected guard).

**Lines changed:** ~50 new lines + delete old `restartWifi()` (~30 lines)
**New ADR:** ADR-047 -- Non-Blocking WiFi Reconnect State Machine

---

## PRIORITY 10 -- Score 12 | Webhook Blocking HTTP + State Machine (User Requested)

**File:** `src/OTGW-firmware/webhook.ino:168-233`
**ADR:** ADR-007, ADR-003

### Problem

`sendWebhook()` blocks the main loop for up to **3 seconds** (`http.setTimeout(3000)`). Every failed webhook = 3-second freeze: no OT data processing, no MQTT, no HTTP. `evalWebhook()` is called every main loop iteration (not rate-limited) and on state change immediately calls `sendWebhook()` -- no retry, no backoff, no decoupling of detection from sending.

Additional issues:
- `http.errorToString(code).c_str()` creates a `String` object (heap allocation, ADR-004 violation)
- If `sendWebhook()` is interrupted mid-call (e.g., by heap pressure), no retry on failure
- `evalWebhook()` and `sendWebhook()` are tightly coupled -- impossible to add retry without restructure

### Fix -- Webhook State Machine with Retry and Reduced Timeout

```cpp
enum WebhookState_t {
  WH_IDLE,          // monitoring trigger bit, no pending send
  WH_PENDING,       // state change detected, ready to send on next iteration
  WH_RETRY_WAIT     // last attempt failed, waiting before retry
};
static WebhookState_t webhookState = WH_IDLE;
static bool webhookPendingStateOn = false;
static uint8_t webhookRetryCount = 0;

void loopWebhook() {  // replaces evalWebhook()
  DECLARE_TIMER_SEC(timerWebhookRetry, 30, SKIP_MISSED_TICKS);

  bool bitState = evalTriggerBit();  // extract trigger-bit evaluation

  switch (webhookState) {
    case WH_IDLE:
      if (!webhookInitialized) {
        webhookLastState = bitState;
        webhookInitialized = true;
        break;
      }
      if (bitState != webhookLastState) {
        webhookLastState = bitState;
        webhookPendingStateOn = bitState;
        webhookRetryCount = 0;
        webhookState = WH_PENDING;
        DebugTf(PSTR("Webhook: bit changed -> %s, queuing send\r\n"),
                bitState ? "ON" : "OFF");
      }
      break;

    case WH_PENDING:
      if (WiFi.status() != WL_CONNECTED) break;
      {
        bool ok = attemptSendWebhook(webhookPendingStateOn);  // 1000ms timeout
        if (ok) {
          webhookState = WH_IDLE;
          webhookRetryCount = 0;
        } else {
          webhookRetryCount++;
          if (webhookRetryCount >= 3) {
            DebugTln(F("Webhook: max retries reached, giving up"));
            webhookState = WH_IDLE;
          } else {
            RESTART_TIMER(timerWebhookRetry);
            webhookState = WH_RETRY_WAIT;
            DebugTf(PSTR("Webhook: send failed, retry %d/3 in 30s\r\n"),
                    webhookRetryCount);
          }
        }
      }
      break;

    case WH_RETRY_WAIT:
      if DUE(timerWebhookRetry) webhookState = WH_PENDING;
      break;
  }
}
```

`attemptSendWebhook()`: Same as current `sendWebhook()` but:
1. `http.setTimeout(1000)` -- reduced from 3000ms (local network should respond in <500ms)
2. Returns `bool` (true = HTTP 2xx, false = any error)
3. Replace `http.errorToString(code).c_str()` with `char errBuf[24]; snprintf_P(...)` (ADR-004 compliant)

**Note on "truly non-blocking":** ESP8266 `HTTPClient` is inherently synchronous -- achieving zero-block sending requires async HTTP libraries not available within platform constraints. The 1-second timeout is the practical minimum that avoids false failures on a local LAN.

**Lines changed:** ~60 new lines; replace `evalWebhook()` (~35 lines)
**New ADR:** ADR-048 -- Webhook Non-Blocking State Machine with Retry

---

## DO NOT TOUCH (High Risk, Medium Impact)

These would require fundamental architectural changes inconsistent with the ESP8266 Arduino model:

- **Decomposing OTGW-Core.ino** (3306 lines) into multiple files -- breaks Arduino `.ino` architecture
- **Moving all definitions from headers to .cpp** -- not applicable in Arduino single-TU
- **Adding virtual functions or RTTI** -- memory cost unacceptable on ESP8266
- **Replacing the `safeTimers.h` macro system** -- well-tested, handles 49-day rollover correctly
- **Refactoring the MQTT 6-state machine** -- correct, well-designed, covered by ADR-006

---

## Proposed New ADRs

### ADR-049: String Class Prohibition in Protocol Paths
- **Status:** PROPOSED
- **Decision:** `String` is prohibited in all serial I/O, command execution, and protocol parsing paths
- **Extends:** ADR-004 (static buffer allocation)
- **Required pattern:** `char buf[N]; OTGWSerial.readBytesUntil('\n', buf, N);`
- **Violations to remediate:** `executeCommand()`, `getpicfwversion()`, `queryOTGWgatewaymode()`
- **Acceptable use:** WiFiManager configuration portal (library-internal only)

### ADR-050: Centralized API Route Dispatch Table
- **Status:** PROPOSED
- **Decision:** All v2 API routing via `kV2Routes[]` struct-array dispatch table
- **Pattern:** `ApiRoute { PGM_P segment; ApiResourceHandler handler }` with sentinel entry
- **Benefit:** Adding endpoint = +1 table entry + 1 handler function
- **Removes:** Dual registration in FSexplorer.ino for deprecated v0/v1 endpoints
- **Constraint:** Route table stays in DRAM (~120-160 bytes acceptable); string literals in PROGMEM

### ADR-051: Settings and Runtime State Organization with Dual Encapsulating Objects
- **Status:** PROPOSED
- **Two objects, parallel two-level design with Hungarian-prefixed members (b/s/i/f):**
  - `OTGWSettings settings` -- all 62+ persistent `setting*` globals, sub-sections: `settings.mqtt.sBroker`, `settings.ntp.sTimezone`, `settings.sensors.iPin`, `settings.webhook.bEnabled`, `settings.ui.iGraphTimeWindow`, etc.
  - `OTGWState state` -- all 20+ transient runtime globals, named sub-section types: `PICSection pic` (`state.pic.bAvailable`), `OTGWProtocol otgw` (`state.otgw.bOnline`), `MQTTRuntimeSection mqtt` (`state.mqtt.bConnected`), `FlashSection flash`, `DebugSection debug` (`state.debug.bMQTT`), `UptimeSection uptime` (`state.uptime.iSeconds`)
- **Justification:** Language-level distinction between "what the user configured" (settings) and "what the system is currently doing" (state); consistent two-level dotted access for both
- **JSON keys:** Unchanged for settings (backward compat with existing LittleFS settings files)
- **Factory reset patterns:** `settings = OTGWSettings{}` / `settings.mqtt = MQTTSection{}` / `state.debug = {}`
- **Migration:** One sub-section per commit; settings and state migrations are independent

### ADR-047: Non-Blocking WiFi Reconnect State Machine
- **Status:** PROPOSED
- **Decision:** Replace blocking `restartWifi()` with non-blocking `loopWifi()` state machine
- **States:** `WIFI_IDLE` -> `WIFI_DISCONNECTED` -> `WIFI_CONNECTING` -> `WIFI_RECONNECTED` / `WIFI_FAILED`
- **Constraint:** No `delay()` in any reconnect path; uses `DECLARE_TIMER` + `DUE()` for backoff
- **Service restart:** On `WIFI_RECONNECTED`: Telnet, OTGWstream, MQTT, WebSocket (same as before)
- **`isConnected`:** Updated synchronously with state transitions (no staleness)
- **Justification:** Eliminates up to 30-second main loop freeze during WiFi reconnect; critical for heating system that must not stop processing OpenTherm data during network recovery

### ADR-048: Webhook Non-Blocking State Machine with Retry
- **Status:** PROPOSED
- **Decision:** Replace direct `evalWebhook()` call with `loopWebhook()` state machine
- **States:** `WH_IDLE` -> `WH_PENDING` -> `WH_RETRY_WAIT` -> `WH_IDLE`
- **HTTP timeout:** Reduce from 3000ms to 1000ms (local network constraint per ADR-003/032)
- **Retry policy:** Up to 3 attempts with 30-second backoff; silent discard after max retries
- **Detection/send decoupling:** Trigger bit evaluation continues during retry wait
- **ADR-004 compliance:** Remove `String` from `http.errorToString()` path; use `char errBuf[24]`
- **Justification:** Prevents 3-second main loop blocking on webhook send failure; adds retry for transient local network failures

---

## Critical Files to Modify

| File | Changes | Priority |
|---|---|---|
| `src/OTGW-firmware/OTGW-Core.ino` | Fix `executeCommand()` String->char[], rename cmdptr | P1, P6 |
| `src/OTGW-firmware/restAPI.ino` | Route dispatch table, extract resource handlers | P2 |
| `src/OTGW-firmware/FSexplorer.ino` | Remove deprecated direct route registrations | P2 |
| `src/OTGW-firmware/OTGW-Core.h` | Move msglastupdated[]/cmdqueue definitions to .ino | P3, P6 |
| `src/OTGW-firmware/MQTTstuff.ino` | Reduce MQTTAutoConfigBuffers sizes | P4 |
| `src/OTGW-firmware/OTGW-firmware.h` | OTGWSettings struct + OTGWState struct + extern declarations | P5 |
| `src/OTGW-firmware/settingStuff.ino` | Update readSettings/writeSettings/updateSetting | P5 |
| `src/OTGW-firmware/sensors_ext.ino` | Move Dallas array definitions to module scope | P5 |
| `src/OTGW-firmware/s0PulseCount.ino` | Move S0 counter variable definitions to module scope | P5 |
| `src/OTGW-firmware/OTGW-firmware.ino` | loopWifi() replaces restartWifi() | P9 |
| `src/OTGW-firmware/webhook.ino` | loopWebhook() replaces evalWebhook() + retry state | P10 |
| `src/OTGW-firmware/Debug.h` | Document conditional debug macro pattern | P7 |
| `docs/adr/ADR-049-*.md` | New: String prohibition in protocol paths | P1 |
| `docs/adr/ADR-050-*.md` | New: API route dispatch table | P2 |
| `docs/adr/ADR-051-*.md` | New: Settings and state dual encapsulating objects | P5 |
| `docs/adr/ADR-047-*.md` | New: Non-blocking WiFi reconnect state machine | P9 |
| `docs/adr/ADR-048-*.md` | New: Webhook state machine with retry | P10 |
| `docs/adr/ADR-016-*.md` | Update: document cmdqueue fill-pointer pattern | P6 |

---

## Existing Utilities to Reuse

- **`safeTimers.h`** `DECLARE_TIMER_MS` / `DUE` -- use for all throttle/debounce (do not replace)
- **`checkGPIOConflict()`** in `settingStuff.ino:54` -- already centralized, reference in ADR-051
- **`sendApiError()` / `sendApiMethodNotAllowed()`** in `restAPI.ino:27-40` -- keep and extend
- **`PROGMEM_readAnything<T>()`** in `helperStuff.ino:16` -- use for PROGMEM route table reads
- **`extractJsonFieldText()`** in `helperStuff.ino` -- use in new API handler functions
- **`getHeapHealth()` / `canPublishMQTT()`** -- heap guards to apply in new API handlers
- **`strlcpy`, `snprintf_P`, `strncat_P`** -- approved safe string functions (use exclusively)

---

## Implementation Order

Execute as separate commits, each independently buildable and testable:

1. **ADR-049 + Fix executeCommand()** -- highest impact, isolated to OTGW-Core.ino
2. **P3 Fix: msglastupdated[]** -- 2-line change, code quality improvement
3. **ADR-050 + API dispatch table** -- restructure restAPI.ino, no functional change
4. **P4 Fix: MQTTAutoConfigBuffers lazy alloc** -- isolated to MQTTstuff.ino
5. **ADR-051 + Settings struct grouping** -- largest scope, do per-struct incrementally
6. **P6: cmdqueue audit** -- rename cmdptr, move definition, add documentation
7. **P7: Debug macro documentation** -- final cleanup
8. **P9: WiFi reconnect state machine** -- isolated to OTGW-firmware.ino
9. **P10: Webhook state machine** -- isolated to webhook.ino
10. **P8: Static throttle state comments** -- comments only, no code changes

---

## Verification Checklist

For each change:

1. **Build:** `make` binaries must succeed with zero warnings on new code
2. **Flash & connect:** Telnet to port 23, verify boot sequence completes (WD armed, PIC detected)
3. **API routing:** `curl http://{ip}/api/v2/health` -> `{"status":"ok"}` (or equivalent)
4. **Settings round-trip:** Change a setting via `POST /api/v2/settings`, reboot, verify persisted
5. **OT command:** `POST /api/v2/otgw/commands` with `{"command":"PR=A"}` -> 202 Accepted
6. **executeCommand fix:** Monitor telnet; `PR=M` query should not trigger heap allocation spike
7. **RAM baseline:** `GET /api/v2/device/info` -> `freeHeap` should be >= 1400 bytes higher after MQTTAutoConfigBuffers lazy alloc
8. **evaluate.py:** Run `python3 evaluate.py` -- must pass all PROGMEM and unsafe-pattern checks

---

## Summary of RAM Savings

| Fix | Estimated Savings |
|---|---|
| MQTTAutoConfigBuffers size reduction | ~1400 bytes permanent |
| executeCommand() String removal | ~200 bytes heap fragmentation + ~400 bytes peak reduction |
| Move definitions from headers to .ino | 0 bytes (single-TU: no duplication) |
| **Total** | **~1600 bytes permanent + reduced heap fragmentation** |

**Note:** `time_t` is 4 bytes on ESP8266 (32-bit platform), not 8 bytes. Reducing `msglastupdated[256]` size is unsafe (full byte range needed for vendor IDs). The savings from this array were initially overstated -- the real gain is code quality, not RAM.

---

## Corrections from Initial Analysis

During review, the following initial assumptions were corrected:

1. **cmdqueue is correct** -- it uses a fill-pointer + left-shift-delete pattern, NOT a circular buffer with a wraparound bug. ADR-016 is accurate.
2. **time_t is 4 bytes on ESP8266** -- not 8 bytes as assumed. msglastupdated[256] uses 1024 bytes, not 2048.
3. **msglastupdated[256] full range IS needed** -- OTdata.id is a byte (0-255); vendor-specific IDs (Remeha, etc.) use the full range. Capping to OT_MSGID_MAX+1=134 would silently lose vendor updates.
4. **0xA5 magic byte** -- Used as a sentinel value in PIC firmware upload framing. While undocumented, it's a standard pattern for UART frame detection. Low priority.
