# OTGW-firmware Library Upgrade Research Report

**Date:** January 2, 2026  
**Project:** OTGW-firmware  
**Current Version:** v0.10.4-beta  

## Executive Summary

This report provides a comprehensive analysis of upgrading all libraries used in the OTGW-firmware project from their current versions to the latest available versions as of January 2026. Each library has been researched for breaking changes, compatibility issues, and required refactoring efforts.

---

## Table of Contents

1. [ESP8266 Arduino Core](#1-esp8266-arduino-core)
2. [WiFiManager](#2-wifimanager)
3. [ArduinoJson](#3-arduinojson)
4. [PubSubClient](#4-pubsubclient)
5. [TelnetStream](#5-telnetstream)
6. [AceTime](#6-acetime)
7. [OneWire](#7-onewire)
8. [DallasTemperature](#8-dallastemperature)
9. [Overall Recommendations](#overall-recommendations)
10. [Migration Strategy](#migration-strategy)

---

## 1. ESP8266 Arduino Core

### Current Version
- **2.7.4** (May 2021)

### Latest Version
- **3.1.2** (March 2023)

### Breaking Changes & Impact

#### Critical Changes:
1. **WiFiServer API Changes**
   - `WiFiServer::available()` deprecated → use `WiFiServer::accept()`
   - Impact: **LOW** - Not extensively used in OTGW-firmware

2. **Toolchain Requirements**
   - Arduino IDE 1.8.9+ mandatory
   - Updated Python and build tools
   - Impact: **MEDIUM** - Build system needs update

3. **Flash Size Agnostic Builds**
   - New build system for flash layouts
   - Impact: **LOW** - Current 4M2M layout compatible

4. **WiFiClient Changes**
   - New `WiFiClient::abort()` method
   - Remote/local IP functions revised
   - Impact: **LOW** - Minimal API usage in code

5. **Memory Management**
   - Stricter buffer/stack management
   - Memory optimizations
   - Impact: **MEDIUM** - May reveal existing issues

6. **HTTP Update Changes**
   - OTA and HTTPUpdate API changes
   - Impact: **MEDIUM** - Custom ModUpdateServer used

#### Compatibility Concerns:
- The project notes "bug in http stream, fallback to 2.7.4" in Makefile
- This suggests a known issue with HTTP streaming in ESP8266 Core 3.x
- **Risk: HIGH** - May break OTA updates or HTTP functionality

### Code Refactoring Required
- [ ] Test HTTP streaming functionality
- [ ] Verify OTA update mechanism
- [ ] Update build configuration if needed
- [ ] Test WiFi connection stability
- [ ] Review memory usage patterns

### Recommendation
**DEFER** - Stay on 2.7.4 until HTTP streaming issue is confirmed resolved. The Makefile comment indicates a deliberate decision to stay on 2.7.4 due to known bugs in 3.x.

---

## 2. WiFiManager

### Current Version
- **2.0.15-rc.1** (December 2022)

### Latest Version
- **2.0.17** (March 2024)

### Breaking Changes & Impact

#### Changes from 2.0.15-rc.1 to 2.0.17:

1. **Enhanced Callback Functions**
   - `setAPCallback`, `setSaveConfigCallback` now accept `std::function`
   - Impact: **LOW** - Existing callbacks compatible

2. **New Features**
   - CORS headers support
   - UTF-8 charset improvements
   - Custom DNS support
   - Impact: **NONE** - Backward compatible

3. **Bug Fixes**
   - HTTP_HEAD conflicts resolved
   - Memory leak fixes
   - Crash fixes in doParamSave
   - Impact: **POSITIVE** - Stability improvements

4. **New Methods**
   - `getParameters()` and `getParametersCount()`
   - Reset settings callback
   - Impact: **NONE** - Optional features

### Code Refactoring Required
- [ ] Test WiFi configuration portal
- [ ] Verify captive portal functionality
- [ ] Test parameter saving/loading
- [ ] Validate MDNS behavior

### Recommendation
**UPGRADE** - Safe upgrade with bug fixes and improvements. No breaking changes detected. The update includes important stability fixes.

---

## 3. ArduinoJson

### Current Version
- **6.17.2** (2020)

### Latest Version
- **7.4.0** (2024)

### Breaking Changes & Impact

#### MAJOR VERSION CHANGE - Version 7.x introduces significant breaking changes:

1. **JsonDocument Unification**
   - `StaticJsonDocument` and `DynamicJsonDocument` merged into `JsonDocument`
   - Now always heap-allocated with elastic capacity
   - Impact: **HIGH** - Code uses both types extensively
   
   **Current code examples found:**
   ```cpp
   DynamicJsonDocument doc(1280);  // settingStuff.ino:30
   StaticJsonDocument<1280> doc;   // settingStuff.ino:86
   StaticJsonDocument<256> doc;    // restAPI.ino:202, 232
   ```

2. **Deprecated Macros Removed**
   - `JSON_ARRAY_SIZE()`, `JSON_OBJECT_SIZE()`, `JSON_STRING_SIZE()` removed
   - Impact: **LOW** - Not used in current code

3. **API Changes**
   - `createNestedArray()` / `createNestedObject()` → `add<JsonArray>()` / `add<JsonObject>()`
   - `containsKey()` → `is<T>()`
   - Impact: **LOW** - These methods not found in current code

4. **Proxy Class Changes**
   - `MemberProxy` and `ElementProxy` now non-copyable
   - Cannot use `auto value = doc["key"]` directly
   - Must use `.as<T>()` or `.to<T>()`
   - Impact: **MEDIUM** - May affect existing code patterns

5. **Memory Overhead**
   - Version 7 has significantly larger code size
   - Impact: **HIGH** - ESP8266 has limited flash/RAM

### Code Files Affected
- `settingStuff.ino` - Settings read/write (2 instances)
- `restAPI.ino` - REST API responses (2 instances)
- Potentially other files using JSON

### Code Refactoring Required
- [ ] Replace `DynamicJsonDocument(size)` with `JsonDocument`
- [ ] Replace `StaticJsonDocument<size>` with `JsonDocument`
- [ ] Remove capacity specifications
- [ ] Test memory usage - critical for ESP8266
- [ ] Verify serialization/deserialization
- [ ] Check error handling with `overflowed()`

### Migration Example
```cpp
// Before (v6):
DynamicJsonDocument doc(1280);
StaticJsonDocument<256> doc;

// After (v7):
JsonDocument doc;  // Always heap, elastic
```

### Recommendation
**DEFER** - Major version upgrade with significant breaking changes and increased memory usage. The ESP8266 has limited resources (80KB RAM). Version 6.x is stable and sufficient. Upgrade only if specific v7 features are needed.

**Alternative:** Could upgrade to latest 6.x (6.21.5) for bug fixes without breaking changes.

---

## 4. PubSubClient

### Current Version
- **2.8.0** (May 2020)

### Latest Version
- **2.8.0** (Still current)

### Breaking Changes & Impact
- **NONE** - Already on latest version
- No updates since May 2020
- Library is stable and mature

### Code Refactoring Required
- No changes needed

### Recommendation
**NO ACTION** - Already on latest version. Library is stable with no updates required.

---

## 5. TelnetStream

### Current Version
- **1.2.4** (Early 2023)

### Latest Version
- **1.3.0** (December 2023)

### Breaking Changes & Impact

#### Changes from 1.2.4 to 1.3.0:

1. **New Dependency**
   - Now requires **NetApiHelpers** library
   - Provides print-to-all-clients support
   - Impact: **LOW** - Simple dependency addition

2. **Compatibility**
   - Enhanced support for ESP8266WiFi, ESP32, RP2040, Mbed
   - Impact: **POSITIVE** - Better platform support

### Code Refactoring Required
- [ ] Install NetApiHelpers library
- [ ] Test telnet debug output
- [ ] Verify multi-client scenarios

### Recommendation
**UPGRADE** - Safe upgrade with minimal impact. New dependency is lightweight. Provides better multi-client support.

---

## 6. AceTime

### Current Version
- **2.0.1** (2022/2023)

### Latest Version
- **4.1.0** (November 2025)

### Breaking Changes & Impact

#### Major Version Changes (2.x → 3.x → 4.x):

1. **TZDB Updates**
   - Version 3.0.0: TZDB 2025a/2025b
   - Version 4.x: Latest TZDB with expanded coverage
   - Impact: **LOW** - Timezone data improvements

2. **API Compatibility**
   - Core API remains stable across versions
   - Impact: **LOW** - Minimal code changes expected

3. **Performance Improvements**
   - Better memory efficiency
   - Optimizations for 8-bit and 32-bit MCUs
   - Impact: **POSITIVE** - Better for ESP8266

4. **Enhanced Zone Support**
   - More timezones and links
   - Improved DST handling
   - Impact: **POSITIVE** - Better accuracy

### Code Files Using AceTime
- `OTGW-firmware.h` - Core time management
- Timezone conversion and NTP sync code

### Code Refactoring Required
- [ ] Test timezone conversions
- [ ] Verify NTP sync behavior
- [ ] Test DST transitions
- [ ] Check memory usage

### Recommendation
**UPGRADE** - Safe upgrade path from 2.0.1 to 4.1.0. API is stable, improvements are mainly in timezone data and performance. The library maintainer provides good backward compatibility.

**Suggested Path:** 2.0.1 → 2.4.0 (stable 2.x) → 4.1.0 (latest)

---

## 7. OneWire

### Current Version
- **2.3.6** (November 2021)

### Latest Version
- **2.3.8** (February 2024)

### Breaking Changes & Impact

#### Changes from 2.3.6 to 2.3.8:

1. **ESP32 Core 3.0.0 Support**
   - Updated for ESP32 core 3.0.0
   - Impact: **NONE** - Project uses ESP8266

2. **Compatibility Updates**
   - Minor maintenance updates
   - Impact: **LOW** - Bug fixes only

### Code Refactoring Required
- [ ] Test Dallas temperature sensors
- [ ] Verify sensor detection
- [ ] Check reading accuracy

### Recommendation
**UPGRADE** - Safe minor version upgrade with bug fixes. No breaking changes. Minimal testing required.

---

## 8. DallasTemperature

### Current Version
- **3.9.0** (2020)

### Latest Version
- **4.0.5** (September 2025)

### Breaking Changes & Impact

#### Major Version Change (3.x → 4.x):

1. **Enhanced Device Support**
   - Added DS1825, DS28EA00, MAX31850
   - Impact: **POSITIVE** - More sensors supported

2. **Improved Error Handling**
   - Better error codes for device faults
   - Enhanced detection of open/short circuits
   - Impact: **POSITIVE** - Better diagnostics

3. **API Enhancements**
   - Better asynchronous operations
   - Configurable conversion settings per sensor
   - Impact: **LOW** - Backward compatible

4. **Initialization Changes**
   - Improved timeout handling
   - Better scratchpad operations
   - Impact: **LOW** - Internal improvements

### Code Files Using DallasTemperature
- `OTGW-firmware.h` - Library include
- `sensors_ext.ino` - Temperature sensor handling

### Code Refactoring Required
- [ ] Test temperature sensor initialization
- [ ] Verify sensor readings
- [ ] Test error handling
- [ ] Check conversion timing

### Recommendation
**UPGRADE** - Safe major version upgrade. API is backward compatible with enhancements. Better error handling and device support.

---

## Overall Recommendations

### Priority Upgrades (Low Risk, High Benefit)

1. **WiFiManager: 2.0.15-rc.1 → 2.0.17**
   - Risk: LOW
   - Benefit: Bug fixes, stability improvements
   - Action: UPGRADE

2. **TelnetStream: 1.2.4 → 1.3.0**
   - Risk: LOW
   - Benefit: Better multi-client support
   - Action: UPGRADE
   - Note: Requires NetApiHelpers dependency

3. **OneWire: 2.3.6 → 2.3.8**
   - Risk: LOW
   - Benefit: Bug fixes
   - Action: UPGRADE

4. **DallasTemperature: 3.9.0 → 4.0.5**
   - Risk: LOW
   - Benefit: Better error handling, more device support
   - Action: UPGRADE

5. **AceTime: 2.0.1 → 4.1.0**
   - Risk: LOW-MEDIUM
   - Benefit: Better timezone data, performance
   - Action: UPGRADE
   - Note: Test thoroughly

### Deferred Upgrades (High Risk or Low Benefit)

1. **ESP8266 Arduino Core: 2.7.4 → 3.1.2**
   - Risk: HIGH
   - Reason: Known HTTP streaming bug (noted in Makefile)
   - Action: DEFER
   - Alternative: Monitor for 3.2.x releases with fixes

2. **ArduinoJson: 6.17.2 → 7.4.0**
   - Risk: HIGH
   - Reason: Major version with breaking changes, larger code size
   - Action: DEFER
   - Alternative: Upgrade to 6.21.5 (latest v6) for bug fixes

### No Action Required

1. **PubSubClient: 2.8.0**
   - Already on latest version
   - Action: NONE

---

## Migration Strategy

### Phase 1: Low-Risk Libraries (Immediate)
1. WiFiManager 2.0.15-rc.1 → 2.0.17
2. OneWire 2.3.6 → 2.3.8
3. Test build and basic functionality

### Phase 2: Medium-Risk Libraries (After Phase 1 Success)
1. TelnetStream 1.2.4 → 1.3.0 (+ NetApiHelpers)
2. DallasTemperature 3.9.0 → 4.0.5
3. Test telnet debug and temperature sensors

### Phase 3: Time Library (After Phase 2 Success)
1. AceTime 2.0.1 → 4.1.0
2. Extensive testing of timezone, NTP, DST

### Phase 4: High-Risk Libraries (Future Consideration)
1. Monitor ESP8266 Core for HTTP streaming fixes
2. Consider ArduinoJson 6.21.5 (minor update) vs 7.x (major)
3. Only upgrade when compelling need exists

### Testing Checklist for Each Phase
- [ ] Build succeeds without errors
- [ ] OTA update works
- [ ] Web UI accessible and functional
- [ ] MQTT connection and publishing works
- [ ] WiFi connection and reconnection works
- [ ] Telnet debug output works
- [ ] Dallas temperature sensors detected and read
- [ ] NTP time sync works
- [ ] Timezone conversion works
- [ ] Settings save/load works
- [ ] REST API endpoints work
- [ ] Memory usage acceptable (heap, stack)

---

## Estimated Refactoring Effort

| Library | Version Change | Lines of Code Affected | Effort (Hours) | Risk |
|---------|---------------|------------------------|----------------|------|
| WiFiManager | 2.0.15-rc.1 → 2.0.17 | 0-5 | 0.5 | LOW |
| OneWire | 2.3.6 → 2.3.8 | 0 | 0.25 | LOW |
| TelnetStream | 1.2.4 → 1.3.0 | 0 | 0.5 | LOW |
| DallasTemperature | 3.9.0 → 4.0.5 | 0-10 | 1.0 | LOW |
| AceTime | 2.0.1 → 4.1.0 | 0-20 | 2.0 | MEDIUM |
| ArduinoJson | 6.17.2 → 7.4.0 | 50-100 | 8.0 | HIGH |
| ESP8266 Core | 2.7.4 → 3.1.2 | 10-50 | 4.0 | HIGH |
| **Total (Recommended)** | | **0-35** | **4.25** | **LOW-MEDIUM** |
| **Total (All)** | | **60-185** | **16.25** | **HIGH** |

---

## Conclusion

The recommended upgrade path focuses on low-risk, high-benefit library updates that provide stability improvements, bug fixes, and enhanced functionality without major code refactoring. 

**Immediate Action:**
- Upgrade 4 libraries (WiFiManager, TelnetStream, OneWire, DallasTemperature)
- Upgrade AceTime with thorough testing
- Total effort: ~4.25 hours
- Risk: LOW to MEDIUM

**Deferred Action:**
- ESP8266 Core: Wait for HTTP streaming fix
- ArduinoJson: Stay on v6 or upgrade to 6.21.5 for fixes only

This strategy minimizes risk while providing meaningful improvements to the firmware stability and capabilities.

---

**Report Prepared By:** GitHub Copilot Agent  
**Date:** January 2, 2026  
**Status:** Research Complete, Ready for Implementation
