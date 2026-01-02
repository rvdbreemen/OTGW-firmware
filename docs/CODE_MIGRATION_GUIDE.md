# Code Migration Guide for Deferred Upgrades

This document provides specific code examples for the high-risk library upgrades that were deferred.

---

## ArduinoJson 6 → 7 Migration

### Breaking Changes Overview

1. Document classes simplified
2. JsonObject/JsonArray proxy types are now non-copyable
3. `containsKey()` deprecated
4. String handling improvements

### Code Changes Required

#### File: `settingStuff.ino`

**Location: Line 30 (writeSettings function)**

```cpp
// CURRENT (v6):
DynamicJsonDocument doc(1280);
JsonObject root = doc.to<JsonObject>();
root["hostname"] = settingHostname;
root["MQTTenable"] = settingMQTTenable;
// ... etc

// OPTION 1 (v7 - Keep old class name if supported):
DynamicJsonDocument doc(1280);  // May still work in v7
JsonObject root = doc.to<JsonObject>();
root["hostname"] = settingHostname;
root["MQTTenable"] = settingMQTTenable;
// ... etc

// OPTION 2 (v7 - Use new JsonDocument class):
JsonDocument doc;  // Auto heap allocation for large documents
JsonObject root = doc.to<JsonObject>();
root["hostname"] = settingHostname;
root["MQTTenable"] = settingMQTTenable;
// ... etc
```

**Recommended:** Try Option 1 first (least change). If compilation fails, use Option 2.

---

**Location: Line 86 (readSettings function)**

```cpp
// CURRENT (v6):
StaticJsonDocument<1280> doc;
DeserializationError error = deserializeJson(doc, file);

// OPTION 1 (v7 - Keep old class name if supported):
StaticJsonDocument<1280> doc;  // May still work in v7
DeserializationError error = deserializeJson(doc, file);

// OPTION 2 (v7 - Use new JsonDocument with stack allocator):
JsonDocument doc;  // Auto stack allocation for documents
DeserializationError error = deserializeJson(doc, file);
```

---

**Location: Lines 96-137 (JsonObject field access)**

Most of the current code should work as-is:

```cpp
// CURRENT (v6) - Should still work in v7:
settingHostname = doc["hostname"].as<String>();
settingMQTTenable = doc["MQTTenable"]|settingMQTTenable;
settingMQTTbroker = doc["MQTTbroker"].as<String>();
```

**No change needed** - This pattern is compatible with v7.

---

#### File: `restAPI.ino`

**Location: Line 202 (doAPIgetStatusJSON)**

```cpp
// CURRENT (v6):
StaticJsonDocument<256> doc;
JsonObject root = doc.to<JsonObject>();

// OPTION 1 (v7):
StaticJsonDocument<256> doc;  // Keep if supported
JsonObject root = doc.to<JsonObject>();

// OPTION 2 (v7):
JsonDocument doc;  // Auto stack allocation
JsonObject root = doc.to<JsonObject>();
```

---

**Location: Line 232 (doAPIotgw function)**

```cpp
// CURRENT (v6):
StaticJsonDocument<256> doc;
DeserializationError error = deserializeJson(doc, httpServer.arg("plain"));

// OPTION 1 (v7):
StaticJsonDocument<256> doc;  // Keep if supported
DeserializationError error = deserializeJson(doc, httpServer.arg("plain"));

// OPTION 2 (v7):
JsonDocument doc;  // Auto stack allocation
DeserializationError error = deserializeJson(doc, httpServer.arg("plain"));
```

---

### Potential Proxy Issues to Watch For

If you see code like this (currently not in the codebase, but watch for it):

```cpp
// WRONG in v7 (will cause compilation error):
JsonObject config = doc["config"];  // Proxy type, non-copyable!

// CORRECT in v7:
JsonObject config = doc["config"].as<JsonObject>();  // For reading
JsonObject config = doc["config"].to<JsonObject>();  // For modifying

// ALTERNATIVE (v7):
auto config = doc["config"].as<JsonObject>();
```

**Current code check:** None of the current code has this pattern, so no changes needed there.

---

### containsKey() Migration

**Search for:** `containsKey()`

```bash
grep -r "containsKey" *.ino *.h
```

**If found, replace:**

```cpp
// OLD (v6):
if (doc.containsKey("setting")) {
    // ...
}

// NEW (v7):
if (doc["setting"].is<JsonVariant>()) {
    // ...
}
// Or more specific:
if (doc["setting"].is<String>()) {
    // ...
}
```

**Current code check:** Doesn't appear to use `containsKey()`, so likely safe.

---

### Testing Checklist for ArduinoJson 7

After upgrade:
- [ ] Build compiles without errors
- [ ] Settings read from file correctly
- [ ] Settings write to file correctly
- [ ] Settings persist across reboot
- [ ] REST API `/api/v1/otgw/*` returns valid JSON
- [ ] No JSON parsing errors in logs
- [ ] Memory usage checked (should improve!)
- [ ] Large JSON documents handled correctly

---

## AceTime 2 → 4 Migration

### Breaking Changes Overview

1. Timezone database namespaces changed
2. New database versions (2025 suffix)
3. Epoch reference changed to 2050-01-01
4. API may have changed (need migration guide)

### Code Changes Required

#### File: `OTGW-firmware.h`

**Location: Lines 84-90 (ExtendedZoneManager setup)**

```cpp
// CURRENT (v2):
static ExtendedZoneProcessor tzProcessor;
static const int CACHE_SIZE = 3;
static ExtendedZoneProcessorCache<CACHE_SIZE> zoneProcessorCache;
static ExtendedZoneManager timezoneManager(
  zonedbx::kZoneAndLinkRegistrySize,
  zonedbx::kZoneAndLinkRegistry,
  zoneProcessorCache);

// EXPECTED (v4) - NEEDS VERIFICATION:
static ExtendedZoneProcessor tzProcessor;
static const int CACHE_SIZE = 3;
static ExtendedZoneProcessorCache<CACHE_SIZE> zoneProcessorCache;
static ExtendedZoneManager timezoneManager(
  zonedbx2025::kZoneAndLinkRegistrySize,    // ← Changed
  zonedbx2025::kZoneAndLinkRegistry,        // ← Changed
  zoneProcessorCache);

// OR POSSIBLY (v4):
static ExtendedZoneProcessor tzProcessor;
static const int CACHE_SIZE = 3;
static ExtendedZoneProcessorCache<CACHE_SIZE> zoneProcessorCache;
static ExtendedZoneManager timezoneManager(
  zonedbx::kZoneAndLinkRegistrySize,    // ← May stay same
  zonedbx::kZoneAndLinkRegistry,        // ← May stay same
  zoneProcessorCache);
```

**Important:** The exact namespace change depends on AceTime v4 API. **Must research migration guide first!**

---

#### Files: Multiple (TimeZone usage)

**Pattern used throughout:**

```cpp
TimeZone myTz = timezoneManager.createForZoneName(CSTR(settingNTPtimezone));
```

**Affected files:**
- `helperStuff.ino` (lines 259, 333, 346, 359, 372)
- `Debug.h` (lines 50, 58)
- `restAPI.ino` (line 465)
- `networkStuff.h` (line 303)
- `OTGW-firmware.ino` (line 173)

**Expected change (if any):**

```cpp
// v2:
TimeZone myTz = timezoneManager.createForZoneName(CSTR(settingNTPtimezone));

// v4 (likely still works, but verify):
TimeZone myTz = timezoneManager.createForZoneName(CSTR(settingNTPtimezone));
```

**Likely:** TimeZone usage API is stable, no changes needed.

---

### Migration Steps

1. **Research First!**
   ```bash
   # Visit AceTime GitHub
   # Look for CHANGELOG.md or MIGRATION.md
   # Specifically look for v2 → v3 and v3 → v4 migration notes
   ```

2. **Update includes if needed:**
   ```cpp
   // Check if v4 requires different includes
   #include <AceTime.h>
   // May need additional includes
   ```

3. **Update namespace references:**
   ```cpp
   // Find all occurrences of zonedbx::
   grep -r "zonedbx::" *.h *.ino
   
   // Update to zonedbx2025:: if required
   ```

4. **Test timezone operations:**
   - Parse timezone strings
   - Format times
   - Handle DST transitions
   - Verify NTP sync

---

### Testing Checklist for AceTime 4

After upgrade:
- [ ] Build compiles without errors
- [ ] Timezone string "Europe/Amsterdam" parses correctly
- [ ] Time displays correctly in web UI
- [ ] Telnet debug shows correct timestamps
- [ ] NTP synchronization works
- [ ] DST transitions handled (test with historical data)
- [ ] MQTT timestamps are correct
- [ ] REST API time fields are correct

---

## ESP8266 Core 2.7.4 → 3.1.2 Migration

### Good News: Minimal Changes Expected!

Unlike ESP32, ESP8266 Core 3.x has **much fewer breaking changes**.

### Code Review Areas

#### 1. WiFi Events (if used)

**Check:** `networkStuff.h` for WiFi event handlers

```cpp
// v2.7.4 pattern (if exists):
WiFi.onEvent(WiFiEvent);

// v3.1.2 - likely same, but verify event types
WiFi.onEvent(WiFiEvent);
```

**Current code check:** Uses WiFiManager which handles events internally. Likely no changes needed.

---

#### 2. HTTP Server

**Check:** `restAPI.ino`, `FSexplorer.ino`

```cpp
// v2.7.4 - Current usage:
httpServer.send(200, "application/json", jsonString);
httpServer.sendHeader("Access-Control-Allow-Origin", "*");
// etc.

// v3.1.2 - Should be identical
httpServer.send(200, "application/json", jsonString);
httpServer.sendHeader("Access-Control-Allow-Origin", "*");
// etc.
```

**Expected:** No changes needed.

---

#### 3. String Usage Warnings

**New in v3.x:** Compiler warns about excessive String usage

```cpp
// May see warnings like:
// "warning: reallocating String buffer too many times"

// Fix by using char buffers instead:
// OLD:
String myStr = "Hello " + name + "!";

// BETTER:
char myStr[64];
snprintf(myStr, sizeof(myStr), "Hello %s!", name);
```

**Action:** Review any new compiler warnings and address them.

---

#### 4. LittleFS

**Good news:** No changes expected

```cpp
// v2.7.4 usage:
LittleFS.begin();
File file = LittleFS.open(SETTINGS_FILE, "r");

// v3.1.2 - same:
LittleFS.begin();
File file = LittleFS.open(SETTINGS_FILE, "r");
```

---

#### 5. GPIO Operations

**No changes expected:**

```cpp
// All of these work identically in 3.1.2:
pinMode(pin, OUTPUT);
digitalWrite(pin, HIGH);
digitalRead(pin);
analogWrite(pin, value);
analogRead(pin);
```

---

### Compiler Flag Changes

**Check Makefile build flags:**

Current:
```makefile
CFLAGS_DEFAULT = -DNO_GLOBAL_HTTPUPDATE
```

**May need to add:**
```makefile
# For v3.1.2, verify these are set correctly:
CFLAGS_DEFAULT = -DNO_GLOBAL_HTTPUPDATE -MMD -c
```

(The `-MMD -c` flags may be auto-added by arduino-cli)

---

### Testing Checklist for ESP8266 Core 3.1.2

After upgrade:
- [ ] Build compiles without errors
- [ ] Review all compiler warnings
- [ ] WiFi connection works
- [ ] WiFi reconnection works
- [ ] WiFiManager portal works
- [ ] HTTP server responds correctly
- [ ] OTA updates work
- [ ] LittleFS read/write works
- [ ] Settings persist
- [ ] MQTT connection stable
- [ ] Serial communication with OTGW works
- [ ] Memory usage monitored (free heap)
- [ ] No crashes in 24-hour test
- [ ] No memory leaks detected

---

## Combined Migration (All Upgrades)

If doing all upgrades together:

### Order of Implementation

1. **First:** Update Arduino Core to 3.1.2
   - Build and fix any compilation errors
   - Test WiFi and HTTP functionality
   - Commit working state

2. **Second:** Update ArduinoJson to 7.4.2
   - Fix JSON code as documented above
   - Test all JSON operations
   - Monitor memory improvements
   - Commit working state

3. **Third:** Update AceTime to 4.1.0
   - Research migration guide
   - Update namespace references
   - Test timezone operations
   - Commit working state

4. **Fourth:** Full integration testing
   - Test all features together
   - Long-duration stability
   - Memory usage monitoring
   - Performance benchmarking

---

## Verification Script

After any upgrade, use this checklist:

```bash
#!/bin/bash
# upgrade-verification.sh

echo "🔍 Verification Checklist for Library Upgrades"
echo ""

echo "📦 Build Check:"
echo "  [ ] make clean"
echo "  [ ] make -j\$(nproc)"
echo "  [ ] No compilation errors"
echo "  [ ] Warnings reviewed and addressed"
echo ""

echo "⚡ Flash Check:"
echo "  [ ] Firmware uploaded via OTA or serial"
echo "  [ ] Device boots successfully"
echo "  [ ] Serial output shows normal startup"
echo ""

echo "📡 Connectivity Check:"
echo "  [ ] WiFi connects"
echo "  [ ] WiFi reconnects after router reboot"
echo "  [ ] MQTT connects"
echo "  [ ] MQTT reconnects after broker restart"
echo ""

echo "🌐 Web UI Check:"
echo "  [ ] Web interface accessible"
echo "  [ ] Settings page loads"
echo "  [ ] Settings can be changed"
echo "  [ ] Settings persist across reboot"
echo ""

echo "🔧 REST API Check:"
echo "  [ ] /api/v1/otgw/id responds"
echo "  [ ] /api/v1/otgw/status responds"
echo "  [ ] JSON is valid"
echo ""

echo "🕐 Time Check (if AceTime upgraded):"
echo "  [ ] Timezone parsing works"
echo "  [ ] Time displays correctly"
echo "  [ ] NTP synchronization works"
echo ""

echo "🌡️ Sensor Check (if Dallas libs upgraded):"
echo "  [ ] Sensors detected"
echo "  [ ] Temperature readings valid"
echo ""

echo "💾 Memory Check:"
echo "  [ ] Free heap > 20KB during operation"
echo "  [ ] No memory leaks in 24h test"
echo ""

echo "🎯 OTGW Check:"
echo "  [ ] Serial communication works"
echo "  [ ] OpenTherm messages processed"
echo "  [ ] MQTT messages published"
echo ""

echo "✅ Final Checks:"
echo "  [ ] No crashes in 24-hour test"
echo "  [ ] No errors in serial/telnet log"
echo "  [ ] Performance acceptable"
```

Save this script and run through the checklist after each upgrade.

---

## Getting Help

If you encounter issues during migration:

1. **Check compiler errors carefully** - They often point to the exact issue
2. **Review library changelogs** - Look for migration notes
3. **Test incrementally** - Don't change everything at once
4. **Ask the community** - OTGW firmware has active users

---

## Summary

### ArduinoJson 7 Migration
- **Main Risk:** JsonObject proxy type changes
- **Current Code:** Appears mostly compatible
- **Changes Needed:** Minimal (2-3 files)
- **Test Focus:** Settings and REST API JSON

### AceTime 4 Migration
- **Main Risk:** Namespace changes
- **Research First:** Must find migration guide
- **Changes Needed:** Namespace updates (1-2 locations)
- **Test Focus:** Timezone operations and NTP

### ESP8266 Core 3.1.2 Migration
- **Main Risk:** Lower than expected!
- **ESP32 Issues:** Don't apply to ESP8266
- **Changes Needed:** Minimal to none
- **Test Focus:** WiFi and HTTP stability

---

**Last Updated:** January 2, 2026
**Status:** Ready for implementation when library testing is needed
