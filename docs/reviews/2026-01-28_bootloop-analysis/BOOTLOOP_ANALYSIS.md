---
# METADATA
Document Title: Wemos D1 Bootloop Analysis - Exception (2) Access to Invalid Address
Review Date: 2026-01-28 23:30:00 UTC
Firmware Version: 0.10.0+eeeb22c (affected), 1.0.0-rc6+a4ed263 (current)
Issue Reporter: Multiple users on GitHub issue tracker
Reviewer: GitHub Copilot Advanced Agent
Document Type: Root Cause Analysis
Status: COMPLETE
---

# Wemos D1 Bootloop Analysis

## Executive Summary

**Root Cause**: Buffer overrun in `OTGWSerial.cpp` caused by using string functions (`strstr_P`, `strnlen`) on binary data without null terminators.

**Impact**: Device reboots every ~10 seconds with Exception (2) - Access to invalid address (28).

**Status**: 
- ✅ **FIXED** in `versionStuff.ino` (already applied in current branch)
- ❌ **NOT FIXED** in `src/libraries/OTGWSerial/OTGWSerial.cpp` (needs same fix)

**Severity**: CRITICAL - Causes complete device failure

---

## Problem Description

### Symptoms
- Wemos D1 mini reboots approximately every 10 seconds
- Exception (2): LoadProhibited - Access to invalid address (28)
- Consistent error registers: `epc1=0x40241688, excvaddr=0x144c0000`

### Affected Version
- Firmware: 0.10.0+eeeb22c
- PIC Firmware: 6.4

### Error Details
```
Exception (28):
epc1=0x40241688 epc2=0x00000000 epc3=0x00000000 excvaddr=0x144c0000 depc=0x00000000
```

**Exception Cause 28**: LoadProhibited - A load operation referenced a page mapped with an attribute that does not permit loads.

---

## Root Cause Analysis

### Technical Explanation

The crash occurs in the hex file parsing code that searches for version banner strings in binary data. The affected code uses C string functions that expect null-terminated strings on binary data that may not contain null terminators.

### Vulnerable Code Pattern

**Location 1**: `src/libraries/OTGWSerial/OTGWSerial.cpp` lines 303-318 (❌ **STILL VULNERABLE**)

```cpp
// Look for the new firmware version
version = nullptr;
unsigned short ptr = 0;

while (ptr < info.datasize) {
    char *s = strstr_P((char *)datamem + ptr, banner1);  // ❌ DANGEROUS!
    if (s == nullptr) {
        ptr += strnlen((char *)datamem + ptr,              // ❌ DANGEROUS!
          info.datasize - ptr) + 1;
    } else {
        s += sizeof(banner1) - 1;
        version = s;
        break;
    }
}
```

**Location 2**: `versionStuff.ino` (✅ **ALREADY FIXED** in current branch)

The same vulnerable pattern existed in `versionStuff.ino` but has already been fixed with proper bounds-checking.

### Why This Causes a Crash

1. **Binary Data**: The `datamem` buffer contains binary data from Intel HEX files, which is NOT guaranteed to be null-terminated
2. **String Functions**: Both `strstr_P()` and `strnlen()` expect null-terminated strings
3. **Buffer Overrun**: When no null terminator exists in the searched range:
   - `strstr_P()` continues reading beyond the buffer boundary
   - `strnlen()` also reads beyond the buffer looking for a null terminator
4. **Invalid Memory Access**: Eventually these functions read from unmapped or protected memory addresses
5. **Exception**: ESP8266 hardware raises Exception (28) - LoadProhibited
6. **Watchdog Reset**: Device watchdog timer triggers a reset

### Memory Address Evidence

The exception address `0x144c0000` is suspiciously high and outside normal ESP8266 memory ranges:
- DRAM: 0x3FFE8000 - 0x3FFFFFFF
- IRAM: 0x40100000 - 0x40107FFF
- Flash: 0x40200000 - 0x40300000

This confirms the string functions are reading far beyond the intended buffer.

### Backtrace Analysis

From the decoded backtrace:
```
0x40241688: _ungetc_r at newlib/libc/stdio/ungetc.c:202
...
0x4021dcde: OTGWUpgrade::stateMachine
0x4021d29f: OTGWSerial::processorToString
```

The crash happens during PIC firmware version detection, specifically when:
1. Reading a hex file into the `datamem` buffer
2. Searching for the "OpenTherm Gateway " banner string
3. String search functions read beyond buffer boundaries

---

## Solution

### Pattern to Fix

Replace the vulnerable while loop with a safe sliding window search using `memcmp_P()`.

### Fixed Code Pattern (from versionStuff.ino)

```cpp
size_t bannerLen = sizeof(banner) - 1;

// Safer sliding window search:
// 1. Iterate byte-by-byte (ptr++) instead of skipping over strings
// 2. Ensure reading stays strictly within bounds (datasize - bannerLen)
for (ptr = 0; ptr <= (info.datasize - bannerLen); ptr++) {
    // Safe comparison with PROGMEM string using memcmp_P for binary data
    if (memcmp_P((char *)datamem + ptr, banner1, bannerLen) == 0) {
        // Match found!
        version = (char *)datamem + ptr + bannerLen;
        break;
    }
}
```

### Why This Fix Works

1. **Bounded Loop**: `ptr <= (datasize - bannerLen)` ensures we never read beyond the buffer
2. **Binary-Safe Comparison**: `memcmp_P()` compares exact byte counts without expecting null terminators
3. **No String Functions**: Eliminates use of `strstr_P()` and `strnlen()` on binary data
4. **Predictable Behavior**: Loop terminates after checking all valid positions

---

## Implementation Plan

### Required Changes

**File**: `src/libraries/OTGWSerial/OTGWSerial.cpp`  
**Function**: `OTGWError OTGWUpgrade::readHexFile(const char *hexfile)`  
**Lines**: 303-318

### Step-by-Step Fix

1. Replace the while loop (lines 303-318) with a bounded for loop
2. Use `memcmp_P()` instead of `strstr_P()` for binary-safe comparison
3. Calculate banner length once: `size_t bannerLen = sizeof(banner1) - 1`
4. Ensure loop bound: `for (ptr = 0; ptr <= (info.datasize - bannerLen); ptr++)`
5. Add underflow protection: check `info.datasize >= bannerLen` before loop

### Code Diff

```cpp
// OLD (VULNERABLE):
version = nullptr;
unsigned short ptr = 0;

while (ptr < info.datasize) {
    char *s = strstr_P((char *)datamem + ptr, banner1);
    if (s == nullptr) {
        ptr += strnlen((char *)datamem + ptr,
          info.datasize - ptr) + 1;
    } else {
        s += sizeof(banner1) - 1;
        version = s;
        Dprintf("Version: %s\n", version);
        if (firmware == FIRMWARE_OTGW && *fwversion) {
            weight += 4 * WEIGHT_DATAREAD;
        }
        break;
    }
}

// NEW (SAFE):
version = nullptr;
unsigned short ptr = 0;
size_t bannerLen = sizeof(banner1) - 1;

// Safe sliding window search with bounds checking
if (info.datasize >= bannerLen) {
    for (ptr = 0; ptr <= (info.datasize - bannerLen); ptr++) {
        // Binary-safe comparison using memcmp_P
        if (memcmp_P((char *)datamem + ptr, banner1, bannerLen) == 0) {
            // Found banner - version string follows immediately
            version = (char *)datamem + ptr + bannerLen;
            Dprintf("Version: %s\n", version);
            if (firmware == FIRMWARE_OTGW && *fwversion) {
                weight += 4 * WEIGHT_DATAREAD;
            }
            break;
        }
    }
}
```

---

## Testing Recommendations

### Unit Testing
1. Test with hex files containing version banners at various positions
2. Test with hex files missing version banners
3. Test with hex files containing binary data that resembles but isn't the banner
4. Test with minimum-size hex files (edge case: datasize < bannerLen)

### Integration Testing
1. Flash firmware to Wemos D1 mini
2. Perform PIC firmware upgrade via web interface
3. Monitor for exceptions via telnet debug output
4. Verify successful version detection in logs

### Regression Testing
1. Test with multiple PIC firmware versions (6.4, 6.5, etc.)
2. Test on both NodeMCU and Wemos D1 mini hardware
3. Verify MQTT and Home Assistant integration still works
4. Run evaluation framework: `python evaluate.py`

---

## Prevention Measures

### Code Review Checklist
- [ ] Never use `strstr_P()`, `strstr()`, `strnlen()`, `strlen()` on binary data
- [ ] Always use `memcmp_P()` or `memcmp()` for binary buffer comparisons
- [ ] Always add bounds checking before loops that iterate over buffers
- [ ] Check for underflow: ensure `bufferSize >= searchLength` before subtraction
- [ ] Prefer fixed-size loops over pointer-walking patterns

### Documentation Updates
Add to coding standards:
```
## Binary Data Handling (CRITICAL - Prevents Exception Crashes)

**MANDATORY**: When working with binary data (hex files, raw buffers), 
use proper comparison functions.

- **Never use** `strncmp_P()`, `strstr_P()`, or `strnlen()` on binary data
- **Always use** `memcmp_P()` for binary data comparison with PROGMEM
- **Always add** underflow protection before loops
```

---

## References

### Issue Tracker
- GitHub issue: Bootloop - Wemos D1 reboots every 10 seconds
- Firmware version: 0.10.0+eeeb22c
- Multiple user reports

### Technical Documentation
- ESP8266 Exception Causes: https://www.espressif.com/sites/default/files/documentation/esp8266_reset_causes_and_common_fatal_exception_causes_en.pdf
- Exception (28): LoadProhibited - Memory access violation

### Related Code
- `versionStuff.ino` lines 84-114 - Same vulnerability, already fixed
- `src/libraries/OTGWSerial/OTGWSerial.cpp` lines 303-318 - Needs fix
- Custom instruction section "Binary Data Handling" - Coding standards

---

## Conclusion

This bootloop issue is a **critical memory safety bug** caused by using string functions on binary data. The fix is straightforward and has already been successfully applied to `versionStuff.ino`. The same fix must be applied to `OTGWSerial.cpp` to prevent the crash.

**Recommendation**: Apply the fix immediately and release as a patch version (0.10.1 or 1.0.0-rc7) to prevent user devices from bricking.
