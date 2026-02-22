# OTGW Firmware v1.2.0-beta — Feature & Change Reference
### Branch: `dev-branch-v1.2.0-beta-adr040-clean`

---

## Table of Contents

1. [ADR-040 — Source-Specific MQTT Topics](#1-adr-040--source-specific-mqtt-topics)
2. [Shared Scratch Buffer System](#2-shared-scratch-buffer-system)
3. [OpenTherm v4.2 Protocol Compliance](#3-opentherm-v42-protocol-compliance)
4. [OOB Protection for Unknown OpenTherm Message IDs](#4-oob-protection-for-unknown-opentherm-message-ids)
5. [WebSocket Hardening](#5-websocket-hardening)
6. [REST API Improvements](#6-rest-api-improvements)
7. [Settings System Improvements](#7-settings-system-improvements)
8. [MQTT Publish Reliability Improvements](#8-mqtt-publish-reliability-improvements)
9. [GPIO Conflict Detection Refactoring](#9-gpio-conflict-detection-refactoring)
10. [Bug Fixes & Typo Corrections](#10-bug-fixes--typo-corrections)
11. [Memory Footprint Changes](#11-memory-footprint-changes)

---

## 1. ADR-040 — Source-Specific MQTT Topics

### Background and Problem Statement

The OpenTherm protocol is a master/slave bus protocol between a thermostat (master) and a boiler (slave). Because the OTGW sits in the middle, it sees every message that travels on the bus in both directions, as well as any injected override messages that the gateway itself generates. Before this release, all OpenTherm values were published to a single, undifferentiated MQTT topic. For example, the setpoint temperature `TrSet` was published to:

```
OTGW/value/<node_id>/TrSet
```

However, the thermostat (the master) may write a value to `TrSet`, and the boiler (the slave) may acknowledge a different effective setpoint. The OTGW itself may also inject an override value. All three values collapsed onto the same topic, making it impossible for a Home Assistant automation or dashboard to distinguish which device was the source of the current value.

ADR-040 solves this by publishing each writable or read-write OpenTherm value to a second, source-annotated MQTT topic in addition to the existing topic. The existing topic is always preserved for full backward compatibility.

### New Setting: `MQTTSeparateSources`

A new firmware setting, `settingMQTTSeparateSources` (default: `true`), controls whether the source-specific topics are published. It is persisted in the device's JSON settings file under the key `"MQTTSeparateSources"` and can be toggled from the web UI settings page (displayed as **"MQTT Separate Sources (thermostat/boiler/gateway)"**). A hint text is shown below the toggle in the UI explaining what it does.

When this setting is `false`, behaviour is identical to v1.1.0-beta: only the original topic is published, and no source-specific topic is published.

### Source Classification

Each incoming OpenTherm message is tagged with a response type (`rsptype`) by the OTGW PIC firmware. The four relevant types are:

| rsptype constant         | Bus direction | Source label | Topic suffix  | Internal index |
|--------------------------|---------------|--------------|---------------|----------------|
| `OTGW_THERMOSTAT` (`T`)  | Master → Slave request | Thermostat   | `_thermostat` | 0 |
| `OTGW_BOILER` (`B`)      | Slave → Master response | Boiler      | `_boiler`     | 1 |
| `OTGW_REQUEST_BOILER` (`R`) | Gateway → Boiler override | Gateway  | `_gateway`    | 2 |
| `OTGW_ANSWER_THERMOSTAT` (`A`) | Gateway → Thermostat override | Gateway | `_gateway` | 2 |

Both `R` and `A` messages are injected by the gateway itself and are therefore grouped under a single `_gateway` index. Parity errors and undefined message types are silently ignored — no source topic is published for them.

The resolution logic lives in the new helper `resolveSourcePublishInfo()` in [MQTTstuff.ino](../src/OTGW-firmware/MQTTstuff.ino).

### Which Messages Get Source Topics

Source-specific topics are only published for messages whose OpenTherm data direction is `OT_WRITE` or `OT_RW` (read-write). Read-only messages (`OT_READ`) carry values that originate exclusively from the slave and for which direction is unambiguous, so source annotation is not needed. This filter is applied inside the `publishOTValueTopic()` wrapper in [OTGW-Core.ino](../src/OTGW-firmware/OTGW-Core.ino).

### Topic Naming

For a base topic such as `TrSet`, the source-specific topics become:

```
OTGW/value/<node_id>/TrSet_thermostat   ← value as sent by the thermostat
OTGW/value/<node_id>/TrSet_boiler       ← value as acknowledged by the boiler
OTGW/value/<node_id>/TrSet_gateway      ← gateway override value, if any
```

The original `OTGW/value/<node_id>/TrSet` topic continues to be published unchanged. Applications that depend on the original topic are completely unaffected.

### Home Assistant Auto-Discovery for Source Topics

The firmware already includes a general-purpose Home Assistant MQTT discovery mechanism that sends discovery payloads for all sensors listed in `mqttha.cfg`. The source-specific topics are dynamic — they only appear when the corresponding device source actually sends a message — and they are not listed in `mqttha.cfg`. Instead, the firmware sends discovery payloads on demand the first time a source-specific topic is published.

The discovery payload follows the standard Home Assistant sensor schema and includes:

- **`name`**: The base topic name suffixed with the source in parentheses, for example: `"TrSet (Thermostat)"`.
- **`state_topic`**: The full MQTT path to the source-specific topic.
- **`unique_id`**: The device unique ID concatenated with the source topic path, guaranteeing entity uniqueness even if the device is renamed.
- **`device`**: Standard OTGW device block referencing the same device identifiers as all other OTGW entities, so all source-specific entities appear grouped under the same device in Home Assistant.

Discovery payloads are sent **retained** so that Home Assistant learns about the sensors even if it restarts after the OTGW publishes them.

### Bloom Filter Deduplication: `sourceConfigFilter`

Sending a discovery payload every time a source publishes a value would flood the MQTT broker. To avoid this, the firmware maintains a per-source **Bloom filter** — a compact, probabilistic membership data structure — that tracks which sensors have already had discovery published.

The implementation uses:
- A 2048-bit filter array per source (3 sources × 2048 bits = 768 bytes of BSS). This is a deliberate trade-off: 768 bytes of RAM to avoid any heap allocation.
- 5 hash probes per query, using **FNV-1a 32-bit** for the primary hash and **double hashing** (via `mixHash32()`) for probe stepping. Double hashing with an odd step avoids clustering and maximises bit distribution across the filter.
- A false-positive rate of approximately 1.7% at 500 entries per source, meaning a very small fraction of sensors may occasionally skip a discovery publish until the next cache reset. This is considered acceptable since discovery retries naturally happen on MQTT reconnect.

The filter is reset on every MQTT reconnect by calling `clearSourceConfigDone()` from `startMQTT()`. This ensures that any Home Assistant entities that may have been lost during a broker restart are re-discovered.

The four functions implementing this mechanism are:

- `hashSourceSensorId(sensorId)` — Computes the FNV-1a primary hash over the sensor ID string.
- `mixHash32(x)` — Applies a fast 32-bit finaliser (Murmur-inspired) to produce the probe step.
- `testFilterBit(sourceIdx, bitIndex)` — Tests one bit in the filter for the given source.
- `setFilterBit(sourceIdx, bitIndex)` — Sets one bit in the filter for the given source.
- `getSourceConfigDone(sourceIdx, sensorId)` — Returns `true` if all 5 probe bits are set (i.e., discovery has already been sent).
- `setSourceConfigDone(sourceIdx, sensorId)` — Sets all 5 probe bits to mark the sensor as discovered.

### `publishToSourceTopic()` — The Core Publishing Function

The new public function `publishToSourceTopic(baseTopic, value, rsptype)` is the entry point for source-specific publishing. Its call flow is:

1. If `settingMQTTSeparateSources` is `false`, return immediately.
2. Call `resolveSourcePublishInfo()` to map `rsptype` to a suffix, human-readable source name, and source index. If the source type is unknown (e.g. parity error), return.
3. Build the source-specific topic string: `<baseTopic><suffix>` using `snprintf_P` with a `static char sourceTopic[200]` buffer. `static` is safe here because `feedWatchDog()` does not yield and therefore this function cannot be re-entered.
4. Call `sendMQTTData()` (non-retained) to publish the value to the source-specific topic.
5. Consult the Bloom filter via `getSourceConfigDone()`. If this sensor is already discovered, return.
6. Attempt to acquire the shared MQTT scratch buffer. If it is held by a concurrent `doAutoConfigure()` or `doAutoConfigureMsgid()` call (which can happen because `doBackgroundTasks()` is re-entrant), defer the discovery by returning without marking the sensor as discovered. The discovery will be retried on the next message.
7. Build the discovery topic and discovery JSON payload into the scratch buffers using `snprintf_P`.
8. Call `sendMQTTStreaming()` to publish the discovery payload retained. If the publish fails (e.g. due to low heap), release the scratch buffer without marking the sensor as discovered, so it retries.
9. Call `setSourceConfigDone()` to mark the sensor as discovered in the Bloom filter.
10. Release the scratch buffer.

### `publishOTValueTopic()` — Unified Publishing Wrapper

All OpenTherm value publishing functions in [OTGW-Core.ino](../src/OTGW-firmware/OTGW-Core.ino) (`print_f88`, `print_s16`, `print_u16`, `print_s8s8`, `print_u8u8`, `print_date`, `print_daytime`, `print_command`, etc.) previously called `sendMQTTData()` directly. They now call the new `publishOTValueTopic()` instead:

```cpp
static inline void publishOTValueTopic(const char *topic, const char *value) {
  sendMQTTData(topic, value);
  if (OTlookupitem.msgcmd == OT_WRITE || OTlookupitem.msgcmd == OT_RW) {
    publishToSourceTopic(topic, value, OTdata.rsptype);
  }
}
```

This wrapper ensures that:
- The original (backward-compatible) topic is always published first.
- Source-specific publishing is only triggered for writable/read-write data IDs where direction is meaningful.
- Source-specific publishing is skipped entirely for read-only data IDs.

The inline qualifier keeps this as a zero-overhead abstraction: the compiler folds it directly into each call site.

---

## 2. Shared Scratch Buffer System

### Problem Statement

Both `doAutoConfigure()` and `doAutoConfigureMsgid()` need large temporary buffers while parsing and publishing MQTT auto-discovery payloads from `mqttha.cfg`. In previous versions, these were declared as `static` local variables inside each function individually, or as a single large `static` block-allocated array partitioned manually by pointer arithmetic. This approach duplicated the memory: both functions held their own permanent 1200-byte message buffer and 200-byte topic buffer even when idle.

The new approach provides a **single** shared scratch buffer allocated once in BSS (i.e. never on the stack and never on the heap) that is borrowed exclusively by whichever function is currently running.

### `MQTTAutoConfigScratch` Struct and `mqttAutoCfgScratch`

```cpp
struct MQTTAutoConfigScratch {
  bool inUse;
  char sMsg[1200];    // MQTT_MSG_MAX_LEN
  char sTopic[200];   // MQTT_TOPIC_MAX_LEN
};
static MQTTAutoConfigScratch mqttAutoCfgScratch = { false, {0}, {0} };
```

The struct is placed in BSS at file scope, occupying exactly 1401 bytes (plus alignment) once, permanently. The `inUse` flag is a concurrency guard: because `doBackgroundTasks()` is called from inside the file-parsing loops in both `doAutoConfigure()` and `doAutoConfigureMsgid()`, the `processOT()` / `handleOTGW()` path can run and potentially call `publishToSourceTopic()` while file parsing is in progress. The guard prevents `publishToSourceTopic()` from corrupting the buffers while they are in use.

### `acquireMQTTAutoConfigScratch()` and `releaseMQTTAutoConfigScratch()`

```cpp
static bool acquireMQTTAutoConfigScratch(char *&sMsg, char *&sTopic);
static void releaseMQTTAutoConfigScratch();
```

`acquireMQTTAutoConfigScratch()` attempts to lock the scratch buffer. If it is already held (`inUse == true`), it returns `false` and the caller defers its operation. If it is free, it marks `inUse = true`, assigns the caller's pointer references to the internal buffers, and returns `true`.

`releaseMQTTAutoConfigScratch()` simply clears `inUse`.

Callers must always call `releaseMQTTAutoConfigScratch()` on all code paths, including error paths. All early-return paths in `doAutoConfigure()` and `doAutoConfigureMsgid()` were updated to call the release function before returning.

### Dual-Use of `sMsg` as a Line-Read Buffer

An important implementation detail: `doAutoConfigure()` reads each line of `mqttha.cfg` directly into `sMsg` using `fh.readBytesUntil('\n', sMsg, MQTT_MSG_MAX_LEN - 1)`. The same `sMsg` buffer is then passed to `splitLine()` which overwrites it in-place with the parsed value field. This is intentional and correct — the raw line content is only needed long enough to parse it, after which the buffer is repurposed for the final payload. This saves a separate line buffer.

---

## 3. OpenTherm v4.2 Protocol Compliance

The OpenTherm protocol specification was updated from v4.0 to v4.2. This release brings the firmware's message ID table into compliance with the v4.2 specification. The changes affect the enum `OpenThermMessageID`, the lookup table `OTmap[]`, and the `OTcurrentSystemState_t` struct.

### New Message IDs

The following data IDs are newly decoded and published to MQTT:

| ID | Name | Type | Direction | Unit | Description |
|----|------|------|-----------|------|-------------|
| 39 | `TrOverride2` | f8.8 | READ | °C | Remote override room setpoint 2 (second zone override) |
| 93 | `Brand` | u8/u8 | READ | — | Boiler brand name encoded as index/character pairs (mandatory since v4.1) |
| 94 | `BrandVersion` | u8/u8 | READ | — | Boiler brand firmware version |
| 95 | `BrandSerialNumber` | u8/u8 | READ | — | Boiler brand serial number |
| 96 | `CoolingOperationHours` | u16 | RW | hrs | Total cooling operation hours |
| 97 | `PowerCycles` | u16 | RW | — | Total number of boiler power cycles |

**ID 39** was previously `OT_UNDEF` (undefined/reserved). The OpenTherm v4.2 specification assigns it as a second remote override setpoint, useful in installations with two heating zones controlled by separate thermostats.

**IDs 93–95** (Brand, BrandVersion, BrandSerialNumber) are part of the mandatory brand identification block introduced in OpenTherm v4.1. Before this release, these IDs were decoded as undefined. The data format follows the same index/character encoding used for other string data in the protocol.

**IDs 96–97** (CoolingOperationHours, PowerCycles) are new operational counters in v4.2 intended for predictive maintenance and service logging.

### Corrected Access Modes

Several data IDs had incorrect access mode annotations in the lookup table. These have been corrected to match the v4.2 specification:

| ID | Name | Old mode | New mode | Reason |
|----|------|----------|----------|--------|
| 27 | `Toutside` | READ | **RW** | Masters can write outdoor temperature for comfort correction |
| 37 | `TRoomCH2` | READ | **WRITE** | Thermostat writes room temp for second circuit |
| 38 | `RelativeHumidity` | READ | **RW** | Humidity can be written (e.g. from external sensor) |
| 98 | `RFstrengthbatterylevel` | READ | **WRITE** | RF receiver updates signal/battery |
| 99 | `OperatingMode_HC1_HC2_DHW` | READ | **RW** | Mode can be set or read |
| 109 | `ElectricityProducerStarts` | READ | **RW** | Counter can be reset |
| 110 | `ElectricityProducerHours` | READ | **RW** | Counter can be reset |
| 112 | `CumulativElectricityProduction` | READ | **RW** | Counter can be reset |
| 124 | `OpenThermVersionMaster` | READ | **WRITE** | Master writes its OT protocol version |
| 126 | `MasterVersion` | READ | **WRITE** | Master writes its product version |

These corrections matter for source-specific topic publishing: only `OT_WRITE` and `OT_RW` data IDs trigger `publishToSourceTopic()`. Previously incorrect `OT_READ` modes would have silently suppressed source-specific publishing for these data IDs.

### Corrected Data Types

| ID | Name | Old type | New type | Reason |
|----|------|----------|----------|--------|
| 35 | `FanSpeed` | `ot_u16` (16-bit unsigned) | `ot_u8u8` (two 8-bit values) | v4.2 specifies fan speed as setpoint (HB) / actual (LB), both in Hz |

The unit annotation for `FanSpeed` was also corrected from `"rpm"` to `"Hz"` to match the v4.2 specification. This is a **breaking change** for any integration that parses the raw MQTT topic for `FanSpeed`: the value will now arrive as two separate sub-topics (`FanSpeed_hb_u8` and `FanSpeed_lb_u8`) in the same format as other `u8u8` encoded data IDs, instead of a single integer.

### DHW Flow Rate Unit Correction

The `DHWFlowRate` (ID 19) unit annotation was corrected from `"l/m"` to `"l/min"` for clarity. This has no functional impact on MQTT values, only on display strings.

### Fixed `OT_RemehadFdUcodes` Enum Anchor

The `OT_RemehadFdUcodes` enum member was previously defined without an explicit value, relying on sequential numbering from its predecessor. Because new IDs were inserted in the range 93–97, the sequential offset would have shifted `OT_RemehadFdUcodes` from its correct value of 131 to 136. The enum now explicitly anchors it: `OT_RemehadFdUcodes = 131`.

### New `getOTGWValue()` Cases

The `getOTGWValue()` function, which converts the current in-memory system state to a string value for REST API and WebSocket use, was extended with `case` entries for all newly decoded data IDs: `TrOverride2`, `Brand`, `BrandVersion`, `BrandSerialNumber`, `CoolingOperationHours`, `PowerCycles`, `BurnerUnsuccessfulStarts`, and `FlameSignalTooLow`. The last two were previously decoded by `processOT()` but missing from `getOTGWValue()`, making them invisible to the REST API.

---

## 4. OOB Protection for Unknown OpenTherm Message IDs

### Problem

The OpenTherm standard allows 256 message IDs (0–255) but the firmware's lookup table `OTmap[]` only covers IDs 0 through `OT_MSGID_MAX` (133). If the OTGW PIC ever forwarded a message with an ID above 133 — which can happen with vendor-specific extensions — the previous code would execute `PROGMEM_readAnything(&OTmap[OTdata.id], OTlookupitem)` with an out-of-bounds index, reading uninitialised flash memory and producing garbage values that could corrupt the MQTT topic or result in a crash.

### Fix

A bounds check is now performed before every lookup table access in `processOT()`:

```cpp
if (OTdata.id <= OT_MSGID_MAX) {
    PROGMEM_readAnything(&OTmap[OTdata.id], OTlookupitem);
} else {
    OTlookupitem.id = OTdata.id;
    OTlookupitem.msgcmd = OT_UNDEF;
    OTlookupitem.type = ot_undef;
    OTlookupitem.label = "Unknown";
    OTlookupitem.friendlyname = "Unknown";
    OTlookupitem.unit = "";
}
```

When the ID is out of range, a safe default `OTlookupitem` is filled in. The `OT_UNDEF` message command and `ot_undef` data type ensure that the message is logged but not published to MQTT, and that `publishOTValueTopic()` and `publishToSourceTopic()` are not invoked. The `msglastupdated[]` timestamp array is still updated (it is dimensioned for all 256 IDs), so message timing is tracked even for vendor-specific extensions.

The same bounds check was also added to `sendOTGWvalue()` in the REST API, where a caller supplying a message ID greater than 133 would previously access out-of-bounds PROGMEM. The API now returns `{"error":"message id: out of range"}` for such requests.

---

## 5. WebSocket Hardening

### `broadcastWebSocketCapped()`

All WebSocket broadcast calls previously called `webSocket.broadcastTXT()` directly with no size check. This is unsafe because the WebSocket library writes into a fixed internal buffer, and payloads larger than that buffer are truncated silently or cause an assertion failure on some builds.

The new `broadcastWebSocketCapped()` function centralises all broadcast calls:

```cpp
static size_t broadcastWebSocketCapped(const char* payload, size_t payloadLen = 0);
```

It enforces a hard cap of `WEBSOCKET_PAYLOAD_CAP` bytes (equal to `WEBSOCKETS_MAX_DATA_SIZE`, currently 512 bytes) on every outgoing payload. If the payload exceeds this cap, only the first 512 bytes are transmitted, a warning is logged to the serial port, and the actual transmitted length is returned. All three broadcast sites have been updated to call this function:

- `sendWebSocketJSON()` — used for firmware upgrade progress notifications.
- `handleWebSocket()` — used for periodic keepalive messages.
- `sendLogToWebSocket()` — used for streaming the OT log to the browser.

Additionally, the `WStype_TEXT` handler (handling messages **received** from browser clients) now disconnects any client that sends a message larger than `WEBSOCKET_PAYLOAD_CAP`. This prevents a malicious or misbehaving client from consuming disproportionate memory in the library's receive buffer.

### `WEBSOCKETS_MAX_DATA_SIZE` Increased to 512

The previous version set `WEBSOCKETS_MAX_DATA_SIZE` to 256. Diagnostic log lines from the OTGW can exceed 256 bytes when displaying detailed OpenTherm frame decoding, causing log lines to be truncated in the browser's live log view. The limit has been restored to 512 bytes, which is the library default.

To compensate for the increased per-connection overhead, the maximum number of simultaneous WebSocket connections is now enforced at the library level as well as the application level, by setting `WEBSOCKETS_SERVER_CLIENT_MAX` to 3 in [networkStuff.h](../src/OTGW-firmware/networkStuff.h). This was previously only a soft limit enforced by application code via `MAX_WEBSOCKET_CLIENTS`, which the library itself did not respect. The constant `MAX_WEBSOCKET_CLIENTS` now derives from `WEBSOCKETS_SERVER_CLIENT_MAX` to keep the two in sync.

---

## 6. REST API Improvements

### `parsePostSettingsPayload()` — Extracted Settings Parse Helper

The `postSettings()` handler previously contained inline JSON parsing logic mixed with validation, error response generation, and the actual setting update call. This was refactored: the JSON parsing and field extraction is now performed by a standalone static helper:

```cpp
static bool parsePostSettingsPayload(const String& body,
                                     char *field,  size_t fieldSize,
                                     char *newValue, size_t newValueSize,
                                     const __FlashStringHelper* &errorMsg);
```

The function uses a `StaticJsonDocument<256>` (stack-allocated, zero heap) to parse the incoming JSON body. It handles three value types:
- **String** values are copied directly.
- **Boolean** values are converted to the strings `"true"` or `"false"`.
- **Numeric and other** values are serialised back to a string via `serializeJson()`.

On any parse or validation failure, `errorMsg` is set to a flash-stored error string and the function returns `false`. The caller (`postSettings()`) is then responsible only for dispatching the error response.

### Deferred Settings Save (Queued Response)

In the previous version, `postSettings()` called `flushSettings()` synchronously before sending the HTTP 200 response. This meant the HTTP handler was blocked while the firmware wrote the full settings JSON to LittleFS flash — a multi-millisecond blocking operation that could cause the OTGW serial receive buffer to overflow (the PIC sends data continuously).

In this release, `postSettings()` no longer calls `flushSettings()` directly. Instead, `updateSetting()` marks a `pendingSideEffects` dirty flag (which already existed for deferred MQTT reconnect and NTP restart), and the existing periodic settings flush in the main loop writes to flash asynchronously. The HTTP response now returns:

```json
{"status":"queued","name":"<field>","pending":true}
```

The browser-side JavaScript (`sendPostSetting()` in `index.js`) detects the `"status":"queued"` field and displays **"Saving changes... QUEUED"** instead of **"SUCCESS"**, so the user knows the change is accepted but not yet persisted. The `sendPostSetting()` function now also returns a `Promise<boolean>` (previously it was `void`), enabling callers to await the result.

### `sendOTGWvalue()` Bounds Check Fix

The REST API endpoint `GET /api/v2/otgw/value/<id>` previously checked the message ID range in the wrong order: it first read from `OTmap[msgid]` and then checked if `msgid > OT_MSGID_MAX`. This meant IDs above 133 would perform an out-of-bounds PROGMEM read before the bounds check could prevent it. The check order has been corrected: bounds are now verified first, and an appropriate error is returned without touching `OTmap`.

### `sendDeviceSettings()` Max-Length Fix

The `sendDeviceSettings()` function serialises the device settings to JSON for the browser. Three MQTT setting fields were previously passing hardcoded maximum lengths to `sendJsonSettingObj()`:

- `mqtttoptopic`: was `15`, now `sizeof(settingMQTTtopTopic) - 1` = 12
- `mqtthaprefix`: was `20`, now `sizeof(settingMQTThaprefix) - 1` = 16
- `mqttuniqueid`: was `20`, now `sizeof(settingMQTTuniqueid) - 1` = 25

Using `sizeof(field) - 1` ties the advertised maximum directly to the actual buffer size, preventing the web UI from presenting a maximum that exceeds what the firmware can store. A new setting entry, `mqttseparatesources`, is also included in the settings payload.

---

## 7. Settings System Improvements

### New MQTT Buffer Size Constants

Three new preprocessor constants were added to [OTGW-firmware.h](../src/OTGW-firmware/OTGW-firmware.h) to document the intended maximum character count of the MQTT identifier settings:

```c
#define MQTT_TOP_TOPIC_MAX_CHARS  12   // settingMQTTtopTopic
#define MQTT_HA_PREFIX_MAX_CHARS  16   // settingMQTThaprefix
#define MQTT_UNIQUE_ID_MAX_CHARS  25   // settingMQTTuniqueid
```

The corresponding `char` array declarations were updated to use these constants:
```c
char settingMQTTtopTopic[MQTT_TOP_TOPIC_MAX_CHARS + 1];
char settingMQTThaprefix[MQTT_HA_PREFIX_MAX_CHARS + 1];
char settingMQTTuniqueid[MQTT_UNIQUE_ID_MAX_CHARS + 1];
```

Previously all three were declared as `char ...[41]` regardless of their actual maximum content, wasting 60+ bytes of global RAM. The new sizes are tightly fitted: the default top topic "OTGW" is 4 characters, the default HA prefix "homeassistant" is 13 characters, and the default unique ID "otgw-" + 12 hex digits is 17 characters. Headroom is provided for user customisation.

### `MQTTSeparateSources` Persistence

The new `settingMQTTSeparateSources` boolean is fully round-tripped through the settings system:
- **Written** to the settings JSON by `writeSettings()`.
- **Read** from the settings JSON by `readSettings()`, with the current default as fallback.
- **Updated** from the web API by the `MQTTSeparateSources` case in `updateSetting()`.

Any MQTT-related setting change (including this one) triggers the `SIDE_EFFECT_MQTT` pending flag, which causes a deferred MQTT reconnect. This ensures that when source-specific topics are enabled or disabled, the MQTT client reconnects and clears the discovery cache.

### Timezone Processor Cache Reduction

The `ExtendedZoneProcessorCache` size was reduced from 3 to 2. This cache stores parsed timezone structures for the AceTime library. The firmware only ever needs one active timezone at a time, and a cache of 2 provides sufficient capacity for one active zone plus one recently used zone without the overhead of a third slot. Each `ExtendedZoneProcessor` object is ~240 bytes on ESP8266, so this saves approximately 240 bytes of RAM.

---

## 8. MQTT Publish Reliability Improvements

### `sendMQTTStreaming()` Now Returns `bool`

The `sendMQTTStreaming()` function previously had `void` return type. All failure cases inside it (broker not connected, invalid IP, low heap, `beginPublish` failure, `write` failure, `endPublish` failure) would return silently. Callers had no way to know whether the publish actually succeeded.

In this release, `sendMQTTStreaming()` returns `bool`:
- Returns `false` on every failure path.
- Returns `true` only after a successful `endPublish()`.

All callers have been updated:
- `doAutoConfigure()` now only calls `setMQTTConfigDone(lineID)` if the publish succeeds. Previously, the config-done bit was set unconditionally, meaning a publish failure would permanently suppress retries for that message ID.
- `doAutoConfigureMsgid()` similarly only sets `_result = true` on successful publish.
- `publishToSourceTopic()` uses the return value to decide whether to mark the sensor as discovered.

### `setMQTTConfigDone()` Simplified

The return value of `setMQTTConfigDone()` was `bool` in the previous version, but the return value was `bitSet(...)` which in the Arduino macro evaluates to the modified integer value, not a meaningful success/failure indication. No callers used the return value. The function signature has been simplified to `void`. The internal implementation is also simplified: the previous code contained an `if (bitSet(...) > 0)` branch that was logically equivalent to always returning `true` (since `bitSet` always succeeds), replaced by a direct `bitSet()` call.

### `doAutoConfigure()` Logic Fix

The condition for deciding whether to re-send an auto-configuration entry was **inverted** in the previous version. The condition was:

```cpp
if (bForceAll || getMQTTConfigDone(lineID) || ...) {
```

This means the entry was re-sent if it had already been configured, and skipped if it had not yet been configured — exactly backwards. The corrected condition is:

```cpp
if (bForceAll || !getMQTTConfigDone(lineID)) {
```

This ensures entries are sent on first boot (when `getMQTTConfigDone` returns `false`) or when a full resend is forced, and are skipped once successfully configured.

The same inversion existed for the Dallas sensor trigger at the end of `doAutoConfigure()`, which has been corrected in the same way.

### Flash Payload Truncation Warning

The `sendMQTTData(const __FlashStringHelper*, const __FlashStringHelper*)` overload copies the flash string into a static character buffer before passing it to the main string overload. The internal buffer was previously 1200 bytes (`MQTT_MSG_MAX_LEN`). Since this overload is only used for short status strings and error literals stored in flash, the buffer has been reduced to 256 bytes to save 944 bytes of persistent static RAM. A truncation warning is now logged if the flash string is longer than 255 characters.

---

## 9. GPIO Conflict Detection Refactoring

### Problem

The `checkGPIOConflict()` function compared the caller identity using `strcasecmp_P()` against flash-stored string literals (`PSTR("sensor")`, `PSTR("s0")`, `PSTR("output")`). This pattern has two issues: it is fragile (a typo in the string silently breaks conflict detection) and it wastes PROGMEM for what is semantically an enumerated type.

Additionally, the old implementation checked for a GPIO conflict even when the feature holding the conflicting pin was **disabled**. For example, if the S0 counter was assigned to pin 5 but `settingS0COUNTERenabled` was `false`, a conflict would still be reported when the output feature tried to use pin 5.

### `GPIOFeatureOwner` Enum

A new enum `GPIOFeatureOwner` is defined in [OTGW-firmware.h](../src/OTGW-firmware/OTGW-firmware.h):

```cpp
enum GPIOFeatureOwner {
  GPIO_FEATURE_SENSOR = 0,
  GPIO_FEATURE_S0,
  GPIO_FEATURE_OUTPUT
};
```

The enum is placed in the global header so that all `.ino` translation units can use it, including `settingStuff.ino` which defines the functions and `OTGW-firmware.ino` which calls them.

### `isGPIOFeatureEnabled()`

The new helper `isGPIOFeatureEnabled(GPIOFeatureOwner feature)` maps the enum value to the corresponding global settings boolean:

```cpp
static bool isGPIOFeatureEnabled(GPIOFeatureOwner feature) {
  switch (feature) {
    case GPIO_FEATURE_SENSOR: return settingGPIOSENSORSenabled;
    case GPIO_FEATURE_S0:     return settingS0COUNTERenabled;
    case GPIO_FEATURE_OUTPUT: return settingGPIOOUTPUTSenabled;
    default:                  return false;
  }
}
```

### Updated `checkGPIOConflict()`

The function signature changes from `bool checkGPIOConflict(int pin, PGM_P caller)` to `bool checkGPIOConflict(int pin, GPIOFeatureOwner caller)`. Each conflict check now additionally requires that the feature being tested is enabled:

```cpp
if (caller != GPIO_FEATURE_SENSOR && isGPIOFeatureEnabled(GPIO_FEATURE_SENSOR) && pin == settingGPIOSENSORSpin) {
    ...
}
```

This means that assigning two features to the same GPIO pin is only reported as a conflict when both features are enabled — which is the only situation where it would actually cause a hardware problem.

---

## 10. Bug Fixes & Typo Corrections

### `eletric_production` → `electric_production`

The MQTT topic name and all associated code for the electricity production status bit in the OpenTherm status byte was previously misspelled as `eletric_production`. This has been corrected to `electric_production` throughout:

- The topic string in `print_status()` in [OTGW-Core.ino](../src/OTGW-firmware/OTGW-Core.ino).
- The Home Assistant discovery entry in [mqttha.cfg](../src/OTGW-firmware/data/mqttha.cfg).
- A comment in `print_status()` that described the bit's meaning.

**Migration note**: Any Home Assistant automation or dashboard that references `OTGW/value/<node_id>/eletric_production` must be updated to use `electric_production`. The old topic will no longer be published.

### `solar_storage_slave_fault_incidator` → `solar_storage_slave_fault_indicator`

The MQTT topic name for the solar storage slave fault indicator was misspelled as `fault_incidator`. This has been corrected to `fault_indicator` in both the C code (`print_solar_storage_status()` in [OTGW-Core.ino](../src/OTGW-firmware/OTGW-Core.ino)) and the Home Assistant discovery entry in [mqttha.cfg](../src/OTGW-firmware/data/mqttha.cfg).

**Migration note**: Any integration referencing `solar_storage_slave_fault_incidator` must be updated.

### `Diagonostic_Indicator` → `Diagnostic_Indicator`

The friendly name for the diagnostic indicator sensor in [mqttha.cfg](../src/OTGW-firmware/data/mqttha.cfg) was misspelled as `Diagonostic_Indicator`. Corrected to `Diagnostic_Indicator`. This only affects the display name in Home Assistant, not the MQTT topic itself.

### `CMSG_SIZE` Comment Correction

A comment in `executeCommand()` in [OTGW-Core.ino](../src/OTGW-firmware/OTGW-Core.ino) stated that `CMSG_SIZE` was 256. The actual value in [OTGW-firmware.h](../src/OTGW-firmware/OTGW-firmware.h) is 512. The comment has been corrected. There is no functional change.

### `copyTrimmedField()` Replaces Inline Trim/Copy in `splitLine()`

The `splitLine()` function previously performed trimming by copying a substring with `strlcpy` and then calling `trimInPlace()` on the result. This approach used temporary intermediate copies and could silently lose data if a field was larger than the destination buffer without reporting the overflow. The new `copyTrimmedField()` helper trims in-place on the source range (using pointer arithmetic over `[begin, end)` bounds) and returns `false` if the trimmed content would overflow the destination buffer, allowing `splitLine()` to skip malformed lines cleanly.

---

## 11. Memory Footprint Changes

The following table summarises the significant RAM changes in this release:

| Change | RAM delta | Direction |
|--------|-----------|-----------|
| `mqttAutoCfgScratch` struct in BSS (replaces two sets of static locals across two functions) | ~0 | Neutral (same total size, now shared) |
| `sourceConfigFilter` Bloom filter (3 × 256 bytes) | +768 bytes | Increase |
| `settingMQTTtopTopic` resized from 41 to 13 bytes | −28 bytes | Decrease |
| `settingMQTThaprefix` resized from 41 to 17 bytes | −24 bytes | Decrease |
| `settingMQTTuniqueid` resized from 41 to 26 bytes | −15 bytes | Decrease |
| `sendMQTTData(F(),F())` payloadBuf reduced from 1200 to 256 bytes | −944 bytes | Decrease |
| `ExtendedZoneProcessorCache` reduced from 3 to 2 slots (~240 bytes each) | −240 bytes | Decrease |
| `WEBSOCKETS_MAX_DATA_SIZE` increased from 256 to 512 bytes (per-client buffer) | +768 bytes (3 clients × 256) | Increase |
| `settingMQTTSeparateSources` new bool | +1 byte | Negligible |
| **Net change (approximate)** | **+286 bytes** | |

The net RAM increase is modest (~286 bytes) and is dominated by the Bloom filter. The WebSocket buffer increase and the Bloom filter are offset by the savings from right-sizing the MQTT setting buffers and the flash payload buffer. The 4 KB CONT stack is not affected: all new buffers are placed in BSS (global/static) or are borrowed from BSS (scratch).
