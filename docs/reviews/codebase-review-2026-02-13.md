# OTGW-firmware Codebase Review

**Date:** 2026-02-13
**Scope:** Full source code review of all `.ino`, `.h`, and `.cpp` files in `src/OTGW-firmware/`

---

## Executive Summary

The OTGW-firmware is a mature, well-organized ESP8266 firmware for the OpenTherm Gateway. The modular `.ino` file organization and PROGMEM usage show good embedded practices. However, the review uncovered **8 critical/high bugs**, **several security gaps**, and numerous medium/low issues. The most impactful findings are an out-of-bounds array write, incorrect bitmasks corrupting MQTT data, ISR race conditions, and a reflected XSS vulnerability.

---

## Critical & High Priority Issues

### 1. Out-of-bounds array write when `OTdata.id == 255`
**File:** `OTGW-Core.ino:1657`, `OTGW-Core.h:472`

```cpp
msglastupdated[OTdata.id] = now;
```

`OTdata.id` ranges 0-255 (extracted as `(value >> 16) & 0xFF`), but `msglastupdated` is declared as `time_t msglastupdated[255]` (indices 0-254). When `OTdata.id` is 255, this writes past the end of the array, corrupting adjacent memory.

**Fix:** Change declaration to `time_t msglastupdated[256]` or add bounds checking.

---

### 2. Wrong bitmask corrupts afternoon/evening hours in MQTT
**File:** `OTGW-Core.ino:1297`

```cpp
sendMQTTData(_topic, itoa((OTdata.valueHB & 0x0F), _msg, 10));  // BUG: 0x0F
```

The OpenTherm spec uses bits 0-4 for hours (0-23, requires 5 bits, mask `0x1F`). The mask `0x0F` (4 bits) truncates hours 16-23 to incorrect values (e.g., hour 20 becomes 4). The `AddLog` line above correctly uses `0x1F`.

**Fix:** Change `0x0F` to `0x1F`.

---

### 3. `is_value_valid()` references global instead of parameter
**File:** `OTGW-Core.ino:622-624`

```cpp
bool is_value_valid(OpenthermData_t OT, OTlookup_t OTlookup) {
  _valid = _valid || (OTlookup.msgcmd==OT_WRITE && OTdata.type==OT_WRITE_DATA);   // <-- OTdata, not OT
  _valid = _valid || (OTlookup.msgcmd==OT_RW && (OT.type==OT_READ_ACK || OTdata.type==OT_WRITE_DATA));
  _valid = _valid || (OTdata.id==OT_Statusflags) || (OTdata.id==OT_StatusVH) || ...;
```

Lines 622-624 reference the global `OTdata` instead of the function parameter `OT`. Currently masked because the function is always called with `OTdata`, but will break if ever called with a different argument.

**Fix:** Replace `OTdata` with `OT` in these lines.

---

### 4. `sizeof()` vs `strlen()` off-by-one in PIC version parsing
**File:** `OTGW-Core.ino:167`

```cpp
p += sizeof(OTGW_BANNER);   // Returns 18 (17 chars + null terminator)
```

`sizeof(OTGW_BANNER)` includes the null terminator, skipping one character too many. This always drops the first character of the PIC firmware version string.

**Fix:** Use `strlen(OTGW_BANNER)` or `sizeof(OTGW_BANNER) - 1`.

---

### 5. Stack buffer overflow in hex file parser
**File:** `versionStuff.ino:43-55`

```cpp
char datamem[256];
// ...
if (addr >= 0x4200 && addr <= 0x4300) {
  addr = (addr - 0x4200) >> 1;   // 0 to 128
  while (len > 0) {
    datamem[addr++] = byteswap(data);   // addr can exceed 255
```

The address calculation can produce indices 0-128, and the loop increments `addr` up to `len` times (up to 127). Starting at index 128, the loop can write to `datamem[255]` and beyond, causing a stack buffer overflow.

**Fix:** Add bounds check `if (addr >= sizeof(datamem)) break;` inside the loop.

---

### 6. ISR race conditions in S0 pulse counter
**File:** `s0PulseCount.ino:32-44, 63-70`

Three related issues:
- `settingS0COUNTERdebouncetime` is read in the ISR but not declared `volatile`
- `last_pulse_duration` is read outside the `noInterrupts()` critical section
- `pulseCount` is checked (`!= 0`) before disabling interrupts (TOCTOU race)

Additionally, `pulseCount` is `uint8_t` and will wrap at 255 pulses per interval, which can happen at moderate power consumption levels with a 60-second reporting interval.

**Fix:** Make `settingS0COUNTERdebouncetime` volatile, move all shared variable reads inside the critical section, change `pulseCount` to `uint16_t`.

---

### 7. Reflected XSS in `sendApiNotFound()`
**File:** `restAPI.ino:876`

```cpp
httpServer.sendContent(URI);   // Raw user input injected into HTML
```

The URI from `httpServer.uri()` is reflected directly into an HTML response without escaping. An attacker can craft a URL like `/api/<script>alert(1)</script>` to execute JavaScript.

**Fix:** HTML-escape the URI before including it in the response.

---

### 8. `evalOutputs()` gated by debug flag -- never runs normally
**File:** `outputs_ext.ino:85-86`

```cpp
if (!settingMyDEBUG) return;
settingMyDEBUG = false;
```

The function that drives GPIO outputs based on boiler status only executes when `settingMyDEBUG` is true, and immediately clears the flag. In normal operation, outputs are never updated.

**Fix:** Remove or restructure the debug gate so outputs are evaluated on every call.

---

## Security Issues

### 9. No authentication on any endpoint
All REST API, MQTT command, WebSocket, and Telnet endpoints have zero authentication. Any device on the local network can change settings, send OTGW commands, flash firmware, upload/delete files, and read MQTT credentials.

### 10. MQTT credentials exposed via Telnet
**File:** `settingStuff.ino:182`
`readSettings(true)` prints the MQTT password in cleartext to the unauthenticated Telnet debug stream (invoked via debug commands `q` and `k`).

### 11. Arbitrary file deletion/upload without authentication
**File:** `FSexplorer.ino:441-491`
Any HTTP request with a `?delete=/settings.ini` parameter deletes that file. The upload handler accepts files to any path. Combined, this allows replacing `index.html` with a malicious page (stored XSS).

### 12. PIC firmware downloaded over plain HTTP
**File:** `OTGW-Core.ino:2355`
Firmware for the PIC microcontroller is fetched from `http://otgw.tclcode.com/` without TLS, enabling man-in-the-middle attacks to inject malicious PIC firmware.

### 13. Wildcard CORS (`Access-Control-Allow-Origin: *`)
**File:** `restAPI.ino:267`
Combined with no authentication, any malicious website a user visits can silently interact with the OTGW device via cross-origin requests.

### 14. OTA firmware update with no authentication by default
**File:** `OTGW-ModUpdateServer-impl.h:89-93`
When username/password are empty (the default), authentication is completely bypassed for OTA firmware flashing.

---

## Medium Priority Issues

### 15. `settingMQTTbrokerPort` type is `int16_t` (max 32767)
**File:** `OTGW-firmware.h:157`
Valid TCP ports go up to 65535. Should be `uint16_t`.

### 16. `ETX` constant has wrong value
**File:** `OTGW-firmware.h:74`
`#define ETX ((uint8_t)0x04)` -- ASCII ETX is `0x03`; `0x04` is EOT. If used for ETX protocol detection, this matches the wrong byte.

### 17. `setMQTTConfigDone()` always returns true
**File:** `MQTTstuff.ino:699-712`
Arduino's `bitSet()` macro sets a bit and returns the modified variable, which is always > 0. The else branch is dead code.

### 18. Null pointer dereference in MQTT callback
**File:** `MQTTstuff.ino:312-317`
`strtok()` return value is not null-checked before being passed to `strcasecmp()` on lines 312 and 315. A malformed MQTT topic would crash the device.

### 19. `sendJsonSettingObj` discards escaped value
**File:** `jsonStuff.ino:449-462`
The function escapes `cValue` into `buffer` via `str_cstrlit()`, then uses the original unescaped `cValue` in `snprintf_P`. The escaped buffer is never used.

### 20. File descriptor leak in `readSettings()`
**File:** `settingStuff.ino:88-97`
The file is opened before checking if it exists. On the non-existent path, the file handle is never closed before the recursive call.

### 21. Year truncated to `int8_t` in `yearChanged()`
**File:** `helperStuff.ino:413`
`int8_t thisyear = myTime.year();` -- 2026 overflows `int8_t` (range -128 to 127). The comparison still detects changes by accident but the values are meaningless.

### 22. `requestTemperatures()` blocks for ~750ms
**File:** `sensors_ext.ino:122`
`DallasTemperature::requestTemperatures()` blocks the main loop for up to 750ms (12-bit resolution), risking a watchdog reset. The non-blocking pattern should be used instead.

### 23. Settings are written to flash on every individual field update
**File:** `settingStuff.ino:351`
Batch updates from the UI trigger `writeSettings()` once per field, causing excessive flash wear and unnecessary MQTT reconnections.

### 24. `http.end()` called on uninitialized HTTPClient
**File:** `OTGW-Core.ino:2406`
If `latest == version`, the `http.begin()` is never called but `http.end()` is called unconditionally, which may crash.

### 25. Wrong variable in debug message
**File:** `settingStuff.ino:335`
Prints `settingGPIOOUTPUTSenabled` (bool 0/1) instead of `settingGPIOOUTPUTSpin` (the actual pin number).

### 26. `settingMQTTbrokerPort` missing default fallback
**File:** `settingStuff.ino:116`
Unlike other settings, the ArduinoJson `| default` pattern is missing. If the key is absent from the config file, port defaults to 0 instead of 1883.

### 27. No GPIO conflict detection
**File:** `outputs_ext.ino`, `sensors_ext.ino`, `s0PulseCount.ino`
Users can configure all three features (sensor, S0 counter, output) to the same GPIO pin through settings. No validation prevents this.

### 28. `byteswap` macro has no parentheses around parameter
**File:** `versionStuff.ino:6`
`#define byteswap(val) ((val << 8) | (val >> 8))` -- `val` is not parenthesized, so `byteswap(a + b)` produces wrong results due to operator precedence.

### 29. Dallas temperature -127C not filtered
**File:** `sensors_ext.ino:132`
`DEVICE_DISCONNECTED_C` (-127.0) from a disconnected sensor is published to MQTT without validation, potentially confusing downstream automation.

---

## Low Priority / Code Quality

### 30. Global variables defined (not declared) in header files
**Files:** `OTGW-firmware.h`, `OTGW-Core.h`, `networkStuff.h`
Dozens of globals are defined in `.h` files. Works only because Arduino concatenates `.ino` files into a single translation unit. Any refactoring to proper `.cpp` files would break the build.

### 31. Functions defined in headers without `inline`
**Files:** `Debug.h:45`, `safeTimers.h:160`, `networkStuff.h`
Same root cause as #30. Would cause multiple-definition linker errors in a standard C++ build.

### 32. `using namespace ace_time;` in header file
**File:** `OTGW-firmware.h:117`
Pollutes the global namespace for all files including this header.

### 33. Typos in MQTT topic names (now part of published API)
**File:** `OTGW-Core.ino`
Multiple typos: `fault_incidator`, `vh_free_ventlation_mode`, `vh_ventlation_mode`, `eletric_production`, `vh_tramfer_enble_nominal_ventlation_value`. Fixing these would be a breaking change for downstream consumers.

### 34. Redundant `break` after `return` in 110-case switch
**File:** `OTGW-Core.ino:2045-2154`
Every case in `getOTGWValue()` has `return X; break;` -- the `break` is dead code.

### 35. `OTGWs0pulseCount` missing initializer
**File:** `OTGW-firmware.h:208`
Only global in the file without an explicit `= 0` initializer. Zero-initialized by default, but inconsistent.

### 36. `weekDayName` array lacks bounds checking
**File:** `OTGW-firmware.h:131`
Out-of-bounds reads possible if `dayOfWeek()` returns an unexpected value.

### 37. `contentType()` mutates its input parameter
**File:** `FSexplorer.ino:512-529`
The function takes `String& filename`, overwrites it with the MIME type, and returns a reference. Destroys the original filename.

### 38. Division by zero possible in timer system
**File:** `safeTimers.h:182`
`timer_interval / timer_interval` with `timer_interval == 0` causes a fatal exception on ESP8266. No guard exists.

### 39. Admin password not persisted
**File:** `settingStuff.ino:237`
`AdminPassword` can be set via `updateSetting()` but is never written to or read from the settings file, meaning it resets to empty on every reboot.

### 40. `postSettings()` uses manual string parsing instead of ArduinoJson
**File:** `restAPI.ino:811-863`
JSON is "parsed" by stripping `{}` and `"`, then splitting on `,` and `:`. This breaks for values containing these characters and bypasses the ArduinoJson library that is already linked.

---

## Recommendations (prioritized)

1. **Fix the critical bugs (#1-#8)** -- These affect correctness and can cause crashes, data corruption, or security exploits.
2. **Address ISR safety (#6)** -- The S0 pulse counter has multiple race conditions that produce incorrect energy readings.
3. **Add input validation** -- Sanitize HTTP parameters, validate GPIO pin ranges, bounds-check array indices.
4. **Consider basic authentication** -- At minimum on destructive endpoints (settings, file operations, OTA update, OTGW commands).
5. **Fix the MQTT data bugs (#2, #17, #18)** -- Incorrect hour values and always-true config tracking affect Home Assistant integration.
6. **Batch settings writes** -- Reduce flash wear by coalescing multiple setting updates.
7. **Use ArduinoJson consistently** -- Replace manual string parsing in `postSettings()`.
8. **Add temperature validation** -- Filter `DEVICE_DISCONNECTED_C` (-127) before publishing to MQTT.
