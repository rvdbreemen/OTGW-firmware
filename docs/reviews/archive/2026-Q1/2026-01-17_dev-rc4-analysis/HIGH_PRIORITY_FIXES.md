---
# METADATA
Document Title: High Priority Fixes for dev-RC4-branch
Review Date: 2026-01-17 10:26:28 UTC
Branch Reviewed: dev-rc4-branch → dev (merge commit 9f918e9)
Target Version: v1.0.0-rc4
Reviewer: GitHub Copilot Advanced Agent
Document Type: Fix Suggestions
PR Branch: copilot/review-dev-rc4-branch
Commit: 575f92b
Status: Issue #3 ready for implementation, Issue #4 kept as-is per developer request
---

# High Priority Issue Fixes - Detailed Suggestions

This document provides concrete, actionable fixes for the **High Priority (P1)** issues identified in the dev-RC4-branch code review.

**UPDATE:** Per developer feedback, Issue #4 (Legacy Format) should be **KEPT** as-is. Only Issue #3 (PROGMEM) will be addressed.

---

## Issue #3: PROGMEM Violations ⚠️

**Priority:** P1 (High)  
**Estimated Time:** 1 hour  
**Impact:** Wasting ~100+ bytes RAM on ESP8266

### Files Affected:
- `versionStuff.ino`
- `MQTTstuff.ino`  
- `sensors_ext.ino`

### Problem:
String literals are stored in RAM instead of flash memory, wasting precious ESP8266 RAM.

---

### Fix #1: versionStuff.ino

**Current code (INCORRECT):**
```cpp
// Line ~85-99 in versionStuff.ino
while (ptr < 256) {
  char *s = strstr((char *)datamem + ptr, banner);  // ❌ banner in RAM
  if (!s) {
    ptr += strnlen((char *)datamem + ptr, 256 - ptr) + 1;
  } else {
    s += sizeof(banner) - 1;
    strlcpy(version, s, destSize);
    return;
  }
}
```

**Fixed code (CORRECT):**
```cpp
// Use PROGMEM-aware string functions
const char banner[] PROGMEM = "OpenTherm Gateway ";  // Ensure banner is PROGMEM

// Change strstr to use memcmp_P for binary data (also fixes critical issue #1)
size_t bannerLen = sizeof(banner) - 1;
if (256 >= bannerLen) {
  for (ptr = 0; ptr <= (256 - bannerLen); ptr++) {
    // Use memcmp_P for PROGMEM comparison on binary data
    if (memcmp_P((char *)datamem + ptr, banner, bannerLen) == 0) {
      char *content = (char *)datamem + ptr + bannerLen;
      size_t maxContentLen = 256 - (ptr + bannerLen);
      size_t vLen = 0;
      while(vLen < maxContentLen && vLen < (destSize - 1)) {
        char c = content[vLen];
        if (c == '\0' || !isprint(c)) break;
        vLen++;
      }
      memcpy(version, content, vLen);
      version[vLen] = '\0';
      return;
    }
  }
}
DebugTf(PSTR("GetVersion: banner not found in %s\r\n"), hexfile);
```

**What changed:**
1. ✅ Ensure `banner` uses `PROGMEM` keyword
2. ✅ Use `memcmp_P()` instead of `strstr()` for PROGMEM-aware comparison
3. ✅ Use `PSTR()` macro in Debug statement
4. ✅ This also fixes Critical Issue #1 (binary data safety)

**RAM Saved:** ~20 bytes

---

### Fix #2: sensors_ext.ino

**Current code (INCORRECT):**
```cpp
// Line ~141 in sensors_ext.ino
if (bDebugSensors) DebugTf(PSTR("Sensor device no[%d] addr[%s] TempC: %f\r\n"), i, strDeviceAddress, DallasrealDevice[i].tempC);
```

This one is already correct! ✅

**Check other Debug statements:**
```cpp
// Ensure all Debug macros use PSTR():
DebugTf(PSTR("Format string here\r\n"), args);
DebugTln(F("Simple string here"));
```

**Audit command:**
```bash
# Find Debug statements without PSTR/F macros
grep -n 'Debug.*("' sensors_ext.ino versionStuff.ino MQTTstuff.ino | grep -v 'PSTR\|F('
```

If any are found, wrap string literals in `PSTR()` for formatted strings or `F()` for simple strings.

**RAM Saved:** ~10-20 bytes per fix

---

### Fix #3: MQTTstuff.ino

**Audit the file:**
```bash
# Search for string literals not using PROGMEM
grep -n '"[^"]*"' MQTTstuff.ino | grep -v 'PSTR\|F(' | head -20
```

**Common patterns to fix:**

**Pattern 1: Debug statements**
```cpp
// BEFORE (WRONG):
MQTTDebugTf("Message: %s\r\n", msg);

// AFTER (CORRECT):
MQTTDebugTf(PSTR("Message: %s\r\n"), msg);
```

**Pattern 2: snprintf calls**
```cpp
// BEFORE (WRONG):
snprintf(buffer, size, "Format: %d", value);

// AFTER (CORRECT):
snprintf_P(buffer, size, PSTR("Format: %d"), value);
```

**Pattern 3: String comparisons**
```cpp
// BEFORE (WRONG):
if (strcmp(field, "value") == 0)

// AFTER (CORRECT):
if (strcasecmp_P(field, PSTR("value")) == 0)
```

**RAM Saved:** ~50-100 bytes across all fixes

---

### Verification Steps:

1. **Build and check flash usage:**
```bash
python build.py --firmware
# Note the "Program storage" and "RAM usage" before and after
```

2. **Expected result:**
   - Flash usage: May increase slightly (strings moved to flash)
   - RAM usage: Should decrease by 100+ bytes
   
3. **Test on hardware:**
   - All functionality should work identically
   - Debug output should be unchanged

---

## Issue #4: Legacy Format Technical Debt ⚠️

**Priority:** P1 (High) → **DECISION: KEEP AS-IS**  
**Developer Feedback:** Legacy sensor format support is required for backward compatibility  
**Status:** No changes needed - feature will be maintained

### Files Affected:
- `sensors_ext.ino` (lines 161-189)
- `OTGW-firmware.h` (line 155)
- `settingStuff.ino` (lines 57, 148, 289-295)
- `data/index.js` (line 2021)

### Original Concern:
The code intentionally replicates a bug from v0.10.x to maintain backward compatibility with incorrectly formatted Dallas sensor IDs.

---

### Developer Decision: KEEP Legacy Mode

**Rationale provided by @rvdbreemen:**
Legacy sensor format support is essential for users upgrading from v0.10.x. The backward compatibility feature should be maintained.

**Impact of keeping legacy format:**
1. ✅ Users can opt-in to legacy format for backward compatibility
2. ✅ No breaking changes for existing Home Assistant setups
3. ✅ Smooth upgrade path from v0.10.x
4. ⚠️ Maintains 35 lines of bug emulation code (accepted technical debt)
5. ⚠️ Two code paths to maintain (legacy + correct format)

**Recommendation:** 
Document the `settingGPIOSENSORSlegacyformat` setting in user documentation to help users understand when to use it.

**No code changes required for Issue #4.**

---

## Summary of Changes

### Issue #3: PROGMEM Violations
**Files Modified:** 3 (versionStuff.ino, sensors_ext.ino, MQTTstuff.ino)  
**Lines Changed:** ~10-15 locations  
**RAM Saved:** ~100+ bytes  
**Risk:** LOW (no functional changes)  
**Testing:** Build verification, basic functionality test
**Status:** ✅ Ready to implement

### Issue #4: Legacy Format 
**Status:** ❌ NOT CHANGING - Keep as-is per developer request
**Rationale:** Legacy sensor format support is required for backward compatibility
**Files:** No changes
**Impact:** Feature maintained, 35 lines of legacy code preserved

---

## Recommended Implementation Order

1. **Implement Issue #3 (PROGMEM)** - Low risk, immediate RAM benefit
   - Apply fixes to versionStuff.ino (also fixes critical issue #1)
   - Apply fixes to MQTTstuff.ino
   - Audit sensors_ext.ino
   - Test build and basic functionality

2. **Skip Issue #4** - Keep legacy format support
   - No changes needed
   - Consider documenting `settingGPIOSENSORSlegacyformat` in user guide

3. **Testing**
   - Full functionality test on hardware
   - Verify RAM usage improvement
   - Confirm no regressions
   - Verify both legacy and standard sensor formats work

---

## Expected Results

### After Issue #3 Fix:
- ✅ RAM usage decreased by 100+ bytes
- ✅ All Debug output unchanged
- ✅ Critical binary parsing issue also resolved
- ✅ No functional changes for users

### Issue #4 (Legacy Format):
- ✅ Legacy format support maintained
- ✅ Users upgrading from v0.10.x have smooth migration path
- ✅ Both legacy and standard formats available via setting

### Overall:
- **Code Quality:** Improved (PROGMEM violations fixed)
- **RAM Efficiency:** 100+ bytes saved
- **Maintainability:** Better (fixed critical + high priority issue)
- **User Impact:** None (no breaking changes)
- **Backward Compatibility:** Preserved via legacy format setting

---

## Next Steps

**Decision Made:** Implement Issue #3 only, keep Issue #4 as-is.

When ready to proceed:
1. Apply PROGMEM fixes to versionStuff.ino, MQTTstuff.ino, sensors_ext.ino
2. Test build and functionality
3. Verify RAM savings
4. Confirm binary parsing safety is restored

**Awaiting go-ahead to implement Issue #3 fixes.** 🚀
