# C4 Code Level: Settings Module

## Overview

- **Name**: Settings Module (OTGW-firmware Settings & Persistence Layer)
- **Description**: Persistent configuration management system for OTGW-firmware. Handles reading/writing settings to LittleFS as JSON without ArduinoJson library, provides server-side validation, and deferred-write optimization to reduce flash wear.
- **Location**: `/src/OTGW-firmware/settingStuff.ino` + settings structs in `/src/OTGW-firmware/OTGW-firmware.h`
- **Language**: Arduino C/C++ (ESP8266/ESP32)
- **Purpose**: Provide persistent configuration storage, REST API handler for settings updates, and service restart coordination through deferred-write patterns. Foundation for all module configuration: network, MQTT, GPIO, WebSocket, SAT thermostat, OT-direct, and hardware-specific features.

## Code Elements

### Core Functions

#### `readSettings(bool show): void`
- **File**: `settingStuff.ino:412`
- **Signature**: `void readSettings(bool show)`
- **Parameters**:
  - `show` (bool): If true, prints all loaded settings to debug output
- **Returns**: void
- **Purpose**: Load all persistent settings from LittleFS JSON file into global `settings` struct. Creates file with defaults if missing. Uses line-buffer-based parsing to avoid cMsg contention during yield() calls from file operations.
- **Key Behaviors**:
  - Opens `/settings.ini` for read; creates if missing via recursive `writeSettings()` call
  - Line buffer (256 bytes static) prevents interference with global `cMsg` used by HTTP handlers
  - Parses JSON KV pairs via `parseJsonKVLine()` helper
  - Calls `updateSetting()` for each field (applies validation, constraints, defaults)
  - Clears `settingsDirty` and `pendingSideEffects` after load (prevent unwanted service restarts during boot)
  - Post-processing applies defaults for empty values (hostname, MQTT topic, NTP timezone, etc.)
  - Updates timer intervals for sensors and S0 counter based on loaded settings
- **Dependencies**:
  - LittleFS filesystem
  - `updateSetting()` (validation logic)
  - `parseJsonKVLine()` (JSON parser)
  - Timer macros: `CHANGE_INTERVAL_SEC()`, `CATCH_UP_MISSED_TICKS`

#### `writeSettings(bool show): void`
- **File**: `settingStuff.ino:216`
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
  - Covers all 9 settings sub-sections + device-level fields
  - SAT settings alone: ~60+ fields (heating system, presets, weather, multi-zone, BLE, etc.)
  - OT-direct settings (HAS_DIRECT_OT): ~20 fields (mode, bypass, room compensation, heating curve)
  - Ethernet settings (HAS_ETH_CAPABLE): static IP configuration
- **Dependencies**:
  - LittleFS filesystem
  - JSON escaping via `escapeJsonStringTo()`
  - Type conversion: `dtostrf()` for float→string

#### `updateSetting(const char* field, const char* newValue): void`
- **File**: `settingStuff.ino:548`
- **Signature**: `void updateSetting(const char *field, const char *newValue)`
- **Parameters**:
  - `field` (const char*): Settings field name (case-insensitive, e.g., "hostname", "MQTTbroker")
  - `newValue` (const char*): New value as string
- **Returns**: void
- **Purpose**: Server-side validation dispatcher. Routes field updates to appropriate struct member with range checks, type conversion, GPIO conflict detection, and deferred service restart coordination. Called by REST API handler and during settings load.
- **Key Behaviors**:
  - Case-insensitive field matching via `strcasecmp_P()`
  - Masks password fields in debug output
  - Type-specific validation:
    - **Hostname**: auto-generates unique ID if empty, strips domain suffix, triggers MDNS+MQTT restart
    - **HTTP password**: placeholder detection (rejects "notthepassword", "password=N"), trims whitespace, updates OTA server credentials
    - **MQTT fields**: broker, port, user, password with range/placeholder checks; MQTT restart deferred
    - **GPIO pins**: 0-16 range check + conflict detection via `checkGPIOConflict()` against other enabled features
    - **Numeric fields**: constrained to safe ranges (e.g., MQTTinterval 0-65535, S0 debounce 0-1000ms)
    - **Float fields**: constrained (e.g., SAT target temp 5-30°C, heating curve coeff 0.1-5.0)
  - **Side-effect coordination**: Sets `pendingSideEffects` bitmask (SIDE_EFFECT_MQTT, SIDE_EFFECT_NTP, SIDE_EFFECT_MDNS) instead of immediate service restart
  - **Deferred write**: Sets `settingsDirty=true` and restarts `timerFlushSettings` to batch multiple updates
  - Covers all sub-sections:
    - Device: hostname, HTTP password, LED blink, dark theme, nightly restart (enable + hour), manufacturer, model
    - MQTT: 13 fields (enable, broker, port, user, passwd, topic, HA prefix, unique ID, OT msg, interval, separate sources, HA reboot detect)
    - NTP: timezone, hostname, send-time, enable
    - GPIO Sensors: enabled, legacy format, pin (D7/SDIO3), interval
    - S0 Counter: enabled, pin (D6), debounce, pulses/kW, interval
    - GPIO Outputs: enabled, pin, trigger bit
    - Webhook: enabled, URL on/off, trigger bit, payload template, content-type
    - UI: autoscroll, timestamps, capture mode, auto-screenshot, auto-export, graph time window
    - PIC boot: command enable, command string
    - SAT: 80+ settings (enabled, heating system, PID, presets, weather, simulation, multi-zone, BLE, etc.)
    - OT-direct: mode, auto-detect, setback, fail-safe, heating curve, room compensation PID
    - Ethernet: static IP, gateway, subnet, DNS (IP validation via `IPAddress::fromString()`)
- **Dependencies**:
  - `checkGPIOConflict()` for GPIO conflict detection
  - `trimwhitespace()` helper
  - Global state: `state.bSetupComplete`, `state.sat.*` for SAT state transitions
  - Timer macros: `CHANGE_INTERVAL_SEC()`, `RESTART_TIMER()`

#### `flushSettings(): void`
- **File**: `settingStuff.ino:58`
- **Signature**: `void flushSettings()`
- **Parameters**: None
- **Returns**: void
- **Purpose**: Deferred-write endpoint. Called from main loop timer (`timerFlushSettings`, 2s debounce). Writes dirty settings to flash once, then applies all pending side-effects exactly once per batch.
- **Key Behaviors**:
  - Returns early if `settingsDirty == false` (no-op optimization)
  - Calls `writeSettings(false)` to flush to LittleFS
  - Clears `settingsDirty` flag
  - Applies pending service restarts in order: MDNS/LLMNR → MQTT → NTP
  - Clears `pendingSideEffects` bitmask after applying all effects
  - Prevents redundant service restarts per batch
- **Purpose in Finding #23**: Coalesces rapid REST API updates (e.g., UI form submission) into single flash write + one restart per service, reducing flash wear and MQTT churn
- **Dependencies**:
  - `writeSettings()` for actual flash write
  - `startMDNS()`, `startLLMNR()`, `startMQTT()`, `startNTP()` for service restarts

#### `settingsMarkClean(): void`
- **File**: `settingStuff.ino:52`
- **Signature**: `void settingsMarkClean()`
- **Parameters**: None
- **Returns**: void
- **Purpose**: Manually clear dirty flag and pending side-effects without writing or restarting services. Used after direct `writeSettings()` calls (e.g., during OTA reboot window) to prevent deferred-flush timer from triggering unnecessary service restarts.
- **Dependencies**: None (stateless)

#### `checkGPIOConflict(int pin, GPIOConflictCaller caller): bool`
- **File**: `settingStuff.ino:87`
- **Signature**: `bool checkGPIOConflict(int pin, GPIOConflictCaller caller)`
- **Parameters**:
  - `pin` (int): GPIO pin number to check
  - `caller` (GPIOConflictCaller enum): Feature requesting the pin (Sensor, S0, Output)
- **Returns**: bool - true if pin conflicts with another enabled feature
- **Purpose**: Prevent simultaneous use of same GPIO by multiple features (Finding #27: GPIO conflict detection).
- **Key Behaviors**:
  - Returns false for negative pins (disabled/not set)
  - Checks pin against all other configurable GPIOs: sensors, S0 counter, outputs
  - Skips check against caller's own feature
  - Logs conflict warnings to debug
- **Supported Features**:
  - `GPIOConflictCaller::Sensor`: Dallas DS18B20 sensor pin (default GPIO 13/D7 or GPIO 10/SDIO3)
  - `GPIOConflictCaller::S0`: S0 pulse counter pin (default GPIO 12/D6)
  - `GPIOConflictCaller::Output`: GPIO relay output pin (default GPIO 16)
- **Dependencies**: None (stateless, accesses `settings` global)

#### `isHttpPasswordPlaceholder(const char* value): static bool`
- **File**: `settingStuff.ino:23`
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

### JSON Serialization Helpers

#### `parseJsonKVLine(const char* line, char* keyOut, size_t keyOutSize, char* valueOut, size_t valueOutSize): static bool`
- **File**: `settingStuff.ino:109`
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
- **File**: `settingStuff.ino:178`
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
- **File**: `settingStuff.ino:189`
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
- **File**: `settingStuff.ino:197`
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
- **File**: `settingStuff.ino:205`
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
- `OTGW-firmware.h:940` — struct definition
- Global instance: `OTGWSettings settings` (OTGW-firmware.h:966)

### Architecture (ADR-051: Two-Level Named Sub-Sections)

Settings use two-level named sub-sections accessed as `settings.mqtt.sBroker`, `settings.ntp.sTimezone`, etc.
Hungarian prefixes: **b** = bool, **s** = char[], **i** = int/uint, **f** = float.

```cpp
struct OTGWSettings {
  // Device-level fields (universal device identity)
  char sHostname[41];           // Device hostname (default "OTGW")
  char sHTTPpasswd[41];         // HTTP Basic Auth password
  bool bLEDblink;               // LED blink enable
  bool bDarkTheme;              // UI dark theme
  bool bMyDEBUG;                // Reserved
  bool bNightlyRestart;         // Scheduled daily restart for heap recovery (default false)
  uint8_t iRestartHour;         // Hour (0-23) for nightly restart (default 4)
  
  // Named sub-sections
  DeviceSection device;         // 2 fields: manufacturer, model identity
  MQTTSettingsSection mqtt;     // 13 fields: broker, auth, topics, HA discovery
  NTPSection ntp;               // 4 fields: enable, timezone, hostname, sendtime
  SensorsSection sensors;       // 4 fields: Dallas DS18B20 (enabled, pin, interval, format)
  S0Section s0;                 // 5 fields: S0 pulse counter (enabled, pin, debounce, pulses/kW, interval)
  OutputsSection outputs;       // 3 fields: GPIO relay (enabled, pin, trigger bit)
  WebhookSection webhook;       // 6 fields: HTTP webhook trigger (enabled, URLs, payload, content-type)
  UISection ui;                 // 6 fields: Web UI settings (autoscroll, timestamps, capture, etc.)
  PICBootSection picBoot;       // 2 fields: PIC boot command injection (enabled, commands)
  SATSection sat;               // 80+ fields: Smart Autotune Thermostat (all control/tuning params)
  OTDirectSettingsSection otd;  // 20 fields: OT-direct mode (HAS_DIRECT_OT only)
  EthernetSection eth;          // 5 fields: Ethernet static IP (HAS_ETH_CAPABLE only)
};
```

### Sub-Section Details

#### DeviceSection (device) — Device Identity Metadata
```cpp
struct DeviceSection {
  char sManufacturer[32] = "NodoShop";   // Device manufacturer (e.g. "NodoShop", "Seegel")
  char sModel[32]        = "OTGW";       // Device model (e.g. "OTGW", "OTGW32", "OT-Thing")
};
```
**Purpose**: Store device identity metadata for REST API and HA discovery. Updated via REST settings API (`updateSetting("DeviceManufacturer", ...)` and `updateSetting("DeviceModel", ...)`). Allows custom device identification independent of firmware version. Used by Home Assistant discovery to label device in UI.

#### MQTTSettingsSection (mqtt)
```cpp
struct MQTTSettingsSection {
  bool    bEnable            = true;                         // MQTT enable/disable
  bool    bSecure            = false;                        // TLS enable (not implemented)
  char    sBroker[65]        = "homeassistant.local";        // Broker hostname/IP
  int16_t iBrokerPort        = 1883;                         // Broker port (1-65535)
  char    sUser[41]          = "";                           // Username
  char    sPasswd[41]        = "";                           // Password
  char    sHaprefix[41]      = HOME_ASSISTANT_DISCOVERY_PREFIX; // HA discovery prefix
  bool    bHaRebootDetect    = true;                         // Reboot detection via MQTT
  char    sTopTopic[41]      = "OTGW";                       // Root topic
  char    sUniqueid[41]      = "";                           // HA unique device ID
  bool    bOTmessage         = false;                        // Publish full OT messages
  uint16_t iInterval         = 0;                            // Publish interval (0=every message)
  bool    bSeparateSources   = false;                        // ADR-040: source-specific topics
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

#### SensorsSection (sensors) — Dallas DS18B20
```cpp
struct SensorsSection {
  bool    bEnabled       = false;             // Enable 1-Wire sensor scan
  bool    bLegacyFormat  = false;             // JSON format (false=new standard)
  int8_t  iPin           = 10;                // GPIO pin (10=SDIO3, 13=D7)
  int16_t iInterval      = 20;                // Poll interval (seconds)
};
```

#### S0Section (s0) — Energy Pulse Counter
```cpp
struct S0Section {
  bool     bEnabled      = false;             // Enable S0 counter
  uint8_t  iPin          = 12;                // GPIO pin (12=D6, preferred)
  uint16_t iDebounceTime = 80;                // Debounce (ms)
  uint16_t iPulsekw      = 1000;              // Pulses per kW (typical S0)
  uint16_t iInterval     = 60;                // Report interval (seconds)
};
```

#### OutputsSection (outputs) — GPIO Relay
```cpp
struct OutputsSection {
  bool   bEnabled    = false;                 // Enable GPIO output relay
  int8_t iPin        = 16;                    // GPIO pin (16=D0)
  int8_t iTriggerBit = 0;                     // OT message bit to trigger (0-15)
};
```

#### WebhookSection (webhook) — HTTP Trigger
```cpp
struct WebhookSection {
  bool   bEnabled         = false;            // Enable webhook
  char   sURLon[101]      = "http://...";     // URL when trigger ON
  char   sURLoff[101]     = "http://...";     // URL when trigger OFF
  int8_t iTriggerBit      = 1;                // OT message bit (default bit 1 = CH mode)
  char   sPayload[201]    = "";               // POST body (empty=GET)
  char   sContentType[32] = "application/json"; // Content-Type header
};
```

#### UISection (ui) — Web Interface Settings
```cpp
struct UISection {
  bool bAutoScroll      = true;               // Auto-scroll OT log
  bool bShowTimestamp   = true;               // Show timestamps
  bool bCaptureMode     = false;              // Capture mode
  bool bAutoScreenshot  = false;              // Auto-screenshot
  bool bAutoDownloadLog = false;              // Auto-download log
  bool bAutoExport      = false;              // Auto-export
  int  iGraphTimeWindow = 60;                 // Graph time window (minutes, 1-1440)
};
```

#### PICBootSection (picBoot) — PIC Boot Commands
```cpp
struct PICBootSection {
  bool bEnable        = false;                // Enable command injection
  char sCommands[129] = "";                   // Newline-separated PIC commands
};
```

#### SATSection (sat) — Smart Autotune Thermostat (~80 fields)
Controls autonomous heating/cooling via OpenTherm. Includes:
- **Core**: enabled, heating system (auto/radiators/heat pump/underfloor), target temp, heating curve coefficient, deadband, control interval
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
- **BLE sensor** (ESP32 only): enable, MAC, scan interval
- **LittleFS persistence**: auto-flush threshold (hours)
- See `OTGW-firmware.h:802-901` for complete list with defaults and ranges.

#### OTDirectSettingsSection (otd) — OT-Direct Mode (HAS_DIRECT_OT only, OTGW32/OT-Thing)
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
  bool     bHasBypassRelay    = false;         // Runtime: bypass relay present
  
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
};
```

#### EthernetSection (eth) — Ethernet Static IP (HAS_ETH_CAPABLE only, OTGW32)
```cpp
struct EthernetSection {
  bool bStaticIP        = false;               // Enable static IP
  char sIPaddress[16]   = "";                  // Static IP (e.g., "192.168.1.100")
  char sGateway[16]     = "";                  // Gateway IP
  char sSubnet[16]      = "";                  // Subnet mask
  char sDNS[16]         = "";                  // DNS server IP
};
```

## Dependencies

### Internal Dependencies (Firmware Modules)

- **restAPI.ino**: Calls `updateSetting()` for REST API PATCH `/api/v1/settings` and `/api/v2/settings`
- **MQTTstuff.ino**: Reads MQTT settings (`settings.mqtt.*`); `startMQTT()` restarted by `flushSettings()`
- **networkStuff.ino**: `startMDNS()`, `startLLMNR()`, `startNTP()` service restart functions
- **OTGW-Core.ino**: Timer infrastructure (`CHANGE_INTERVAL_SEC()`, `RESTART_TIMER()`)
- **helperStuff.ino**: `trimwhitespace()`, `escapeJsonStringTo()`
- **SATcontrol.ino**: `satDisable()` called when SAT disabled via settings update
- **Debug.h**: Debug output macros (`DebugTln()`, `DebugTf()`, `DebugT()`)
- **safeTimers.h**: Timer macros for sensor/S0 interval updates

### External Dependencies (Libraries)

- **LittleFS**: File I/O for `/settings.ini` persistence
- **Arduino Core**: `constrain()`, `atoi()`, `atof()`, `strlcpy()`, `strcasecmp_P()`, `strncasecmp_P()`, `strchr()`, `strstr_P()`, `memmove()`, `tolower()`, `IPAddress::fromString()`
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
- Reduces firmware size and RAM footprint (critical on ESP8266's ~40KB usable RAM)

### 3. PROGMEM String Management
- All JSON keys and format strings use `F()` macro or `PSTR()` to stay in flash
- Passwords masked in debug output (prevent credential leaks to telnet logs)
- `_P` suffix helpers: `strcasecmp_P()`, `strncasecmp_P()`, `strcmp_P()`, `strstr_P()`, `snprintf_P()`, etc.

### 4. GPIO Conflict Detection (Finding #27)
- `checkGPIOConflict()` prevents simultaneous use of same GPIO by sensors, S0 counter, or outputs
- Called during `updateSetting()` when GPIO pin field changes
- Logs warnings but does NOT block the change (user can override via UI)

### 5. Password Placeholder Handling
- Web UI sends "notthepassword" or "password=N" when password field submitted empty (keep existing)
- `isHttpPasswordPlaceholder()` detects pattern; `updateSetting()` ignores placeholders
- OTA server credentials updated immediately on HTTP password change (for security)

### 6. Service Restart Coordination
Three types of deferred side-effects:
- **SIDE_EFFECT_MDNS**: Restart mDNS/LLMNR service (hostname change)
- **SIDE_EFFECT_MQTT**: Restart MQTT client (broker, port, credentials)
- **SIDE_EFFECT_NTP**: Restart NTP sync (timezone, hostname)

Each applied exactly once per flush batch, in order: MDNS → MQTT → NTP.

### 7. Settings Load Safety
- Clears `settingsDirty` and `pendingSideEffects` after load (prevent unwanted restarts during boot)
- Line buffer (256 bytes static) decouples file read from global `cMsg` (used by HTTP handlers during yield())
- Creates file with defaults if missing via recursive call pattern
- Post-processing applies sensible defaults for empty values (hostname fallback, MQTT topic defaults, etc.)

### 8. Type Validation & Range Checking
All numeric/enum fields constrained during `updateSetting()`:
- **Integer ranges**: MQTTbrokerPort (1-65535), S0 debounce (0-1000ms), SAT control interval (10-300s)
- **Float ranges**: SAT target temp (5-30°C), heating curve (0.1-5.0), PID gains (0-20.0)
- **Enum values**: SAT heating system (0-3), OT-direct mode (0-4)
- **String bounds**: Checked via `strlcpy()` (automatic truncation with safety)
- **IP validation**: Ethernet static IPs validated via `IPAddress::fromString()`

### 9. Settings Dirty Flag Coordination
- `settingsDirty = true` set by `updateSetting()` for any field change
- `flushSettings()` consumes flag; clears after write
- `settingsMarkClean()` allows manual clear (e.g., after OTA reboot window)
- Prevents accidental writes during boot or external flash operations

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

REST API PATCH /api/v1/settings [HTTP handler]
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
      │   └─ file.close()
      ├─ Apply side-effects: startMDNS(), startMQTT(), startNTP()
      └─ Clear pendingSideEffects bitmask

Direct call (e.g., OTA window)
  ├─ writeSettings(false)
  └─ settingsMarkClean() [prevent deferred-flush side-effects]
```

## File Structure

```
/src/OTGW-firmware/
  ├─ OTGW-firmware.h
  │  ├─ struct DeviceSection (line 969) — device identity metadata
  │  ├─ struct OTGWSettings (line 974) — root settings container
  │  ├─ All sub-section struct definitions (lines 758-936)
  │  ├─ Global instance: OTGWSettings settings (line 1003)
  │  └─ Prototypes: readSettings(), writeSettings(), updateSetting()
  └─ settingStuff.ino
     ├─ Deferred-write support (lines 14-82)
     │  ├─ settingsDirty, pendingSideEffects global flags (lines 21-22)
     │  ├─ flushSettings(), settingsMarkClean() (lines 53-82)
     │  └─ SIDE_EFFECT_* bitmask constants (lines 18-20)
     ├─ Password placeholder detection (lines 24-47)
     │  └─ isHttpPasswordPlaceholder()
     ├─ GPIO conflict detection (lines 88-107)
     │  └─ checkGPIOConflict()
     ├─ JSON parser & serializers (lines 110-214)
     │  ├─ parseJsonKVLine() (line 110)
     │  ├─ writeJsonStringKV(), writeJsonBoolKV(), writeJsonIntKV(), writeJsonFloatKV() (lines 179-214)
     ├─ Core functions (lines 217-930)
     │  ├─ writeSettings() (line 217) — serializes entire struct to LittleFS JSON
     │  ├─ readSettings() (line 416) — loads settings from LittleFS, applies defaults
     │  ├─ updateSetting() [dispatcher] (line 553) — validates and updates any field
     │  └─ isHttpPasswordPlaceholder() (line 24)

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
9. **Memory**: Large settings struct (~2KB), line buffer (256B), JSON helpers reuse global `cMsg` (512B)
10. **PROGMEM**: All format strings, keys, and defaults must use PROGMEM macros

## Notes

- **Settings file**: `/settings.ini` on LittleFS (not SD card); survives OTA updates
- **JSON format**: Custom format (no ArduinoJson); manually constructed by helper functions
- **Buffer reuse**: Global `cMsg[512]` shared for JSON escaping; safe within `writeSettings()` (no yield)
- **Line buffer**: Static 256-byte buffer in `readSettings()` prevents cMsg clobbering during file.readBytesUntil() yield() calls
- **Largest setting**: SAT section with ~80+ fields; updates trigger SIDE_EFFECT_MQTT but also local state changes
- **Missing feature**: MQTT TLS (bSecure flag exists but not implemented; HTTP/MQTT only per project design)
- **ADR-051**: Two-level named sub-sections provide better organization than flat 100+ field struct (see docs/adr/adr-051-settings-architecture.md)
- **Recent changes (v1.4.0+)**:
  - **DeviceSection** (commit be3e8764): New `settings.device.sManufacturer` and `settings.device.sModel` for device identity metadata (Home Assistant discovery)
  - **Nightly restart** (v1.3.5+): `settings.bNightlyRestart` and `settings.iRestartHour` allow scheduled daily restart for heap recovery
  - **State rename** (commit f3354dd6): Runtime state member renamed from `state.otgw` to `state.otBus` to reflect OpenTherm bus state (persistent settings unaffected)
  - **MQTT separate sources** (ADR-040): `settings.mqtt.bSeparateSources` enables source-specific MQTT topics, reducing MQTT churn for discovery
  - **WiFi reset button** (board-specific): May be configured via GPIO settings (see OutputsSection for GPIO relay outputs)

---

**Related Code Elements**:
- `/src/OTGW-firmware/restAPI.ino` — REST API `/api/v1/settings` and `/api/v2/settings` endpoints
- `/src/OTGW-firmware/MQTTstuff.ino` — MQTT client using `settings.mqtt.*`
- `/src/OTGW-firmware/SATcontrol.ino` — SAT thermostat using `settings.sat.*`
- `/src/OTGW-firmware/OTGW-Core.ino` — Timer infrastructure and main loop
- `/src/OTGW-firmware/helperStuff.ino` — Helper functions: escaping, whitespace trimming

**Related ADRs**:
- ADR-023: Deferred Settings Write (reduce flash wear, service restart churn)
- ADR-027: GPIO Conflict Detection (prevent simultaneous GPIO use)
- ADR-040: MQTT Separate Source Topics (publishToSourceTopic optimization)
- ADR-051: Settings Architecture (two-level named sub-sections, Hungarian prefixes)
- ADR-054: HTTP Basic Auth (checkHttpAuth guard in restAPI.ino)
