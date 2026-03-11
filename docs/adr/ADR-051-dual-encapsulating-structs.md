# ADR-051: Dual Encapsulating Structs (Settings + State)

**Status:** Accepted
**Date:** 2026-03-01
**Relates to:** ADR-008 (LittleFS Persistence), ADR-004 (Static Buffer Allocation)

## Context

The firmware accumulated 60+ flat global variables for configuration settings and 20+ for runtime state. These globals had inconsistent naming, no grouping, and no way to tell at a glance whether a variable was:

- A persistent setting (saved to LittleFS)
- A runtime state value (transient, never persisted)
- A device identity field vs. a feature toggle

Examples of the naming inconsistency:
```cpp
bool settingMQTTenable;        // "setting" prefix
char settingMQTTbroker[65];    // "setting" prefix, no type hint
bool statusMQTTconnection;     // "status" prefix — is this a setting or state?
bool bOTGWonline;              // "b" prefix, no "setting"/"state" scope
int  settingGPIOOUTPUTSpin;    // abbreviations, unclear grouping
```

Developers had to memorize which globals were persisted and which were transient. Adding a new setting required touching multiple files with no structural guidance.

## Decision

**Group all globals into two top-level structs with named sub-sections:**

### `OTGWSettings settings` — persistent configuration (serialized to LittleFS)
```cpp
struct OTGWSettings {
  char sHostname[41];    // device-level
  bool bLEDblink;
  bool bDarkTheme;
  bool bMyDEBUG;

  MQTTSettingsSection mqtt;     // settings.mqtt.sBroker
  NTPSection          ntp;      // settings.ntp.sTimezone
  SensorsSection      sensors;  // settings.sensors.bEnabled
  S0Section           s0;       // settings.s0.iPulsekw
  OutputsSection      outputs;  // settings.outputs.iPin
  WebhookSection      webhook;  // settings.webhook.sURLon
  UISection           ui;       // settings.ui.bAutoScroll
  OTGWBootSection     otgw;     // settings.otgw.sCommands
};
```

### `OTGWState state` — transient runtime state (never persisted)
```cpp
struct OTGWState {
  PICSection         pic;     // state.pic.bAvailable
  OTGWProtocol       otgw;   // state.otgw.bOnline
  MQTTRuntimeSection mqtt;   // state.mqtt.bConnected
  FlashSection       flash;  // state.flash.bESPactive
  DebugSection       debug;  // state.debug.bOTmsg
  UptimeSection      uptime; // state.uptime.iSeconds
};
```

### Naming conventions (Hungarian notation)
All struct members use type-indicating prefixes for clarity on an embedded platform where debugger access is limited:
- `b` — bool (`bEnabled`, `bOnline`)
- `s` — char array/string (`sBroker`, `sHostname`)
- `i` — integer/uint (`iPin`, `iBrokerPort`)
- `f` — float (`fTemperature`)

### Access pattern
```cpp
// Before: which is a setting? which is state?
if (settingMQTTenable && statusMQTTconnection) { ... }

// After: intent is clear from the path
if (settings.mqtt.bEnable && state.mqtt.bConnected) { ... }
```

### Backward compatibility
JSON keys in `settingStuff.ino` remain unchanged to preserve compatibility with existing LittleFS settings files on deployed devices. Only the C++ variable names changed.

## Alternatives Considered

### Alternative 1: Flat struct with prefixed names
```cpp
struct Settings {
  bool mqtt_bEnable;
  char mqtt_sBroker[65];
  // ...
};
```

**Why not chosen:** Doesn't provide the sub-section grouping that makes the access pattern intuitive. `settings.mqtt.bEnable` reads better than `settings.mqtt_bEnable` and allows passing `settings.mqtt` as a reference to MQTT-specific functions.

### Alternative 2: Namespace-based grouping
Use C++ namespaces to group related globals.

**Why not chosen:** Arduino's single-translation-unit .ino compilation model has limitations with namespaces across files. Structs are more natural in the Arduino ecosystem and provide value semantics (can be passed by reference).

### Alternative 3: Class with getters/setters
Full OOP encapsulation with private members and accessor methods.

**Why not chosen:** Adds complexity without benefit on an embedded platform. The settings struct is essentially a POD (Plain Old Data) type that maps directly to persistent storage. Getters/setters would add code size and call overhead for no functional gain.

## Consequences

### Positive
- **Self-documenting:** `settings.webhook.sURLon` vs `settingWebhookURLon` — the struct path tells you scope, feature area, and type
- **Clear persistence boundary:** Everything in `settings.*` is persisted; everything in `state.*` is transient
- **IDE support:** Auto-completion works on sub-sections (type `settings.mqtt.` to see all MQTT settings)
- **Parallel design:** Settings and state follow the same structural pattern
- **Pass-by-reference:** Can pass `settings.mqtt` to MQTT-specific functions

### Negative
- **Large rename scope:** ~700 replacements across 16 files
- **Longer access paths:** `settings.mqtt.sBroker` vs `settingMQTTbroker` (mitigated by clarity gain)
- **Arduino IDE limitations:** No refactoring tool support (mitigated by sed/grep)

## Implementation

Refactored in P5 of the C++ refactoring plan:
- Struct definitions in OTGW-firmware.h
- All 16 .ino/.h files updated via bulk rename
- JSON keys in settingStuff.ino unchanged (backward compatible)
- `isFlashing()` helper updated to use `state.flash.*`

## Related Decisions
- ADR-008: LittleFS Configuration Persistence (settings storage)
- ADR-004: Static Buffer Allocation (char[] members, not String)
- ADR-009: PROGMEM String Literals (default values use PSTR where applicable)
