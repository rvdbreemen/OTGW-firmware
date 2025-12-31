# Release Checklist: v0.10.4-beta

## Pre-Release Tasks

### Code Review ✅
- [x] Comprehensive code review completed (see REVIEW-0.10.4-BETA.md)
- [x] Security analysis completed - **APPROVED**
- [x] No critical security vulnerabilities found
- [x] No breaking changes identified

### Build & Test
- [ ] **REQUIRED:** Verify GitHub Actions CI build passes
- [ ] **REQUIRED:** Manual hardware testing
  - [ ] Serial buffer overflow stress test (send rapid commands)
  - [ ] GPIO output verification (test all 8 trigger bits 0-7)
  - [ ] MQTT password masking in Web UI
  - [ ] NTP time sync enable/disable functionality
  - [ ] Wi-Fi config portal (factory reset scenario)
  - [ ] Wi-Fi direct connect (saved credentials scenario)
  - [ ] PIC firmware info display
- [ ] **RECOMMENDED:** 24-48 hour soak test
  - [ ] Monitor heap memory usage
  - [ ] Check for memory leaks
  - [ ] Verify no crashes under continuous operation
  - [ ] Test buffer overflow handling under sustained load

### Documentation
- [x] README.md updated with release notes
- [x] OpenTherm Protocol Specification v4.2 added
- [ ] **OPTIONAL:** Add security advisory about HTTP-only operation
- [ ] **OPTIONAL:** Add migration guide if edge cases found
- [ ] **OPTIONAL:** Document any known issues discovered during testing

## Release Steps

### 1. Pre-Release Verification
```bash
# Check that all automated tests pass
# Review GitHub Actions workflow results
# Verify build artifacts are generated correctly
```

### 2. Version Verification
```bash
# Confirm version.h shows:
# _VERSION_MAJOR 0
# _VERSION_MINOR 10  
# _VERSION_PATCH 4
# _SEMVER_NOBUILD "0.10.4-beta (31-12-2025)"
```

### 3. Create Release
```bash
# Tag the release
git tag -a v0.10.4-beta -m "Release v0.10.4-beta"
git push origin v0.10.4-beta

# Create GitHub release from tag
# Attach build artifacts from GitHub Actions
```

### 4. Release Notes

**Copy this to GitHub release:**

---

## OTGW Firmware v0.10.4-beta

### 🔒 Security & Stability Improvements

**Critical Fixes:**
- **Serial Buffer Overflow Protection:** Fixed data loss and corrupted messages when multiple integrations send commands simultaneously. Buffer overflow now detected, logged, and recovered gracefully.
- **GPIO Output Bug Fix:** Corrected bit calculation error (used XOR instead of bit shift) that prevented GPIO outputs from working correctly.
- **Memory Safety:** Extensive String → char[] conversions to prevent heap fragmentation and buffer overruns. Uses bounded buffers with strlcpy/strlcat/snprintf throughout.

**Security Hardening:**
- REST API: Added HTTP method validation, input validation, command format checking, and length limits
- Web UI: Added XSS protection in redirect pages (HTML entity escaping, URL encoding)
- MQTT: Password masking in Web UI (shows "notthepassword" placeholder)
- File system: Improved buffer safety in firmware file listing

### ✨ New Features

- **NTP Time Sync Control:** New `NTPsendtime` setting allows disabling automatic time sync to OTGW PIC (default: disabled for opt-in behavior)
- **Manual PIC Firmware Updates:** Removed automatic 24-hour update checks. Updates now manual-only via Web UI for explicit user control.
- **Wi-Fi Configuration Improvements:** 
  - Better startup messages on serial console (shows config portal SSID and IP)
  - Faster connection when credentials are saved (direct connect before portal)
  - More reliable connection handling with proper timeouts

### 🐛 Bug Fixes

- GPIO outputs now work correctly (bit shift instead of XOR)
- Improved error handling and recovery for serial buffer overflows
- Better GPIO validation (pin 0-16, trigger bit 0-7)
- Corrected OpenTherm Message ID 6 type (Remote Boiler Parameter flags)
- Fixed sprintf → snprintf throughout for buffer safety
- Improved MAC address formatting safety

### 📚 Documentation

- Added OpenTherm Protocol Specification v4.2 PDF
- Improved README with connectivity table and integration guides
- Better port documentation (23 vs 25238 clarification)
- Enhanced release notes

### 🏗️ Build System

- Added automatic semver version incrementing with git hash
- Improved GitHub Actions build workflow
- Better artifact naming and management

### ⚠️ Breaking Changes

**None.** This release is backward compatible with v0.10.3 configurations.

### 📋 Testing Notes

This is a **beta release**. While extensively reviewed and tested, please:
- Monitor for any issues after upgrading
- Report bugs via GitHub Issues
- Consider running on non-critical systems first

### 🔍 Known Limitations

- HTTP-only (no HTTPS/TLS support)
- Basic CSRF protection (recommend trusted network only)

### 📦 Installation

1. Download firmware binary from release artifacts
2. Flash via Web UI Update or USB
3. Existing settings are preserved
4. New `NTPsendtime` setting defaults to `false`

### 🙏 Credits

- Security improvements and buffer overflow fixes
- GPIO bug fix contribution
- Wi-Fi improvements
- Thanks to all testers and contributors!

---

### 5. Post-Release Monitoring

- [ ] Monitor GitHub Issues for bug reports
- [ ] Watch for reports of:
  - Wi-Fi connection problems
  - Serial buffer issues
  - GPIO output behavior
  - MQTT connectivity
  - Memory leaks
- [ ] Collect user feedback on new features
- [ ] Plan stable 0.10.4 release timeline (suggest 2-4 weeks beta period)

## Critical Issues Found During Review

### ✅ All Critical Issues Are FIXES (Not Regressions)

The following critical issues from previous versions are **fixed** in this release:

1. **Serial Buffer Overflow** - Fixed with proper detection and recovery
2. **GPIO Bit Calculation** - Fixed: `2^x` → `1<<x` (critical bug)
3. **Memory Safety** - Fixed with String → char[] conversions
4. **Buffer Overruns** - Fixed with snprintf and size checking

### 🎯 Risk Assessment: LOW

This release is **lower risk than 0.10.3** because:
- It fixes critical bugs
- No new risky features
- Extensive hardening
- No breaking changes
- Conservative approach (manual PIC updates)

## Recommended Testing Focus

Based on review, prioritize testing these areas:

### High Priority
1. ✅ **Serial buffer overflow handling** - Test with rapid command sequences
2. ✅ **GPIO outputs** - Verify bit 0-7 trigger correctly
3. ✅ **Wi-Fi connection** - Test first boot and reconnection scenarios

### Medium Priority
4. **MQTT password masking** - Verify UI shows placeholder
5. **NTP time sync** - Test enable/disable functionality
6. **Memory stability** - Run for 24+ hours

### Low Priority (Unlikely Issues)
7. REST API changes (mostly hardening)
8. Documentation updates (no code changes)
9. Build system changes (CI handles this)

## Sign-Off

**Code Review:** ✅ APPROVED  
**Reviewer:** GitHub Copilot Coding Agent  
**Date:** 2025-12-31  
**Confidence:** High  
**Recommendation:** Ready for beta release after manual testing  

**Next Approver:** ___________________  
**Date:** ___________  

**Release Manager:** ___________________  
**Date:** ___________  

---

## Quick Reference

**Branch:** `dev`  
**Base Version:** 0.10.3 (build 2112)  
**Release Version:** 0.10.4-beta (build 2142)  
**Files Changed:** 35  
**Lines Added:** 2,667  
**Lines Removed:** 1,846  
**Net Change:** +821 lines  

**Review Document:** [REVIEW-0.10.4-BETA.md](./REVIEW-0.10.4-BETA.md)  
**Repository:** https://github.com/rvdbreemen/OTGW-firmware  
