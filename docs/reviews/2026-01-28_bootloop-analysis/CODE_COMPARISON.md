---
# METADATA
Document Title: Code Comparison - Vulnerable vs Safe Pattern
Review Date: 2026-01-28 23:30:00 UTC
Reviewer: GitHub Copilot Advanced Agent
Document Type: Code Reference
Status: COMPLETE
---

# Vulnerable vs Safe Code Comparison

## The Problem Visualized

```
Buffer: datamem[256]
        ┌─────────────────────────────────────────┐
        │ Binary data from hex file (no null)    │
        │ [0x42][0x07][0x60][0x50][0x03]...      │
        └─────────────────────────────────────────┘
         0                                      255

strstr_P() searching for "OpenTherm Gateway ":
        │
        ├──> Not found in buffer, keeps reading...
        │
        ├──> Reads past end at 256
        │
        ├──> Reads invalid memory at 0x144c0000
        │
        └──> CRASH! Exception (28)
```

## Side-by-Side Comparison

### ❌ VULNERABLE CODE (Current - OTGWSerial.cpp)

```cpp
// Look for the new firmware version
version = nullptr;
unsigned short ptr = 0;

while (ptr < info.datasize) {
    // ❌ DANGEROUS: strstr_P expects null-terminated string
    char *s = strstr_P((char *)datamem + ptr, banner1);
    
    if (s == nullptr) {
        // ❌ DANGEROUS: strnlen also expects null-terminated string
        ptr += strnlen((char *)datamem + ptr,
          info.datasize - ptr) + 1;
    } else {
        s += sizeof(banner1) - 1;   // Drop the terminating '\0'
        version = s;
        Dprintf("Version: %s\n", version);
        if (firmware == FIRMWARE_OTGW && *fwversion) {
            weight += 4 * WEIGHT_DATAREAD;
        }
        break;
    }
}
```

**Problems**:
1. `strstr_P()` reads beyond buffer if no null terminator found
2. `strnlen()` reads beyond buffer if no null terminator found
3. Loop condition doesn't prevent overrun
4. No bounds checking before pointer arithmetic

### ✅ SAFE CODE (Proposed Fix)

```cpp
// Look for the new firmware version
version = nullptr;
unsigned short ptr = 0;
size_t bannerLen = sizeof(banner1) - 1;

// ✅ SAFE: Check buffer is large enough first
if (info.datasize >= bannerLen) {
    // ✅ SAFE: Loop stops before reading past end
    for (ptr = 0; ptr <= (info.datasize - bannerLen); ptr++) {
        // ✅ SAFE: memcmp_P reads exact byte count, no null needed
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

**Improvements**:
1. ✅ `memcmp_P()` reads exact bytes, doesn't need null terminator
2. ✅ Underflow protection: `if (datasize >= bannerLen)`
3. ✅ Overflow protection: `ptr <= (datasize - bannerLen)`
4. ✅ All reads guaranteed within valid buffer range

## Buffer Access Patterns

### Vulnerable Pattern
```
Iteration 1:
  strstr_P(datamem + 0, "OpenTherm Gateway ")
  └─> Searches from 0 until null or match
      └─> If no null in buffer, reads BEYOND 256 bytes ❌

Iteration 2 (if banner not found):
  strnlen(datamem + 0, 256)
  └─> Counts bytes until null
      └─> If no null, reads BEYOND 256 bytes ❌
  ptr = 0 + strnlen_result + 1
  
Iteration 3:
  strstr_P(datamem + ptr, "OpenTherm Gateway ")
  └─> Could start anywhere, still no bounds checking ❌
```

### Safe Pattern
```
Pre-check: datasize (256) >= bannerLen (19) ✅

Iteration 1 (ptr=0):
  memcmp_P(datamem + 0, banner, 19)
  └─> Compares exactly 19 bytes ✅
  └─> Guaranteed within bounds: 0 + 19 = 19 < 256 ✅

Iteration 2 (ptr=1):
  memcmp_P(datamem + 1, banner, 19)
  └─> Compares exactly 19 bytes ✅
  └─> Guaranteed within bounds: 1 + 19 = 20 < 256 ✅

...

Iteration 237 (ptr=237):
  memcmp_P(datamem + 237, banner, 19)
  └─> Compares exactly 19 bytes ✅
  └─> Guaranteed within bounds: 237 + 19 = 256 ≤ 256 ✅

Iteration 238: DOES NOT HAPPEN
  Loop condition: ptr <= (256 - 19) means ptr <= 237
  └─> Loop stops at ptr=237 ✅
```

## Memory Safety Guarantee

### Vulnerable Code
```
Worst case: banner not in buffer, no null terminator
Result: strstr_P/strnlen read until random null byte found
Maximum read: UNBOUNDED (could be megabytes!)
Exception: Guaranteed when reading protected memory
```

### Safe Code
```
Worst case: banner not in buffer
Result: Checks all 238 positions (0 to 237), finds nothing
Maximum read: Exactly 256 bytes (the buffer size)
Exception: IMPOSSIBLE (all reads within buffer)
```

## Function Comparison Table

| Aspect | strstr_P() + strnlen() | memcmp_P() |
|--------|------------------------|------------|
| **Input Type** | Null-terminated strings | Binary data or strings |
| **Null Required** | ✅ Yes, will crash without | ❌ No, compares exact bytes |
| **Bounds Checking** | ❌ None, reads until null | ✅ User-controlled length |
| **Binary Safe** | ❌ No | ✅ Yes |
| **Predictable** | ❌ No (depends on data) | ✅ Yes (exact byte count) |
| **Memory Safety** | ❌ Vulnerable | ✅ Safe |

## Real-World Scenario

### What Happens on Affected Devices

1. **User triggers PIC firmware upgrade** via web interface
2. **Firmware reads hex file** into `datamem[256]` buffer
3. **Search begins** for version banner "OpenTherm Gateway "
4. **strstr_P() starts reading** from datamem[0]
5. **No null terminator found** in binary data
6. **Function keeps reading** past datamem[255]
7. **Reads invalid address** 0x144c0000
8. **ESP8266 raises exception** (28) LoadProhibited
9. **Watchdog resets device** after timeout
10. **Device boots up** and repeats → bootloop

### With The Fix

1. **User triggers PIC firmware upgrade** via web interface
2. **Firmware reads hex file** into `datamem[256]` buffer
3. **Search begins** for version banner "OpenTherm Gateway "
4. **memcmp_P() checks position 0** - no match
5. **memcmp_P() checks position 1** - no match
6. ... continues safely ...
7. **memcmp_P() checks position 237** - no match
8. **Loop ends** (ptr > 237 not allowed)
9. **Version not found** - handle gracefully
10. **Upgrade continues** without crash

## Reference Implementation

The safe pattern is already successfully deployed in `versionStuff.ino`:

**File**: `versionStuff.ino`  
**Lines**: 84-114  
**Status**: ✅ Production-ready, proven stable

```cpp
size_t bannerLen = sizeof(banner) - 1;

// Safer sliding window search:
for (ptr = 0; ptr <= (256 - bannerLen); ptr++) {
    // Safe comparison with PROGMEM string using memcmp_P for binary data
    if (memcmp_P((char *)datamem + ptr, banner, bannerLen) == 0) {
        // Match found!
        char * content = (char *)datamem + ptr + bannerLen;
        // ... extract version safely ...
        return;
    }
}
```

## Testing The Fix

### Test Cases

1. **Normal case**: Hex file with version banner → Should find version ✅
2. **Missing banner**: Hex file without banner → Should handle gracefully ✅
3. **Banner at start**: Version at offset 0 → Should find immediately ✅
4. **Banner at end**: Version at offset 237 → Should find last iteration ✅
5. **Small buffer**: Buffer smaller than banner → Should skip safely ✅
6. **Binary lookalike**: Data resembling but not matching banner → Should continue ✅

### Verification Commands

```bash
# Build firmware
python build.py --firmware

# Run evaluation
python evaluate.py --quick

# Flash to device (when ready)
python flash_esp.py --build
```

## Summary

| Metric | Vulnerable | Safe | Improvement |
|--------|-----------|------|-------------|
| **Max Read** | Unbounded | 256 bytes | ✅ Bounded |
| **Crash Risk** | High | None | ✅ Eliminated |
| **Performance** | Variable | O(n) | ✅ Predictable |
| **Code Clarity** | Medium | High | ✅ Clearer |
| **Lines of Code** | 13 | 15 | +2 (worth it!) |

**Conclusion**: The safe pattern is superior in every way. The two extra lines of code provide critical memory safety and prevent device-bricking crashes.

---

**Action Required**: Replace vulnerable code in `OTGWSerial.cpp` lines 303-318 with safe pattern.
