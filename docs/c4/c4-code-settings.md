# C4 Code Level: Settings Module

## Overview

- **Name**: Settings Module (OTGW-firmware Settings & Persistence Layer)
- **Description**: Persistent configuration management system for OTGW-firmware. Handles reading/writing settings to LittleFS as JSON without ArduinoJson library, provides server-side validation, and deferred-write optimization to reduce flash wear.
- **Location**: `/src/OTGW-firmware/settingStuff.ino`; the global `settings` / `state` instances declared in `/src/OTGW-firmware/OTGW-firmware.h`; per-component types live in their own `*types.h` headers under `/src/OTGW-firmware/` (ADR-079 / ADR-081)
- **Language**: Arduino C/C++ (ESP32-S3)
- **Purpose**: Provide persistent configuration storage, REST API handler for settings updates, and service restart coordination through deferred-write patterns. Foundation for all module configuration: network, MQTT, GPIO, webhook, SAT thermostat, OT-direct, and hardware-specific features. Also holds the persisted combo board-mode selector (`settings.iBoardMode`, ADR-127 / ADR-158).

## Code Elements

### Core Functions

#### `readSettings(bool show): void`
- **File**: `settingStuff.ino:504`
- **Signature**: `void readSettings(bool show)`
- **Parameters**:
  - `show` (bool): If true, prints all loaded settings to debug output
- **Returns**: void
- **Purpose**: Load all persistent settings from LittleFS JSON file into global `settings` struct. Creates file with defaults if missing. Uses line-buffer-based parsing to avoid cMsg contention during yield() calls from file operations.
- **Key Behaviors**:
  - Opens `/settings.ini` (`SETTINGS_FILE`) for read; creates if missing via `writeSettings()` then a recursive `readSettings(false)`; an open failure or an empty file leaves the compiled-in defaults
  - Static line buffer (256 bytes; longer lines are skipped), key buffer (64) and value buffer (201, sized for `WebhookPayload`) prevent interference with global `cMsg` used by HTTP handlers
  - Parses JSON KV pairs via `parseJsonKVLine()` helper
  - Calls `updateSetting()` for each field (applies validation, constraints, defaults)
  - Clears `settingsDirty` and `pendingSideEffects` after load (prevent unwanted service restarts during boot)
  - Post-processing applies defaults for empty values (hostname, MQTT top topic and HA prefix, unique id, NTP timezone/hostname) and trims MQTT user/password
  - One-time migrations, persisted through the deferred flush (never a write inside the load): a file without `MQTTuseLegacyOtTopics` is a 1.x.x upgrade and keeps legacy OT topic names (TASK-648); a file without `ui_onboarded` marks the install as already onboarded (TASK-997); `MQTTinterval` 0 with on-change publishing is bumped to `MQTT_DEFAULT_PUBLISH_INTERVAL_SEC` (60, ADR-116)
  - Updates timer intervals for sensors and S0 counter based on loaded settings
  - Ends with `safebootSyncNvs(hostname, HTTP password)` so the safeboot updater can read them from NVS (ADR-191)
- **Dependencies**:
  - LittleFS filesystem
  - `updateSetting()` (validation logic)
  - `parseJsonKVLine()` (JSON parser)
  - Timer macros: `CHANGE_INTERVAL_SEC()`, `CATCH_UP_MISSED_TICKS`

#### `writeSettings(bool show): void`
- **File**: `settingStuff.ino:239`
- **Signature**: `void writeSettings(bool show)`
- **Parameters**:
  - `show` (bool): If true, reads and echoes written file to debug output
- **Returns**: void
- **Purpose**: Serialize entire `settings` struct to LittleFS as formatted JSON. Writes all sub-section fields using type-specific KV helpers. No ArduinoJson dependency.
- **Key Behaviors**:
  - Opens `/settings.ini` in write mode; aborts if open fails
  - Writes JSON root object with typed KV pairs (string, bool, int, float)
  - Uses helper functions: `writeJsonStringKV()`, `writeJsonBoolKV()`, `writeJsonIntKV()`, `writeJsonFloatKV()`
  - Reuses global `cMsg` buffer for JSON escaping (safe: no yield() between calls within writeSettings)
  - Closes file before optional echo read
  - Covers every persisted sub-section (device, mqtt, ntp, ui, sensors, s0, picBoot, outputs, webhook, sat, wifi) plus the device-level fields (`hostname`, `httppasswd`, `LEDblink`, `darktheme`, `nightlyrestart`, `nightlyrestarthour`, `boardmode`)
  - SAT settings alone: ~100 fields (heating system and source, presets, weather, multi-area, PV boost, BLE roster with per-slot bindkeys, etc.)
  - OT-direct settings (`HAS_DIRECT_OT`): 27 `OTD*` keys (mode, bypass relay, heating curve, room compensation, CH hysteresis, ventilation)
  - Ethernet settings (`HAS_ETH_CAPABLE`): static IP configuration
  - WiFi static IP (`WifiStaticIP`, `WifiSubnet`, `WifiGateway`, `WifiDns1`, `WifiDns2`; empty IP = DHCP)
  - Ends with `safebootSyncNvs()` like `readSettings()`
- **Dependencies**:
  - LittleFS filesystem
  - JSON escaping via `escapeJsonStringTo()`
  - Type conversion: `dtostrf()` for float→string

#### `updateSetting(const char* field, const char* newValue): void`
- **File**: `settingStuff.ino:688`
- **Signature**: `void updateSetting(const char *field, const char *newValue)`
- **Parameters**:
  - `field` (const char*): Settings field name (case-insensitive, e.g., "hostname", "MQTTbroker")
  - `newValue` (const char*): New value as string
- **Returns**: void
- **Purpose**: Server-side validation dispatcher. Routes field updates to appropriate struct member with range checks, type conversion, GPIO conflict detection, and deferred service restart coordination. Called by `readSettings()` during load, the REST `postSettings()` handler (`POST|PUT /api/v2/settings`, body `{"name":..., "value":...}`, gated by the `isKnownSetting()` whitelist), the MQTT set-command dispatch (`MQTTstuff.ino`, SAT table entries with a `settingKey`), `OTDirect.ino` (`OTDmode`), `SATble.ino` (`SATblemac`, `SATblerostercount`) and the legacy OT-direct REST arguments.
- **Key Behaviors**:
  - Case-insensitive field matching via `strcasecmp_P()`
  - Masks password fields in debug output (`httppasswd`, `MQTTpasswd`, `SATweatherapikey`, `SATblebindkey*`)
  - Type-specific validation:
    - **Hostname**: auto-generates unique ID if empty, strips domain suffix, triggers MDNS+MQTT restart
    - **HTTP password**: placeholder detection (rejects "notthepassword", "password=N"), trims whitespace, updates the OTA update server credentials (`httpUpdater.updateCredentials("admin", ...)`)
    - **MQTT fields**: broker, port (1-65535), user, password with range/placeholder checks; MQTT restart deferred
    - **GPIO pins** (`GPIOSENSORSpin`, `S0COUNTERpin`, `GPIOOUTPUTSpin`): 0-16 range check (out of range is ignored with a warning) + conflict detection via `checkGPIOConflict()` (warns, does not block)
    - **Numeric fields**: constrained to safe ranges (e.g., MQTTinterval 0-65535, S0 debounce 0-1000ms)
    - **Float fields**: constrained (e.g., SAT target temp 5-30°C, heating curve coeff 0.1-5.0)
  - **Side-effect coordination**: Sets `pendingSideEffects` bitmask (`SIDE_EFFECT_MQTT`, `SIDE_EFFECT_NTP`, `SIDE_EFFECT_MDNS`, `SIDE_EFFECT_OTGWSTREAM`) instead of immediate service restart. After the field cascade, any field name containing the lower-case substring `mqtt` also sets `SIDE_EFFECT_MQTT` (`strstr_P` is case-sensitive)
  - **No-op detection (TASK-564)**: snapshots `settings` with `platformSettingsNoopCapture()` before the cascade and compares afterwards with `platformSettingsNoopUnchanged()`; an unchanged struct restores `pendingSideEffects` and returns without marking dirty
  - **Deferred write**: Sets `settingsDirty=true` and restarts `timerFlushSettings` (2 s) to batch multiple updates
  - Covers all sub-sections:
    - Device: hostname, HTTP password, LED blink, dark theme, nightly restart (enable + hour), board mode (`boardmode`), manufacturer, model
    - MQTT: enable, broker, port, user, passwd, top topic, HA prefix, unique ID, OT message, on-change publishing, interval, separate sources, legacy port 25238, HA reboot detect, discovery auto-verify, legacy OT topics / legacy mode flags, topology stamp and one-shot cleanup flags
    - NTP: timezone, hostname, send-time, enable
    - GPIO Sensors: enabled, legacy format, pin, interval (1-3600 s)
    - S0 Counter: enabled, pin, debounce (0-1000 ms), pulses/kW (1-65535), interval (1-3600 s)
    - GPIO Outputs: enabled, pin, trigger bit (0-15)
    - Webhook: enabled, URL on/off, trigger bit, payload template, content-type
    - UI: first-time-setup and SAT onboarding flags (`ui_onboarded`, `sat_onboarded`)
    - PIC boot: command enable, command string
    - SAT: ~100 settings (enabled, heating system/source, PID, presets, weather, simulation, multi-area, PV boost, BLE roster, etc.)
    - OT-direct (`HAS_DIRECT_OT`): mode, auto-detect, setback, fail-safe, bypass relay, heating curve, room compensation, CH hysteresis, ventilation
    - Ethernet (`HAS_ETH_CAPABLE`) and WiFi: static IP, gateway, subnet, DNS (IP validation via `IPAddress::fromString()`; empty WiFi IP = DHCP)
- **Dependencies**:
  - `checkGPIOConflict()` for GPIO conflict detection
  - `platformSettingsNoopCapture()` / `platformSettingsNoopUnchanged()` (Platform library, `platform_esp32.h`)
  - `trimwhitespace()` helper
  - Global state: `state.bSetupComplete`, `state.sat.*` for SAT state transitions
  - Timer macros: `CHANGE_INTERVAL_SEC()`, `RESTART_TIMER()`

#### `flushSettings(): void`
- **File**: `settingStuff.ino:77`
- **Signature**: `void flushSettings()`
- **Parameters**: None
- **Returns**: void
- **Purpose**: Deferred-write endpoint. Called from main loop timer (`timerFlushSettings`, 2s debounce). Writes dirty settings to flash once, then applies all pending side-effects exactly once per batch.
- **Key Behaviors**:
  - Returns early if `settingsDirty == false` (no-op optimization)
  - Calls `writeSettings(false)` to flush to LittleFS
  - Clears `settingsDirty` flag
  - Applies pending service restarts in order: MDNS/LLMNR → MQTT (only if `settings.mqtt.bEnable`) → NTP → OTGW stream (`applyLegacyPort25238Setting()`)
  - Clears `pendingSideEffects` bitmask after applying all effects
  - Prevents redundant service restarts per batch
- **Purpose in Finding #23**: Coalesces rapid REST API updates (e.g., UI form submission) into single flash write + one restart per service, reducing flash wear and MQTT churn
- **Dependencies**:
  - `writeSettings()` for actual flash write
  - `startMDNS()`, `startLLMNR()`, `startMQTT()`, `startNTP()`, `applyLegacyPort25238Setting()` for service restarts

#### `settingsMarkClean(): void`
- **File**: `settingStuff.ino:71`
- **Signature**: `void settingsMarkClean()`
- **Parameters**: None
- **Returns**: void
- **Purpose**: Manually clear dirty flag and pending side-effects without writing or restarting services. Used after direct `writeSettings()` calls (e.g., during OTA reboot window) to prevent deferred-flush timer from triggering unnecessary service restarts.
- **Dependencies**: None (stateless)

#### `checkGPIOConflict(int pin, GPIOConflictCaller caller): bool`
- **File**: `settingStuff.ino:110`
- **Signature**: `bool checkGPIOConflict(int pin, GPIOConflictCaller caller)`
- **Parameters**:
  - `pin` (int): GPIO pin number to check
  - `caller` (GPIOConflictCaller enum): Feature requesting the pin (Sensor, S0, Output)
- **Returns**: bool - true if pin conflicts with another enabled feature
- **Purpose**: Prevent simultaneous use of same GPIO by multiple features (Finding #27: GPIO conflict detection). `GPIOConflictCaller` is declared in `OTGW-firmware.h`.
- **Key Behaviors**:
  - Returns false for negative pins (disabled/not set)
  - Checks pin against all other configurable GPIOs: sensors, S0 counter, outputs
  - Skips check against caller's own feature
  - Logs conflict warnings to debug
- **Supported Features**:
  - `GPIOConflictCaller::Sensor`: Dallas DS18B20 sensor pin (struct default 10)
  - `GPIOConflictCaller::S0`: S0 pulse counter pin (struct default 12)
  - `GPIOConflictCaller::Output`: GPIO relay output pin (struct default 16)
  - Board pins (I2C, LEDs, button, PIC UART) from `boards.h` are not part of the check
- **Dependencies**: None (stateless, accesses `settings` global)

#### `isHttpPasswordPlaceholder(const char* value): static bool`
- **File**: `settingStuff.ino:25`
- **Signature**: `static bool isHttpPasswordPlaceholder(const char* value)`
- **Parameters**:
  - `value` (const char*): Password string to test
- **Returns**: bool - true if value is a known placeholder/sentinel
- **Purpose**: Detect placeholder passwords sent by web UI to avoid overwriting real credentials. When password field is submitted empty via HTTP form, web UI sends placeholder to indicate "keep existing password".
- **Key Behaviors**:
  - Recognizes exact matches: "notthepassword", "notthispassword"
  - Recognizes pattern: "password=N" where N is decimal digit(s) (e.g., "password=32" for 32-char masked password)
  - Returns false for null, empty, or non-matching values
  - Used by `updateSetting()` for both HTTP and MQTT passwords
- **Dependencies**: None (pure validation)

#### `isWeatherApiKeyPlaceholder(const char* value): static bool`
- **File**: `settingStuff.ino:53`
- **Signature**: `static bool isWeatherApiKeyPlaceholder(const char* value)`
- **Returns**: bool - true if value matches `apikey=N` (N decimal digits)
- **Purpose**: The OpenWeatherMap API key (`SATweatherapikey`) is masked on GET as `apikey=<len>`; a POST echoing that mask must not overwrite the real key (TASK-933 P2). Mirrors the `password=<len>` pattern of `isHttpPasswordPlaceholder()`.

### JSON Serialization Helpers

#### `parseJsonKVLine(const char* line, char* keyOut, size_t keyOutSize, char* valueOut, size_t valueOutSize): static bool`
- **File**: `settingStuff.ino:132`
- **Signature**: `static bool parseJsonKVLine(const char* line, char* keyOut, size_t keyOutSize, char* valueOut, size_t valueOutSize)`
- **Parameters**:
  - `line` (const char*): Single JSON line (e.g., `  "hostname": "OTGW",`)
  - `keyOut` (char*): Output buffer for parsed key
  - `keyOutSize` (size_t): Size of keyOut buffer
  - `valueOut` (char*): Output buffer for parsed value
  - `valueOutSize` (size_t): Size of valueOut buffer
- **Returns**: bool - true if parse succeeds
- **Purpose**: Custom lightweight JSON parser (no ArduinoJson). Extracts key-value pair from single JSON line supporting both quoted-string and unquoted-numeric values. Handles JSON escape sequences.
- **Key Behaviors**:
  - Validates buffers not null, sizes > 0
  - Finds opening `"`, parses quoted key until closing `"` (handles escaped quotes `\"`)
  - Skips whitespace + `:` delimiter
  - Parses value as either:
    - **Quoted string**: handles escape sequences (\", \\, \/, \b, \f, \n, \r, \t, others as-is)
    - **Unquoted**: digits/symbols until comma or closing brace
  - Bounds-checks against output buffers
  - Returns false if key/value missing or truncated
- **Dependencies**: None (pure parsing)

#### `writeJsonStringKV(File& file, const __FlashStringHelper* key, const char* value, bool withComma): static void`
- **File**: `settingStuff.ino:201`
- **Signature**: `static void writeJsonStringKV(File& file, const __FlashStringHelper* key, const char* value, bool withComma)`
- **Parameters**:
  - `file` (File&): Open LittleFS file handle
  - `key` (const __FlashStringHelper*): Field name (PROGMEM string)
  - `value` (const char*): Value string (RAM)
  - `withComma` (bool): Add trailing comma if true
- **Returns**: void
- **Purpose**: Write single JSON string key-value pair with proper escaping. Reuses global `cMsg` buffer for escape work.
- **Key Behaviors**:
  - Escapes value via `escapeJsonStringTo()` into `cMsg`
  - Writes: `"key": "escaped_value"[,]`
  - Key stays in PROGMEM; value escaped to RAM first
- **Dependencies**: `escapeJsonStringTo()` helper

#### `writeJsonBoolKV(File& file, const __FlashStringHelper* key, bool value, bool withComma): static void`
- **File**: `settingStuff.ino:212`
- **Signature**: `static void writeJsonBoolKV(File& file, const __FlashStringHelper* key, bool value, bool withComma)`
- **Parameters**:
  - `file` (File&): Open file handle
  - `key` (const __FlashStringHelper*): Field name
  - `value` (bool): Boolean to write
  - `withComma` (bool): Add trailing comma if true
- **Returns**: void
- **Purpose**: Write JSON boolean (true/false) key-value pair.
- **Format**: `"key": true|false[,]`

#### `writeJsonIntKV(File& file, const __FlashStringHelper* key, int value, bool withComma): static void`
- **File**: `settingStuff.ino:220`
- **Signature**: `static void writeJsonIntKV(File& file, const __FlashStringHelper* key, int value, bool withComma)`
- **Parameters**:
  - `file` (File&): Open file handle
  - `key` (const __FlashStringHelper*): Field name
  - `value` (int): Integer to write
  - `withComma` (bool): Add trailing comma
- **Returns**: void
- **Purpose**: Write JSON integer key-value pair via `printf_P()`.
- **Format**: `"key": 12345[,]`

#### `writeJsonFloatKV(File& file, const __FlashStringHelper* key, float value, bool withComma): static void`
- **File**: `settingStuff.ino:228`
- **Signature**: `static void writeJsonFloatKV(File& file, const __FlashStringHelper* key, float value, bool withComma)`
- **Parameters**:
  - `file` (File&): Open file handle
  - `key` (const __FlashStringHelper*): Field name
  - `value` (float): Float to write
  - `withComma` (bool): Add trailing comma
- **Returns**: void
- **Purpose**: Write JSON floating-point key-value pair. Converts float to string with 2 decimal places via `dtostrf()`.
- **Format**: `"key": 20.50[,]`

## Data Model: OTGWSettings Struct

### Location
- `OTGW-firmware.h`: root `OTGWSettings` (line 701) and `OTGWState` (line 497) struct definitions; global instances `settings`, `state`
- Per-component types in dedicated `*types.h` headers (ADR-079 / ADR-081), each included after `boards.h`. Two sections live in their `stuff.h` sibling instead (ADR-081): `MQTTSettingsSection` / `MQTTRuntimeSection` in `MQTTstuff.h`, and `DebugSection` in `debugStuff.h`.
  - `Devicetypes.h`: `DeviceSection`
  - `Hardwaretypes.h`: `OTGWHardwareMode`, `HardwareSection` (`state.hw`: `eMode`, `bOLEDPresent`, `bEthernetPresent`, `bClassicPro`)
  - `Networktypes.h`: `OTGWNetworkMode`, `NetworkSection` (`state.net`), `EthernetSection` (`HAS_ETH_CAPABLE`), `WifiSection` (`settings.wifi`)
  - `NTPtypes.h`: `NTPSection`
  - `Sensorstypes.h`: `SensorsSection`
  - `S0types.h`: `S0Section`
  - `Outputstypes.h`: `OutputsSection`
  - `Webhooktypes.h`: `WebhookSection`
  - `UItypes.h`: `UISection` (`bOnboarded`, `bSatOnboarded`)
  - `PICtypes.h`: `PICSection` (`state.pic`), `PicSettingsSection` (`state.picSettings`), `PICBootSection`
  - `OTBustypes.h`: `OTBusState` (`state.otBus`: online, PS mode, gateway mode, last-seen epochs)
  - `OTDirecttypes.h`: `OTDirectSettingsSection` (`settings.otd`), `OTDirectSection` (`state.otd`) + mode/override enums (`HAS_DIRECT_OT` only)
  - `Flashtypes.h`: `FlashSection` (`state.flash`: ESP/PIC upgrade active flags, error, PIC progress)
  - `Uptimetypes.h`: `UptimeSection` (`state.uptime`: `iSeconds`, `iRebootCount`)
  - `SATtypes.h`: `SATSection` (settings) + `SATRuntimeSection` (`state.sat`); largest header (multi-area, BLE roster, weather, PV boost)
- ADR-081 folded the previously separate `Componenttypes.h` declarations into the per-component headers above.
- `HeapDiagSection` (`state.heapdiag`), `RestPerfSection` (`state.restperf`) and `DiscoverySection` (`state.discovery`) are declared directly in `OTGW-firmware.h`.

`OTGWState state` (never persisted) aggregates: `hw`, `net`, `pic`, `otd` (`HAS_DIRECT_OT`), `otBus`, `mqtt`, `flash`, `debug`, `uptime`, `heapdiag`, `restperf`, `discovery`, `picSettings`, `sat`, plus `statusMessage` (`StatusMessage` enum: `None`, `LittleFSMismatch`, `PSModeActive`) and `bSetupComplete`.

### Architecture (ADR-051: Two-Level Named Sub-Sections)

Settings use two-level named sub-sections accessed as `settings.mqtt.sBroker`, `settings.ntp.sTimezone`, etc.
Hungarian prefixes: **b** = bool, **s** = char[], **i** = int/uint, **f** = float.

```cpp
struct OTGWSettings {
  // Device-level fields (universal device identity)
  char sHostname[41];           // Device hostname (default _HOSTNAME = "OTGW")
  char sHTTPpasswd[41];         // HTTP Basic Auth password (empty = no authentication)
  bool bLEDblink;               // LED blink enable (default true)
  bool bDarkTheme;              // UI dark theme
  bool bMyDEBUG;                // Debug toggle: telnet 'l'/'f'; evalOutputs() logs once when set
  bool bNightlyRestart;         // Scheduled daily restart for heap recovery (default false)
  uint8_t iRestartHour;         // Hour (0-23) for nightly restart (default 4)
  uint8_t iBoardMode;           // Combo board selector (ADR-127/158): 0 auto, 1 force PIC (S3 Mini), 2 force OTDirect, 3 force PIC (S3 Mini Pro); ignored on fixed boards

  // Named sub-sections
  DeviceSection device;         // manufacturer, model identity
  WifiSection wifi;             // static IP for the WiFi STA interface (empty = DHCP)
  MQTTSettingsSection mqtt;     // broker, auth, topics, HA discovery, publishing modes
  NTPSection ntp;               // enable, timezone, hostname, sendtime
  SensorsSection sensors;       // Dallas DS18B20 (enabled, legacy format, pin, interval)
  S0Section s0;                 // S0 pulse counter (enabled, pin, debounce, pulses/kW, interval)
  OutputsSection outputs;       // GPIO relay (enabled, pin, trigger bit)
  WebhookSection webhook;       // HTTP webhook trigger (enabled, URLs, trigger bit, payload, content-type)
  UISection ui;                 // onboarding flags (bOnboarded, bSatOnboarded)
  PICBootSection picBoot;       // PIC boot command injection (enabled, commands)
  SATSection sat;               // ~100 fields: Smart Auto Thermostat (control, tuning, weather, BLE roster)
#if HAS_DIRECT_OT
  OTDirectSettingsSection otd;  // OT-direct configuration (OTGW32 and combo builds)
#endif
#if HAS_ETH_CAPABLE
  EthernetSection eth;          // Ethernet static IP (W5500 boards)
#endif
};
```

### Sub-Section Details

#### DeviceSection (device): Device Identity Metadata
```cpp
struct DeviceSection {
  char sManufacturer[32] = "NodoShop";   // Device manufacturer (e.g. "NodoShop", "Seegel")
  char sModel[32]        = "OTGW";       // Device model (e.g. "OTGW", "OTGW32", "OT-Thing")
};
```
**Purpose**: Store device identity metadata for REST API and HA discovery. Updated via REST settings API (`updateSetting("DeviceManufacturer", ...)` and `updateSetting("DeviceModel", ...)`). Allows custom device identification independent of firmware version. Used by Home Assistant discovery to label device in UI.

#### MQTTSettingsSection (mqtt)
```cpp
struct MQTTSettingsSection {                               // defined in MQTTstuff.h
  bool    bEnable            = true;                         // MQTT enable/disable
  bool    bSecure            = false;                        // TLS flag (not implemented)
  char    sBroker[65]        = "homeassistant.local";        // Broker hostname/IP
  uint16_t iBrokerPort       = 1883;                         // Broker port (1-65535)
  char    sUser[41]          = "";                           // Username
  char    sPasswd[41]        = "";                           // Password
  char    sHaprefix[41]      = HOME_ASSISTANT_DISCOVERY_PREFIX; // HA discovery prefix
  bool    bHaRebootDetect    = true;                         // Reboot detection via MQTT
  bool    bDiscoveryAutoVerify = true;                       // ADR-062: daily discovery heal
  char    sTopTopic[41]      = "OTGW";                       // Root topic
  char    sUniqueid[41]      = "";                           // HA unique device ID (set in readSettings)
  bool    bOTmessage         = false;                        // Publish full OT messages
  bool    bOnChangePublishing = true;                        // ADR-116: publish on change, heartbeat every iInterval
  uint16_t iInterval         = MQTT_DEFAULT_PUBLISH_INTERVAL_SEC; // Heartbeat interval (60 s); 0 = legacy publish-every-message
  bool    bSeparateSources   = false;                        // ADR-040: source-specific topics
  bool    bLegacyPort25238Enabled = false;                   // TCP port 25238 OTGW stream
  bool    bUseLegacyOtTopics = false;                        // ADR-106: legacy OT-spec topic names
  bool    bLegacyMode        = false;                        // TASK-648: umbrella 1.x.x compatibility mode
  bool    bLastPublishedLegacy = false;                      // TASK-648: topology stamp for stale-topic cleanup
  bool    bThermostatDupsCleared = false;                    // TASK-1201: one-shot cleanup done (not exposed by REST)
};
```

#### NTPSection (ntp)
```cpp
struct NTPSection {
  bool bEnable        = true;                 // NTP enable/disable
  char sTimezone[65]  = NTP_DEFAULT_TIMEZONE; // TZ database name (e.g., "Europe/Amsterdam")
  char sHostname[65]  = NTP_HOST_DEFAULT;     // NTP server (default "pool.ntp.org")
  bool bSendtime      = false;                // Send time via MQTT
};
```

#### SensorsSection (sensors): Dallas DS18B20
```cpp
struct SensorsSection {
  bool    bEnabled       = false;             // Enable 1-Wire sensor scan
  bool    bLegacyFormat  = false;             // Legacy (v0.10.x) Dallas address format (false=standard 16 hex chars)
  int8_t  iPin           = 10;                // GPIO pin (struct default 10)
  int16_t iInterval      = 20;                // Poll interval (seconds)
};
```

#### S0Section (s0): Energy Pulse Counter
```cpp
struct S0Section {
  bool     bEnabled      = false;             // Enable S0 counter
  uint8_t  iPin          = 12;                // GPIO pin (default 12)
  uint16_t iDebounceTime = 80;                // Debounce (ms)
  uint16_t iPulsekw      = 1000;              // Pulses per kW (typical S0)
  uint16_t iInterval     = 60;                // Report interval (seconds)
};
```

#### OutputsSection (outputs): GPIO Relay
```cpp
struct OutputsSection {
  bool   bEnabled    = false;                 // Enable GPIO output relay
  int8_t iPin        = 16;                    // GPIO pin (default 16)
  int8_t iTriggerBit = 0;                     // OT message bit to trigger (0-15)
};
```

#### WebhookSection (webhook): HTTP Trigger
```cpp
struct WebhookSection {
  bool   bEnabled         = false;            // Enable webhook
  char   sURLon[101]      = "http://homeassistant.local:8123/api/webhook/otgw_boiler";  // URL when trigger ON
  char   sURLoff[101]     = "http://homeassistant.local:8123/api/webhook/otgw_boiler";  // URL when trigger OFF
  int8_t iTriggerBit      = 1;                // OT message bit (default bit 1 = CH mode)
  char   sPayload[201]    = "";               // POST body (empty=GET)
  char   sContentType[32] = "application/json"; // Content-Type header
};
```

#### UISection (ui): Onboarding Flags
```cpp
struct UISection {
  bool bOnboarded       = false;   // TASK-997: first-time-setup wizard done (persisted as ui_onboarded)
  bool bSatOnboarded    = false;   // TASK-1012: SAT onboarding wizard done (persisted as sat_onboarded)
};
```
The earlier web UI preferences (auto-scroll, timestamps, capture mode, graph window) are no longer firmware settings (no `updateSetting()` key exists for them).

#### PICBootSection (picBoot): PIC Boot Commands
```cpp
struct PICBootSection {
  bool bEnable        = false;                // Enable command injection
  char sCommands[129] = "";                   // Newline-separated PIC commands
};
```

#### SATSection (sat): Smart Auto Thermostat (~100 fields, `SATtypes.h`)
Controls autonomous heating/cooling via OpenTherm. Includes:
- **Core**: enabled, heating system (`iHeatingSystem`: auto/radiators/underfloor), heating source (`iHeatingSource`: auto/gas boiler/heat pump/hybrid, with `iHpCycleSeconds`), target temp, heating curve coefficient, deadband, control interval
- **Presets**: comfort (21°C), eco (18°C), away (15°C), sleep (16°C), activity (10°C), home (18°C)
- **Modulation**: PWM auto-switch, max relative modulation, overshoot protection, modulation suppression
- **PID**: manual/auto gains (Kp, Ki, Kd), auto-tune rate
- **Weather**: enable, latitude, longitude, poll interval, boiler capacity/efficiency
- **Simulation**: enable, heat/cool rates, thermal coefficient
- **Solar gain**: enable, min rise rate, setpoint offset, min elevation angle
- **Summer simmer**: enable, outdoor threshold, min hours to trigger
- **Comfort adjustment**: humidity-based setpoint correction
- **Multi-area**: weighted multi-room temperature (1-4 areas)
- **Window detection**: via MQTT, min open time
- **Flow/pressure**: min/max pressure, pressure drop alarm, flow offset, DHW setpoint
- **Multi-zone**: zone count (1-4), zone timeout
- **Sensor-to-area mapping**: `sSensorArea[4][18]` maps a Dallas address or BLE MAC to each of 4 areas (TASK-587)
- **PV boost**: enable, threshold, hold, delta, max indoor temp, max duration
- **BLE sensors** (`HAS_SAT_BLE`): enable, risk acknowledgement, failover, pinned MAC, publish interval, name-prefix filter, and a persisted roster of `SAT_BLE_MAX_ROSTER` (8) slots with MAC, label and per-slot MiBeacon bindkey (secret, masked on read)
- **LittleFS persistence**: auto-flush threshold (hours)
- See `SATtypes.h` (`SATSection`, line 424) for the complete list with defaults; ranges are enforced in `updateSetting()`.

#### OTDirectSettingsSection (otd): OT-Direct Mode (`HAS_DIRECT_OT` only: OTGW32 and combo builds; defined in `OTDirecttypes.h`)
```cpp
struct OTDirectSettingsSection {
  // Operating modes and boiler interface
  uint8_t  iMode              = 1;             // OT Direct mode (0=bypass, 1=gateway, 2=monitor, 3=master, 4=loopback)
  bool     bAutoDetect        = true;          // Auto-detect thermostat at boot
  float    fSetbackTemp       = 16.0f;         // Setback temp on disconnect (°C)
  uint8_t  iSetbackTimeout    = 30;            // Disconnect timeout (seconds)
  bool     bEnableSlave       = true;          // Enable slave interface in master mode
  bool     bSummerMode        = false;         // SM= summer mode bit
  bool     bFailSafe          = true;          // FS= fail-safe setback
  uint16_t iMsgInterval       = 100;           // MI= inter-message gap (ms, 100-1275)
  bool     bHasBypassRelay    = false;         // Persisted: bypass relay fitted (user-declared)
  
  // PI room compensation + heating curve (TASK-183)
  uint8_t  iCHMode            = 1;             // CH mode (0=off, 1=fixed flow, 2=heating curve)
  float    fFlowTemp          = 45.0f;         // Fixed flow temp (°C)
  float    fFlowMax           = 75.0f;         // Maximum flow temp (°C)
  float    fRoomSetpoint      = 20.0f;         // Room setpoint for curve (°C)
  float    fGradient          = 1.5f;          // Heating curve gradient
  float    fExponent          = 1.0f;          // Heating curve exponent (1.0=linear)
  float    fOffset            = 0.0f;          // Heating curve offset (°C)
  bool     bRoomCompEnabled   = false;         // PI room compensation enable
  float    fKp                = 5.0f;          // Proportional gain (K/K)
  float    fKi                = 0.5f;          // Integral gain (1/h)
  float    fKboost            = 2.0f;          // Boost gain (K/K) for large error

  // CH hysteresis deadband (TASK-582)
  bool     bHysteresisEnable  = false;         // Enable CH suspension hysteresis
  float    fHysteresis        = 0.5f;          // Total deadband width (degC, 0.0-2.0)

  // Ventilation override persistence (TASK-584)
  bool     bVentEnable, bOpenBypass, bAutoBypass, bFreeVentEnable;  // OT MsgID 70 bits
  uint8_t  iVentSetpoint      = 0;             // Ventilation setpoint 0-100% (OT MsgID 71)
};
```

#### EthernetSection (eth): Ethernet Static IP (`HAS_ETH_CAPABLE` only, W5500 boards; `Networktypes.h`)
```cpp
struct EthernetSection {
  bool bStaticIP        = false;               // Enable static IP
  char sIPaddress[16]   = "0.0.0.0";           // Static IP (e.g., "192.168.1.100")
  char sGateway[16]     = "0.0.0.0";           // Gateway IP
  char sSubnet[16]      = "255.255.255.0";     // Subnet mask
  char sDNS[16]         = "0.0.0.0";           // DNS server IP (0.0.0.0 = use gateway)
};
```

#### WifiSection (wifi): WiFi Static IP (`Networktypes.h`)
```cpp
struct WifiSection {
  char sStaticIp[16] = "";   // empty = DHCP; the other fields are then ignored
  char sSubnet[16]   = "";
  char sGateway[16]  = "";
  char sDns1[16]     = "";
  char sDns2[16]     = "";
};
```
Persisted as `WifiStaticIP`, `WifiSubnet`, `WifiGateway`, `WifiDns1`, `WifiDns2`; applied by `startWiFi()` in `networkStuff.ino` when `sStaticIp` is set and parses with the subnet and gateway.

## Dependencies

### Internal Dependencies (Firmware Modules)

- **restAPI.ino**: `postSettings()` calls `updateSetting()` for `POST|PUT /api/v2/settings`; `sendDeviceSettings()` serves `GET /api/v2/settings` (both behind `checkHttpAuth()`)
- **MQTTstuff.ino**: Reads MQTT settings (`settings.mqtt.*`); `startMQTT()` restarted by `flushSettings()`
- **networkStuff.ino**: `startMDNS()`, `startLLMNR()`, `startNTP()` service restart functions
- **OTGW-firmware.h / OTGW-firmware.ino**: `timerFlushSettings` (2 s debounce, declared by hand in `OTGW-firmware.h`, driven by `loop()`), `timerpollsensor`, `timers0counter`
- **helperStuff.ino**: `trimwhitespace()`; **jsonStuff.ino**: `escapeJsonStringTo()`
- **SATcontrol.ino**: `satDisable()` called when SAT disabled via settings update
- **debugStuff.h**: Debug output macros (`DebugTln()`, `DebugTf()`, `DebugT()`)
- **MQTTstuff.ino / OTDirect.ino / SATble.ino**: additional `updateSetting()` callers (MQTT set-commands, OT-direct mode persistence, BLE roster)
- **ModUpdateServer (`OTGW-ModUpdateServer-esp32.h`)**: `safebootSyncNvs()` mirrors hostname and HTTP password into NVS for the safeboot updater (ADR-191)
- **safeTimers.h**: Timer macros for sensor/S0 interval updates

### External Dependencies (Libraries)

- **LittleFS**: File I/O for `/settings.ini` persistence
- **Arduino-ESP32 Core**: `constrain()`, `atoi()`, `atof()`, `strlcpy()`, `strcasecmp_P()`, `strncasecmp_P()`, `strchr()`, `strstr_P()`, `memmove()`, `tolower()`, `IPAddress::fromString()`
- **Standard C**: `ctype.h` (isdigit, isspace), `string.h` functions with `_P` suffixes for PROGMEM access

## Key Behaviors & Patterns

### 1. Deferred-Write Optimization (Finding #23)
- **Problem**: Rapid REST API updates (e.g., web form submission with 10+ fields) cause multiple flash writes and service restarts, wearing flash and causing MQTT churn.
- **Solution**: 
  - `updateSetting()` sets `settingsDirty=true` and restarts `timerFlushSettings` (2s debounce) instead of immediate write
  - Sets `pendingSideEffects` bitmask for deferred service restarts
  - `flushSettings()` (called from loop timer) coalesces writes: single `writeSettings()` call, then applies all pending side-effects once
  - Multiple field updates → one flash write + one restart per service per batch
- **Result**: Reduced flash wear, reduced MQTT connection churn, better UI responsiveness

### 2. No ArduinoJson Library
- Custom lightweight `parseJsonKVLine()` parser (no heap allocation)
- `writeJson*KV()` helpers for serialization
- Reuses global `cMsg` buffer for JSON escaping (safe: no yield() within `writeSettings()`)
- Reduces firmware size and RAM footprint (ADR-146: no ArduinoJson anywhere)

### 3. PROGMEM String Management
- All JSON keys and format strings use `F()` macro or `PSTR()` to stay in flash
- Passwords masked in debug output (prevent credential leaks to telnet logs)
- `_P` suffix helpers: `strcasecmp_P()`, `strncasecmp_P()`, `strcmp_P()`, `strstr_P()`, `snprintf_P()`, etc.

### 4. GPIO Conflict Detection (Finding #27)
- `checkGPIOConflict()` prevents simultaneous use of same GPIO by sensors, S0 counter, or outputs
- Called during `updateSetting()` when GPIO pin field changes
- Logs warnings but does NOT block the change (user can override via UI); only sensors, S0 and outputs are checked against each other

### 5. Password Placeholder Handling
- Web UI sends "notthepassword" or "password=N" when password field submitted empty (keep existing)
- `isHttpPasswordPlaceholder()` detects pattern; `updateSetting()` ignores placeholders
- OTA server credentials updated immediately on HTTP password change (for security)

### 6. Service Restart Coordination
Four types of deferred side-effects:
- **SIDE_EFFECT_MDNS**: Restart mDNS/LLMNR service (hostname change)
- **SIDE_EFFECT_MQTT**: Restart MQTT client (hostname, broker, port, credentials; any field name containing lower-case `mqtt`)
- **SIDE_EFFECT_NTP**: Restart NTP sync (timezone, hostname)
- **SIDE_EFFECT_OTGWSTREAM**: Re-apply the legacy TCP port 25238 listener (`LegacyPort25238Enabled`)

Each applied exactly once per flush batch, in order: MDNS → MQTT → NTP → OTGW stream.

### 7. Settings Load Safety
- Clears `settingsDirty` and `pendingSideEffects` after load (prevent unwanted restarts during boot)
- Line buffer (256 bytes static) decouples file read from global `cMsg` (used by HTTP handlers during yield()); the matching key/value buffers are 64 and 201 bytes
- Creates file with defaults if missing via recursive call pattern
- Post-processing applies sensible defaults for empty values (hostname fallback, MQTT topic defaults, etc.)

### 8. Type Validation & Range Checking
All numeric/enum fields constrained during `updateSetting()`:
- **Integer ranges**: MQTTbrokerPort (1-65535), MQTTinterval (0-65535), GPIO pins (0-16), sensor/S0 interval (1-3600 s), S0 debounce (0-1000 ms), SAT control interval (10-300 s)
- **Float ranges**: SAT target temp (5-30°C), heating curve (0.1-5.0), PID gains (0-20.0)
- **Enum values**: SAT heating system (0-3), OT-direct mode (0-4)
- **String bounds**: Checked via `strlcpy()` (automatic truncation with safety)
- **IP validation**: Ethernet and WiFi static IPs validated via `IPAddress::fromString()` (invalid values are ignored)

### 9. Settings Dirty Flag Coordination
- `settingsDirty = true` set by `updateSetting()` for any field change
- `flushSettings()` consumes flag; clears after write
- `settingsMarkClean()` allows manual clear (e.g., after OTA reboot window)
- Prevents accidental writes during boot or external flash operations
- **Per-field no-op detection (TASK-564)**: `updateSetting()` compares the new value against the current stored value for each field and skips dirty-flagging + debounce-timer restart when they match. Cosmetic UI saves (an unchanged form posted back) no longer cost a flash write or a service restart.

## Call Graph

```
readSettings() [boot]
  ├─ LittleFS.open(SETTINGS_FILE, "r")
  ├─ parseJsonKVLine() [for each line]
  │   └─ updateSetting()
  │       ├─ Type validation & range checks
  │       ├─ checkGPIOConflict() [GPIO pins]
  │       └─ RESTART_TIMER(timerFlushSettings) [deferred write]
  └─ Post-processing defaults

REST API POST|PUT /api/v2/settings [postSettings()], MQTT set-command, OTDirect/SATble callers
  └─ updateSetting(field, newValue)
      ├─ Validate & convert type
      ├─ Set pendingSideEffects bitmask
      └─ settingsDirty = true, RESTART_TIMER(timerFlushSettings)

Main loop timer (2s debounce)
  └─ flushSettings()
      ├─ writeSettings(false)
      │   ├─ LittleFS.open(SETTINGS_FILE, "w")
      │   ├─ writeJsonStringKV() [for string fields]
      │   ├─ writeJsonBoolKV() [for bool fields]
      │   ├─ writeJsonIntKV() [for int fields]
      │   ├─ writeJsonFloatKV() [for float fields]
      │   ├─ file.close()
      │   └─ safebootSyncNvs()
      ├─ Apply side-effects: startMDNS()/startLLMNR(), startMQTT(), startNTP(), applyLegacyPort25238Setting()
      └─ Clear pendingSideEffects bitmask

Direct call (e.g., OTA window)
  ├─ writeSettings(false)
  └─ settingsMarkClean() [prevent deferred-flush side-effects]
```

## File Structure

```
/src/OTGW-firmware/
  ├─ OTGW-firmware.h
  │  ├─ struct OTGWState (line 497) and global `state`
  │  ├─ struct OTGWSettings (line 701): root settings container, global `settings` (line 736)
  │  ├─ GPIOConflictCaller enum (line 275), prototypes for readSettings()/writeSettings()/updateSetting()/checkGPIOConflict()
  │  ├─ Combo pin resolution: comboActivePinMap(), activeI2cSda()/Scl(), activeButton(), activeLed1()/2(), activePicRst()
  │  └─ timerFlushSettings_* (line 838)
  ├─ *types.h headers (one per sub-section, see above), MQTTstuff.h (MQTT sections), debugStuff.h (DebugSection)
  └─ settingStuff.ino (1281 lines)
     ├─ SIDE_EFFECT_* constants (lines 18-21), settingsDirty / pendingSideEffects (lines 22-23)
     ├─ isHttpPasswordPlaceholder() (line 25), isWeatherApiKeyPlaceholder() (line 53)
     ├─ settingsMarkClean() (line 71), flushSettings() (line 77)
     ├─ checkGPIOConflict() (line 110)
     ├─ parseJsonKVLine() (line 132)
     ├─ writeJsonStringKV/BoolKV/IntKV/FloatKV (lines 201-228)
     ├─ writeSettings() (line 239): serializes the struct to LittleFS JSON
     ├─ readSettings() (line 504): loads settings from LittleFS, applies defaults and migrations
     └─ updateSetting() (line 688): validates and updates any field

/
  └─ /settings.ini [LittleFS JSON, read on boot, written on flush]
```

## Test Considerations

1. **Settings Persistence**: Verify all field types serialize/deserialize correctly (strings, bools, ints, floats)
2. **JSON Parser**: Test escape sequences (\n, \r, \t, \\, \"), edge cases (empty keys, missing values)
3. **Deferred Write**: Multiple rapid updates should coalesce into single write; side-effects applied once
4. **Password Placeholder**: Verify "password=N" not saved as literal value; existing password preserved
5. **GPIO Conflicts**: Setting S0 pin to same GPIO as sensors should warn (not block)
6. **Constraints**: Out-of-range values (e.g., MQTT port 99999) should be rejected or clamped
7. **Boot Defaults**: Missing settings file should be created with sensible defaults
8. **Service Restarts**: Hostname change should trigger MDNS+MQTT restart; NTP changes should restart NTP only
9. **Memory**: Large settings struct, line buffer (256B), JSON helpers reuse global `cMsg` (`CMSG_SIZE`)
10. **PROGMEM**: All format strings, keys, and defaults must use PROGMEM macros

## Notes

- **Settings file**: `/settings.ini` on LittleFS; filesystem flashes (`flash_otgw --fs`, or a bare `--update`) reset it, app-only updates keep it
- **JSON format**: Custom format (no ArduinoJson); manually constructed by helper functions
- **Buffer reuse**: Global `cMsg[CMSG_SIZE]` shared for JSON escaping; safe within `writeSettings()` (no yield)
- **Line buffer**: Static 256-byte buffer in `readSettings()` prevents cMsg clobbering during file.readBytesUntil() yield() calls
- **Largest setting**: SAT section with ~100 fields
- **Missing feature**: MQTT TLS (`bSecure` flag exists but is not implemented; HTTP/MQTT only per ADR-003)
- **ADR-051**: Two-level named sub-sections provide better organization than a flat 100+ field struct (see `docs/adr/ADR-051-dual-encapsulating-structs.md`)
- **Notable settings features**:
  - **DeviceSection**: `settings.device.sManufacturer` / `sModel` identity metadata (Home Assistant discovery)
  - **Nightly restart**: `settings.bNightlyRestart` and `settings.iRestartHour` schedule a daily restart for heap recovery
  - **State naming**: runtime OpenTherm bus state is `state.otBus` (formerly `state.otgw`); persisted settings are unaffected
  - **MQTT separate sources** (ADR-040): `settings.mqtt.bSeparateSources` enables source-specific MQTT topics
  - **On-change publishing** (ADR-116) and **legacy topic/mode flags** (ADR-106, TASK-648)
  - **Combo board mode** (ADR-127 / ADR-158): `settings.iBoardMode` caches or overrides the boot-time PIC / OTDirect / S3 Mini Pro detection
  - **Onboarding flags**: `settings.ui.bOnboarded` and `bSatOnboarded` (TASK-997, TASK-1012)

---

**Related Code Elements**:
- `/src/OTGW-firmware/restAPI.ino`: REST API `/api/v2/settings` endpoints (`handleSettings()`, `postSettings()`, `sendDeviceSettings()`)
- `/src/OTGW-firmware/MQTTstuff.ino`: MQTT client using `settings.mqtt.*`
- `/src/OTGW-firmware/SATcontrol.ino`: SAT thermostat using `settings.sat.*`
- `/src/OTGW-firmware/OTGW-firmware.ino`: `loop()` (drives `flushSettings()` via `timerFlushSettings`) and `safeTimers.h` timer macros
- `/src/OTGW-firmware/helperStuff.ino` and `jsonStuff.ino`: whitespace trimming, JSON string escaping

**Related ADRs**:
- ADR-008: LittleFS configuration persistence
- ADR-040: MQTT Separate Source Topics
- ADR-051: Dual encapsulating structs (two-level named sub-sections, Hungarian prefixes)
- ADR-056: Protected admin endpoint security and secret-handling contract (supersedes ADR-054, `checkHttpAuth()` guard in `restAPI.ino`)
- ADR-079 / ADR-081: Per-component types headers
- ADR-106 / ADR-116: MQTT topic naming mode, on-change publishing default
- ADR-127 / ADR-158: Combo board-mode detection and S3 Mini Pro variant
- ADR-191: Safeboot updater (NVS mirror of hostname and HTTP password)

The deferred settings write (Finding #23) and GPIO conflict detection (Finding #27) are findings, not ADRs.
