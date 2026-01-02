# Library Upgrade Scenarios

This document provides different upgrade scenarios for testing. Use these to experiment with different upgrade paths.

## Scenario 1: Conservative (LOW RISK) - IMPLEMENTED ✓

**Status:** Implemented in main Makefile
**Risk:** LOW
**Estimated Effort:** 2-3 hours

### Changes
```makefile
WiFiManager@2.0.17          # Was: 2.0.15-rc.1
TelnetStream@1.3.0          # Was: 1.2.4
OneWire@2.3.8               # Was: 2.3.6
DallasTemperature@4.0.5     # Was: 3.9.0
```

### Testing Checklist
- [ ] WiFi connection and portal
- [ ] Telnet debug output
- [ ] Dallas sensor readings (if configured)
- [ ] Settings persistence
- [ ] MQTT connectivity
- [ ] Web UI access
- [ ] OTA updates
- [ ] 24-hour stability test

---

## Scenario 2: Arduino Core 3.1.2 Only (MODERATE RISK)

**Status:** For testing
**Risk:** MODERATE
**Estimated Effort:** 4-6 hours

### Makefile Changes
```makefile
# Line 19 - Change ESP8266 Core URL
ESP8266URL := https://github.com/esp8266/Arduino/releases/download/3.1.2/package_esp8266com_index.json

# Keep all libraries at Phase 1 versions
WiFiManager@2.0.17
ArduinoJson@6.17.2          # Keep v6 for now
pubsubclient@2.8.0
TelnetStream@1.3.0
Acetime@2.0.1               # Keep v2 for now
OneWire@2.3.8
DallasTemperature@4.0.5
```

### Expected Issues
- Possible WiFi event handling changes
- HTTP server compatibility checks needed
- New compiler warnings (especially String usage)
- May need to adjust build flags

### Code Changes Required
Likely none for ESP8266 (unlike ESP32), but review:
- WiFi connection logic in `networkStuff.h`
- HTTP server in `FSexplorer.ino` and `restAPI.ino`
- Any String operations flagged by compiler

### Testing Focus
- WiFi connection/reconnection
- HTTP server endpoints
- OTA updates
- Serial communication
- Memory usage monitoring

---

## Scenario 3: ArduinoJson 7.4.2 Only (HIGH RISK)

**Status:** For testing after Core upgrade stable
**Risk:** HIGH
**Estimated Effort:** 4-6 hours

### Makefile Changes
```makefile
# Keep ESP8266 Core 3.1.2 (from Scenario 2)
# Or use 2.7.4 if testing ArduinoJson separately

ArduinoJson@7.4.2           # Was: 6.17.2

# Keep other libraries at Phase 1 versions
```

### Code Changes Required

**settingStuff.ino:**
```cpp
// Line 30 - Review DynamicJsonDocument usage
// Option 1: Keep old name if supported
DynamicJsonDocument doc(1280);
JsonObject root = doc.to<JsonObject>();

// Option 2: Use new JsonDocument class
JsonDocument doc;  // Auto-selects allocator
JsonObject root = doc.to<JsonObject>();

// Line 86 - StaticJsonDocument
// Similar changes may be needed
```

**restAPI.ino:**
```cpp
// Lines 202, 232 - StaticJsonDocument<256>
// May need to change to JsonDocument or keep old syntax if supported
```

**Key Changes:**
1. Review all JsonObject assignments for proxy issues
2. Change any code like:
   ```cpp
   // OLD
   JsonObject obj = doc["config"];
   
   // NEW
   JsonObject obj = doc["config"].as<JsonObject>();  // Reading
   JsonObject obj = doc["config"].to<JsonObject>();  // Building
   ```
3. Replace `containsKey()` with `is<T>()` if used

### Testing Focus
- Settings file read/write
- REST API JSON responses
- MQTT JSON messages
- Memory usage (should improve!)
- Error handling

---

## Scenario 4: AceTime 4.1.0 Only (MODERATE to HIGH RISK)

**Status:** For testing - requires migration guide research
**Risk:** MODERATE to HIGH
**Estimated Effort:** 3-6 hours (with guide) / 6-8 hours (without)

### Makefile Changes
```makefile
# Keep ESP8266 Core and other libraries stable

Acetime@4.1.0               # Was: 2.0.1
```

### Code Changes Required

**OTGW-firmware.h (lines 84-90):**
```cpp
// Current v2:
static ExtendedZoneProcessor tzProcessor;
static const int CACHE_SIZE = 3;
static ExtendedZoneProcessorCache<CACHE_SIZE> zoneProcessorCache;
static ExtendedZoneManager timezoneManager(
  zonedbx::kZoneAndLinkRegistrySize,
  zonedbx::kZoneAndLinkRegistry,
  zoneProcessorCache);

// Expected v4 (needs verification):
static ExtendedZoneProcessor tzProcessor;
static const int CACHE_SIZE = 3;
static ExtendedZoneProcessorCache<CACHE_SIZE> zoneProcessorCache;
static ExtendedZoneManager timezoneManager(
  zonedbx2025::kZoneAndLinkRegistrySize,    // Note: 2025 suffix
  zonedbx2025::kZoneAndLinkRegistry,
  zoneProcessorCache);
```

**Files to Review:**
- `OTGW-firmware.h` - Manager setup
- `helperStuff.ino` - TimeZone usage
- `Debug.h` - Cached TimeZone
- `restAPI.ino` - Time formatting
- `networkStuff.h` - NTP handling
- `OTGW-firmware.ino` - Main setup

### Research Required
1. Download AceTime v4.1.0 release notes
2. Find migration guide (likely on GitHub)
3. Check for breaking changes in:
   - TimeZone API
   - ZoneManager API
   - Database namespace changes

### Testing Focus
- Timezone string parsing ("Europe/Amsterdam")
- Time display in web UI
- NTP synchronization
- DST transitions
- Time formatting in logs and MQTT

---

## Scenario 5: Full Upgrade (ALL CHANGES - VERY HIGH RISK)

**Status:** For testing only after all individual scenarios pass
**Risk:** VERY HIGH
**Estimated Effort:** 15-25 hours over multiple weeks

### Makefile Changes
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

### Code Changes Required
Combination of all above scenarios:
- Arduino Core 3.1.2 compatibility
- ArduinoJson v7 API changes
- AceTime v4 namespace changes

### Testing Strategy
1. Implement changes incrementally
2. Test each subsystem thoroughly
3. Monitor memory usage carefully
4. Run extended stability tests (1 week+)
5. Beta test with community

### Rollback Plan
- Keep v2.7.4 based firmware available
- Document downgrade procedure
- Maintain OTA compatibility

---

## Testing Commands

### Build Current (with Phase 1 upgrades)
```bash
make clean
make -j$(nproc)
```

### Build Scenario 2 (Core 3.1.2)
```bash
# Edit Makefile to change ESP8266URL
make clean
make -j$(nproc)
```

### Build Scenario 3 (ArduinoJson 7)
```bash
# Edit Makefile to change ArduinoJson version
make clean
make -j$(nproc)
```

### Build Scenario 4 (AceTime 4)
```bash
# Edit Makefile to change AceTime version
# Update code first!
make clean
make -j$(nproc)
```

---

## Success Criteria

### For Each Scenario
- [ ] Clean compilation (no errors)
- [ ] Minimal warnings
- [ ] Successful OTA upload
- [ ] WiFi connection works
- [ ] MQTT connection works
- [ ] Web UI accessible
- [ ] Settings persist across reboot
- [ ] Serial communication with OTGW works
- [ ] Memory usage acceptable
- [ ] No crashes in 24-hour test

### Performance Benchmarks
- Free heap: Should be > 20KB during normal operation
- Boot time: < 30 seconds to WiFi connected
- MQTT reconnect: < 10 seconds
- HTTP response: < 500ms for simple pages

---

## Recommended Upgrade Path

**Step 1: Phase 1 (Now)**
- Scenario 1: Conservative upgrades
- Test thoroughly
- Release when stable

**Step 2: Core Upgrade (Q1 2026)**
- Scenario 2: ESP8266 Core 3.1.2
- Test in separate branch
- Beta test with community
- Merge when stable

**Step 3: ArduinoJson (Q2 2026)**
- Scenario 3: ArduinoJson 7.4.2
- After Core is stable
- Test memory improvements
- Merge when stable

**Step 4: AceTime (Q2 2026)**
- Scenario 4: AceTime 4.1.0
- Research migration guide thoroughly
- Update timezone database
- Merge when stable

**Step 5: Full Integration (Q3 2026)**
- Scenario 5: All upgrades together
- Extended testing period
- Community beta testing
- Final release

---

## Risk Mitigation

### Before Each Scenario
1. Create new git branch
2. Document current state
3. Plan rollback procedure
4. Prepare test hardware

### During Testing
1. Monitor serial output
2. Watch for memory leaks
3. Check for crashes
4. Log any anomalies

### After Each Scenario
1. Document issues found
2. Update this document
3. Share findings with community
4. Decide on merge or rollback

---

## Community Involvement

### Beta Testing
- Recruit volunteers for each scenario
- Provide clear instructions
- Collect feedback systematically
- Address issues before merge

### Documentation
- Update README for each change
- Maintain migration guides
- Document known issues
- Provide rollback instructions

---

## Notes

- Always test on actual hardware, not just compilation
- ESP8266 behavior can differ from simulation
- Memory issues may only appear under load
- WiFi issues may be intermittent
- MQTT reliability is critical for Home Assistant users

---

**Last Updated:** January 2, 2026
