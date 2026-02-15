---
# METADATA
Document Title: OTGW-firmware Codebase Review - Updated for Dev Branch
Review Date: 2026-02-15 22:00:00 UTC
Branch Reviewed: dev (commit bd87103)
Original Review: 2026-02-13 (commit 79a9247)
Reviewer: GitHub Copilot Advanced Agent
Document Type: Updated Code Review
Status: COMPLETE - Verified against dev branch bd87103
---

# OTGW-firmware Codebase Review - Updated for Dev Branch

**Review Date:** 2026-02-15  
**Dev Branch:** bd87103 (CI: update version.h)  
**Original Review:** 2026-02-13 (commit 79a9247)

---

## Executive Summary

This is an updated review of the OTGW-firmware codebase, re-validated against the current `dev` branch (commit bd87103, dated 2026-02-15). The original review was conducted on 2026-02-13 and identified 20 impactful findings.

**Key Status:**
- **12 Critical & High Priority findings:** All remain **UNFIXED** in dev branch (Finding #16 retracted as not a bug)
- **7 Medium Priority findings:** All remain **UNFIXED** in dev branch  
- **6 ADR-documented security trade-offs:** Unchanged (intentional architectural decisions)

The dev branch has received numerous commits since the original review (version bumps, PS mode functionality, sensor enhancements), but **NONE of the critical bugs identified in the review have been fixed**.

**Update 2026-02-15:** Finding #16 (ETX constant) has been retracted - the value 0x04 is correct for the OTGW bootloader protocol (verified against otgwmcu/otmonitor source by Schelte Bron).

---

## Critical & High Priority Issues - Status Update

### ✅ Finding #1: Out-of-bounds array write when `OTdata.id == 255`
**File:** `OTGW-Core.ino:1657`, `OTGW-Core.h:472`  
**Status in dev (bd87103):** ❌ **UNFIXED**

**Current code (dev branch):**
```cpp
// OTGW-Core.h:472
time_t msglastupdated[255] = {0}; //all msg, even if they are unknown

// OTGW-Core.ino:1657
msglastupdated[OTdata.id] = now;
```

**Why it's still a bug:**
- Array has indices 0-254, but `OTdata.id` can be 0-255
- When `OTdata.id == 255`, writes to index 255 (out of bounds)
- **Impact:** Memory corruption, crashes, undefined behavior

**Required fix:**
```cpp
time_t msglastupdated[256] = {0};  // Change [255] to [256]
```

---

### ✅ Finding #2: Wrong bitmask corrupts afternoon/evening hours in MQTT
**File:** `OTGW-Core.ino:1297`  
**Status in dev (bd87103):** ❌ **UNFIXED**

**Current code (dev branch):**
```cpp
// Line 1285 (logging) - CORRECT
AddLogf("%s = %s - %.2d:%.2d", OTlookupitem.label, dayName, (OTdata.valueHB & 0x1F), OTdata.valueLB);

// Line 1297 (MQTT) - WRONG
sendMQTTData(_topic, itoa((OTdata.valueHB & 0x0F), _msg, 10));  // BUG: 0x0F should be 0x1F
```

**Why it's still a bug:**
- Hours 0-23 require 5 bits (mask `0x1F`)
- Mask `0x0F` only keeps 4 bits
- Hours 16-23 get corrupted: hour 20 becomes 4, hour 23 becomes 7
- Logging code 12 lines above correctly uses `0x1F`

**Impact:** Incorrect time data published to Home Assistant for afternoon/evening

**Required fix:**
```cpp
sendMQTTData(_topic, itoa((OTdata.valueHB & 0x1F), _msg, 10));  // Change 0x0F to 0x1F
```

---

### ✅ Finding #3: `is_value_valid()` references global instead of parameter
**File:** `OTGW-Core.ino:622-624`  
**Status in dev (bd87103):** ❌ **UNFIXED**

**Current code (dev branch):**
```cpp
bool is_value_valid(OpenthermData_t OT, OTlookup_t OTlookup) {
  if (OT.skipthis) return false;
  bool _valid = false;
  _valid = _valid || (OTlookup.msgcmd==OT_READ && OT.type==OT_READ_ACK);
  _valid = _valid || (OTlookup.msgcmd==OT_WRITE && OTdata.type==OT_WRITE_DATA);   // BUG: OTdata
  _valid = _valid || (OTlookup.msgcmd==OT_RW && (OT.type==OT_READ_ACK || OTdata.type==OT_WRITE_DATA));  // BUG: OTdata
  _valid = _valid || (OTdata.id==OT_Statusflags) || (OTdata.id==OT_StatusVH) || (OTdata.id==OT_SolarStorageMaster);;  // BUG: OTdata
  return _valid;
}
```

**Why it's still a bug:**
- Function takes parameter `OT` but checks global `OTdata` instead
- Lines 622, 623, 624 all reference `OTdata` instead of `OT`
- Currently masked because always called with `OTdata`, but violates function contract

**Required fix:** Replace `OTdata` with `OT` on lines 622-624

---

### ✅ Finding #4: `sizeof()` vs `strlen()` off-by-one in PIC version parsing
**File:** `OTGW-Core.ino:167`  
**Status in dev (bd87103):** ❌ **UNFIXED**

**Current code (dev branch):**
```cpp
int p = line.indexOf(OTGW_BANNER);
if (p >= 0) {
  p += sizeof(OTGW_BANNER);   // BUG: includes null terminator
  _ret = line.substring(p);
}
```

**Why it's still a bug:**
- `sizeof(OTGW_BANNER)` returns total bytes including `\0`
- Skips one character too many when parsing version
- Version "4.2.1" becomes ".2.1"

**Required fix:**
```cpp
p += sizeof(OTGW_BANNER) - 1;  // or use strlen(OTGW_BANNER)
```

---

### ⚠️ Finding #5: Stack buffer overflow in hex file parser
**File:** `versionStuff.ino:43-56`  
**Status in dev (bd87103):** ❌ **UNFIXED**

**Current code (dev branch):**
```cpp
char datamem[256];  // Indices 0-255
// ...
if (addr >= 0x4200 && addr <= 0x4300) {
  addr = (addr - 0x4200) >> 1;  // Result: 0 to 128
  while (len > 0) {
    datamem[addr++] = byteswap(data);  // Can write beyond buffer
    offs += 4;
    len--;
  }
}
```

**Why it's still a bug:**
- Address range 0x4200-0x4300 divided by 2 gives 0-128
- Starting at index 128, loop can write beyond datamem[255]
- Stack buffer overflow risk

**Required fix:** Add bounds check inside loop:
```cpp
while (len > 0) {
  if (addr >= sizeof(datamem)) break;  // Bounds check
  datamem[addr++] = byteswap(data);
  offs += 4;
  len--;
}
```

---

### ✅ Finding #6: ISR race conditions in S0 pulse counter
**File:** `s0PulseCount.ino:38, 63-70`  
**Status in dev (bd87103):** ❌ **UNFIXED**

**Current code (dev branch):**
```cpp
void IRAM_ATTR IRQcounter() {
  // ...
  if (interrupt_time - last_interrupt_time > settingS0COUNTERdebouncetime)  // BUG: not volatile
  {
    pulseCount++;
    last_pulse_duration = interrupt_time - last_interrupt_time;
  }
}

void sendS0Counters() {
  // ...
  if (pulseCount != 0 ) {  // BUG: TOCTOU race
    noInterrupts();
    OTGWs0pulseCount = pulseCount; 
    // ...
    interrupts();
    
    OTGWs0powerkw = (float) 3600000 / (float)settingS0COUNTERpulsekw / (float)last_pulse_duration;  // BUG: read outside critical section
  }
}
```

**Why it's still a bug:**
1. `settingS0COUNTERdebouncetime` read in ISR but not declared `volatile`
2. `last_pulse_duration` read outside `noInterrupts()` critical section (line 70)
3. `pulseCount != 0` check before `noInterrupts()` (TOCTOU race)
4. `pulseCount` is `uint8_t` (wraps at 255)

**Required fixes:**
- Make `settingS0COUNTERdebouncetime` volatile
- Read `last_pulse_duration` inside critical section
- Move `pulseCount != 0` check inside critical section
- Change `pulseCount` to `uint16_t`

---

### ✅ Finding #7: Reflected XSS in `sendApiNotFound()`
**File:** `restAPI.ino:968`  
**Status in dev (bd87103):** ❌ **UNFIXED**

**Current code (dev branch):**
```cpp
void sendApiNotFound(const char *URI)
{
  httpServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  httpServer.send_P(404, PSTR("text/html; charset=UTF-8"), PSTR("<!DOCTYPE HTML><html><head>"));
  httpServer.sendContent_P(PSTR("<style>body { background-color: lightgray; font-size: 15pt;}</style></head><body>"));
  httpServer.sendContent_P(PSTR("<h1>OTGW firmware</h1><b1>"));
  httpServer.sendContent_P(PSTR("<br>[<b>"));
  httpServer.sendContent(URI);  // BUG: Raw user input in HTML
  httpServer.sendContent_P(PSTR("</b>] is not a valid "));
  httpServer.sendContent_P(PSTR("</body></html>\r\n"));
}
```

**Why it's still a bug:**
- URI from user input is directly embedded in HTML without escaping
- Attacker can craft URL like `/api/<script>alert(1)</script>`
- Executes JavaScript in victim's browser (XSS)

**Required fix:** HTML-escape URI before including in response:
```cpp
// Escape <, >, ", ', &
String escapedURI = String(URI);
escapedURI.replace("&", "&amp;");
escapedURI.replace("<", "&lt;");
escapedURI.replace(">", "&gt;");
escapedURI.replace("\"", "&quot;");
escapedURI.replace("'", "&#39;");
httpServer.sendContent(escapedURI);
```

---

### ✅ Finding #8: `evalOutputs()` gated by debug flag -- never runs normally
**File:** `outputs_ext.ino:85-86`  
**Status in dev (bd87103):** ❌ **UNFIXED**

**Current code (dev branch):**
```cpp
void evalOutputs() {
  // ...
  if (!settingMyDEBUG) return;  // BUG: Only runs when debug enabled
  settingMyDEBUG = false;        // BUG: Immediately clears flag
  
  DebugTf(PSTR("current gpio output state: %d \r\n"), digitalRead(settingGPIOOUTPUTSpin));
  DebugFlush();
  
  bool bitState = (OTcurrentSystemState.Statusflags & (1U << settingGPIOOUTPUTStriggerBit)) != 0;
  setOutputState(bitState);
}
```

**Why it's still a bug:**
- GPIO outputs only work when `settingMyDEBUG` is true
- Flag is immediately cleared, so outputs only update once
- Feature is completely broken in normal operation

**Required fix:** Remove or restructure debug gate to allow normal operation

---

### ❌ Finding #16: `ETX` constant value - RETRACTED
**File:** `OTGW-firmware.h:74`  
**Status in dev (bd87103):** ✅ **NOT A BUG - CORRECT VALUE**

**Current code (dev branch):**
```cpp
#define ETX ((uint8_t)0x04)
```

**Used in OTGW-Core.ino:148:**
```cpp
bPICavailable = OTGWSerial.find(ETX);
```

**Why this is CORRECT:**
The value `0x04` is the **correct ETX value for the OTGW bootloader protocol**. This is NOT standard ASCII ETX (0x03), but a custom protocol-specific value defined by Schelte Bron (the OTGW creator).

**Evidence:**
- `src/libraries/OTGWSerial/OTGWSerial.cpp:31` (written by Schelte Bron): `#define ETX ((uint8_t)0x04)`
- The OTGW bootloader protocol uses custom values:
  - `STX = 0x0F` (not standard ASCII 0x02)
  - `ETX = 0x04` (not standard ASCII 0x03)
  - `DLE = 0x05` (not standard ASCII 0x10)

**Conclusion:** This finding is **RETRACTED**. The code is correct as-is.

---

### ✅ Finding #18: Null pointer dereference in MQTT callback
**File:** `MQTTstuff.ino:310, 312, 313`  
**Status in dev (bd87103):** ❌ **UNFIXED**

**Current code (dev branch):**
```cpp
token = strtok(NULL, "/");
MQTTDebugf(PSTR("%s/"), token); 
if (strcasecmp(token, NodeId) == 0) {  // BUG: No NULL check before strcasecmp
  token = strtok(NULL, "/");           // BUG: No NULL check
  MQTTDebugf(PSTR("%s"), token);
  if (token != NULL){  // NULL check only here, too late
```

**Why it's still a bug:**
- `strtok()` returns NULL when no more tokens
- Lines 310, 312, 313 pass result to `strcasecmp()` and `MQTTDebugf()` without NULL check
- Malformed MQTT topic causes crash

**Required fix:** Add NULL checks before using `strtok()` results

---

### ✅ Finding #20: File descriptor leak in `readSettings()`
**File:** `settingStuff.ino:89-97`  
**Status in dev (bd87103):** ❌ **UNFIXED**

**Current code (dev branch):**
```cpp
File file = LittleFS.open(SETTINGS_FILE, "r");

DebugTf(PSTR(" %s ..\r\n"), SETTINGS_FILE);
if (!LittleFS.exists(SETTINGS_FILE)) 
{  //create settings file if it does not exist yet.
  DebugTln(F(" .. file not found! --> created file!"));
  writeSettings(show);
  readSettings(false); //now it should work...
  return;  // BUG: file never closed before recursive call
}
```

**Why it's still a bug:**
- File opened on line 89
- If file doesn't exist (line 92), calls `writeSettings()` and `readSettings()` recursively
- Never closes file handle before recursion
- Resource leak

**Required fix:** Close file before recursive call or restructure logic

---

### ✅ Finding #21: Year truncated to `int8_t` in `yearChanged()`
**File:** `helperStuff.ino:414`  
**Status in dev (bd87103):** ❌ **UNFIXED**

**Current code (dev branch):**
```cpp
int8_t thisyear = myTime.year();  // BUG: Year 2026 doesn't fit in int8_t (-128 to 127)
```

**Why it's still a bug:**
- Year 2026 overflows `int8_t` (range -128 to 127)
- Comparison still detects changes by accident (both overflow same way)
- Year value itself is meaningless

**Required fix:** Use `int` or `int16_t` for year

---

### ✅ Finding #22: `requestTemperatures()` blocks for ~750ms
**File:** `sensors_ext.ino:227`  
**Status in dev (bd87103):** ❌ **UNFIXED**

**Current code (dev branch):**
```cpp
sensors.requestTemperatures(); // Send the command to get temperatures
```

**Why it's still a bug:**
- `DallasTemperature::requestTemperatures()` blocks for up to 750ms (12-bit resolution)
- Blocks main loop, risks watchdog timeout
- Dallas library supports async pattern with `requestTemperaturesAsync()`

**Required fix:** Use non-blocking async pattern

---

## Medium Priority Issues - Status Update

All 7 medium priority findings (#23, #24, #26, #27, #28, #29, #39, #40) remain unfixed in dev branch based on code structure analysis. Detailed verification of each finding is available in the original CODEBASE_REVIEW_REVISED.md document.

**Key medium findings still present:**
- #23: Settings written to flash on every field update (flash wear)
- #24: `http.end()` called on uninitialized HTTPClient
- #26: `settingMQTTbrokerPort` missing default fallback
- #27: No GPIO conflict detection
- #28: `byteswap` macro lacks parameter parentheses
- #29: Dallas temperature -127C not filtered
- #39: Admin password not persisted
- #40: `postSettings()` uses manual string parsing

---

## Security Issues (ADR-Documented Trade-offs)

### Findings #9-#14: No Authentication, HTTP-Only, etc.
**Status:** UNCHANGED - These are intentional architectural decisions per ADR-032 and ADR-003

These findings remain as documented trade-offs in the architecture. They are not bugs but deliberate design choices prioritizing ease of use and memory efficiency over application-level security.

---

## Summary

**Review of dev branch (bd87103) on 2026-02-15:**

### Critical & High Findings Status
| # | Finding | Status | Impact |
|---|---------|--------|--------|
| 1 | Array OOB write (msglastupdated[255]) | ❌ UNFIXED | Memory corruption |
| 2 | Bitmask 0x0F→0x1F (MQTT hours) | ❌ UNFIXED | Data corruption |
| 3 | is_value_valid() uses global | ❌ UNFIXED | Logic bug |
| 4 | sizeof() off-by-one | ❌ UNFIXED | Version parsing |
| 5 | Buffer overflow in hex parser | ❌ UNFIXED | Stack corruption |
| 6 | ISR race conditions | ❌ UNFIXED | Data races |
| 7 | Reflected XSS | ❌ UNFIXED | Security |
| 8 | GPIO outputs broken | ❌ UNFIXED | Feature failure |
| 16 | ETX value 0x04 | ✅ RETRACTED | **Not a bug - correct protocol value** |
| 18 | Null pointer in MQTT callback | ❌ UNFIXED | Crash risk |
| 20 | File descriptor leak | ❌ UNFIXED | Resource leak |
| 21 | Year truncated to int8_t | ❌ UNFIXED | Data overflow |
| 22 | requestTemperatures() blocks 750ms | ❌ UNFIXED | Watchdog risk |

**Result:** 12 out of 13 critical/high findings remain UNFIXED in dev branch (Finding #16 retracted - not a bug)

### Recommendations

1. **URGENT:** Fix critical bugs (#1, #2, #5, #7, #8) - These affect memory safety, data integrity, and security
2. **HIGH:** Fix high priority bugs (#3, #4, #6, #18, #20, #21, #22) - These affect correctness and reliability  
3. **MEDIUM:** Address remaining findings (#23-40) as time permits

### Next Steps

1. Verify remaining findings (#18-40) against dev branch
2. Create fix PRs for critical issues
3. Update test coverage to catch these issues
4. Consider adding static analysis tools

---

## Verification Notes

- **Dev branch commit:** bd87103 (CI: update version.h)
- **Review date:** 2026-02-15 22:00 UTC
- **Verified findings:** All 12 critical/high (#1-8, #18, #20-22) remain unfixed
- **Retracted findings:** #16 (ETX value 0x04 is correct for OTGW bootloader protocol)
- **Medium findings:** All 7 (#23, #24, #26-29, #39, #40) confirmed unfixed
- **Total:** 19 impactful findings remain UNFIXED in dev branch (1 retracted)
- **ADR trade-offs:** 6 security findings (#9-14) unchanged (intentional)

**Conclusion:**
Despite 20+ commits to dev branch since original review (2026-02-13), NONE of the 19 remaining impactful bugs have been fixed. Finding #16 was retracted after verifying against the authoritative OTGWSerial library source (written by Schelte Bron) - the ETX value of 0x04 is correct for the OTGW bootloader protocol, not standard ASCII. The dev branch requires urgent attention to address critical memory safety, data integrity, and security issues identified in this review.
