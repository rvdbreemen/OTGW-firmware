# Library Upgrade Implementation Guide

This document provides step-by-step instructions for implementing the library upgrades recommended in `LIBRARY_UPGRADE_REPORT.md`.

## Quick Summary

**5 libraries will be upgraded:**
- WiFiManager: 2.0.15-rc.1 → 2.0.17
- TelnetStream: 1.2.4 → 1.3.0 (+ NetApiHelpers dependency)
- AceTime: 2.0.1 → 4.1.0
- OneWire: 2.3.6 → 2.3.8
- DallasTemperature: 3.9.0 → 4.0.5

**3 libraries will remain unchanged:**
- ESP8266 Arduino Core: 2.7.4 (defer due to HTTP streaming bug)
- ArduinoJson: 6.17.2 (defer v7 major version, could upgrade to 6.21.5)
- PubSubClient: 2.8.0 (already latest)

**Total estimated effort:** 4.25 hours
**Risk level:** LOW to MEDIUM

---

## Implementation Steps

### Step 1: Backup Current Setup

```bash
# Backup the current Makefile
cp Makefile Makefile.backup

# Backup the current libraries (if already installed)
cp -r libraries libraries.backup

# Note current firmware version for comparison
cat version.h | grep SEMVER
```

### Step 2: Apply Makefile Changes

Replace the current `Makefile` with `Makefile.upgraded` or apply these changes manually:

#### Changes to Make:

1. **Line 20: Add NetApiHelpers to LIBRARIES list**
   ```make
   # Before:
   LIBRARIES := libraries/WiFiManager libraries/ArduinoJson libraries/PubSubClient libraries/TelnetStream libraries/AceTime libraries/OneWire libraries/DallasTemperature
   
   # After:
   LIBRARIES := libraries/WiFiManager libraries/ArduinoJson libraries/PubSubClient libraries/TelnetStream libraries/NetApiHelpers libraries/AceTime libraries/OneWire libraries/DallasTemperature
   ```

2. **Line 76: Update WiFiManager version**
   ```make
   # Before:
   $(CLICFG) lib install WiFiManager@2.0.15-rc.1
   
   # After:
   $(CLICFG) lib install WiFiManager@2.0.17
   ```

3. **Line 84: Update TelnetStream version**
   ```make
   # Before:
   $(CLICFG) lib install TelnetStream@1.2.4
   
   # After:
   $(CLICFG) lib install TelnetStream@1.3.0
   ```

4. **Add NetApiHelpers target after TelnetStream (insert new lines)**
   ```make
   # NEW DEPENDENCY: Required by TelnetStream 1.3.0
   libraries/NetApiHelpers:
   	$(CLICFG) lib install NetApiHelpers
   ```

5. **Line 88: Update AceTime version**
   ```make
   # Before:
   $(CLICFG) lib install Acetime@2.0.1
   
   # After:
   $(CLICFG) lib install Acetime@4.1.0
   ```

6. **Line 94: Update OneWire version**
   ```make
   # Before:
   $(CLICFG) lib install OneWire@2.3.6
   
   # After:
   $(CLICFG) lib install OneWire@2.3.8
   ```

7. **Line 98: Update DallasTemperature version**
   ```make
   # Before:
   $(CLICFG) lib install DallasTemperature@3.9.0
   
   # After:
   $(CLICFG) lib install DallasTemperature@4.0.5
   ```

### Step 3: Clean Build Environment

```bash
# Clean all previous builds and libraries
make distclean

# Or use Python build script
python build.py --clean
```

### Step 4: Build with Updated Libraries

```bash
# Build firmware
python build.py --firmware

# Or use make directly
make -j$(nproc)
```

**Expected output:** Build should complete successfully with no errors.

**If build fails:** Check error messages for compatibility issues. Refer to troubleshooting section below.

### Step 5: Build Filesystem

```bash
# Build filesystem
python build.py --filesystem

# Or use make
make filesystem
```

### Step 6: Comprehensive Testing

⚠️ **Important:** Test each component thoroughly before deploying to production devices.

#### 6.1 Basic Functionality Tests

1. **Flash the firmware to test device**
   ```bash
   # Flash firmware
   make upload PORT=/dev/ttyUSB0
   
   # Flash filesystem
   make upload-fs PORT=/dev/ttyUSB0
   ```

2. **Power cycle the device**
   - Unplug and replug power
   - Device should boot normally

3. **Serial/Telnet Debug Output**
   - Connect to telnet port 23
   - Verify debug output is working
   - Check for any error messages during boot

#### 6.2 WiFi Connectivity Tests (WiFiManager 2.0.17)

- [ ] Device connects to WiFi automatically
- [ ] Configuration portal accessible if no WiFi configured
- [ ] Captive portal works (redirect to config page)
- [ ] WiFi reconnection after router reboot
- [ ] MDNS hostname resolution works
- [ ] Static IP configuration (if used)

**How to test:**
```bash
# Check WiFi connection via telnet
# Watch for "WiFi connected" messages
# Access web UI at http://<device-ip>
# Test MDNS: http://otgw.local (or configured hostname)
```

#### 6.3 Telnet Debug Tests (TelnetStream 1.3.0)

- [ ] Telnet connection on port 23 works
- [ ] Debug messages display correctly
- [ ] Multiple telnet clients can connect simultaneously (new feature)
- [ ] No disconnections or hangs
- [ ] Debug output includes all expected messages

**How to test:**
```bash
# Connect via telnet
telnet <device-ip> 23

# Try connecting second client
telnet <device-ip> 23

# Both should receive debug output
```

#### 6.4 Time/Timezone Tests (AceTime 4.1.0)

- [ ] NTP time sync works
- [ ] Timezone conversion accurate
- [ ] DST transitions work correctly
- [ ] Time displays correctly in web UI
- [ ] Time sent in MQTT messages is correct
- [ ] Scheduled commands execute at correct times

**How to test:**
```bash
# Check via telnet debug output
# Look for "NTP sync" messages
# Verify timestamp in web UI
# Check MQTT message timestamps
# Test with different timezone settings
```

#### 6.5 Temperature Sensor Tests (OneWire 2.3.8, DallasTemperature 4.0.5)

- [ ] Dallas sensors detected on boot
- [ ] Temperature readings accurate
- [ ] Sensor addresses correct
- [ ] No reading errors
- [ ] MQTT temperature publishing works
- [ ] Web UI displays temperatures

**How to test:**
```bash
# Check telnet output for sensor detection
# Verify temperature values in web UI
# Check MQTT topics for temperature data
# Compare readings with known good values
```

#### 6.6 MQTT Tests (PubSubClient 2.8.0 - unchanged)

- [ ] MQTT broker connection successful
- [ ] All sensors publish data
- [ ] Home Assistant discovery works
- [ ] Commands via MQTT work
- [ ] Reconnection after broker restart
- [ ] QoS and retained messages work

**How to test:**
```bash
# Monitor MQTT messages
mosquitto_sub -h <broker> -t "otgw/#" -v

# Test commands
mosquitto_pub -h <broker> -t "otgw/set/<node>/setpoint" -m "20.5"

# Check Home Assistant integration
```

#### 6.7 Web UI Tests

- [ ] Web interface loads correctly
- [ ] All pages accessible
- [ ] Settings can be changed and saved
- [ ] Device status displays correctly
- [ ] Firmware update page works (but don't update yet)

#### 6.8 REST API Tests

- [ ] REST API endpoints respond
- [ ] JSON responses valid
- [ ] OTGW values readable via API
- [ ] Commands can be sent via API
- [ ] CORS headers present (new in WiFiManager)

**How to test:**
```bash
# Test API endpoints
curl http://<device-ip>/api/v1/otgw/id/0
curl http://<device-ip>/api/v1/otgw/label/roomtemp

# Test command
curl -X POST http://<device-ip>/api/v1/otgw/command/PR=C
```

#### 6.9 OpenTherm Gateway Tests

- [ ] Serial communication with OTGW PIC works
- [ ] OpenTherm messages processed
- [ ] Commands sent to OTGW execute
- [ ] All OTGW values update
- [ ] No message queue overflow

#### 6.10 OTA Update Test

- [ ] OTA update mechanism works
- [ ] Can upload new firmware via web UI
- [ ] Device reboots successfully after update
- [ ] Settings preserved after update

⚠️ **Warning:** Only test with non-production firmware to avoid bricking device.

### Step 7: Extended Soak Testing

Run the device for 24-48 hours monitoring:
- Memory usage (heap free)
- Uptime stability
- Connection stability (WiFi, MQTT)
- No unexpected reboots
- All sensors continue working

### Step 8: Rollback Plan

If issues are discovered:

```bash
# Restore original Makefile
cp Makefile.backup Makefile

# Clean libraries
make distclean

# Rebuild with original versions
make -j$(nproc)
make filesystem

# Flash original firmware
make install PORT=/dev/ttyUSB0
```

---

## Troubleshooting

### Build Errors

#### Error: NetApiHelpers not found
**Solution:** Ensure line is added to Makefile libraries target:
```make
libraries/NetApiHelpers:
	$(CLICFG) lib install NetApiHelpers
```

#### Error: Library version not found
**Solution:** Update arduino-cli library index:
```bash
make refresh
make flush
```

#### Error: Compilation errors in AceTime
**Solution:** Check that you're using AceTime (capital T), not Acetime in some places:
```make
$(CLICFG) lib install Acetime@4.1.0  # Note: lowercase 't' in package name
```

### Runtime Errors

#### Telnet doesn't work
**Symptom:** Can't connect to telnet port or no output

**Solutions:**
1. Check if NetApiHelpers is installed: `ls libraries/NetApiHelpers`
2. Verify TelnetStream initialized in code
3. Check firewall settings
4. Verify device has valid IP address

#### NTP Sync Fails
**Symptom:** Time not syncing or wrong timezone

**Solutions:**
1. Check NTP server reachable: `ping pool.ntp.org`
2. Verify timezone setting in web UI
3. Check AceTime timezone database for your region
4. Review telnet debug output for NTP errors

#### Temperature Sensors Not Detected
**Symptom:** No Dallas sensors found

**Solutions:**
1. Check sensor wiring (GPIO pin configuration)
2. Verify OneWire and DallasTemperature versions correct
3. Check for sensor power issues
4. Review sensor initialization code in sensors_ext.ino

#### WiFi Connection Issues
**Symptom:** Device doesn't connect to WiFi

**Solutions:**
1. Reset WiFi settings: Access config portal
2. Check WiFi credentials in settings
3. Verify router compatibility (2.4GHz required)
4. Check WiFiManager debug output via Serial during boot

### Memory Issues

#### Out of Memory Errors
**Symptom:** Device crashes or reboots randomly

**Solutions:**
1. Check heap usage in telnet output
2. Review code for memory leaks
3. Consider reverting to older library versions
4. Monitor memory with heap analysis tools

---

## Performance Comparison

Before and after upgrade, measure:

| Metric | Before | After | Change |
|--------|--------|-------|--------|
| Firmware Size (bytes) | _____ | _____ | _____ |
| Free Heap at Boot (bytes) | _____ | _____ | _____ |
| Free Heap Running (bytes) | _____ | _____ | _____ |
| Boot Time (seconds) | _____ | _____ | _____ |
| MQTT Connect Time (ms) | _____ | _____ | _____ |
| WiFi Connect Time (seconds) | _____ | _____ | _____ |

---

## Code Changes Required

**Good News:** No source code changes are required for the recommended upgrades. All changes are backward compatible at the API level.

### Optional Enhancements (Not Required)

If you want to take advantage of new features:

#### 1. WiFiManager CORS Support
Add to `networkStuff.h` if needed:
```cpp
// Already handled automatically by WiFiManager 2.0.17
// CORS headers now sent automatically
```

#### 2. TelnetStream Multi-Client
Already supported automatically - no code changes needed.

#### 3. AceTime Performance
Already optimized - no code changes needed.

#### 4. DallasTemperature Error Codes
Enhanced error detection available:
```cpp
// Optional: Add to sensors_ext.ino for better error handling
float temp = sensors.getTempC(deviceAddress);
if (temp == DEVICE_DISCONNECTED_C) {
  DebugTln(F("Sensor disconnected"));
} else if (temp == DEVICE_FAULT_OPEN_C) {
  DebugTln(F("Sensor fault: open circuit"));
} else if (temp == DEVICE_FAULT_SHORTGND_C) {
  DebugTln(F("Sensor fault: short to ground"));
} else if (temp == DEVICE_FAULT_SHORTVDD_C) {
  DebugTln(F("Sensor fault: short to VDD"));
}
```

---

## Future Upgrades to Consider

### ArduinoJson 6.17.2 → 6.21.5

**When:** Low priority, safe minor upgrade for bug fixes

**Changes needed:** None (API compatible)

**Benefits:** Bug fixes and performance improvements

**How to upgrade:**
```make
# In Makefile, change:
$(CLICFG) lib install ArduinoJson@6.21.5
```

### ArduinoJson 6.x → 7.x

**When:** Only if specific v7 features needed

**Risk:** HIGH - Major version with breaking changes

**Changes needed:** See LIBRARY_UPGRADE_REPORT.md Section 3

**Not recommended** due to:
- Increased code size
- ESP8266 limited memory
- Significant refactoring required
- Current v6 is stable and sufficient

### ESP8266 Core 2.7.4 → 3.1.2+

**When:** After HTTP streaming bug is confirmed fixed

**Risk:** HIGH - Known issues noted in Makefile

**Monitor:** 
- https://github.com/esp8266/Arduino/releases
- Look for fixes to HTTP streaming

**Test thoroughly:**
- OTA updates
- Web server
- HTTP client operations
- Custom ModUpdateServer functionality

---

## Validation Checklist

Before deploying to production:

- [ ] All builds complete without errors
- [ ] All tests in Step 6 passed
- [ ] 24+ hour soak test completed
- [ ] Memory usage acceptable
- [ ] No unexpected reboots
- [ ] Performance metrics recorded
- [ ] Backup firmware available
- [ ] Rollback procedure tested
- [ ] Documentation updated
- [ ] Release notes prepared

---

## Deployment Strategy

### Phased Rollout

1. **Lab Testing (1-2 devices)**
   - Complete all tests
   - Run for 1 week minimum

2. **Beta Testing (5-10 devices)**
   - Deploy to volunteer users
   - Collect feedback
   - Monitor for issues

3. **Production Rollout (All devices)**
   - Deploy gradually
   - Monitor each deployment
   - Be ready to rollback

### Rollback Triggers

Immediately rollback if:
- Device won't boot
- WiFi connection fails
- MQTT connection unstable
- Sensors not working
- Memory issues
- Unexpected crashes

---

## Support and Resources

- **OTGW Firmware Wiki:** https://github.com/rvdbreemen/OTGW-firmware/wiki
- **Discord:** https://discord.gg/zjW3ju7vGQ
- **GitHub Issues:** https://github.com/rvdbreemen/OTGW-firmware/issues

- **WiFiManager:** https://github.com/tzapu/WiFiManager
- **TelnetStream:** https://github.com/jandrassy/TelnetStream
- **AceTime:** https://github.com/bxparks/AceTime
- **OneWire:** https://github.com/PaulStoffregen/OneWire
- **DallasTemperature:** https://github.com/milesburton/Arduino-Temperature-Control-Library

---

## Conclusion

This implementation guide provides a comprehensive, step-by-step process for safely upgrading the OTGW-firmware libraries. The recommended upgrades are low-risk and provide stability improvements, bug fixes, and enhanced functionality without requiring code changes.

**Total Time Required:** 4-6 hours including testing
**Risk Level:** LOW to MEDIUM
**Recommended:** YES

Follow this guide carefully and test thoroughly before deploying to production devices.
