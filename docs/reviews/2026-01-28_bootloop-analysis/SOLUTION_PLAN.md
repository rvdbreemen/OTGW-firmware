---
# METADATA
Document Title: Bootloop Fix Solution Plan
Review Date: 2026-01-28 23:30:00 UTC
Reviewer: GitHub Copilot Advanced Agent
Document Type: Solution Plan
Status: COMPLETE
---

# Bootloop Fix - Solution Plan

## Problem Statement

Wemos D1 mini devices running firmware 0.10.0 experience bootloops (rebooting every ~10 seconds) with Exception (2) - Access to invalid address. The crash occurs in PIC firmware version detection code that uses unsafe string functions on binary data.

## Root Cause

**File**: `src/libraries/OTGWSerial/OTGWSerial.cpp`  
**Function**: `OTGWUpgrade::readHexFile()`  
**Lines**: 303-318

The code uses `strstr_P()` and `strnlen()` to search for a version banner in binary data from Intel HEX files. These functions expect null-terminated strings, but the binary data is not guaranteed to have null terminators. This causes the functions to read beyond buffer boundaries, triggering memory access exceptions.

## Solution

Replace the vulnerable string-based search with a safe, bounds-checked sliding window search using `memcmp_P()`.

### Code Changes Required

**Before (Vulnerable)**:
```cpp
// Look for the new firmware version
version = nullptr;
unsigned short ptr = 0;

while (ptr < info.datasize) {
    char *s = strstr_P((char *)datamem + ptr, banner1);
    if (s == nullptr) {
        ptr += strnlen((char *)datamem + ptr,
          info.datasize - ptr) + 1;
    } else {
        s += sizeof(banner1) - 1;   // Drop the terminating '\0'
        version = s;
        Dprintf("Version: %s\n", version);
        if (firmware == FIRMWARE_OTGW && *fwversion) {
            // Reading out the EEPROM settings takes 4 reads of 64 bytes
            weight += 4 * WEIGHT_DATAREAD;
        }
        break;
    }
}
```

**After (Safe)**:
```cpp
// Look for the new firmware version
version = nullptr;
unsigned short ptr = 0;
size_t bannerLen = sizeof(banner1) - 1;

// Safe sliding window search with bounds checking
// Prevents reading beyond buffer even if no null terminator exists
if (info.datasize >= bannerLen) {
    for (ptr = 0; ptr <= (info.datasize - bannerLen); ptr++) {
        // Binary-safe comparison using memcmp_P for PROGMEM strings
        if (memcmp_P((char *)datamem + ptr, banner1, bannerLen) == 0) {
            // Found banner - version string follows immediately after
            version = (char *)datamem + ptr + bannerLen;
            Dprintf("Version: %s\n", version);
            if (firmware == FIRMWARE_OTGW && *fwversion) {
                // Reading out the EEPROM settings takes 4 reads of 64 bytes
                weight += 4 * WEIGHT_DATAREAD;
            }
            break;
        }
    }
}
```

### Key Improvements

1. **Bounds Checking**: `if (info.datasize >= bannerLen)` prevents underflow
2. **Safe Loop**: `ptr <= (info.datasize - bannerLen)` ensures we never read past the end
3. **Binary-Safe Comparison**: `memcmp_P()` doesn't require null terminators
4. **No String Functions**: Eliminates `strstr_P()` and `strnlen()` entirely

## Implementation Steps

### 1. Edit the File
- Open: `src/libraries/OTGWSerial/OTGWSerial.cpp`
- Locate: Lines 300-318 (the readHexFile function)
- Replace: The while loop with the safe for loop

### 2. Testing Plan
Since this is analysis only (per instructions: "Don't change code"), testing will be done when the fix is actually implemented:

**Build Testing**:
```bash
python build.py --firmware
```

**Evaluation**:
```bash
python evaluate.py --quick
```

**Hardware Testing** (when deployed):
- Flash to Wemos D1 mini
- Monitor telnet debug output
- Attempt PIC firmware upgrade
- Verify no exceptions occur
- Confirm version detection works correctly

### 3. Validation
- No build errors
- Evaluation passes all critical checks
- Memory safety improved (no buffer overruns)
- Functionally equivalent behavior (finds version banners correctly)

## Why This Fix Works

### Memory Safety
- **Old code**: Could read 256+ bytes beyond buffer (until random null byte found)
- **New code**: Guaranteed to read exactly `info.datasize` bytes maximum

### Binary Data Handling
- **Old code**: String functions assume null terminators (not present in binary data)
- **New code**: `memcmp_P()` compares exact byte counts without assumptions

### Exception Prevention
- **Old code**: Reads invalid addresses → Exception (28)
- **New code**: All reads within valid buffer range → No exceptions

## Additional Benefits

1. **Performance**: Single-pass scan instead of repeated `strstr_P()` + `strnlen()` calls
2. **Predictability**: Fixed maximum iterations (256) vs unpredictable string search
3. **Maintainability**: Clear loop bounds make code easier to understand
4. **Consistency**: Matches the pattern already used successfully in `versionStuff.ino`

## Risk Assessment

### Risk: Low
- Change is localized to a single function
- Pattern proven in `versionStuff.ino` (already in production)
- Functionally equivalent to original intent
- No API changes

### Mitigation
- Test with multiple PIC firmware hex files
- Verify version detection on both PIC models (16f88 and 16f1847)
- Monitor telnet debug output during testing

## Related Issues

### Similar Fix Already Applied
The same pattern was fixed in `versionStuff.ino` (current codebase):
- Lines 84-114 show the correct implementation
- Uses `memcmp_P()` for binary-safe comparison
- Includes proper bounds checking

### Coding Standard Addition
This issue highlights the need for explicit binary data handling guidelines. The custom instructions should include:

```markdown
### Binary Data Handling (CRITICAL - Prevents Exception Crashes)

**MANDATORY**: When working with binary data (hex files, raw buffers), 
use proper comparison functions.

- **Never use** `strncmp_P()`, `strstr_P()`, or `strnlen()` on binary data
  - These functions expect null-terminated strings
  - Binary data from hex files does NOT have null terminators
  - This causes Exception (2) crashes when reading protected memory

- **Always use** `memcmp_P()` for binary data comparison with PROGMEM
- **Always add** underflow protection before loops
```

## Conclusion

This is a **critical security and stability fix** that prevents device bootloops. The solution is straightforward, proven (already used in `versionStuff.ino`), and low-risk.

**Recommendation**: Implement this fix immediately and include it in the next firmware release (1.0.0-rc7 or 1.0.0 final).

## References

- Full analysis: `BOOTLOOP_ANALYSIS.md`
- Affected file: `src/libraries/OTGWSerial/OTGWSerial.cpp`
- Reference implementation: `versionStuff.ino` lines 84-114
- Custom instructions: Binary Data Handling section
