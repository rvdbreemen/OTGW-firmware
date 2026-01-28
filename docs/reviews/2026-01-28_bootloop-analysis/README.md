---
# METADATA
Document Title: Bootloop Analysis Archive - README
Review Date: 2026-01-28 23:30:00 UTC
Reviewer: GitHub Copilot Advanced Agent
Document Type: Archive Index
Status: COMPLETE
---

# Bootloop Analysis Archive

This directory contains the complete analysis and solution plan for the Wemos D1 bootloop issue reported in firmware version 0.10.0.

## Issue Summary

**Problem**: Wemos D1 mini devices reboot every ~10 seconds with Exception (2) - Access to invalid address  
**Root Cause**: Buffer overrun caused by using string functions on binary data  
**Severity**: CRITICAL - Complete device failure  
**Status**: Solution identified, ready for implementation

## Documents in This Archive

### 1. BOOTLOOP_ANALYSIS.md
**Purpose**: Complete technical root cause analysis  
**Audience**: Developers, technical reviewers  
**Contents**:
- Detailed problem description
- Root cause analysis with code examples
- Memory address analysis
- Backtrace interpretation
- Testing recommendations
- Prevention measures

### 2. SOLUTION_PLAN.md
**Purpose**: Step-by-step fix implementation guide  
**Audience**: Developers implementing the fix  
**Contents**:
- Exact code changes required
- Before/after code comparison
- Implementation steps
- Testing plan
- Risk assessment
- Additional benefits

### 3. README.md (This File)
**Purpose**: Navigation and overview  
**Audience**: All stakeholders  

## Quick Reference

### For Decision Makers
- **What happened?** Buffer overrun in hex file parsing causes device crashes
- **Impact?** Complete device failure - unusable until fixed
- **Fix complexity?** Low - single function, proven pattern
- **Risk?** Low - localized change, already proven in versionStuff.ino
- **Recommendation?** Implement immediately

### For Developers
- **File to fix**: `src/libraries/OTGWSerial/OTGWSerial.cpp`
- **Function**: `OTGWUpgrade::readHexFile()`
- **Lines**: 303-318
- **Pattern**: Replace `strstr_P()` + `strnlen()` with `memcmp_P()` sliding window
- **Reference**: `versionStuff.ino` lines 84-114 (already fixed with same pattern)

### For Testers
- **Test scenarios**: 
  1. PIC firmware upgrade via web interface
  2. Multiple PIC firmware versions (6.4, 6.5, etc.)
  3. Both PIC models (16f88 and 16f1847)
- **Success criteria**: No exceptions, version detection works
- **Monitoring**: Telnet debug output for exception messages

## Timeline of Events

- **2023-02-02**: Issue reported by users - bootloops on 0.10.0
- **2023-02-xx**: DaveDavenport reproduces crash, provides backtrace
- **2023-02-xx**: Root cause identified via backtrace analysis
- **Unknown date**: Fix applied to `versionStuff.ino`
- **2026-01-28**: Same issue identified in `OTGWSerial.cpp`, analysis documented

## Technical Details

### Exception Information
```
Exception (28): LoadProhibited
epc1=0x40241688 (in _ungetc_r)
excvaddr=0x144c0000 (invalid address - far beyond valid memory)
```

### Vulnerable Pattern
```cpp
// DANGEROUS - reads beyond buffer
char *s = strstr_P((char *)datamem + ptr, banner1);
ptr += strnlen((char *)datamem + ptr, info.datasize - ptr) + 1;
```

### Safe Pattern
```cpp
// SAFE - bounded iteration with binary-safe comparison
for (ptr = 0; ptr <= (info.datasize - bannerLen); ptr++) {
    if (memcmp_P((char *)datamem + ptr, banner1, bannerLen) == 0) {
        // Found it
    }
}
```

## Related Work

### Already Fixed
- `versionStuff.ino` - Same vulnerability, already patched

### Still Vulnerable
- `src/libraries/OTGWSerial/OTGWSerial.cpp` - Needs the same fix

### Documentation Updates
- Custom instructions updated with Binary Data Handling guidelines
- Coding standards expanded to include explicit warnings

## How to Use This Archive

### If you're investigating the issue:
1. Start with this README for overview
2. Read BOOTLOOP_ANALYSIS.md for detailed technical analysis
3. Check SOLUTION_PLAN.md for the fix approach

### If you're implementing the fix:
1. Review SOLUTION_PLAN.md for code changes
2. Reference BOOTLOOP_ANALYSIS.md for context
3. Follow the testing recommendations
4. Check the reference implementation in `versionStuff.ino`

### If you're reviewing the fix:
1. Read BOOTLOOP_ANALYSIS.md to understand the root cause
2. Verify the fix matches the pattern in SOLUTION_PLAN.md
3. Ensure bounds checking is correct
4. Confirm `memcmp_P()` is used instead of string functions

## Key Takeaways

1. **Never use string functions on binary data** - Always use `memcmp_P()` for binary buffers
2. **Always add bounds checking** - Prevent underflow with `if (size >= searchLen)`
3. **Test with real hardware** - ESP8266 exceptions may not appear in simulation
4. **Learn from patterns** - Same issue, same fix in multiple locations

## Future Prevention

This issue has been added to the project's coding standards:

- Binary data handling guidelines (MANDATORY)
- Explicit prohibition of `strstr_P()` / `strnlen()` on non-string data
- Requirement for `memcmp_P()` when comparing binary buffers
- Underflow protection checklist

## Contact

For questions about this analysis:
- Review the detailed documents in this directory
- Reference the original GitHub issue
- Check the Discord firmware chat for community discussion

---

**Archive Date**: 2026-01-28  
**Status**: Complete - Ready for implementation  
**Next Action**: Apply fix to OTGWSerial.cpp per SOLUTION_PLAN.md
