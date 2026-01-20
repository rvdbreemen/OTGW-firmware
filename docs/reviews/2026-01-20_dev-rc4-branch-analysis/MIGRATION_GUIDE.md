---
# METADATA
Document Title: Migration Guide - dev-rc4-branch to dev
Review Date: 2026-01-20 00:00:00 UTC
Branch Reviewed: dev-rc4-branch → dev (merge commit 9f918e9)
Target Version: 1.0.0-rc4+
Reviewer: GitHub Copilot Advanced Agent
Document Type: Migration Guide
Status: COMPLETE
---

# Migration Guide: dev-rc4-branch to dev

## Overview

This guide provides step-by-step instructions for migrating from the dev-rc4-branch to the dev branch of the OTGW-firmware repository.

**Migration Difficulty:** Low-Medium  
**Estimated Time:** 5-10 minutes per device  
**Risk Level:** Low  
**Recommended:** ✅ YES - Critical fixes and improvements

---

## Pre-Migration Checklist

### Prerequisites

- [ ] Backup current device configuration
- [ ] Document current MQTT topics and Home Assistant entities
- [ ] Note Dallas DS18B20 sensor IDs (if applicable)
- [ ] Have physical access to device (in case of issues)
- [ ] Install Python 3.6+ on workstation (for flash_esp.py)
- [ ] Test on one device before mass rollout

### Backup Configuration

#### Option 1: Via REST API (Recommended)

```bash
# Export settings
curl http://<device-ip>/api/v1/settings > settings-backup.json

# Export MQTT configuration
curl http://<device-ip>/api/v1/settings/mqtt > mqtt-backup.json
```

#### Option 2: Via Web UI

1. Navigate to `http://<device-ip>`
2. Go to **Settings** page
3. Take screenshots of all configuration pages
4. Note: MQTT broker, username, topics, etc.

#### Option 3: Manual Documentation

Document the following:
- WiFi SSID
- MQTT broker address and port
- MQTT username (password will need to be re-entered)
- MQTT topics (prefix, unique ID)
- Dallas sensor GPIO pin
- Dallas sensor count
- Dallas sensor legacy format setting (if used)
- GPIO output configurations
- NTP server settings
- Timezone settings

---

## Migration Steps

### Step 1: Prepare Flash Tool

**On your workstation:**

```bash
# Clone or update repository
git clone https://github.com/rvdbreemen/OTGW-firmware.git
cd OTGW-firmware

# Checkout dev branch
git checkout dev
git pull origin dev

# Install Python dependencies (if needed)
# The flash script will auto-install esptool if needed
```

**Verify flash_esp.py is available:**

```bash
# Check if script exists
ls -l flash_esp.py

# Make executable (Linux/Mac)
chmod +x flash_esp.py
```

---

### Step 2: Connect to Device

**USB Connection Method (Recommended):**

1. Connect ESP8266 device to workstation via USB
2. Identify serial port:
   - **Linux:** `/dev/ttyUSB0` or `/dev/ttyACM0`
   - **Mac:** `/dev/cu.usbserial-*`
   - **Windows:** `COM3` or similar

**Verify connection:**

```bash
# Linux/Mac
ls -l /dev/tty*

# Windows (in PowerShell)
Get-WmiObject Win32_SerialPort
```

---

### Step 3: Flash Firmware

**Auto-flash from latest release (Easiest):**

```bash
# Flash firmware and filesystem
python flash_esp.py

# Or with auto-confirm flag
python flash_esp.py -y
```

**Flash from local build:**

```bash
# Build firmware first
python build.py

# Then flash
python flash_esp.py --build
```

**Expected output:**

```
OTGW Firmware Flash Utility v1.0
=================================
1. Checking for esptool...
   ✅ esptool installed
2. Fetching latest release...
   ✅ Latest release: v1.0.0-rc5
3. Downloading firmware...
   ✅ firmware.bin (512 KB)
   ✅ filesystem.bin (128 KB)
4. Erasing flash...
   ✅ Flash erased
5. Writing firmware...
   [████████████████████] 100%
   ✅ Firmware written
6. Writing filesystem...
   [████████████████████] 100%
   ✅ Filesystem written
7. Rebooting device...
   ✅ Device rebooted

✅ Flashing complete!
```

**If flashing fails:**

1. Check USB connection
2. Verify serial port is correct
3. Try with `--port /dev/ttyUSB0` flag
4. Check device is in bootloader mode (GPIO0 to GND on boot)
5. See troubleshooting section below

---

### Step 4: Initial Setup

**First boot after flash:**

1. Device will reboot automatically
2. Wait 30-60 seconds for boot
3. Connect to WiFi:
   - If previous WiFi credentials saved: Auto-reconnect
   - If not: Look for `ESP-OTGW-XXXXXX` AP, connect, configure WiFi

**Verify boot:**

```bash
# Check if device is responding
curl http://<device-ip>/api/v1/health

# Expected response:
# {"status":"UP","heap":25600,"picavailable":true,...}
```

---

### Step 5: Restore Configuration

**Via REST API (Recommended):**

```bash
# Restore settings (edit as needed)
curl -X PUT http://<device-ip>/api/v1/settings \
  -H "Content-Type: application/json" \
  -d @settings-backup.json
```

**Via Web UI:**

1. Navigate to `http://<device-ip>`
2. Go to **Settings**
3. Re-enter:
   - MQTT broker details
   - MQTT username and password
   - Dallas sensor GPIO pin
   - Other custom settings

---

### Step 6: Dallas Sensor Reconfiguration (If Applicable)

**If you use Dallas DS18B20 temperature sensors:**

#### Option A: Use New Sensor ID Format (Recommended)

1. Navigate to **Settings → Sensors**
2. **Legacy Format:** Ensure it's **disabled** (default)
3. Wait for sensors to be discovered
4. Note new sensor IDs (hex format)
5. Update Home Assistant:
   - Delete old MQTT sensor entities
   - Wait for MQTT Auto Discovery to recreate entities
   - New entities will have new names/IDs

**Example:**
- Old ID: `sensor_01` → New ID: `sensor_28ff641e8117013c`

#### Option B: Use Legacy Format (For Backward Compatibility)

1. Navigate to **Settings → Sensors**
2. **Legacy Format:** Enable (check the box)
3. Save settings
4. Reboot device
5. Old sensor IDs will be preserved
6. No changes needed in Home Assistant

**Recommendation:** Use Option A (new format) for better reliability.

---

### Step 7: MQTT Integration Verification

**Check MQTT connection:**

```bash
# Via REST API
curl http://<device-ip>/api/v1/settings/mqtt

# Expected: Connection status = "Connected"
```

**Verify MQTT messages:**

1. Use MQTT client (e.g., MQTT Explorer, mosquitto_sub)
2. Subscribe to: `<your-mqtt-prefix>/#`
3. Verify messages are being published
4. Check Home Assistant entities

**Home Assistant Auto Discovery:**

1. Navigate to **Configuration → Integrations**
2. Look for "MQTT" integration
3. New entities should appear automatically
4. Check for climate entity (OpenTherm thermostat)
5. Check for sensor entities (Dallas sensors, OpenTherm values)

**If entities are missing:**

1. Check MQTT broker logs
2. Verify `settingMqttBroker` and `settingMqttUsername` are correct
3. Restart Home Assistant
4. Wait 2-3 minutes for auto discovery
5. Manually trigger discovery: `http://<device-ip>/api/v1/dev/mqtt-discover`

---

### Step 8: Verification Testing

**Run comprehensive tests:**

1. **Heap Check:**
   ```bash
   curl http://<device-ip>/api/v1/dev/info
   # Check heap: Should be >20,000 bytes free
   ```

2. **Dallas Sensors:**
   - Verify sensor readings in Web UI
   - Check MQTT topics publish correctly
   - Verify Home Assistant entities show data

3. **MQTT Stability:**
   - Monitor for 30 minutes
   - Check for heap exhaustion
   - Verify continuous publishing

4. **PIC Firmware Info:**
   ```bash
   curl http://<device-ip>/api/v1/otgw/version
   # Should return PIC firmware version
   ```

5. **Web UI:**
   - Access `http://<device-ip>`
   - Check all pages load correctly
   - Verify graphs display data

6. **File System:**
   - Navigate to FSexplorer (`http://<device-ip>/FSexplorer.html`)
   - Verify file list loads
   - Test file download

---

## Breaking Changes and Adaptations

### Dallas DS18B20 Sensor ID Format Change

**Change:** Sensor IDs now use hex-based address format.

**Old Format:** `sensor_01`, `sensor_02`, etc.  
**New Format:** `sensor_28ff641e8117013c`, `sensor_28ff123456789abc`, etc.

**Impact:**
- MQTT topics change: `otgw-firmware/sensor_01/temp` → `otgw-firmware/sensor_28ff641e8117013c/temp`
- Home Assistant entity IDs change
- Automations referencing old entity IDs will break

**Migration Strategies:**

#### Strategy 1: Accept New IDs (Recommended)

1. Let MQTT Auto Discovery recreate entities
2. Update Home Assistant automations to use new entity IDs
3. More reliable and future-proof

#### Strategy 2: Use Legacy Format

1. Enable "Legacy Format" in sensor settings
2. Old entity IDs preserved
3. No automation changes needed
4. Less reliable (intentionally replicates old bug for compatibility)

#### Strategy 3: Hybrid (Migration Path)

1. Start with legacy format enabled
2. Note which automations use sensors
3. Gradually update automations to use new IDs
4. Disable legacy format when ready
5. Clean migration with controlled timeline

---

## Rollback Procedure

**If migration fails or issues occur:**

### Option 1: Re-flash dev-rc4-branch

```bash
# Checkout dev-rc4-branch
git checkout dev-rc4-branch
git pull origin dev-rc4-branch

# Build and flash
python build.py
python flash_esp.py --build
```

### Option 2: Flash Previous Release

```bash
# Download specific release
# Visit: https://github.com/rvdbreemen/OTGW-firmware/releases
# Download firmware-rc4.bin and filesystem-rc4.bin

# Flash manually
esptool.py --port /dev/ttyUSB0 write_flash \
  0x00000 firmware-rc4.bin \
  0x200000 filesystem-rc4.bin
```

### Option 3: Factory Reset

1. Hold reset button on device
2. Wait for WiFi portal mode
3. Reconfigure from scratch
4. Restore settings from backup

---

## Troubleshooting

### Issue: Flash Fails with "Failed to connect"

**Cause:** ESP8266 not in bootloader mode

**Solution:**
1. Disconnect device
2. Hold GPIO0 button (or jumper GPIO0 to GND)
3. Reconnect device (while holding GPIO0)
4. Release GPIO0 after 3 seconds
5. Retry flash

---

### Issue: Device not responding after flash

**Cause:** Bad flash or configuration issue

**Solution:**
1. Wait 2 minutes (initial boot is slow)
2. Check serial console for boot messages
3. Try factory reset (hold button, power on)
4. Re-flash with `--erase-all` flag:
   ```bash
   esptool.py --port /dev/ttyUSB0 erase_flash
   python flash_esp.py
   ```

---

### Issue: Dallas sensors not detected

**Cause:** GPIO pin configuration or wiring issue

**Solution:**
1. Check GPIO pin setting (Settings → Sensors)
2. Default is GPIO10 (in dev), was GPIO13 (in some versions)
3. Verify physical wiring matches configured pin
4. Check 4.7kΩ pull-up resistor on data line
5. Enable debug telnet (port 23) to see detection logs

---

### Issue: MQTT not connecting

**Cause:** Configuration error or broker issue

**Solution:**
1. Verify MQTT broker address and port
2. Check username/password (re-enter password)
3. Test broker connectivity from workstation:
   ```bash
   mosquitto_sub -h <broker-ip> -p 1883 -u <user> -P <pass> -t "#"
   ```
4. Check broker logs for connection attempts
5. Verify firewall allows port 1883

---

### Issue: Home Assistant entities missing

**Cause:** MQTT Auto Discovery issue

**Solution:**
1. Wait 3-5 minutes for discovery
2. Restart Home Assistant
3. Check MQTT integration is enabled in HA
4. Manually trigger discovery:
   ```bash
   curl http://<device-ip>/api/v1/dev/mqtt-discover
   ```
5. Check MQTT broker for discovery messages (topic: `homeassistant/...`)

---

### Issue: Heap exhaustion or crashes

**Cause:** Memory leak or configuration issue

**Solution:**
1. Check heap usage:
   ```bash
   curl http://<device-ip>/api/v1/dev/info
   ```
2. If heap <10,000 bytes, investigate:
   - Disable WebSocket logging (if active)
   - Reduce number of Dallas sensors (max 10 recommended)
   - Disable unnecessary features
3. Upgrade to latest dev commit (may have fix)
4. Report issue on GitHub with heap log

---

## Post-Migration Monitoring

### Week 1: Intensive Monitoring

**Daily checks:**
- [ ] Heap usage (should be stable >20KB)
- [ ] MQTT connectivity (should be "Connected")
- [ ] Dallas sensor readings (should be consistent)
- [ ] Home Assistant entity updates (should be regular)
- [ ] Device uptime (should not reboot unexpectedly)

**Monitor via REST API:**

```bash
# Create monitoring script (monitor.sh)
#!/bin/bash
while true; do
  echo "$(date): Heap=$(curl -s http://<device-ip>/api/v1/dev/info | jq .heap)"
  sleep 300  # Every 5 minutes
done
```

**Expected behavior:**
- Heap: 20,000 - 30,000 bytes (stable)
- Uptime: Continuous (no unexpected reboots)
- MQTT: Connected (no disconnections)

---

### Month 1: Stability Validation

**Weekly checks:**
- [ ] Uptime (should be >7 days between reboots)
- [ ] Heap fragmentation (should be minimal)
- [ ] MQTT message delivery (should be 100%)
- [ ] Dallas sensor accuracy (compare to baseline)

**Success Criteria:**
- No crashes or unexpected reboots
- Heap remains stable (no downward trend)
- MQTT messages reliably delivered
- Dallas sensors reporting accurate data

---

## Advanced Topics

### OTA Firmware Updates (Future)

**Note:** OTA updates are available in dev but use with caution for PIC firmware.

**For ESP firmware:**
```bash
# Via Web UI
http://<device-ip>/update

# Upload firmware.bin
# Device will reboot automatically
```

**For PIC firmware:**
⚠️ **WARNING:** Do NOT flash PIC firmware over WiFi (can brick the PIC).

**Safe method:**
1. Use Web UI to upload hex file to filesystem
2. Trigger flash via Web UI (wired connection recommended)
3. Monitor progress bar
4. Wait for completion (2-3 minutes)

---

### Evaluation Framework

**New in dev:** Code quality evaluation

```bash
# Run full evaluation
python evaluate.py

# Quick check
python evaluate.py --quick

# Generate report
python evaluate.py --report
```

**Output:**
- Code structure analysis
- Memory usage patterns
- Security checks
- Build system health

---

## Success Metrics

### Migration is Successful When:

- ✅ Device boots and responds to ping
- ✅ Web UI accessible
- ✅ MQTT connected and publishing
- ✅ Dallas sensors detected and reporting (if applicable)
- ✅ Home Assistant entities recreated and updating
- ✅ Heap stable >20KB
- ✅ No crashes or reboots for 24 hours
- ✅ All expected features working

### Migration May Need Adjustment When:

- ⚠️ Heap <15KB (investigate configuration)
- ⚠️ Dallas sensors not detected (check GPIO pin)
- ⚠️ MQTT not connecting (verify broker settings)
- ⚠️ Home Assistant entities missing (trigger discovery)

### Migration Failed When:

- ❌ Device won't boot (re-flash or factory reset)
- ❌ Constant reboots (check serial console, report bug)
- ❌ MQTT never connects (rollback, investigate)
- ❌ Critical feature broken (report bug, consider rollback)

---

## Support and Resources

### Documentation

- **Main Analysis:** `DEV_RC4_COMPREHENSIVE_ANALYSIS.md`
- **Feature Comparison:** `FEATURE_COMPARISON_MATRIX.md`
- **Executive Summary:** `EXECUTIVE_SUMMARY.md`
- **Build Guide:** `BUILD.md`
- **Flash Guide:** `FLASH_GUIDE.md` (in dev branch)
- **Evaluation Guide:** `EVALUATION.md`

### Community Support

- **GitHub Issues:** https://github.com/rvdbreemen/OTGW-firmware/issues
- **GitHub Discussions:** https://github.com/rvdbreemen/OTGW-firmware/discussions
- **OTGW Forum:** https://otgw.tclcode.com/support/forum

### Emergency Contact

If critical production issues occur:
1. Open GitHub issue with "URGENT" tag
2. Provide full logs and configuration
3. Heap dump: `curl http://<device-ip>/api/v1/dev/info`
4. Consider rollback to previous version

---

## Appendix: Command Reference

### Quick Command Cheat Sheet

```bash
# Flash firmware (easiest)
python flash_esp.py -y

# Build and flash
python build.py && python flash_esp.py --build

# Check device health
curl http://<device-ip>/api/v1/health

# Check heap usage
curl http://<device-ip>/api/v1/dev/info

# Get device settings
curl http://<device-ip>/api/v1/settings

# Reboot device
curl -X POST http://<device-ip>/api/v1/dev/reboot

# Trigger MQTT discovery
curl -X POST http://<device-ip>/api/v1/dev/mqtt-discover

# Run code evaluation
python evaluate.py --quick
```

---

## Conclusion

Migrating from dev-rc4-branch to dev is straightforward and highly recommended. The dev branch includes all dev-rc4-branch features plus critical bug fixes, security improvements, and modern tooling.

**Estimated total migration time per device:** 5-10 minutes  
**Recommended rollout:** Test one device first, then mass deploy  
**Support:** Available via GitHub issues and community forum

**Remember:** Backup configuration before starting, and test thoroughly after migration!

---

**Document End**
