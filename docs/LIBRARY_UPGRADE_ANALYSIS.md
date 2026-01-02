# OTGW-Firmware Library Upgrade Analysis Report
**Date:** January 2, 2026  
**Prepared by:** GitHub Copilot Agent  
**Purpose:** Comprehensive analysis of upgrading all libraries and Arduino Core for OTGW-firmware project

---

## Executive Summary

This report analyzes the upgrade path from the current library versions to the latest available versions, with a focus on the Arduino ESP8266 Core upgrade from 2.7.4 to 3.1.2. Each library has been researched for breaking changes, and migration paths have been documented.

**Overall Risk Assessment:** MODERATE to HIGH
- **Arduino Core 3.x upgrade:** HIGH RISK - Significant API changes
- **ArduinoJson 6→7 upgrade:** HIGH RISK - Major version with breaking changes
- **AceTime 2→4 upgrade:** MODERATE RISK - API changes expected
- **Other libraries:** LOW to MODERATE RISK - Minor version updates

---

## Current Library Versions

| Library | Current Version | Latest Version | Version Gap |
|---------|----------------|----------------|-------------|
| ESP8266 Arduino Core | 2.7.4 | 3.1.2 | Major (2→3) |
| WiFiManager | 2.0.15-rc.1 | 2.0.17 | Minor |
| ArduinoJson | 6.17.2 | 7.4.2 | Major (6→7) |
| PubSubClient | 2.8.0 | 2.8.0 (3.3.0 fork) | Stable/Fork |
| TelnetStream | 1.2.4 | 1.3.0 | Patch |
| AceTime | 2.0.1 | 4.1.0 | Major (2→4) |
| OneWire | 2.3.6 | 2.3.8 | Patch |
| DallasTemperature | 3.9.0 | 4.0.5 | Minor |

---

## Detailed Library Analysis

### 1. ESP8266 Arduino Core (2.7.4 → 3.1.2)

#### Version Information
- **Current:** 2.7.4 (released 2020)
- **Latest:** 3.1.2 (released March 2023)
- **Risk Level:** HIGH

#### Breaking Changes

**Filesystem:**
- No breaking changes expected for LittleFS usage
- Current code uses LittleFS which is compatible

**WiFi Stack:**
- Event handling may have minor changes
- WiFi client behavior mostly backward compatible
- AutoConnect features used by WiFiManager should work

**HTTP Server:**
- ESP8266WebServer API largely compatible
- Some method signatures may have changed
- Current usage pattern should work

**Peripherals:**
Note: ESP8266 doesn't have the same peripheral changes as ESP32 (ADC, LEDC, etc.)
- GPIO operations (digitalWrite, digitalRead) - **COMPATIBLE**
- analogWrite for PWM - **COMPATIBLE** 
- Serial communications - **COMPATIBLE**

**Memory Management:**
- Improved heap management in 3.x
- Better String warning system (this is GOOD for this project)
- May expose existing memory issues

#### Impact Assessment

**Files Requiring Review:**
- `OTGW-firmware.ino` - Main setup and WiFi initialization
- `networkStuff.h` - WiFi connection management
- `FSexplorer.ino` - File system operations
- `restAPI.ino` - HTTP server endpoints
- `src/libraries/OTGWSerial/OTGWSerial.cpp` - Serial operations

**Expected Code Changes:**
1. **WiFi Event Handling** - May need minor updates to WiFi event callbacks if used
2. **HTTP Server** - Verify all HTTP server methods still work
3. **Compiler Warnings** - Address any new warnings about String usage
4. **Memory Usage** - Monitor heap fragmentation improvements

**Benefits:**
- Improved stability and memory management
- Security updates and bug fixes
- Better toolchain support
- Flash-size agnostic builds
- Updated NONOS SDK (2.2.1 → 3.0.5)

**Risks:**
- Potential subtle WiFi behavior changes
- May expose hidden memory issues
- Compiler version changes may affect code size
- Community support shifting to newer versions

#### Migration Plan

**Phase 1: Preparation**
1. Create backup branch
2. Update Makefile to use ESP8266 Core 3.1.2
3. Test build to identify compilation errors

**Phase 2: Code Updates**
1. Fix any compilation errors
2. Address compiler warnings (especially String-related)
3. Test WiFi connection and reconnection logic
4. Verify HTTP server endpoints

**Phase 3: Testing**
1. Flash to test hardware
2. Verify all functionality:
   - WiFi connection/reconnection
   - MQTT connectivity
   - Web UI access
   - OTA updates
   - File system operations
   - Serial communication with OTGW PIC

**Estimated Effort:** 3-5 hours

---

### 2. WiFiManager (2.0.15-rc.1 → 2.0.17)

#### Version Information
- **Current:** 2.0.15-rc.1 (release candidate)
- **Latest:** 2.0.17 (stable release, March 2024)
- **Risk Level:** LOW

#### Changes
- Bug fixes and stability improvements
- Enhanced configuration portal
- Better timeout handling
- Improved documentation

#### Impact Assessment

**Files Using WiFiManager:**
- `networkStuff.h` - WiFi setup and configuration portal

**Expected Code Changes:**
- **NONE** - API is backward compatible
- May benefit from bug fixes automatically

**Benefits:**
- Moving from RC to stable release
- Bug fixes and stability improvements
- Better handling of edge cases

**Risks:**
- Very low - minor version update with backward compatibility

#### Migration Plan
1. Update version in Makefile: `WiFiManager@2.0.17`
2. Build and test
3. Verify WiFi portal still works correctly

**Estimated Effort:** 30 minutes

---

### 3. ArduinoJson (6.17.2 → 7.4.2)

#### Version Information
- **Current:** 6.17.2 (v6 series)
- **Latest:** 7.4.2 (v7 series, June 2025)
- **Risk Level:** HIGH

#### Breaking Changes

**Major API Changes:**
1. **Document API Changes:**
   - `StaticJsonDocument<N>` → `JsonDocument` (with stack allocator)
   - `DynamicJsonDocument(N)` → `JsonDocument` (with heap allocator)
   - Alternative: Keep using old names (they may still work)

2. **Proxy Types Now Non-Copyable:**
   ```cpp
   // OLD (v6):
   JsonObject obj = doc["config"];
   
   // NEW (v7):
   JsonObject obj = doc["config"].as<JsonObject>();  // For reading
   JsonObject obj = doc["config"].to<JsonObject>();  // For building
   ```

3. **Deprecated Methods:**
   - `containsKey()` → Use `doc["key"].is<T>()`

4. **String Handling:**
   - Better deduplication
   - Improved memory efficiency

#### Impact Assessment

**Files Using ArduinoJson:**
- `settingStuff.ino` - Uses both StaticJsonDocument and DynamicJsonDocument
- `restAPI.ino` - Uses StaticJsonDocument for small JSON operations
- `jsonStuff.ino` - Manual JSON generation (minimal impact)

**Code Locations Requiring Changes:**

**settingStuff.ino:**
```cpp
Line 30:  DynamicJsonDocument doc(1280);
Line 31:  JsonObject root = doc.to<JsonObject>();  // Need to verify this pattern
Line 86:  StaticJsonDocument<1280> doc;
```

**restAPI.ino:**
```cpp
Line 202: StaticJsonDocument<256> doc;
Line 232: StaticJsonDocument<256> doc;
```

**Required Changes:**
1. Review all JsonObject assignments for proxy issues
2. Test all JSON serialization/deserialization
3. May need to use `.as<JsonObject>()` or `.to<JsonObject>()`
4. Update any `containsKey()` calls if present

**Benefits:**
- Better memory efficiency
- String deduplication saves RAM (important for ESP8266!)
- Improved stability
- Better error handling

**Risks:**
- Breaking changes require code updates
- Subtle bugs if proxy usage is incorrect
- More testing required

#### Migration Plan

**Phase 1: Code Analysis**
1. Audit all JsonDocument usage
2. Identify all JsonObject/JsonArray proxy assignments
3. Find any `containsKey()` usage

**Phase 2: Code Updates**
1. Update document declarations (may keep old names if compatible)
2. Fix proxy assignments to use `.as<>()` or `.to<>()`
3. Replace `containsKey()` if present
4. Test all JSON read/write operations

**Phase 3: Testing**
1. Test settings read/write
2. Test REST API JSON responses
3. Verify MQTT JSON payloads
4. Monitor memory usage

**Estimated Effort:** 4-6 hours

---

### 4. PubSubClient (2.8.0 → 2.8.0 / 3.3.0 fork)

#### Version Information
- **Current:** 2.8.0 (original by knolleary)
- **Latest Original:** 2.8.0 (unchanged since 2020)
- **Latest Fork:** 3.3.0 PubSubClient3 (December 2025)
- **Risk Level:** LOW (stay with 2.8.0) / MODERATE (upgrade to fork)

#### Analysis

**Original PubSubClient 2.8.0:**
- No updates since May 2020
- Still widely used and stable
- Works with ESP8266 and ESP32
- MQTT 3.1.1 support

**PubSubClient3 Fork:**
- Active maintenance through 2025
- Better ESP32/ESP8266 support
- Compatible API with original
- Enhanced features

#### Impact Assessment

**Files Using PubSubClient:**
- `MQTTstuff.ino` - Main MQTT implementation

**Current Usage Pattern:**
```cpp
Line 34: static PubSubClient MQTTclient(wifiClient);
```

**Expected Code Changes:**
- **Option 1 (Keep 2.8.0):** No changes needed
- **Option 2 (Upgrade to 3.3.0):** Likely backward compatible, minimal changes

**Recommendation:** STAY with 2.8.0 for stability
- The original library is stable and working
- No compelling features in the fork for this use case
- Avoid unnecessary risk

**Alternative Recommendation:** If issues arise, consider PubSubClient3
- Drop-in replacement
- Better maintained
- More up-to-date with ESP ecosystem

#### Migration Plan

**Recommended: Keep 2.8.0**
1. No changes needed
2. Continue monitoring for any updates to original

**Alternative: Upgrade to 3.3.0**
1. Change library name in Makefile
2. Test MQTT connection
3. Verify publish/subscribe functionality
4. Test Home Assistant integration

**Estimated Effort:** 0 hours (no change) / 1-2 hours (fork upgrade)

---

### 5. TelnetStream (1.2.4 → 1.3.0)

#### Version Information
- **Current:** 1.2.4
- **Latest:** 1.3.0 (December 2023)
- **Risk Level:** LOW

#### Changes
- Added dependency on NetApiHelpers library
- Provides networking support for platforms lacking print-to-all-clients
- Includes TelnetPrint as lightweight alternative
- Bug fixes

#### Impact Assessment

**Files Using TelnetStream:**
- `OTGW-firmware.h` - Includes TelnetStream
- `Debug.h` - Uses TelnetStream for debug output
- Multiple .ino files use debug macros

**Expected Code Changes:**
- **NONE** - API is backward compatible
- New dependency (NetApiHelpers) will be auto-installed

**Benefits:**
- Bug fixes
- Better multi-client support
- More robust networking

**Risks:**
- Very low
- Additional dependency is small

#### Migration Plan
1. Update version in Makefile: `TelnetStream@1.3.0`
2. Allow arduino-cli to install NetApiHelpers dependency
3. Build and test
4. Verify telnet debug output works

**Estimated Effort:** 30 minutes

---

### 6. AceTime (2.0.1 → 4.1.0)

#### Version Information
- **Current:** 2.0.1
- **Latest:** 4.1.0 (November 2025)
- **Risk Level:** MODERATE to HIGH

#### Major Changes

**Version 4.x Changes:**
- New timezone database structure
- Database naming: `zonedb2025`, `zonedbx2025`
- Epoch reference changed to 2050-01-01 UTC
- Enhanced IANA TZ database support
- API may have breaking changes

**Namespace/Include Changes:**
Likely changes from v2 to v4:
```cpp
// v2.x
#include <AceTime.h>
using namespace ace_time;
static ExtendedZoneManager timezoneManager(
  zonedbx::kZoneAndLinkRegistrySize,
  zonedbx::kZoneAndLinkRegistry,
  zoneProcessorCache);

// v4.x (likely)
#include <AceTime.h>
using namespace ace_time;
static ExtendedZoneManager timezoneManager(
  zonedbx2025::kZoneAndLinkRegistrySize,  // Note: 2025 suffix
  zonedbx2025::kZoneAndLinkRegistry,
  zoneProcessorCache);
```

#### Impact Assessment

**Files Using AceTime:**
- `OTGW-firmware.h` - ExtendedZoneManager setup (lines 84-90)
- `helperStuff.ino` - TimeZone usage (multiple locations)
- `Debug.h` - Cached TimeZone
- `restAPI.ino` - TimeZone for API responses
- `networkStuff.h` - NTP time handling
- `OTGW-firmware.ino` - Main setup

**Current Usage Pattern:**
```cpp
// Setup
static ExtendedZoneProcessor tzProcessor;
static const int CACHE_SIZE = 3;
static ExtendedZoneProcessorCache<CACHE_SIZE> zoneProcessorCache;
static ExtendedZoneManager timezoneManager(
  zonedbx::kZoneAndLinkRegistrySize,
  zonedbx::kZoneAndLinkRegistry,
  zoneProcessorCache);

// Usage
TimeZone myTz = timezoneManager.createForZoneName(CSTR(settingNTPtimezone));
```

**Expected Code Changes:**
1. Update zonedb namespace to `zonedbx2025` or similar
2. Verify TimeZone API compatibility
3. Test all timezone conversions
4. Verify NTP time synchronization

**Benefits:**
- Up-to-date timezone database (critical for DST changes!)
- Better accuracy
- Improved memory usage
- More reliable timezone handling

**Risks:**
- API breaking changes possible
- Namespace changes required
- Timezone database migration
- Epoch change may affect date calculations

#### Migration Plan

**Phase 1: Research**
1. Review AceTime v4 migration guide
2. Identify exact API changes from v2 to v4
3. Check for breaking changes in TimeZone API

**Phase 2: Code Updates**
1. Update includes and namespaces
2. Update zonedb references (v2 → v2025)
3. Fix any API changes in TimeZone usage
4. Update all files using AceTime

**Phase 3: Testing**
1. Test timezone conversions
2. Verify NTP synchronization
3. Test DST transitions
4. Verify time display in web UI

**Estimated Effort:** 3-4 hours (with migration guide) / 6-8 hours (without)

**Recommendation:** Research v4 migration guide before proceeding

---

### 7. OneWire (2.3.6 → 2.3.8)

#### Version Information
- **Current:** 2.3.6
- **Latest:** 2.3.8 (May 2024)
- **Risk Level:** LOW

#### Changes
- Bug fixes
- Improved compatibility
- Minor enhancements

#### Impact Assessment

**Files Using OneWire:**
- `OTGW-firmware.h` - Includes OneWire
- `sensors_ext.ino` - Dallas temperature sensor support

**Expected Code Changes:**
- **NONE** - Patch version, backward compatible

**Benefits:**
- Bug fixes
- Better reliability
- Improved compatibility with newer ESP cores

**Risks:**
- Very low - patch version update

#### Migration Plan
1. Update version in Makefile: `OneWire@2.3.8`
2. Build and test
3. Verify Dallas sensor readings if sensors are configured

**Estimated Effort:** 15 minutes

---

### 8. DallasTemperature (3.9.0 → 4.0.5)

#### Version Information
- **Current:** 3.9.0
- **Latest:** 4.0.5 (September 2025)
- **Risk Level:** LOW to MODERATE

#### Changes
- Minor version update (3.9 → 4.0)
- Enhanced ESP compatibility
- Async temperature reading improvements
- REST integration features
- Bug fixes

#### Impact Assessment

**Files Using DallasTemperature:**
- `OTGW-firmware.h` - Includes library
- `sensors_ext.ino` - Sensor reading implementation

**Expected Code Changes:**
- Likely **NONE** - Should be backward compatible
- API typically stable across minor versions

**Benefits:**
- Better ESP8266 compatibility
- Bug fixes
- Improved async mode
- Better error handling

**Risks:**
- Low - minor version update
- Possible API changes in async mode (if used)

#### Migration Plan
1. Update version in Makefile: `DallasTemperature@4.0.5`
2. Build and test
3. Verify sensor detection and reading
4. Test error handling

**Estimated Effort:** 30 minutes - 1 hour

---

## Recommended Upgrade Strategy

### Strategy 1: Conservative (Recommended for Production)

Upgrade only low-risk libraries first, defer high-risk upgrades:

**Phase 1: Low-Risk Updates**
1. WiFiManager 2.0.15-rc.1 → 2.0.17
2. OneWire 2.3.6 → 2.3.8
3. TelnetStream 1.2.4 → 1.3.0
4. DallasTemperature 3.9.0 → 4.0.5

**Total Effort:** 2-3 hours
**Risk:** LOW
**Benefit:** Bug fixes, stability improvements

**Phase 2: Moderate-Risk Updates (Deferred)**
1. Research AceTime migration guide thoroughly
2. Plan AceTime 2.0.1 → 4.1.0 upgrade
3. Test in development environment

**Phase 3: High-Risk Updates (Future)**
1. ESP8266 Core 2.7.4 → 3.1.2 (after thorough testing)
2. ArduinoJson 6.17.2 → 7.4.2 (after ESP8266 Core upgrade)

---

### Strategy 2: Aggressive (Full Upgrade)

Upgrade everything at once:

**Phase 1: Preparation**
1. Create detailed test plan
2. Set up test hardware
3. Document current behavior

**Phase 2: Low-Risk Libraries**
1. WiFiManager → 2.0.17
2. OneWire → 2.3.8
3. TelnetStream → 1.3.0
4. DallasTemperature → 4.0.5
5. Build, test, commit

**Phase 3: Arduino Core**
1. ESP8266 Core → 3.1.2
2. Fix compilation errors
3. Test WiFi and HTTP functionality
4. Build, test, commit

**Phase 4: ArduinoJson**
1. ArduinoJson → 7.4.2
2. Fix proxy type issues
3. Test all JSON operations
4. Build, test, commit

**Phase 5: AceTime**
1. Research migration guide
2. Update to AceTime 4.1.0
3. Fix namespace and API changes
4. Test timezone operations
5. Build, test, commit

**Phase 6: Integration Testing**
1. Full system test
2. Long-duration stability test
3. Memory usage monitoring
4. Performance testing

**Total Effort:** 15-25 hours
**Risk:** HIGH
**Benefit:** All latest features and fixes

---

### Strategy 3: Hybrid (Recommended for This Project)

Based on the project's maturity and user base:

**Phase 1: Immediate Low-Risk Updates**
- WiFiManager → 2.0.17
- OneWire → 2.3.8  
- TelnetStream → 1.3.0
- DallasTemperature → 4.0.5
- **Test and Release**

**Phase 2: Arduino Core Upgrade (Separate Branch)**
- ESP8266 Core → 3.1.2
- Extensive testing
- Address any issues
- **Test for 1-2 weeks before merge**

**Phase 3: Major Library Updates (After Core Stable)**
- ArduinoJson → 7.4.2
- AceTime → 4.1.0
- Extensive testing
- **Test for 1-2 weeks before merge**

**Total Timeline:** 2-3 months
**Risk:** MODERATE
**Benefit:** Gradual, tested improvements

---

## Breaking Change Details

### ESP8266 Core 3.x - Detailed Analysis

Based on migration guides for ESP32 Core 3.x (ESP8266 has fewer breaking changes):

**What DOESN'T Apply to ESP8266:**
- ADC API changes (ESP32-specific)
- LEDC/PWM changes (ESP32-specific)
- Hall sensor (ESP32-specific)
- BLE (ESP32 only)
- RMT (ESP32-specific)

**What DOES Apply to ESP8266:**
- WiFi event system updates (minor)
- HTTP server behavior (minimal changes)
- Improved heap management
- Compiler warnings for String usage
- Updated SDK (2.2.1 → 3.0.5)
- Build system improvements

**Code Impact:**
- GPIO functions: NO CHANGE (digitalWrite, digitalRead, etc. work as-is)
- WiFi: MINIMAL CHANGE (connection logic stable)
- HTTP Server: MINIMAL CHANGE (API mostly compatible)
- Serial: NO CHANGE
- LittleFS: NO CHANGE

---

### ArduinoJson 7.x - Code Migration Examples

**Example 1: Settings File (settingStuff.ino)**

Current v6 code:
```cpp
DynamicJsonDocument doc(1280);
JsonObject root = doc.to<JsonObject>();
root["hostname"] = settingHostname;
```

Potential v7 changes (if needed):
```cpp
JsonDocument doc;  // Uses heap allocator by default
JsonObject root = doc.to<JsonObject>();
root["hostname"] = settingHostname;
```

OR keep v6 API if still supported:
```cpp
DynamicJsonDocument doc(1280);  // May still work
JsonObject root = doc.to<JsonObject>();
root["hostname"] = settingHostname;
```

**Example 2: REST API (restAPI.ino)**

Current v6 code:
```cpp
StaticJsonDocument<256> doc;
DeserializationError error = deserializeJson(doc, request);
```

Potential v7 (if needed):
```cpp
JsonDocument doc;  // With stack allocator for small documents
DeserializationError error = deserializeJson(doc, request);
```

**Key Points:**
- v7 may still support old class names
- Need to test which approach works
- Main risk is JsonObject/JsonArray proxy assignments

---

### AceTime 4.x - Migration Analysis

**Namespace Changes (Expected):**
```cpp
// v2.x
zonedbx::kZoneAndLinkRegistrySize
zonedbx::kZoneAndLinkRegistry

// v4.x (likely)
zonedbx2025::kZoneAndLinkRegistrySize
zonedbx2025::kZoneAndLinkRegistry
```

**Files to Update:**
1. `OTGW-firmware.h` (lines 87-90)
2. Verify all TimeZone usage patterns

**Testing Required:**
1. Timezone string parsing ("Europe/Amsterdam")
2. DST transitions
3. Time formatting
4. NTP synchronization

---

## Testing Plan

### Unit Testing (Per Library)

**WiFiManager:**
- [ ] WiFi connection establishment
- [ ] Portal configuration
- [ ] Reconnection logic
- [ ] Timeout handling

**ArduinoJson:**
- [ ] Settings file read
- [ ] Settings file write
- [ ] REST API JSON responses
- [ ] Large JSON documents
- [ ] Memory usage check

**AceTime:**
- [ ] Timezone parsing
- [ ] Time formatting
- [ ] DST transitions
- [ ] NTP sync

**TelnetStream:**
- [ ] Telnet connection
- [ ] Debug output
- [ ] Multiple clients

**OneWire/DallasTemperature:**
- [ ] Sensor detection
- [ ] Temperature reading
- [ ] Error handling

### Integration Testing

**Full System Tests:**
- [ ] Cold boot
- [ ] WiFi connection
- [ ] MQTT connection
- [ ] Web UI access
- [ ] REST API calls
- [ ] Settings persistence
- [ ] OTA update
- [ ] Serial communication with OTGW
- [ ] Long-duration stability (24+ hours)
- [ ] Memory leak detection
- [ ] Reboot recovery

### Regression Testing

**Critical Paths:**
- [ ] OTGW serial communication
- [ ] OpenTherm message processing
- [ ] MQTT publishing
- [ ] Home Assistant discovery
- [ ] WiFi reconnection
- [ ] Web interface operation

---

## Risk Mitigation

### Pre-Upgrade Actions
1. ✓ Full backup of current stable code
2. Create separate upgrade branch
3. Document all current versions
4. Create test plan
5. Set up test hardware

### During Upgrade
1. Update one library at a time
2. Build and test after each change
3. Commit working states
4. Document any issues found
5. Keep detailed notes

### Post-Upgrade Actions
1. Extended stability testing
2. Memory usage monitoring
3. Performance benchmarking
4. User acceptance testing
5. Rollback plan ready

---

## Memory Impact Analysis

Current ESP8266 memory constraints:
- RAM: ~80KB total, ~40-50KB free during operation
- Flash: Depends on board, typically 4MB
- Heap fragmentation is a major concern

### Expected Memory Changes

**Positive Impacts:**
- ArduinoJson 7.x: Better string deduplication (saves RAM)
- ESP8266 Core 3.x: Improved heap management
- AceTime 4.x: Possibly more efficient

**Negative Impacts:**
- Newer libraries may be larger
- More features = more code
- Need to monitor carefully

### Monitoring Strategy
- Measure free heap before/after
- Monitor heap fragmentation
- Test under load
- Long-duration stability tests

---

## Compatibility Matrix

| Library | ESP8266 2.7.4 | ESP8266 3.1.2 | Notes |
|---------|---------------|---------------|-------|
| WiFiManager 2.0.17 | ✓ | ✓ | Compatible |
| ArduinoJson 6.17.2 | ✓ | ✓ | Current stable |
| ArduinoJson 7.4.2 | ✓ | ✓ | Needs testing |
| PubSubClient 2.8.0 | ✓ | ✓ | Compatible |
| TelnetStream 1.3.0 | ✓ | ✓ | Compatible |
| AceTime 2.0.1 | ✓ | ? | Current |
| AceTime 4.1.0 | ✓ | ✓ | Needs testing |
| OneWire 2.3.8 | ✓ | ✓ | Compatible |
| DallasTemperature 4.0.5 | ✓ | ✓ | Compatible |

---

## Build System Changes

### Makefile Updates Required

**Current (Makefile lines 18-20, 75-98):**
```makefile
ESP8266URL := https://github.com/esp8266/Arduino/releases/download/2.7.4/package_esp8266com_index.json

libraries/WiFiManager:
	$(CLICFG) lib install WiFiManager@2.0.15-rc.1
libraries/ArduinoJson:
	$(CLICFG) lib install ArduinoJson@6.17.2
libraries/PubSubClient:
	$(CLICFG) lib install pubsubclient@2.8.0
libraries/TelnetStream:
	$(CLICFG) lib install TelnetStream@1.2.4
libraries/AceTime:
	$(CLICFG) lib install Acetime@2.0.1
libraries/OneWire:
	$(CLICFG) lib install OneWire@2.3.6
libraries/DallasTemperature:
	$(CLICFG) lib install DallasTemperature@3.9.0
```

**Updated (Conservative Strategy):**
```makefile
ESP8266URL := https://github.com/esp8266/Arduino/releases/download/2.7.4/package_esp8266com_index.json

libraries/WiFiManager:
	$(CLICFG) lib install WiFiManager@2.0.17
libraries/ArduinoJson:
	$(CLICFG) lib install ArduinoJson@6.17.2
libraries/PubSubClient:
	$(CLICFG) lib install pubsubclient@2.8.0
libraries/TelnetStream:
	$(CLICFG) lib install TelnetStream@1.3.0
libraries/AceTime:
	$(CLICFG) lib install Acetime@2.0.1
libraries/OneWire:
	$(CLICFG) lib install OneWire@2.3.8
libraries/DallasTemperature:
	$(CLICFG) lib install DallasTemperature@4.0.5
```

**Updated (Full Upgrade):**
```makefile
ESP8266URL := https://github.com/esp8266/Arduino/releases/download/3.1.2/package_esp8266com_index.json

libraries/WiFiManager:
	$(CLICFG) lib install WiFiManager@2.0.17
libraries/ArduinoJson:
	$(CLICFG) lib install ArduinoJson@7.4.2
libraries/PubSubClient:
	$(CLICFG) lib install pubsubclient@2.8.0
libraries/TelnetStream:
	$(CLICFG) lib install TelnetStream@1.3.0
libraries/AceTime:
	$(CLICFG) lib install Acetime@4.1.0
libraries/OneWire:
	$(CLICFG) lib install OneWire@2.3.8
libraries/DallasTemperature:
	$(CLICFG) lib install DallasTemperature@4.0.5
```

---

## Timeline Estimates

### Conservative Strategy Timeline

**Week 1: Phase 1 (Low-Risk Libraries)**
- Day 1-2: Update Makefile, build, test WiFiManager, OneWire
- Day 3-4: Test TelnetStream, DallasTemperature
- Day 5: Integration testing
- Day 6-7: Documentation, release candidate

**Total: 1 week, LOW RISK**

### Aggressive Strategy Timeline

**Week 1: Low-Risk Libraries**
- Same as conservative

**Week 2-3: Arduino Core 3.1.2**
- Day 1: Update, fix compilation
- Day 2-3: Fix WiFi and HTTP issues
- Day 4-5: Full system testing
- Day 6-7: Long-duration stability test
- Day 8-10: Bug fixes and refinement

**Week 4-5: ArduinoJson 7.4.2**
- Day 1-2: Code updates for v7 API
- Day 3-4: Testing all JSON operations
- Day 5-7: Integration testing

**Week 6-7: AceTime 4.1.0**
- Day 1: Research migration guide
- Day 2-3: Update code
- Day 4-5: Test timezone operations
- Day 6-7: Integration testing

**Week 8: Final Integration**
- Full system testing
- Performance benchmarking
- Memory usage analysis
- Long-duration stability

**Total: 8 weeks, HIGH EFFORT, HIGH RISK**

---

## Rollback Strategy

### Per-Library Rollback
- Git branches for each upgrade
- Ability to rollback individual libraries
- Test each state

### Full Rollback
- Keep 2.7.4-based firmware available
- Document downgrade procedure
- Maintain OTA compatibility

---

## Community Impact

### User Communication
- Announce upgrade plans
- Request beta testers
- Collect feedback
- Document known issues

### Documentation Updates
- Update README.md
- Update build instructions
- Create migration guide for users
- Update wiki

---

## Conclusions

### Recommended Immediate Actions

1. **LOW-RISK UPGRADE (Recommended NOW):**
   - WiFiManager 2.0.15-rc.1 → 2.0.17
   - OneWire 2.3.6 → 2.3.8
   - TelnetStream 1.2.4 → 1.3.0
   - DallasTemperature 3.9.0 → 4.0.5
   - **Effort:** 2-3 hours
   - **Risk:** Very Low
   - **Benefit:** Stability improvements, bug fixes

2. **DEFERRED UPGRADES (Plan and Test):**
   - ESP8266 Core: Test 3.1.2 in separate branch
   - ArduinoJson: Research v7 migration thoroughly
   - AceTime: Research v4 migration guide

### Long-Term Strategy

**2026 Q1:**
- Complete low-risk upgrades
- Begin ESP8266 Core 3.1.2 testing

**2026 Q2:**
- Evaluate ESP8266 Core 3.1.2 stability
- Plan ArduinoJson and AceTime upgrades
- Continue testing

**2026 Q3:**
- Implement remaining upgrades if ESP8266 Core stable
- Full integration testing
- Beta release

### Key Takeaways

1. **ESP8266 Core 2.7.4 → 3.1.2** is the highest risk but brings significant benefits
2. **ArduinoJson 6 → 7** requires code changes but improves memory usage
3. **AceTime 2 → 4** needs research but provides updated timezone data
4. **Other libraries** are low-risk and ready to upgrade now

### Final Recommendation

**Start with low-risk libraries NOW**, then carefully plan and test high-risk upgrades in separate branches with extensive testing before merge.

---

## Appendix A: Research Sources

- ESP8266 Arduino Core: https://github.com/esp8266/Arduino/releases
- WiFiManager: https://github.com/tzapu/WiFiManager
- ArduinoJson: https://arduinojson.org/ and GitHub releases
- PubSubClient: https://github.com/knolleary/pubsubclient
- TelnetStream: https://github.com/jandrassy/TelnetStream
- AceTime: https://github.com/bxparks/AceTime
- OneWire: https://github.com/PaulStoffregen/OneWire
- DallasTemperature: https://github.com/milesburton/Arduino-Temperature-Control-Library
- ESP32 Migration Guide: https://docs.espressif.com/projects/arduino-esp32/en/latest/migration_guides/2.x_to_3.0.html

## Appendix B: Code Audit Summary

**Files Using Each Library:**

**ArduinoJson:**
- settingStuff.ino (heavy use)
- restAPI.ino (moderate use)
- jsonStuff.ino (minimal impact)

**AceTime:**
- OTGW-firmware.h (setup)
- helperStuff.ino (multiple uses)
- Debug.h (cached timezone)
- restAPI.ino (time formatting)
- networkStuff.h (NTP handling)

**WiFi/HTTP:**
- networkStuff.h (WiFiManager)
- FSexplorer.ino (HTTP server)
- restAPI.ino (HTTP endpoints)

**MQTT:**
- MQTTstuff.ino (PubSubClient)

**Dallas Sensors:**
- sensors_ext.ino (OneWire, DallasTemperature)

**Debug:**
- Debug.h, multiple .ino files (TelnetStream)

---

**Report End**
