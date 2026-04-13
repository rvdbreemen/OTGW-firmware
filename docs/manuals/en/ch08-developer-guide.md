## Chapter 8: Developer Guide

This chapter is the primary reference for contributors and developers working with the OTGW-firmware source code. It covers the project layout, build system, architecture model, key coding patterns, and the step-by-step workflows for extending the firmware with new REST endpoints, MQTT topics, and settings. Read this chapter before writing a single line of code.

---

### Project Layout

The repository root contains the firmware source, build tooling, documentation, tests, and reference projects. The primary source tree is under `src/OTGW-firmware/`.

#### Source Files

All firmware code lives in `src/OTGW-firmware/` as Arduino `.ino` files. Arduino compiles the main sketch plus all `.ino` files in the same directory as a single translation unit. Each `.ino` file owns one logical domain:

| File | Domain |
|------|--------|
| `OTGW-firmware.ino` | Main entry point: `setup()`, `loop()`, boot sequence, LED management |
| `OTGW-Core.ino` | OpenTherm protocol decoding, PIC serial I/O, command queue, MQTT throttle |
| `OTGW-Core.h` | OpenTherm data structures: `OTdataStruct`, `OTLibMessageID`, frame type enums |
| `OTGW-firmware.h` | Shared typedefs, all struct definitions (`OTGWSettings`, `OTGWState`), forward declarations, constants |
| `MQTTstuff.ino` | MQTT client state machine, HA auto-discovery, command subscriptions, `sendMQTTData()` |
| `networkStuff.ino` | WiFi, mDNS, LLMNR, NTP, OTA, network platform abstraction |
| `restAPI.ino` | REST API v2 dispatch table, all resource handlers, HTTP Basic Auth, CSRF, file serving |
| `FSexplorer.ino` | LittleFS browser: file listing, upload/delete endpoints |
| `jsonStuff.ino` | Low-level JSON formatting helpers: `sendJsonMapEntry()`, `sendJsonKVLine()`, streaming map builders |
| `settingStuff.ino` | Settings load/save (LittleFS JSON), `updateSetting()` validation, side-effect dispatch |
| `webSocketStuff.ino` | WebSocket server on port 81, OT log streaming, heap backpressure gate |
| `helperStuff.ino` | Heap health monitor, `canSendWebSocket()`, `canPublishMQTT()`, LittleFS status helpers |
| `OTDirect.ino` | ESP32-only: direct GPIO OpenTherm bus via ISR, OTDirect operating modes, frame bridge |
| `SAT.ino` | Smart Autotune Thermostat: master enable/disable, SAT settings bridge |
| `SATcontrol.ino` | SAT control loop, heating curve, boiler state machine, setpoint injection |
| `SATpid.ino` | PID v3 implementation: proportional, integral, derivative with deadband |
| `SATcycles.ino` | Cycle classification, overshoot detection, anti-cycling |
| `SATweather.ino` | Open-Meteo weather fetch, outdoor temperature for SAT heating curve |
| `SATble.ino` | ESP32 BLE room temperature sensor integration (BTHome protocol) |
| `sensorStuff.ino` | Dallas DS18B20 temperature sensors, S0 pulse counter, OLED display |
| `boards.h` | Board-specific pin maps and feature flags (`HAS_PIC`, `HAS_DIRECT_OT`, `HAS_ETH_CAPABLE`) |
| `safeTimers.h` | Timer macros: `DECLARE_TIMER_SEC()`, `DUE()`, `RESTART_TIMER()` |
| `platform.h`, `platform_esp8266.h`, `platform_esp32.h` | Platform abstraction layer |

#### Web Assets

The browser SPA lives in `src/OTGW-firmware/data/`. These files are flashed to LittleFS:

| File | Purpose |
|------|---------|
| `index.html` | Single-page application shell (~11 KB — always stream, never load into RAM) |
| `index.js` | Main SPA JavaScript: live OT log, WebSocket client, settings forms |
| `graph.js` | ECharts-based real-time temperature graphs |
| `index.css` | Stylesheet with dark/light theme support |

#### Build Infrastructure

| File / Directory | Purpose |
|-----------------|---------|
| `build.py` | Primary build script: invokes PlatformIO internally, packages firmware and LittleFS artifacts |
| `platformio.ini` | PlatformIO project: `esp8266` and `esp32` environments |
| `evaluate.py` | Static code quality checker: PROGMEM usage, unsafe patterns, String class audit |
| `scripts/` | PlatformIO pre-build scripts: version injection, library patching |
| `libraries/` | Vendored Arduino libraries used by both build systems |
| `docs/` | C4 architecture docs, ADRs, API references, feature docs |
| `other-projects/` | Read-only reference codebases (PIC firmware, OTmonitor, OT-Thing, SAT) |
| `backlog/` | Project task management (Backlog.md) |

---

### Build System

#### build.py (release builds)

The primary build script for producing release artifacts. It invokes PlatformIO internally, handles version embedding, filesystem packaging, and artifact collection.

```bash
python build.py              # Build firmware + LittleFS filesystem image
python build.py --firmware   # Build firmware only (skip filesystem)
python build.py --clean      # Clean build artifacts before building
python build.py --upload     # Build and upload to connected device
```

The script reads the Git tag to embed the version string and places output in the `build/` directory.

#### PlatformIO

PlatformIO is the preferred build system and is used for both ESP8266 and ESP32. Two environments are defined in `platformio.ini`:

| Environment | Target | Board | Core | CPU |
|-------------|--------|-------|------|-----|
| `esp8266` | Wemos D1 mini / NodeMCU (NodoShop OTGW) | `d1_mini` | Arduino Core 3.1.2 (espressif8266) | 160 MHz |
| `esp32` | NodoShop OTGW32 | `esp32dev` | pioarduino espressif32 fork | 240 MHz |

The ESP8266 LittleFS partition size is 2 072 576 bytes (approximately 2 MB). This is configured in `platformio.ini` via the board options.

```bash
pio run                          # Build all environments
pio run -e esp8266               # Build ESP8266 only
pio run -e esp32                 # Build ESP32 only
pio run -e esp8266 -t upload     # Build and upload ESP8266
pio run -e esp8266 -t uploadfs   # Upload LittleFS filesystem (ESP8266)
pio run -e esp32 -t upload       # Build and upload ESP32
pio run -e esp32 -t uploadfs     # Upload LittleFS filesystem (ESP32)
```

PlatformIO's pre-build scripts in `scripts/` handle version injection (`platformio_version.py`) and library compatibility patching (`patch_pio_libs.py`).

##### ESP8266-specific PlatformIO notes

The ESP8266 build excludes `OTDirect.ino` (ESP32-only) via `build_src_filter`. PlatformIO's ctags scanner processes all `.ino` files regardless of the filter, so ESP32-specific types (`OpenThermResponseStatus`, `OTDirectMode`, `OTDirectRequestOrigin`) are stubbed out as `-D` flags to prevent ctags-generated forward declaration errors. These stubs have no runtime effect.

The `OpenTherm Library` is explicitly excluded in `lib_ignore` because PlatformIO's LDF scanner ignores `#if` guards and would otherwise compile the ESP32-only OT library for ESP8266.

##### ESP32-specific PlatformIO notes

PlatformIO's ctags scanner processes all `.ino` files regardless of `#if` guards and generates forward declarations for every function it finds. On ESP32 (`HAS_PIC=0`), `OTGWSerial.h` is never included, so `OTGWFirmware` (an enum type used as a parameter in `fwreportinfo()`) is unknown at ctags forward-declaration time. The flag `-DOTGWFirmware=int` stubs it out so the generated forward declaration compiles. The actual `fwreportinfo()` body is inside a `#if HAS_PIC` block and is never compiled on ESP32. This follows the same pattern as the OTDirect type stubs for ESP8266.

#### Arduino IDE

Arduino IDE is supported for ESP8266 only. Install the ESP8266 Arduino core, then open `src/OTGW-firmware/OTGW-firmware.ino`. Required libraries are in `libraries/` — add that directory to your Arduino sketchbook library path. The IDE does not support the ESP32 target or `OTDirect.ino`.

#### evaluate.py

Run the code quality checker after making changes to catch common ESP8266-specific mistakes:

```bash
python evaluate.py           # Full analysis (all files)
python evaluate.py --quick   # Fast pass (changed files only)
```

The checker flags:
- `Serial.print()` calls (must never appear after init)
- String literal not wrapped in `F()` or `PSTR()`
- `String` class usage in non-setup code
- `strcpy`/`sprintf` without bounds (should use `strlcpy`/`snprintf_P`)
- `strncmp_P`/`strstr_P` on binary data (use `memcmp_P`)

---

### Board Hardware Variants

The firmware supports two official Nodoshop hardware variants, defined in `boards.h`. The board is selected at build time by a `BOARD_*` preprocessor flag set in `platformio.ini`. When no explicit flag is set, the board is auto-detected from the MCU type (`ESP8266` or `ESP32`).

**These flags are board-level, not MCU-level.** The presence of a PIC, Ethernet hardware, or a direct OpenTherm GPIO interface is a property of the specific Nodoshop PCB, not of the MCU family. Replacing an ESP8266 with an ESP32 on the OTGW WiFi board is not an official Nodoshop configuration and is not modelled.

#### Feature Flags

Three compile-time flags in `boards.h` describe the hardware capabilities of each board:

| Flag | Type | OTGW WiFi (ESP8266) | OTGW32 (ESP32) | Meaning |
|------|------|---------------------|----------------|---------|
| `HAS_PIC` | `0` / `1` | `1` | `0` | Board has a PIC co-processor that handles the OpenTherm electrical bus over UART |
| `HAS_DIRECT_OT` | `0` / `1` | `0` | `1` | Board drives the OpenTherm bus directly from GPIO via the OTDirect ISR |
| `HAS_ETH_CAPABLE` | `0` / `1` | `0` | `1` | Board has a W5500 SPI Ethernet module |

Use these flags in conditional compilation:

```cpp
#if HAS_PIC
  // PIC-specific code: addCommandToQueue(), OTGWSerial, fwreportinfo(), etc.
#endif

#if HAS_DIRECT_OT
  // OTDirect GPIO code: loopOTDirect(), OTDirectMode, etc.
#endif

#if HAS_ETH_CAPABLE
  // Ethernet code: loopEthernet(), state.hw.bEthernetPresent, etc.
#endif
```

#### W5500 Runtime Detection

`HAS_ETH_CAPABLE=1` means the board design includes a W5500 module header, but the module may not be physically installed on every unit. At boot, `probeW5500()` in `Ethernet.ino` reads the W5500 VERSION register via SPI:

- If the register returns the expected value, `state.hw.bEthernetPresent` is set to `true` and Ethernet is initialised.
- If the read fails or returns an unexpected value, `state.hw.bEthernetPresent` is set to `false` and the firmware runs in WiFi-only mode.

No user configuration is required. `loopEthernet()` guards itself with `if (!state.hw.bEthernetPresent) return;` so it is a no-op when no hardware is present.

---

### C4 Architecture Overview

The firmware uses a four-level C4 model documented in `docs/c4/`. Read the relevant level before touching code.

#### Level 1: Context (`c4-context.md`)

The OTGW-firmware sits between the OpenTherm heating bus and the home automation network. External actors are: Room Thermostat, Boiler (via PIC co-processor or OTDirect GPIO on ESP32), MQTT Broker, Home Assistant, NTP Server, Web Browser, OTmonitor TCP client, Dallas/S0/BLE sensors.

#### Level 2: Container (`c4-container.md`)

Two deployment targets share one codebase:
- **ESP8266 (OTGW)**: NodoShop board with PIC16F co-processor. PIC handles the OpenTherm electrical bus; ESP8266 communicates with it over UART at 9600 baud.
- **ESP32 (OTGW32)**: NodoShop board with direct GPIO OpenTherm interface (`OTDirect`), W5500 Ethernet, BLE, and OLED. The PIC is absent; the ESP32 drives the OT bus directly via interrupt-driven Manchester coding.

#### Level 3: Component (`c4-component.md`)

Seven components, each with a dedicated doc in `docs/c4/c4-component-*.md`:

| Component | Primary files | Responsibility |
|-----------|--------------|----------------|
| OpenTherm Core | `OTGW-Core.ino`, `OTDirect.ino` | Protocol decode, PIC UART, command queue |
| Network and Connectivity | `networkStuff.ino` | WiFi, NTP, mDNS, Ethernet failover |
| Integration Layer | `MQTTstuff.ino`, `restAPI.ino`, `jsonStuff.ino` | MQTT, REST API, HA discovery |
| Configuration and State | `settingStuff.ino`, `OTGW-firmware.h` | Settings persistence, OTGWSettings/State |
| Smart Thermostat (SAT) | `SAT*.ino`, `SATcontrol.ino`, `SATpid.ino` | Heating curve, PID, cycle management |
| Sensors and Hardware | `sensorStuff.ino` | Dallas, S0, GPIO relay, OLED |
| Web Interface | `webSocketStuff.ino`, `helperStuff.ino`, `data/` | WebSocket, telnet, heap gate, browser SPA |

#### Level 4: Code (`c4-code-*.md`)

Detailed function signatures, line numbers, and inter-file dependencies for every module. Read the relevant `c4-code-*.md` file before modifying any non-trivial function.

---

### Key Architectural Patterns

#### Cooperative Scheduling

The ESP8266 Arduino framework is single-threaded and cooperative. The main `loop()` calls `doBackgroundTasks()`, which in turn calls all subsystem handlers. There is no RTOS; `delay()` blocks everything and must never be used. Long operations must be broken into state machines checked on each `loop()` iteration.

`feedWatchDog()` services the hardware watchdog. It does not call `yield()` (that line is intentionally commented out to prevent re-entrancy of `doBackgroundTasks()`). The background task re-entrancy that does exist comes from MQTT autoconfig's file-reading loop calling `handleOTGW()` mid-iteration.

#### Static Buffers (ADR-004 / ADR-053)

The ESP8266 has approximately 40 KB of usable heap. Dynamic allocation fragments the heap and leads to crashes after days of operation. All buffers are statically allocated:

- Use `char buf[SIZE]` with `snprintf_P(buf, sizeof(buf), PSTR(...))`.
- Never use `String` class in hot paths (ISR, MQTT publish, OT processing loop).
- `String` is tolerated only in `setup()` / one-off init code, and only when the ESP SDK forces it (e.g., `ESP.getResetReason()`).
- Key buffer constants: `CMSG_SIZE` = 512 bytes (general scratch), `SLINE_SIZE` = 1200 bytes (MQTT autoconfig line buffer).

#### PROGMEM (ADR-009)

All string literals must live in flash (PROGMEM), not RAM:

```cpp
DebugTln(F("Starting MQTT"));                         // F() macro: flash-resident string
DebugTf(PSTR("Heap: %u bytes\r\n"), freeHeap);        // PSTR(): for printf-style format strings
snprintf_P(buf, sizeof(buf), PSTR("val=%d"), v);      // snprintf_P: format from flash
const char label[] PROGMEM = "boilertemperature";     // PROGMEM constant in flash
```

For comparisons use `strcmp_P(ram_ptr, PSTR("keyword"))` or `strcasecmp_P()`. Never pass a PROGMEM pointer to a function expecting a RAM pointer.

Binary data (non-null-terminated buffers) must use `memcmp_P()`. Using `strncmp_P` or `strstr_P` on binary data causes an Exception 2 crash.

#### Platform Abstraction Layer (ADR-061)

All ESP8266/ESP32 SDK differences are isolated in three files: `platform.h`, `platform_esp8266.h`, and `platform_esp32.h`. Application code includes only `platform.h` and calls the unified functions. Never call platform-specific SDK functions directly in `.ino` files.

Key functions provided by the abstraction layer:

| Function | Description |
|----------|-------------|
| `platformSetHostname(hostname)` | Set the WiFi station hostname for DHCP |
| `platformGetHostname()` | Read back the current hostname |
| `platformRestartDHCP()` | Force a DHCP re-announce (e.g., after hostname change) |
| `platformFreeHeap()` | Free heap in bytes |
| `platformMaxFreeBlock()` | Largest contiguous free block (fragmentation indicator) |
| `platformRestart()` | Platform-safe reboot |
| `platformCoreVersion()` | Arduino core version string |
| `platformChipId()` | Unique chip identifier |
| `platformGetMacAddress(mac)` | Fill a 6-byte buffer with the MAC address |

On ESP8266, `MDNS_NEEDS_UPDATE` is defined as `1`, which means the main loop must call `MDNS.update()` on every iteration. On ESP32 this is not needed (`MDNS_NEEDS_UPDATE` is not defined). The main loop guards this with `#if defined(MDNS_NEEDS_UPDATE)`.

The `PlatformDir` class in `platform.h` provides a unified directory iteration interface over LittleFS (ESP8266 uses `Dir`; ESP32 uses `File`-based iteration). Use `PlatformDir` instead of calling filesystem APIs directly.

#### No ArduinoJson

Do not add ArduinoJson to this project. JSON is:
- **Built**: using `snprintf_P`, `sendJsonMapEntry()`, and chunked `httpServer.sendContent()`.
- **Parsed**: using the project's own `parseJsonKVLine()` helper (key-value line scanner, sufficient for the flat settings JSON and MQTT command payloads).

#### Heap Backpressure Gate

Two gate functions protect heap health during normal operation:

```cpp
bool canPublishMQTT();     // false when heap is below HEAP_LOW threshold
bool canSendWebSocket();   // false when heap is below HEAP_WARNING threshold
```

Always check the appropriate gate before sending MQTT messages in background tasks. The heap monitor in `helperStuff.ino` defines four levels: `HEAP_HEALTHY` (>8 KB), `HEAP_LOW` (5-8 KB), `HEAP_WARNING` (3-5 KB), `HEAP_CRITICAL` (<3 KB).

#### Settings / State Separation (ADR-051)

All persistent configuration lives in `OTGWSettings settings` (serialised to `/settings.ini` on LittleFS). All transient runtime state lives in `OTGWState state`. Both structs are defined in `OTGW-firmware.h` and use two-level named sub-sections with Hungarian prefixes:

```cpp
settings.mqtt.sBroker          // persistent: MQTT broker hostname
settings.sat.fHeatingCurveCoeff // persistent: SAT heating curve coefficient
state.otgw.bOnline             // transient: PIC serial link alive
state.pic.sFwversion           // transient: PIC firmware version string
```

Never persist transient state. Never access `state` fields from `writeSettings()`.

---

### Adding a New REST Endpoint

The REST API v2 uses a centralized dispatch table (ADR-050) in `restAPI.ino`. Adding an endpoint requires:

1. Write the handler function.
2. Add one entry to `kV2Routes[]`.
3. No changes to the router.

#### Step 1: Write the Handler

Handler signature:

```cpp
void handleMyFeature(const char words[][API_WORD_LEN], uint8_t wc,
                     HTTPMethod method, const char* originalURI)
```

Parameters:
- `words`: URI tokens after `/api/v2/`, e.g., `words[0]` = `"myfeature"`, `words[1]` = `"status"`.
- `wc`: Word count.
- `method`: `HTTP_GET`, `HTTP_POST`, etc.
- `originalURI`: The full original URI string.

Example handler for `GET /api/v2/myfeature/status`:

```cpp
void handleMyFeature(const char words[][API_WORD_LEN], uint8_t wc,
                     HTTPMethod method, const char* originalURI) {
  if (method != HTTP_GET) {
    httpServer.send(405, F("application/json"),
                    F("{\"error\":{\"status\":405,\"message\":\"Method Not Allowed\"}}"));
    return;
  }
  // Require auth for all endpoints that expose sensitive data
  if (!checkHttpAuth()) return;

  char buf[CMSG_SIZE];
  snprintf_P(buf, sizeof(buf),
    PSTR("{\"myvalue\":%d,\"status\":\"ok\"}"),
    myGlobalValue);
  httpServer.send(200, F("application/json"), buf);
}
```

Rules:
- Never use `Serial.print()` for debug — use `DebugTln()` or `DebugTf()`.
- Use `CMSG_SIZE` (512 bytes) as the standard response buffer unless you need a larger response.
- For responses larger than `CMSG_SIZE`, use `httpServer.sendContent()` in a chunked loop with `httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN)`.
- All mutation endpoints (POST, PUT, DELETE) must call `checkHttpAuth()` and return immediately if it returns false.
- Use `sendApiError(httpCode, F("message"))` to return consistent JSON error responses.
- File serving must use `httpServer.streamFile()` for any file larger than 2 KB.

#### Step 2: Register in the Dispatch Table

Locate `kV2Routes[]` in `restAPI.ino` (search for `kV2Routes`). Add one entry:

```cpp
{ PSTR("myfeature"), handleMyFeature },
```

The first field is the PROGMEM resource name matching the first URI token after `/api/v2/`. The second is the handler function pointer. Order in the table does not affect routing.

That is all. The router in `processAPI()` matches `words[0]` against the table and dispatches automatically.

---

### Adding a New MQTT Topic

#### Publish Side

All MQTT publishing flows through `sendMQTTData()` in `MQTTstuff.ino`:

```cpp
void sendMQTTData(const char* topic, const char* value, const bool retain = false);
void sendMQTTData(const __FlashStringHelper* topic, const char* value, const bool retain = false);
void sendMQTTData(const __FlashStringHelper* topic, const __FlashStringHelper* value,
                  const bool retain = false);
```

The function prepends the configured topic namespace (`{TopTopic}/value/{UniqueId}/`) automatically. It also checks `canPublishMQTT()` before sending.

Example — publish a new float value:

```cpp
if (canPublishMQTT()) {
  char payload[16];
  dtostrf(myFloatValue, 0, 2, payload);
  sendMQTTData(F("mynewtopic"), payload, true);  // retain=true for sensor values
}
```

For source-separated topics (when `settings.mqtt.bSeparateSources` is enabled), use `publishToSourceTopic()` instead:

```cpp
publishToSourceTopic(label, valueStr, sourceFlag);  // sourceFlag: 'T', 'B', or 'A' (Answer)
```

For periodically published values, call `sendMQTTData()` inside a `DUE()` check so you do not flood the broker.

#### Subscribe Side

The firmware subscribes to `{TopTopic}/set/{UniqueId}/#` on MQTT connect. Incoming messages are dispatched in `mqttCallback()` in `MQTTstuff.ino`. The dispatch table is `cmdtable[]`, an array of `MQTT_set_cmd_t` structs:

```cpp
static const MQTT_set_cmd_t cmdtable[] PROGMEM = {
  { PSTR("setpoint"),    PSTR("TT"), PSTR("temperature") },
  { PSTR("mycommand"),   PSTR("XX"), PSTR("raw")         },  // new entry
  // ...
};
```

Fields:
- `setcmd`: The MQTT topic suffix (what comes after `{UniqueId}/`).
- `otgwcmd`: The two-character OTGW PIC command code sent to `addCommandToQueue()`.
- `ottype`: Value type: `"temperature"`, `"on/off"`, `"level"`, `"raw"`, or `"int"`.

For commands that do not map to a PIC command (e.g., SAT-specific topics), add a separate handler in `mqttCallback()` before the table dispatch. SAT command topics are handled this way.

#### Home Assistant Auto-Discovery

HA discovery payloads are generated from `mqttha.cfg` (stored in LittleFS). This is a template file processed by `doAutoConfigure()` / `doAutoConfigureMsgid()`. If your new topic needs a HA entity, add an entry to `mqttha.cfg` following the existing template format. The file is too large to document fully here — examine existing entries for the pattern.

---

### Adding a New Setting

Adding a setting requires changes in four places: the struct definition, the serializer, the deserializer/validator, and (optionally) the web UI.

#### Step 1: Add to the Settings Struct

Open `OTGW-firmware.h`. Find the relevant sub-section struct (e.g., `MQTTSection`, `SATSection`, `DeviceSection`). Add the new field with its type and default value:

```cpp
struct MySection {
  bool   bMyNewFlag   = false;     // b prefix for bool
  char   sMyString[32] = "";       // s prefix for char array
  int    iMyInteger   = 0;         // i prefix for int
  float  fMyFloat     = 1.0f;      // f prefix for float
};
```

If adding to an existing section, add to the appropriate struct. If creating a new section, add it to `OTGWSettings`:

```cpp
struct OTGWSettings {
  // ... existing sections ...
  MySection my;   // access as settings.my.bMyNewFlag
};
```

#### Step 2: Add to writeSettings()

In `settingStuff.ino`, find `writeSettings()`. Add the serialization call in the appropriate section block:

```cpp
writeJsonBoolKV(f, PSTR("mynewflag"), settings.my.bMyNewFlag);
writeJsonStringKV(f, PSTR("mystring"), settings.my.sMyString);
writeJsonIntKV(f, PSTR("myinteger"), settings.my.iMyInteger);
writeJsonFloatKV(f, PSTR("myfloat"), settings.my.fMyFloat, 2);  // 2 decimal places
```

#### Step 3: Add to updateSetting()

In `settingStuff.ino`, find `updateSetting()`. Add a case for the new field. Use `strcasecmp_P` for case-insensitive matching:

```cpp
else if (strcasecmp_P(field, PSTR("mynewflag")) == 0) {
  settings.my.bMyNewFlag = EVALBOOLEAN(newValue);
  settingsDirty = true;
}
else if (strcasecmp_P(field, PSTR("mystring")) == 0) {
  strlcpy(settings.my.sMyString, newValue, sizeof(settings.my.sMyString));
  settingsDirty = true;
  // If this setting requires a service restart, set the side-effect bit:
  pendingSideEffects |= SIDE_EFFECT_MQTT;  // or SIDE_EFFECT_NTP, SIDE_EFFECT_MDNS
}
```

The `settingsDirty` flag triggers a deferred LittleFS write (typically after a 2-second debounce timer).

#### Step 4: Expose via REST API (Optional)

`GET /api/v2/settings` returns all settings. The response is built in `sendDeviceSettings()`. Add the new field to the JSON output there. For `POST /api/v2/settings`, the existing `postSettings()` function iterates the JSON body and calls `updateSetting()` for each key-value pair, so new settings are automatically accepted without code changes in the POST handler.

#### Step 5: Web UI Field (Optional)

Settings form fields live in `data/index.js`. Add an entry to the settings form configuration object following the existing pattern. For a boolean toggle:

```javascript
{ key: 'mynewflag', label: 'My New Feature', type: 'checkbox', section: 'device' }
```

For a text field:

```javascript
{ key: 'mystring', label: 'My String Value', type: 'text', section: 'device', maxlength: 31 }
```

---

### Coding Conventions

#### Naming

| Kind | Convention | Example |
|------|-----------|---------|
| Variables (local, member) | camelCase | `lastReset`, `msgCount` |
| Global settings fields | `setting` prefix + camelCase | `settingMqttBroker` (legacy), `settings.mqtt.sBroker` (current) |
| Constants and `#define` | UPPER_CASE | `CMSG_SIZE`, `ON`, `NTP_HOST_DEFAULT` |
| Functions | camelCase | `startMQTT()`, `processOT()`, `handleOTGW()` |
| Enums (class) | PascalCase type, UPPER_CASE values | `HeapHealthLevel::HEAP_HEALTHY` |
| Struct types | PascalCase + `Section` suffix | `MQTTSection`, `SATSection` |

#### Safety Rules

- Never write to `Serial` after the boot sequence. The UART is the PIC serial link. Use `DebugTln(F("..."))` instead.
- Never send OTGW commands directly to the serial port. Always use `addCommandToQueue(cmd, len)`.
- Always validate buffer sizes before string operations. Use `strlcpy()`, `snprintf_P()`, never `strcpy()` or `sprintf()`.
- Guard `#ifdef ESP8266` / `#ifdef ESP32` blocks where platform-specific hardware is involved, including debug macro sections that use platform-specific I/O.
- Use `enum class` for internal discriminators, not string tokens.

---

### Debug Tools

#### Telnet Debug Console (Port 23)

The firmware streams diagnostic output to Telnet port 23 via the SimpleTelnet library (`SimpleTelnet<1> debugTelnet`, declared extern in `OTGW-firmware.h` and defined in `networkStuff.ino`). SimpleTelnet replaces the older TelnetStream and ESPTelnet libraries and provides a unified API on both ESP8266 and ESP32. Connect with any Telnet client:

```bash
telnet otgw.local 23
# or by IP:
telnet 192.168.1.x 23
```

Output includes OpenTherm message decoding, MQTT state transitions, heap stats, sensor readings, and any text output from `DebugTln()`/`DebugTf()`. The stream is plain text, one line per event. Each line is prefixed with timestamp, free heap, and max free block size.

When you connect, a banner is printed listing all available debug toggle keys. Press `h` to redisplay the full menu. The current key bindings are:

| Key | Action |
|-----|--------|
| `1` | Toggle OT message parsing log (`state.debug.bOTmsg`) |
| `2` | Toggle REST API handler log (`state.debug.bRestAPI`) |
| `3` | Toggle MQTT module log (`state.debug.bMQTT`) |
| `4` | Toggle sensor module log (`state.debug.bSensors`) |
| `5` | Toggle SAT control loop log (`state.debug.bSAT`) |
| `6` | Toggle OTDirect frame log (`state.debug.bOTDirect`) |
| `g` | Toggle MQTT interval gating log (`state.debug.bMQTTGate`) |
| `d` | Toggle Dallas sensor simulation (`state.debug.bSensorSim`) |
| `s` / `S` | Toggle OTGW serial simulation replay |
| `r` | Reconnect WiFi, Telnet, OTGW stream, MQTT |
| `F` | Force MQTT HA discovery for all message IDs |
| `q` | Force re-read of settings from LittleFS |
| `p` | Reset PIC manually |
| `h` | Print the full debug help menu |

#### Debug Macros

```cpp
DebugTln(F("Message text"));                  // Print with newline, string in flash
DebugTf(PSTR("Value: %d\r\n"), myInt);        // Printf-style, format string in flash
DebugT(F("Partial: "));                       // Print without newline
```

The macros send output to the `debugTelnet` instance (a `SimpleTelnet<1>` on port 23). They are never written to `Serial`. Module-specific conditional macros follow a per-module flag in `state.debug`:

| Flag | Module macros | Description |
|------|--------------|-------------|
| `state.debug.bOTmsg` | `OTDebugTln()`, `OTDebugTf()` | OpenTherm message parsing |
| `state.debug.bRestAPI` | `RESTDebugTln()`, `RESTDebugTf()` | REST API handler logging |
| `state.debug.bMQTT` | `MQTTDebugTln()`, `MQTTDebugTf()` | MQTT client and publish/subscribe |
| `state.debug.bMQTTGate` | (inline checks) | MQTT interval gating decisions |
| `state.debug.bSensors` | `SensorDebugTln()`, `SensorDebugTf()` | Dallas, S0, sensor polling |
| `state.debug.bSAT` | `SATDebugTln()`, `SATDebugTf()` | SAT control loop, cycles, heating curve |
| `state.debug.bOTDirect` | `OTDDebugTln()`, `OTDDebugTf()` | OTDirect GPIO frame handling (ESP32) |

#### Browser Console Debug

When `window.otgwDebug = true` is set in the browser console before page load, the JavaScript SPA logs verbose diagnostics. The SAT subsystem has a separate flag: `window.SAT_DEBUG = true`.

#### evaluate.py

After making changes, always run:

```bash
python evaluate.py
```

This checks for the patterns most likely to cause crashes on ESP8266. Address all warnings before submitting a pull request.

---

### Timer Management

The firmware uses the `safeTimers.h` macros for all periodic tasks. These are overflow-safe and handle the 49-day `millis()` rollover correctly.

#### Declaring a Timer

```cpp
DECLARE_TIMER_SEC(timerMQTTpublish, 30, SKIP_MISSED_TICKS);   // fire every 30 seconds
DECLARE_TIMER_MS(timerLedBlink, 500, CATCH_UP_MISSED_TICKS);  // fire every 500 ms
DECLARE_TIMER_MIN(timerNTPsync, 30, SKIP_MISSED_TICKS);       // fire every 30 minutes
```

Timer type options:
- `SKIP_MISSED_TICKS`: Advance `due` once past the current tick. Use for most periodic tasks.
- `CATCH_UP_MISSED_TICKS`: Advance `due` one interval at a time until current. Use for tasks that must account for every missed cycle.
- `SKIP_MISSED_TICKS_WITH_SYNC`: O(1) sync then skip. For tasks that benefit from phase synchronization.

#### Checking and Firing

```cpp
if (DUE(timerMQTTpublish)) {
  // publish periodic MQTT data
}
```

`DUE()` returns `true` (the current `millis()` value) when the interval has elapsed, and `false` otherwise. It also updates the timer's `_due` field.

#### Modifying a Timer

```cpp
CHANGE_INTERVAL_SEC(timerMQTTpublish, 60, SKIP_MISSED_TICKS);  // change to 60 seconds
RESTART_TIMER(timerMQTTpublish);                                 // reset countdown from now
```

---

### Command Queue

All commands to the OTGW PIC must go through `addCommandToQueue()`. Direct UART writes are forbidden because the PIC requires response matching and command ordering.

```cpp
// Signature:
void addCommandToQueue(const char* cmd, int len,
                       const bool urgent = false,
                       const int16_t responseTimeout = 1000);

// Examples:
addCommandToQueue(PSTR("TT=21.50"), 8);            // temporary temperature setpoint
addCommandToQueue(PSTR("CS=55.00"), 8, true);      // urgent: control setpoint, high priority
addCommandToQueue(PSTR("GW=1"), 4);                // gateway mode on
```

The queue is processed in the OTGW-Core background task. Responses from the PIC are matched against outstanding commands and published to MQTT. On OTGW32 (OTDirect), the queue is handled internally by `loopOTDirect()`.

Never call `addCommandToQueue()` from an ISR or from within the PIC response handler.

---

### OpenTherm Message Processing Flow

Understanding this pipeline is essential before modifying any OT message handling:

```
Boiler/Thermostat
       |
  OpenTherm bus (2-wire)
       |
  PIC co-processor (ESP8266) or OTDirect GPIO ISR (ESP32)
       |
  UART 9600 baud ASCII line (e.g., "BA000001\r")   [ESP8266]
  or decoded OT frame struct                        [ESP32]
       |
  handleOTGW() / OTGW-Core.ino
    - reads serial line byte by byte into sRead[256] (static)
    - on CR: dispatches to processOT()
       |
  processOT(buf, len) / OTGW-Core.ino:3689
    - parses frame type prefix (T=thermostat, B=boiler, R=response, A=answer, E=error)
    - extracts 8-hex-digit data word
    - decodes message ID (bits 23-16)
    - decodes message type (bits 31-28)
    - extracts typed value via OTValueType (f8.8, s16, u16, flag8, status, datetime, ...)
    - updates OTcurrentSystemState (the global OTdataStruct)
    - checks mqttlastsent[msgId] throttle timestamp
    - calls sendMQTTData(label, value) if throttle allows
    - calls sendEventToWebSocket(frame) for every frame
    - calls publishToSourceTopic() when source-separated topics enabled
       |
  sendMQTTData(topic, value) / MQTTstuff.ino
    - checks canPublishMQTT()
    - prepends {TopTopic}/value/{UniqueId}/ namespace
    - calls PubSubClient.publish()
       |
  MQTT broker
       |
  Home Assistant / automation clients
```

The `sRead[256]` buffer in `handleOTGW()` is a `static` local, making it re-entrancy-safe. The function can be called from both the main loop and from within MQTT autoconfig's file-reading yield points.

---

### Architecture Decision Records

ADRs live in `docs/adr/`. There are 76+ ADRs covering every significant design decision. ADRs are immutable once Accepted.

#### When to Read ADRs

Before: modifying architecture, changing API contracts, adding dependencies, touching build tooling.

Not required for: pure bug fixes, minor features within existing patterns.

#### Key ADRs to Know

| ADR | Topic | Impact |
|-----|-------|--------|
| ADR-004 | Static buffer allocation | No `String` in hot paths; all buffers `char[]` |
| ADR-009 | PROGMEM string literals | All literals in flash via `F()` / `PSTR()` |
| ADR-014 | Dual build system | `build.py` for Arduino CLI, PlatformIO for ESP32 |
| ADR-016 | OpenTherm command queue | Always use `addCommandToQueue()` |
| ADR-019 | REST API versioning | `/api/v2/` is current; v0/v1 return 410 Gone |
| ADR-028 | File streaming | Files >2 KB must use `streamFile()`, never load |
| ADR-040 | MQTT source-specific topics | Source separation topic structure |
| ADR-042 | Streaming JSON | No ArduinoJson; manual `snprintf_P` construction |
| ADR-050 | Centralized API dispatch | Single `kV2Routes[]` table; no per-route router changes |
| ADR-051 | Dual encapsulating structs | `OTGWSettings` / `OTGWState` architecture |
| ADR-053 | Large feature buffer allocation | Static allocation for large per-feature buffers |
| ADR-061 | ESP8266/ESP32 platform abstraction | Use `platform.h` abstractions, not direct SDK calls |

#### Creating a New ADR

Create a new ADR when your change: introduces a new dependency, modifies an API contract, changes the build system, or shifts architectural boundaries. Use the format:

```
# ADR-NNN-Short-Title
## Status: Proposed
## Context
## Decision
## Consequences
## Related
```

To reverse an Accepted ADR: create a new ADR that supersedes it and mark the old one "Superseded by ADR-NNN".

---

### Contributing Workflow

1. Fork the repository on GitHub.
2. Create a feature branch from `dev`: `git checkout -b feature/my-feature dev`.
3. Search for an existing backlog task: `backlog search "my topic" --plain`. Create one if absent.
4. Set the task in progress and assign to yourself: `backlog task edit <id> -s "In Progress" -a @yourname`.
5. Write an implementation plan in the task before writing code.
6. Implement the change. Mark acceptance criteria done as you go.
7. Run `python evaluate.py` and fix all reported issues.
8. Build for both targets and confirm no compile errors: `pio run`.
9. Test on real hardware if possible; use the simulation mode in `state.debug.bOTGWSimulation` if hardware is unavailable.
10. Write a final summary in the backlog task.
11. Open a pull request against the `dev` branch, not `main`.
12. Include the backlog task ID in the PR description.

Pull requests to `main` are not accepted directly. All merges to `main` go through `dev` and are coordinated by the maintainer.
