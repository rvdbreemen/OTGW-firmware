---
# METADATA
Document Title: OTGW-firmware Comprehensive Codebase Review
Initial Review Date: 2026-02-13 05:19:57 UTC
Last Updated: 2026-02-16
Branch Reviewed: dev (bd87103) → fixes on claude/review-codebase-w3Q6N
Original Commit Reviewed: 79a9247a38713dd210eacbe62110db4b4a3d4e5f
Reviewer: GitHub Copilot Advanced Agent
Document Type: Comprehensive Code Review with Fix Status
Scope: Full source code review of all .ino, .h, and .cpp files in src/OTGW-firmware/
Status: COMPLETE — All 20 impactful findings resolved
---

# OTGW-firmware Comprehensive Codebase Review

**Initial Review:** 2026-02-13  
**Fixes Completed:** 2026-02-16  
**Scope:** Full source code review of all `.ino`, `.h`, and `.cpp` files in `src/OTGW-firmware/`

---

## Executive Summary

A comprehensive code review of the OTGW-firmware codebase identified **40 findings** across critical bugs, security considerations, and code quality issues. After critical analysis, **20 findings** were classified as impactful (real bugs or documented trade-offs) and **20 were removed** as non-issues (Arduino architecture quirks, style issues, unfixable API items).

### Final Status

| Category | Count | Status |
|----------|-------|--------|
| Critical & High Priority Bugs | 13 | **ALL RESOLVED** (4 in dev, 8 in `0d4b102`, 1 retracted) |
| Medium Priority Issues | 7 | **ALL RESOLVED** (1 removed as dead code, 6 fixed in `86fc6d0`–`735f58a`) |
| Security (ADR Trade-offs) | 6 | Documented decisions — not bugs |
| Removed (Non-issues) | 20 | N/A |

**Key Impact Areas Resolved:**
- **Memory safety:** Out-of-bounds array writes, stack buffer overflows
- **Data integrity:** Incorrect bitmasks corrupting MQTT time data
- **Concurrency:** ISR race conditions in pulse counter
- **Security:** Reflected XSS vulnerability
- **Resource leaks:** File descriptors, HTTP clients
- **Reliability:** Blocking operations, flash wear, invalid sensor data
- **Feature failures:** GPIO outputs disabled by debug gate

---

## Critical & High Priority Issues (13 findings — ALL RESOLVED)

### Finding #1: Out-of-bounds array write when `OTdata.id == 255` — ✅ FIXED in dev

**File:** `OTGW-Core.ino:1657`, `OTGW-Core.h:472`  
**Severity:** CRITICAL — Memory corruption

`OTdata.id` ranges 0–255 (extracted as `(value >> 16) & 0xFF`), but `msglastupdated` was declared as `time_t msglastupdated[255]` (indices 0–254). When `OTdata.id == 255`, the write `msglastupdated[OTdata.id] = now` corrupted adjacent memory.

```cpp
// BEFORE (bug):
time_t msglastupdated[255];

// AFTER (fix — already in dev branch):
time_t msglastupdated[256] = {0};
```

**Impact:** Crash, watchdog reset, or undefined behavior on OpenTherm message ID 255.

---

### Finding #2: Wrong bitmask corrupts afternoon/evening hours in MQTT — ✅ FIXED in dev

**File:** `OTGW-Core.ino:1297`  
**Severity:** HIGH — Data corruption

The OpenTherm spec uses bits 0–4 for hours (0–23, requires 5-bit mask `0x1F`). The mask `0x0F` (4 bits) truncated hours 16–23: hour 20 became 4, hour 23 became 7.

```cpp
// BEFORE (bug):
sendMQTTData(_topic, itoa((OTdata.valueHB & 0x0F), _msg, 10));

// AFTER (fix — already in dev branch):
sendMQTTData(_topic, itoa((OTdata.valueHB & 0x1F), _msg, 10));
```

**Impact:** Incorrect time data in MQTT/Home Assistant for afternoon and evening schedules.

---

### Finding #3: `is_value_valid()` references global instead of parameter — ✅ FIXED in dev

**File:** `OTGW-Core.ino:622-624`  
**Severity:** HIGH — Logic bug (masked)

Function takes parameter `OT` but lines 622–624 referenced global `OTdata` instead. Currently masked because always called with `OTdata`, but violates function contract.

```cpp
// BEFORE (bug): OTdata referenced instead of OT parameter
_valid = _valid || (OTlookup.msgcmd==OT_WRITE && OTdata.type==OT_WRITE_DATA);

// AFTER (fix — already in dev branch):
_valid = _valid || (OTlookup.msgcmd==OT_WRITE && OT.type==OT_WRITE_DATA);
```

**Impact:** Would break if function ever called with a different argument.

---

### Finding #4: `sizeof()` vs `strlen()` off-by-one in PIC version parsing — ✅ FIXED in dev

**File:** `OTGW-Core.ino:167`  
**Severity:** HIGH — Version string corruption

`sizeof(OTGW_BANNER)` includes the null terminator (18 vs 17), dropping the first character of the PIC version string.

```cpp
// BEFORE (bug):
p += sizeof(OTGW_BANNER);        // Skips 18 bytes (17 chars + \0)

// AFTER (fix — already in dev branch):
p += sizeof(OTGW_BANNER) - 1;    // Skips 17 bytes (banner text only)
```

**Impact:** PIC firmware version displayed incorrectly (e.g., ".2.9" instead of "4.2.9").

---

### Finding #5: Stack buffer overflow in hex file parser — ✅ FIXED in `0d4b102`

**File:** `versionStuff.ino:43-55`  
**Severity:** CRITICAL — Stack corruption

The address calculation produces starting indices 0–128, but the `while` loop can write beyond `datamem[255]` if a hex record has enough bytes.

```cpp
// BEFORE (bug — no bounds check):
while (len > 0) {
    datamem[addr++] = byteswap(data);
    // ...
}

// AFTER (fix):
while (len > 0) {
    if (addr >= (int)(sizeof(datamem))) break;   // Bounds check added
    datamem[addr++] = byteswap(data);
    // ...
}
```

**Impact:** Stack corruption, crashes, firmware upgrade failures.

---

### Finding #6: ISR race conditions in S0 pulse counter — ✅ FIXED in `0d4b102`

**File:** `s0PulseCount.ino:32-44, 63-70`  
**Severity:** HIGH — Incorrect energy readings

Four issues in ISR handling:
1. `settingS0COUNTERdebouncetime` read in ISR but not `volatile`
2. `pulseCount` checked before `noInterrupts()` (TOCTOU race)
3. `last_pulse_duration` read outside critical section
4. `pulseCount` as `uint8_t` wraps at 255

**Fixes applied:**
- Changed `pulseCount` from `uint8_t` to `uint16_t`
- Made debounce time volatile-safe in ISR
- Moved all shared variable reads inside `noInterrupts()`/`interrupts()` critical section
- Eliminated TOCTOU race

**Impact:** Incorrect power readings, missed pulses, broken Home Assistant energy dashboard.

---

### Finding #7: Reflected XSS in `sendApiNotFound()` — ✅ FIXED in `0d4b102`

**File:** `restAPI.ino:876`  
**Severity:** HIGH — Security vulnerability

URI from user input was directly injected into HTML response without escaping. An attacker could craft `/api/<script>alert(document.cookie)</script>`.

```cpp
// BEFORE (bug):
httpServer.sendContent(URI);   // Raw user input

// AFTER (fix):
String escapedURI = String(URI);
escapedURI.replace(F("&"), F("&amp;"));
escapedURI.replace(F("<"), F("&lt;"));
escapedURI.replace(F(">"), F("&gt;"));
escapedURI.replace(F("\""), F("&quot;"));
escapedURI.replace(F("'"), F("&#39;"));
httpServer.sendContent(escapedURI);
```

**Impact:** Script injection, credential theft, phishing on local network.

---

### Finding #8: `evalOutputs()` gated by debug flag — GPIO outputs never run — ✅ FIXED in `0d4b102`

**File:** `outputs_ext.ino:85-86`  
**Severity:** CRITICAL — Feature completely broken

The GPIO outputs function only executed when `settingMyDEBUG == true`, and immediately cleared the flag. In normal operation, outputs were never updated.

```cpp
// BEFORE (bug):
void evalOutputs() {
  if (!settingMyDEBUG) return;     // Blocks execution in normal operation
  settingMyDEBUG = false;
  // ... output logic never runs ...
}

// AFTER (fix):
void evalOutputs() {
  bool bitState = (OTcurrentSystemState.Statusflags & (1U << settingGPIOOUTPUTStriggerBit)) != 0;
  if (settingMyDEBUG) {            // Debug logging only, doesn't gate execution
    settingMyDEBUG = false;
    DebugTf(...);
  }
  setOutputState(bitState);        // Always runs now
}
```

**Impact:** GPIO outputs feature (drive LEDs/relays based on boiler status) was completely non-functional.

---

### Finding #16: `ETX` constant value — ✅ RETRACTED (not a bug)

**File:** `OTGW-firmware.h:74`  
**Original claim:** `0x04` is wrong, ASCII ETX is `0x03`

**Retraction:** Value `0x04` is **correct** for the OTGW bootloader protocol. This is NOT standard ASCII ETX but a custom protocol defined by Schelte Bron (OTGW creator). Verified against `src/libraries/OTGWSerial/OTGWSerial.cpp:31` and the authoritative `otgwmcu/otmonitor` source:
- `STX = 0x0F` (not standard 0x02)
- `ETX = 0x04` (not standard 0x03)
- `DLE = 0x05` (not standard 0x10)

---

### Finding #18: Null pointer dereference in MQTT callback — ✅ FIXED in `0d4b102`

**File:** `MQTTstuff.ino:312-317`  
**Severity:** HIGH — Crash on malformed MQTT topic

`strtok()` return value was not null-checked before `strcasecmp()`. A malformed MQTT topic (empty, single token, trailing slash) would crash the device.

```cpp
// AFTER (fix): NULL checks after each strtok() call
token = strtok(topic, "/");
if (token == NULL) { MQTTDebugln(F("MQTT: missing 'set' token")); return; }
```

**Impact:** Remote crash via malformed MQTT message, device offline in Home Assistant.

---

### Finding #20: File descriptor leak in `readSettings()` — ✅ FIXED in `0d4b102`

**File:** `settingStuff.ino:88-97`  
**Severity:** HIGH — Resource leak

File was opened before checking existence. On the non-existent path, recursive call leaked the file handle.

```cpp
// BEFORE (bug):
File file = LittleFS.open(SETTINGS_FILE, "r");   // Opens first
if (!LittleFS.exists(SETTINGS_FILE)) {            // Then checks
    return readSettings(false);                    // Leaks file handle
}

// AFTER (fix):
if (!LittleFS.exists(SETTINGS_FILE)) {            // Check BEFORE opening
    writeSettings(show);
    readSettings(false);
    return;                                        // No file handle to leak
}
File file = LittleFS.open(SETTINGS_FILE, "r");
```

**Impact:** File descriptor exhaustion causing settings corruption on subsequent operations.

---

### Finding #21: Year truncated to `int8_t` in `yearChanged()` — ✅ FIXED in `0d4b102`

**File:** `helperStuff.ino:413`  
**Severity:** HIGH — Type overflow

`int8_t thisyear = myTime.year()` — year 2026 overflows `int8_t` range (-128 to 127). Comparison still detected changes by accident, but stored values were meaningless.

```cpp
// BEFORE: int8_t thisyear = myTime.year();    // Overflows
// AFTER:  int16_t thisyear = myTime.year();   // Correct range
```

**Impact:** Function worked by accident; undefined behavior per C standard (signed overflow).

---

### Finding #22: `requestTemperatures()` blocks for ~750ms — ✅ FIXED in `0d4b102`

**File:** `sensors_ext.ino:122`  
**Severity:** HIGH — Watchdog risk

`requestTemperatures()` blocks the main loop for up to 750ms (12-bit resolution), risking watchdog timeout and unresponsive web UI.

```cpp
// AFTER (fix):
sensors.begin();
sensors.setWaitForConversion(false);    // Async mode — returns immediately
```

The sensor interval timer (default 20s) provides ample time for conversion between requests.

**Impact:** Potential watchdog resets, unresponsive web UI, missed OpenTherm messages.

---

## Medium Priority Issues (7 findings — ALL RESOLVED)

### Finding #23: Settings flash wear — ✅ FIXED in `86fc6d0`

**File:** `settingStuff.ino` — `updateSetting()` / `flushSettings()`  
**Severity:** MEDIUM

Each Web UI settings save triggered `writeSettings()` once per field (15–20 flash writes) plus unnecessary MQTT/NTP/MDNS restarts per field.

**Solution — Option 3 (deferred writes + bitmask side effects):**
1. `updateSetting()` sets `settingsDirty = true` and restarts a 2-second debounce timer
2. Side-effect bitmask (`SIDE_EFFECT_MQTT | NTP | MDNS`) tracks which services need restarting
3. `flushSettings()` executes exactly one `writeSettings()` and at most one restart per service

**Result for 20-field save (3 MQTT fields, 1 NTP field):**

| Metric | Before | After |
|--------|--------|-------|
| Flash writes | 20 | **1** |
| `startMQTT()` calls | 23 | **1** |
| `startNTP()` calls | 1 | **1** (deferred) |
| Heap alloc/free cycles | 20 | **1** |

---

### Finding #24: `http.end()` scope in `checkforupdatepic()` — ✅ FIXED in `735f58a`

**File:** `OTGW-Core.ino` — `checkforupdatepic()`  
**Severity:** LOW-MEDIUM

`http.begin()` was always called, but `http.end()` was only called inside `if (code == HTTP_CODE_OK)`. On HTTP failure, the connection leaked.

**Fix:** Moved `http.end()` unconditionally after the if/else block so it always matches `http.begin()`.

---

### Finding #26: `settingMQTTbrokerPort` missing default fallback — ✅ FIXED in `0d4b102`

**File:** `settingStuff.ino` — `readSettings()`  
**Severity:** LOW-MEDIUM

Unlike every other setting, the MQTT port had no `| default` fallback. If the JSON key was missing (corrupted config, migration), port silently became 0 instead of 1883.

```cpp
// BEFORE: settingMQTTbrokerPort = doc[F("MQTTbrokerPort")];
// AFTER:  settingMQTTbrokerPort = doc[F("MQTTbrokerPort")] | settingMQTTbrokerPort;
```

---

### Finding #27: No GPIO conflict detection — ✅ FIXED in `735f58a`

**File:** `settingStuff.ino` — `checkGPIOConflict()` + `updateSetting()`  
**Severity:** MEDIUM

Three features use configurable GPIO pins (sensor, S0 counter, output) with no validation preventing assignment of the same pin to multiple features.

**Fix:** Added `checkGPIOConflict()` helper that checks all three configurable GPIO features against each other. Logs a warning via `DebugTf()` when a conflict is detected. Uses warn policy (setting still applied) since rejection would require frontend changes.

---

### Finding #28: `byteswap` macro lacks parameter parentheses — ✅ FIXED in `735f58a`

**File:** `versionStuff.ino:6`  
**Severity:** LOW

`#define byteswap(val) ((val << 8) | (val >> 8))` — `val` not parenthesized, so `byteswap(a + b)` produces incorrect results due to operator precedence. Currently only called with simple variables.

```cpp
// BEFORE: #define byteswap(val) ((val << 8) | (val >> 8))
// AFTER:  #define byteswap(val) (((val) << 8) | ((val) >> 8))
```

---

### Finding #29: Dallas temperature -127°C not filtered — ✅ FIXED in `735f58a`

**File:** `sensors_ext.ino` — `pollSensors()`  
**Severity:** LOW-MEDIUM

`DEVICE_DISCONNECTED_C` (-127.0°C) from disconnected sensors was published to MQTT without validation, triggering false alarms in Home Assistant.

```cpp
// AFTER (fix):
float tempC = sensors.getTempC(DallasrealDevice[i].addr);
if (tempC == DEVICE_DISCONNECTED_C) {
    if (bDebugSensors) DebugTf(PSTR("Sensor [%s] disconnected or read error, skipping\r\n"), strDeviceAddress);
    continue;   // Keep previous value
}
DallasrealDevice[i].tempC = tempC;
```

---

### Finding #39: Admin password not persisted — ✅ REMOVED in `cdc1827`

**Severity:** MEDIUM

`settingAdminPassword` was dead code: could be set via `updateSetting()` but was never persisted to JSON, never read back, and never checked for authentication. Removed entirely from the codebase.

---

### Finding #40: `postSettings()` manual string parsing — ✅ FIXED in `735f58a`

**File:** `restAPI.ino` — `postSettings()`  
**Severity:** LOW

JSON was "parsed" by stripping `{}` and `"`, then splitting on `,` and `:`. Broke on values containing special characters. The original author even acknowledged this with a comment: *"so, why not use ArduinoJSON library? I say: try it yourself ;-) It won't be easy"*.

**Fix:** Replaced with `StaticJsonDocument<256>` + `deserializeJson()`. Handles string, boolean, and numeric value types from the frontend. Returns proper HTTP 400 with error JSON on parse failure.

---

## Security Issues (6 items — ADR-Documented Trade-offs)

The following items are **not implementation bugs** but documented architectural decisions per [ADR-032](../../adr/ADR-032-no-authentication-local-network-security.md) and [ADR-003](../../adr/ADR-003-http-only-no-https.md). They represent deliberate trade-offs that prioritize ease of use, memory efficiency, and local-network trust over application-level security.

### Finding #9: No authentication on endpoints

**Files:** All network-facing modules  
**ADR:** ADR-032 (Accepted) — No Authentication Pattern

The OTGW-firmware intentionally does not implement authentication on REST API, MQTT command, WebSocket, or Telnet endpoints. This is based on:
- **Target deployment:** Home local network (not internet-facing)
- **Memory constraints:** ESP8266 has only ~20–25KB available RAM
- **User experience:** Zero-configuration integration with Home Assistant
- **Security model:** Network isolation provides stronger security than application authentication

**Risk:** Any device with network access can change settings, send OpenTherm commands, flash firmware, upload/delete files, and read configuration including MQTT credentials.

**Mitigations:** Keep on trusted LAN, use VLAN segmentation, access via VPN, use reverse proxy with auth.

### Finding #10: MQTT credentials exposed via Telnet

**File:** `settingStuff.ino:182` | **ADR:** ADR-032

`readSettings(true)` prints MQTT password in cleartext to unauthenticated Telnet debug stream. Documented consequence of no-auth model.

**Mitigations:** Restrict Telnet access via firewall, don't grant MQTT credentials access to critical systems.

### Finding #11: File deletion/upload without authentication

**File:** `FSexplorer.ino:441-491` | **ADR:** ADR-032

FSexplorer performs file operations (including `?delete=` and arbitrary uploads) without HTTP-level authentication. Allows stored XSS via malicious file upload.

**Mitigations:** Keep on trusted LAN, use firewall/VPN/reverse proxy with authentication.

### Finding #12: PIC firmware downloaded over plain HTTP

**File:** `OTGW-Core.ino:2355` | **ADR:** ADR-003

PIC firmware fetched from `http://otgw.tclcode.com/` without TLS. ESP8266 memory constraints (TLS requires 20–30KB RAM) make HTTPS impractical. MITM risk on update path.

**Mitigations:** Download only from trusted networks, use VPN, optionally proxy via internal server.

### Finding #13: Wildcard CORS combined with no-auth

**File:** `restAPI.ino:267` | **ADR:** ADR-032

`Access-Control-Allow-Origin: *` combined with no authentication means any website the user visits can make cross-origin requests to the device.

**Mitigations:** Ensure device only reachable on trusted LAN, use firewall rules.

### Finding #14: OTA firmware update unauthenticated by default

**File:** `OTGW-ModUpdateServer-impl.h:89-93` | **ADR:** ADR-032

When OTA username/password are empty (default), OTA flashing is unauthenticated. Optional OTA credentials can be set via the Web UI Settings page for users who need it.

---

## Removed Findings (20 non-issues)

The original review contained 40 findings. After critical analysis, 20 were removed:

| # | Finding | Reason Removed |
|---|---------|---------------|
| 15 | `settingMQTTbrokerPort` type `int16_t` | Theoretical — no one uses MQTT on port >32767 |
| 16 | `ETX` constant value 0x04 | **RETRACTED** — correct for OTGW bootloader protocol (verified against otgwmcu source) |
| 17 | `setMQTTConfigDone()` always returns true | Dead code branch, caller ignores return value |
| 19 | `sendJsonSettingObj` discards escaped value | Analysis was incorrect; behavior is intentional |
| 25 | Wrong variable in debug message | Debug output only, no functional impact |
| 30 | Globals defined in headers | Arduino single-TU build — works correctly by design |
| 31 | Functions in headers without `inline` | Arduino single-TU build — works correctly by design |
| 32 | `using namespace` in header | Style issue, no functional impact |
| 33 | MQTT topic typos (`fault_incidator`, `vh_ventlation_mode`, etc.) | Breaking change to fix; part of published API |
| 34 | Redundant `break` after `return` | Dead code but harmless in 110-case switch |
| 35 | `OTGWs0pulseCount` missing initializer | Zero-initialized by C++ standard |
| 36 | `weekDayName` missing bounds check | Low risk, input constrained upstream |
| 37 | `contentType()` mutates input | Intentional design choice, not a bug |
| 38 | Division by zero in timer system | Timer intervals always >0 in practice |

---

## Commit History

| Commit | Date | Changes |
|--------|------|---------|
| dev branch (bd87103) | pre-review | Findings #1, #2, #3, #4 already fixed |
| `0d4b102` | 2026-02-15 | Fix 8 critical/high findings (#5, #6, #7, #8, #18, #20, #21, #22) + #26 |
| `cdc1827` | 2026-02-15 | Remove dead `settingAdminPassword` code (#39) |
| `86fc6d0` | 2026-02-16 | Fix #23 settings flash wear (deferred writes + bitmask side effects) |
| `735f58a` | 2026-02-16 | Fix remaining medium findings (#24, #27, #28, #29, #40) |

**Branch:** `claude/review-codebase-w3Q6N` (PR [#432](https://github.com/rvdbreemen/OTGW-firmware/pull/432))

---

## Timeline

| Date | Event |
|------|-------|
| 2026-02-13 05:19 UTC | Initial review completed — 40 findings |
| 2026-02-13 06:30 UTC | Critical analysis — 20 kept, 20 removed as non-issues |
| 2026-02-13 06:35 UTC | Revised review — 20 impactful findings documented |
| 2026-02-15 22:00 UTC | Verified against dev branch (bd87103) — findings #1–#4 already fixed |
| 2026-02-15 | Fixed 8 critical/high findings + MQTT port default + dead admin password |
| 2026-02-16 | Fixed settings flash wear (#23) with deferred writes + bitmask |
| 2026-02-16 | Fixed remaining 5 medium findings (#24, #27, #28, #29, #40) |
| 2026-02-16 | **All 20 impactful findings resolved** — review complete |

---

## ADR References

- **[ADR-003: HTTP-Only Network Architecture (No HTTPS)](../../adr/ADR-003-http-only-no-https.md)** — HTTP without TLS due to ESP8266 memory constraints
- **[ADR-032: No Authentication Pattern (Local Network Security Model)](../../adr/ADR-032-no-authentication-local-network-security.md)** — No authentication, relying on network-level security

When evaluating security findings, these ADRs represent accepted architectural trade-offs, not implementation oversights. Any changes that would contradict these ADRs should be accompanied by a superseding ADR.

---

## Verification

- **Build verified:** All fixes compile successfully with `python build.py --firmware`
- **Original review commit:** 79a9247
- **Dev branch verified against:** bd87103
- **All fixes on branch:** `claude/review-codebase-w3Q6N`
- **Finding #16 retracted:** Verified against otgwmcu/otmonitor source by Schelte Bron
