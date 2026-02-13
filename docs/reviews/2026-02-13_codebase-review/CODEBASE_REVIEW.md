---
# METADATA
Document Title: OTGW-firmware Comprehensive Codebase Review
Review Date: 2026-02-13 05:19:57 UTC
Branch Reviewed: copilot/sub-pr-432
Commit Reviewed: 79a9247a38713dd210eacbe62110db4b4a3d4e5f
Reviewer: GitHub Copilot Advanced Agent
Document Type: Comprehensive Code Review
Scope: Full source code review of all .ino, .h, and .cpp files in src/OTGW-firmware/
Status: COMPLETE
---

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

## Security Issues (Documented Trade-offs per ADR-032 and ADR-003)

The following items are **not implementation bugs** but documented architectural decisions. They represent deliberate trade-offs that prioritize ease of use, memory efficiency, and local-network trust over application-level security.

### 9. No authentication on endpoints (documented trade-off per ADR-032)
**Files:** All network-facing modules  
**ADR:** [ADR-032: No Authentication Pattern (Local Network Security Model)](../adr/ADR-032-no-authentication-local-network-security.md)

Per **ADR-032 (Accepted)**, the OTGW-firmware intentionally does not implement authentication on REST API, MQTT command, WebSocket, or Telnet endpoints. This is a deliberate architectural decision based on:
- **Target deployment:** Home local network (not internet-facing)
- **Memory constraints:** ESP8266 has only ~20-25KB available RAM
- **User experience:** Zero-configuration integration with Home Assistant
- **Security model:** Network isolation provides stronger security than application authentication

**Risk within this model:** Any device with network access to the OTGW can change settings, send OpenTherm commands, flash firmware, upload/delete files, and read configuration including MQTT credentials.

**Mitigations (per ADR-032):**
- Keep device on trusted local network only (WPA2/WPA3-encrypted WiFi)
- Use network segmentation (VLAN) to isolate IoT devices
- Access remotely via VPN or secure tunnel (external to device)
- Use reverse proxy with authentication if exposing to untrusted clients
- Physical security (device inside home)

**If stronger guarantees are desired:** Create a superseding ADR that revisits ADR-032 and implements application-level authentication, accepting the memory and complexity trade-offs.

---

### 10. MQTT credentials exposed via Telnet (documented consequence of ADR-032)
**File:** `settingStuff.ino:182`  
**ADR:** [ADR-032: No Authentication Pattern](../adr/ADR-032-no-authentication-local-network-security.md)

`readSettings(true)` prints the MQTT password in cleartext to the unauthenticated Telnet debug stream (invoked via debug commands `q` and `k`). This is a documented consequence of ADR-032's no-authentication model.

**Risk within this model:** Anyone with network access can retrieve MQTT credentials via Telnet (port 23).

**Mitigations (per ADR-032):**
- Telnet server only accessible on trusted local network
- Use firewall rules to restrict Telnet access if needed
- MQTT credentials should not grant access to critical systems
- Consider disabling Telnet in production deployments

**If this is unacceptable:** Create a superseding ADR that adds authentication to Telnet or removes credential printing from debug output.

---

### 11. File deletion/upload without authentication (documented trade-off per ADR-032)
**File:** `FSexplorer.ino:441-491`  
**ADR:** [ADR-032: No Authentication Pattern](../adr/ADR-032-no-authentication-local-network-security.md)

Per **ADR-032 (Accepted)**, FSexplorer intentionally performs file operations (including `?delete=/settings.ini` and arbitrary uploads) without HTTP-level authentication. Within that accepted design, this creates a risk that anyone with network access to the device can delete configuration files or replace `index.html` with a malicious page (stored XSS).

**Mitigations within the current architecture:**
- Keep the device on a trusted LAN only
- Restrict access via firewall/VPN or a reverse proxy with authentication
- Network segmentation to isolate IoT devices

**Potential follow-up work:** Consider hardening FSexplorer behavior (e.g., limiting which paths can be modified, adding confirmation for destructive operations) via a new ADR if stronger guarantees are desired within the no-auth model.

---

### 12. PIC firmware downloaded over plain HTTP (risk under ADR-003 HTTP-only)
**File:** `OTGW-Core.ino:2355`  
**ADR:** [ADR-003: HTTP-Only Network Architecture (No HTTPS)](../adr/ADR-003-http-only-no-https.md)

Consistent with **ADR-003 (Accepted - HTTP-only, no HTTPS/TLS)**, firmware for the PIC microcontroller is fetched from `http://otgw.tclcode.com/` without TLS. This is an accepted architectural constraint based on ESP8266 memory limitations (TLS requires 20-30KB RAM).

**Risk within this model:** A network attacker on the update path could perform a man-in-the-middle attack to tamper with the downloaded firmware (integrity risk / potential malicious PIC firmware).

**Recommended mitigations within the ADR-003 model:**
- Perform downloads only from trusted networks
- Optionally proxy or mirror the firmware via a trusted internal server
- If feasible, validate firmware integrity via checksums or signatures (if available from upstream)
- Use VPN when triggering PIC firmware updates from untrusted networks

**If TLS is required:** Moving to a TLS-based download flow would require a new ADR that revisits ADR-003 and accepts the memory constraints and implementation complexity.

---

### 13. Wildcard CORS (`Access-Control-Allow-Origin: *`) combined with no-auth (ADR-032)
**File:** `restAPI.ino:267`  
**ADR:** [ADR-032: No Authentication Pattern](../adr/ADR-032-no-authentication-local-network-security.md)

The REST API currently sends `Access-Control-Allow-Origin: *`. Together with **ADR-032's no-auth-by-design** model, this means that any website the user visits can make cross-origin requests to the device from the user's browser.

**Risk within this model:** This is an intentional consequence of prioritizing ease of local integration over in-device auth, but increases exposure to drive-by attacks if the device is reachable from untrusted clients.

**Mitigations while keeping ADR-032:**
- Ensure the device is only reachable on trusted LAN segments
- Use firewall rules to restrict access to the device
- Access device Web UI via VPN when on untrusted networks

**Potential follow-up work:** Consider stricter CORS policies (e.g., limiting origins or restricting state-changing endpoints) in a future change governed by a new or updated ADR.

---

### 14. OTA firmware update with no authentication by default (per ADR-032)
**File:** `OTGW-ModUpdateServer-impl.h:89-93`  
**ADR:** [ADR-032: No Authentication Pattern](../adr/ADR-032-no-authentication-local-network-security.md)

When the OTA username/password are left empty (the default), OTA firmware flashing is intentionally unauthenticated in line with **ADR-032 (Accepted - no authentication by default, local-network trust model)**. This is not an implementation bug but a documented trade-off: it simplifies setup at the cost of allowing any host with network access to trigger an OTA update.

**Guidance for users:**
- **Configure OTA credentials** when operating in environments where untrusted clients may reach the device
- OTA credentials can be set via the Web UI Settings page
- This provides optional authentication for users who need it while keeping default setup simple

**If mandatory authentication is desired:** Any move toward mandatory authentication or more restrictive defaults should be made via a superseding ADR that revisits ADR-032.

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
4. **Consider authentication options within ADR-032 model** -- While full authentication would violate ADR-032, consider hardening specific high-risk endpoints (file operations, OTA updates) with optional authentication, or create a superseding ADR if architectural changes are desired.
5. **Fix the MQTT data bugs (#2, #17, #18)** -- Incorrect hour values and always-true config tracking affect Home Assistant integration.
6. **Batch settings writes** -- Reduce flash wear by coalescing multiple setting updates.
7. **Use ArduinoJson consistently** -- Replace manual string parsing in `postSettings()`.
8. **Add temperature validation** -- Filter `DEVICE_DISCONNECTED_C` (-127) before publishing to MQTT.

---

## ADR References

This review references the following Architecture Decision Records:

- **[ADR-003: HTTP-Only Network Architecture (No HTTPS)](../adr/ADR-003-http-only-no-https.md)** - Documents the decision to use HTTP-only protocols without TLS/SSL due to ESP8266 memory constraints
- **[ADR-032: No Authentication Pattern (Local Network Security Model)](../adr/ADR-032-no-authentication-local-network-security.md)** - Documents the decision to not implement authentication, relying on network-level security instead

When evaluating security findings in this review, these ADRs represent accepted architectural trade-offs, not implementation oversights. Any changes that would contradict these ADRs should be accompanied by a superseding ADR that revisits the original decision.
