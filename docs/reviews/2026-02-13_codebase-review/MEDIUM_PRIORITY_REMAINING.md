---
# METADATA
Document Title: Medium Priority Issues - Remaining Work
Review Date: 2026-02-15
Branch: claude/review-codebase-w3Q6N
Source Review: CODEBASE_REVIEW_UPDATED_DEV.md + FINDINGS_ANALYSIS.md
Status: COMPLETE - All 7 findings resolved
---

# Medium Priority Issues - Remaining Work

All 12 critical/high priority findings from the codebase review have been resolved.
All 7 medium priority issues have been resolved as documented below.

---

## Finding #23: ~~Settings written to flash on every individual field update~~ FIXED

**File:** [settingStuff.ino](../../../src/OTGW-firmware/settingStuff.ino) — `updateSetting()` / `flushSettings()`  
**Priority:** MEDIUM  
**Status:** FIXED — Implemented Option 3 (deferred writes + bitmask side effects)

### Solution Applied

Refactored `updateSetting()` to defer both flash writes and service restarts:

1. **Dirty flag + debounce timer**: `updateSetting()` sets `settingsDirty = true` and
   restarts a 2-second `timerFlushSettings`. The actual `writeSettings()` call is removed
   from `updateSetting()`.

2. **Side-effect bitmask**: Instead of calling `startMQTT()`, `startNTP()`, `startMDNS()`
   inline, each field sets a bitmask flag (`SIDE_EFFECT_MQTT`, `SIDE_EFFECT_NTP`,
   `SIDE_EFFECT_MDNS`). This tracks *which* services need restarting.

3. **`flushSettings()`**: A new function called from `loop()` when `DUE(timerFlushSettings)`.
   It performs exactly one `writeSettings()` call and at most one restart per service —
   regardless of how many fields were changed.

**Result for 20-field save (3 MQTT fields, 1 NTP field)**:
- Flash writes: 20 → **1**
- `startMQTT()` calls: 23 → **1**  
- `startNTP()` calls: 1 → **1** (deferred)
- Heap alloc/free cycles: 20 → **1**

---

## Finding #24: ~~`http.end()` called on uninitialized HTTPClient~~ FIXED

**File:** [OTGW-Core.ino](../../../src/OTGW-firmware/OTGW-Core.ino) — `checkforupdatepic()`  
**Priority:** LOW-MEDIUM  
**Status:** FIXED — Moved `http.end()` unconditionally after the if/else block so it always
matches `http.begin()`, preventing connection leaks on HTTP failure.

### Problem

```cpp
if (latest != version) {
    http.begin(client, "http://otgw.tclcode.com/download/...");
    // ... HTTP GET and file write ...
}
http.end();  // Called unconditionally, even if http.begin() was never called
```

When `latest == version` (no update needed), the code skips `http.begin()` but still
calls `http.end()`. On ESP8266's HTTPClient, calling `end()` on an uninitialized
client may be harmless (it checks internal state), but it's defensive-programming
bad practice and could break with library updates.

### Suggested Fix

Move `http.end()` inside the `if` block:

```cpp
if (latest != version) {
    http.begin(client, url);
    // ... HTTP operations ...
    http.end();  // Only end what was begun
}
```

### Complexity

Trivial — Move one line.

---

## Finding #26: ~~`settingMQTTbrokerPort` missing default fallback~~ FIXED

**File:** [settingStuff.ino](../../../src/OTGW-firmware/settingStuff.ino) — `readSettings()`  
**Priority:** LOW-MEDIUM  
**Status:** FIXED — Added `| settingMQTTbrokerPort` fallback (resolved in commit `0d4b102`).

### Problem

```cpp
settingMQTTbrokerPort   = doc[F("MQTTbrokerPort")]; //default port
```

Unlike every other setting in `readSettings()`, this line has no `| default` fallback.
If the JSON key is missing (corrupted config, migration from older version), the port
silently becomes 0 instead of 1883.

Compare with other settings that do it correctly:
```cpp
settingMQTTenable       = doc[F("MQTTenable")]|settingMQTTenable;  // Has fallback
strlcpy(settingMQTTbroker, doc[F("MQTTbroker")] | "", ...);       // Has fallback
```

### Suggested Fix

```cpp
settingMQTTbrokerPort   = doc[F("MQTTbrokerPort")] | settingMQTTbrokerPort;
```

The global default for `settingMQTTbrokerPort` is already 1883 (set in OTGW-firmware.h),
so the `| settingMQTTbrokerPort` pattern preserves the default when the key is missing.

### Complexity

Trivial — Add `| settingMQTTbrokerPort` to one line.

---

## Finding #27: ~~No GPIO conflict detection~~ FIXED

**Files:** [settingStuff.ino](../../../src/OTGW-firmware/settingStuff.ino) — `checkGPIOConflict()` + `updateSetting()`  
**Priority:** MEDIUM  
**Status:** FIXED — Added `checkGPIOConflict()` helper that checks all three configurable
GPIO features (sensor, S0 counter, output) against each other. Logs a warning via
`DebugTf()` when a conflict is detected. The setting is still applied (warn policy)
since rejection would require frontend changes.

### Problem

Three features use configurable GPIO pins:
- Dallas temperature sensors (`settingGPIOSENSORSpin`)
- S0 pulse counter (`settingS0COUNTERpin`)
- GPIO outputs (`settingGPIOOUTPUTSpin`)

There is no validation preventing a user from configuring two or three features
on the same GPIO pin. When this happens, features interfere silently — e.g.,
the S0 counter interrupt fires on temperature sensor communication, or
the GPIO output overrides the sensor bus.

### Suggested Fix

Add validation in `updateSetting()` when any GPIO pin setting changes:

```cpp
bool hasGPIOConflict(int pin, const char* excludeFeature) {
  if (settingGPIOSENSORSenabled && strcmp_P(excludeFeature, PSTR("sensor")) != 0
      && settingGPIOSENSORSpin == pin) return true;
  if (settingS0COUNTERenabled && strcmp_P(excludeFeature, PSTR("s0")) != 0
      && settingS0COUNTERpin == pin) return true;
  if (settingGPIOOUTPUTSenabled && strcmp_P(excludeFeature, PSTR("output")) != 0
      && settingGPIOOUTPUTSpin == pin) return true;
  return false;
}
```

Log a warning on conflict. Optionally reject the setting change and return an
error to the Web UI.

### Complexity

Medium — Requires adding validation logic and deciding conflict policy (warn vs. reject).

---

## Finding #28: ~~`byteswap` macro lacks parameter parentheses~~ FIXED

**File:** [versionStuff.ino](../../../src/OTGW-firmware/versionStuff.ino) — line 6  
**Priority:** LOW  
**Status:** FIXED — Added parentheses: `((val) << 8) | ((val) >> 8)`.

### Problem

```cpp
#define byteswap(val) ((val << 8) | (val >> 8))
```

If called as `byteswap(a + b)`, this expands to `((a + b << 8) | (a + b >> 8))`.
Due to operator precedence, `<<` binds tighter than `+`, so this becomes
`((a + (b << 8)) | (a + (b >> 8)))` — completely wrong.

Currently only called with simple variables, so no bug manifests today.

### Suggested Fix

```cpp
#define byteswap(val) (((val) << 8) | ((val) >> 8))
```

### Complexity

Trivial — Add parentheses around `val`.

---

## Finding #29: ~~Dallas temperature -127°C not filtered~~ FIXED

**File:** [sensors_ext.ino](../../../src/OTGW-firmware/sensors_ext.ino) — `pollSensors()`  
**Priority:** LOW-MEDIUM  
**Status:** FIXED — Added `DEVICE_DISCONNECTED_C` check before storing temperature.
Disconnected sensors now skip the update and retain their previous reading, with a
debug log message when `bDebugSensors` is enabled.

### Problem

When a Dallas sensor is disconnected or has a read error, the DallasTemperature
library returns `DEVICE_DISCONNECTED_C` (-127.0°C). This value is stored and
published to MQTT without any validation:

```cpp
DallasrealDevice[i].tempC = sensors.getTempC(DallasrealDevice[i].addr);
// ... no validation ...
snprintf_P(_msg, sizeof _msg, PSTR("%4.1f"), DallasrealDevice[i].tempC);
sendMQTTData(strDeviceAddress, _msg);
```

Home Assistant automations that trigger on temperature thresholds may fire
incorrectly when receiving -127.0 as a valid temperature reading.

### Suggested Fix

Filter the error value before storing and publishing:

```cpp
float tempC = sensors.getTempC(DallasrealDevice[i].addr);
if (tempC == DEVICE_DISCONNECTED_C) {
  if (bDebugSensors) DebugTf(PSTR("Sensor [%s] disconnected or read error\r\n"), strDeviceAddress);
  continue;  // Skip this sensor, keep previous value
}
DallasrealDevice[i].tempC = tempC;
```

### Complexity

Low — Add a single `if` check before storing the temperature.

---

## Finding #39: ~~Admin password not persisted~~ REMOVED

**Status:** RESOLVED — `settingAdminPassword` was entirely removed from the codebase
as dead code (commit `cdc1827`). The feature was never implemented: the variable
could be set via `updateSetting()` but was never persisted to JSON, never read back,
and never checked for authentication. Removing it eliminates the finding entirely.

---

## Finding #40: ~~`postSettings()` uses manual string parsing instead of ArduinoJson~~ FIXED

**File:** [restAPI.ino](../../../src/OTGW-firmware/restAPI.ino) — `postSettings()`  
**Priority:** LOW  
**Status:** FIXED — Replaced manual brace/quote stripping + `splitString()` with
`StaticJsonDocument<256>` + `deserializeJson()`. Handles string, boolean, and numeric
value types from the frontend. Returns proper HTTP 400 with error JSON on parse failure.

### Problem

The `postSettings()` function receives JSON like `{"name":"hostname","value":"mygateway"}`
but parses it by stripping braces and quotes, then splitting on `,` and `:`:

```cpp
replaceAll(jsonIn, sizeof(jsonIn), "{", "");
replaceAll(jsonIn, sizeof(jsonIn), "}", "");
replaceAll(jsonIn, sizeof(jsonIn), "\"", "");
uint_fast8_t wp = splitString(jsonIn, ',',  wPair, 5);
```

This breaks if values contain `{`, `}`, `"`, `,`, or `:` — for example, an MQTT
password with special characters or a hostname with unusual characters.

The ArduinoJson library is already linked and used elsewhere in the codebase
(settings read/write, REST API responses). The original author even left a comment:
```cpp
// so, why not use ArduinoJSON library?
// I say: try it yourself ;-) It won't be easy
```

### Suggested Fix

Replace the manual parsing with ArduinoJson:

```cpp
void postSettings() {
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, httpServer.arg(0));
  if (error) {
    httpServer.send(400, F("application/json"), F("{\"error\":\"Invalid JSON\"}"));
    return;
  }

  const char* field = doc[F("name")];
  const char* value = doc[F("value")];

  if (field && value) {
    updateSetting(field, value);
    httpServer.send(200, F("application/json"), F("{\"status\":\"ok\"}"));
  } else {
    httpServer.send(400, F("application/json"), F("{\"error\":\"Missing name/value\"}"));
  }
}
```

### Complexity

Low-Medium — Simple replacement, but needs testing with all setting types. The
`StaticJsonDocument<256>` is adequate since setting payloads are small.

---

## Summary

| # | Finding | Priority | Complexity | Status |
|---|---------|----------|------------|--------|
| 23 | ~~Settings flash wear (write per field)~~ | MEDIUM | Medium | **FIXED** — deferred writes + bitmask |
| 24 | ~~`http.end()` scope in checkforupdatepic~~ | LOW | Trivial | **FIXED** — unconditional http.end() |
| 26 | ~~MQTT port missing default fallback~~ | LOW | Trivial | **FIXED** — `\| settingMQTTbrokerPort` |
| 27 | ~~No GPIO conflict detection~~ | MEDIUM | Medium | **FIXED** — checkGPIOConflict() |
| 28 | ~~`byteswap` macro unparenthesized~~ | LOW | Trivial | **FIXED** — added `(val)` parens |
| 29 | ~~Dallas -127°C not filtered~~ | LOW-MEDIUM | Low | **FIXED** — DEVICE_DISCONNECTED_C check |
| 39 | ~~Admin password not persisted~~ | MEDIUM | Trivial | **REMOVED** — dead code deleted |
| 40 | ~~Manual JSON parsing in postSettings~~ | LOW | Low-Medium | **FIXED** — ArduinoJson replacement |

All findings have been resolved. No remaining work items.
