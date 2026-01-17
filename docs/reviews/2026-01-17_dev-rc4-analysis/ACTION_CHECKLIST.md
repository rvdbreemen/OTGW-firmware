---
# METADATA
Document Title: Action Checklist for dev-RC4-branch Fixes
Review Date: 2026-01-17 10:26:28 UTC
Branch Reviewed: dev-rc4-branch → dev (merge commit 9f918e9)
Target Version: v1.0.0-rc4
Reviewer: GitHub Copilot Advanced Agent
Document Type: Implementation Checklist
PR Branch: copilot/review-dev-rc4-branch
Commit: 575f92b
Status: ALL ITEMS COMPLETED
---

# Action Checklist for dev-RC4-branch

This checklist provides the developer with concrete steps to fix the issues identified in the code review.

---

## 🔴 CRITICAL - Fix Before Merge (3 hours)

### [ ] Issue #1: Fix Binary Data Parsing (30 min)

**Files to modify:**
- `versionStuff.ino` (lines 85-99)
- `src/libraries/OTGWSerial/OTGWSerial.cpp` (lines 299-313)

**Problem:** Using `strstr()` and `strnlen()` on binary hex file data causes Exception (2) crashes

**Action Steps:**
1. Open `versionStuff.ino`
2. Replace the `while (ptr < 256)` loop with the safe implementation:
```cpp
size_t bannerLen = sizeof(banner) - 1;
if (256 >= bannerLen) {
  for (ptr = 0; ptr <= (256 - bannerLen); ptr++) {
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

3. Apply same fix to `OTGWSerial.cpp` readHexFile()
4. Test with actual PIC hex files
5. Verify no Exception (2) crashes

**Verification:**
```bash
# Flash test - should complete without crashes
python flash_esp.py
# Check for Exception crashes in serial monitor
```

---

### [ ] Issue #2: Fix MQTT Buffer Strategy (2 hours)

**Files to modify:**
- `MQTTstuff.ino`

**Problem:** Dynamic buffer resizing causes heap fragmentation on ESP8266

**Option A: Revert to Static (RECOMMENDED)**

1. In `startMQTT()` function (line ~163):
```cpp
void startMQTT() {
  if (!settingMQTTenable) return;
  
  // STATIC BUFFER STRATEGY for ESP8266
  // Prevents heap fragmentation on resource-constrained device
  MQTTclient.setBufferSize(1350);
  
  stateMQTT = MQTT_STATE_INIT;
  clearMQTTConfigDone();
}
```

2. Remove dynamic resizing from `doAutoConfigure()` (lines 634-643):
```cpp
// DELETE these lines:
// size_t requiredSize = msgLen + topicLen + 128;
// if (currentSize < requiredSize) {
//    MQTTclient.setBufferSize(requiredSize);
// }
```

3. Remove `resetMQTTBufferSize()` calls (lines 655, 792, 805)

4. Delete the `resetMQTTBufferSize()` function entirely (lines 519-522)

**Option B: Justify Dynamic (NOT RECOMMENDED without data)**

1. Add heap monitoring:
```cpp
void doAutoConfigure() {
  uint32_t heapBefore = ESP.getFreeHeap();
  // ... existing code ...
  uint32_t heapAfter = ESP.getFreeHeap();
  MQTTDebugTf(PSTR("Heap: before=%d after=%d delta=%d\r\n"), 
              heapBefore, heapAfter, (int32_t)heapBefore - (int32_t)heapAfter);
}
```

2. Run 24-hour test with heap fragmentation monitoring
3. Provide data showing dynamic is better than static
4. Document findings in commit message

**Verification:**
```bash
# Monitor heap over 24h
# Check for fragmentation
# Verify MQTT messages don't fail
```

---

### [ ] Test Critical Fixes (30 min)

1. Flash firmware to actual hardware
2. Test PIC firmware flashing
3. Monitor for Exception crashes
4. Check MQTT AutoDiscovery works
5. Monitor heap for 1 hour minimum

---

## 🟠 HIGH Priority - Fix Before v1.0.0 (3 hours)

### [ ] Issue #3: Apply PROGMEM to String Literals (1 hour)

**Files to audit:**
- `versionStuff.ino`
- `MQTTstuff.ino`
- `sensors_ext.ino`

**Action Steps:**

1. Search for string literals in changed files:
```bash
grep -n '"[^"]*"' versionStuff.ino MQTTstuff.ino sensors_ext.ino
```

2. Apply `F()` macro to Debug statements:
```cpp
// BEFORE:
DebugTf("Error: %s\r\n", error);

// AFTER:
DebugTf(PSTR("Error: %s\r\n"), error);
```

3. Apply `PSTR()` to sprintf-style functions:
```cpp
// BEFORE:
snprintf(buffer, size, "Format: %d", val);

// AFTER:
snprintf_P(buffer, size, PSTR("Format: %d"), val);
```

4. Use `strcasecmp_P()` for PROGMEM comparisons:
```cpp
// BEFORE:
if (strcmp(field, "value") == 0)

// AFTER:
if (strcasecmp_P(field, PSTR("value")) == 0)
```

**Verification:**
```bash
# Build and check flash usage
python build.py
# Should see reduced RAM usage
```

---

### [ ] Issue #4: Remove Legacy Format Mode (2 hours)

**Files to modify:**
- `sensors_ext.ino` (remove legacy code)
- `OTGW-firmware.h` (remove setting)
- `settingStuff.ino` (remove setting handling)
- `data/index.js` (remove UI element)
- `README.md` (add migration guide)

**Action Steps:**

1. **Remove legacy code from `sensors_ext.ino`:**
```cpp
char* getDallasAddress(DeviceAddress deviceAddress) {
  static char dest[17];
  static const char hexchars[] PROGMEM = "0123456789ABCDEF";
  
  // DELETE the entire if (settingGPIOSENSORSlegacyformat) block
  
  // KEEP only the correct implementation:
  for (uint8_t i = 0; i < 8; i++) {
    uint8_t b = deviceAddress[i];
    dest[i*2]   = pgm_read_byte(&hexchars[b >> 4]);
    dest[i*2+1] = pgm_read_byte(&hexchars[b & 0x0F]);
  }
  dest[16] = '\0';
  return dest;
}
```

2. **Remove setting from `OTGW-firmware.h`:**
```cpp
// DELETE this line:
// bool settingGPIOSENSORSlegacyformat = false;
```

3. **Remove from `settingStuff.ino`:**
- Delete line reading/writing `GPIOSENSORSlegacyformat` in `readSettings()`
- Delete line in `writeSettings()`
- Delete `updateSetting()` handler

4. **Update README.md:**
```markdown
## Breaking Changes in v1.0.0-rc4

### Dallas DS18B20 Sensor ID Format

**IMPORTANT:** Sensor IDs changed from buggy 9-10 char format to correct 16-char format.

**Migration Steps:**
1. Update firmware
2. Check new sensor IDs: MQTT topic `{prefix}/value/{sensorID}`
3. Update Home Assistant automations

**Example:**
- Old: `28FF641E8` (incorrect)
- New: `28FF641E8216C3A1` (correct full address)

**Time required:** ~10 minutes
```

**Verification:**
```bash
# Ensure setting doesn't appear in UI
# Verify sensors work with new format
# Check MQTT topics are correct
```

---

## 🟡 MEDIUM Priority - Fix Soon (2 hours)

### [ ] Issue #5: SafeTimers Random Offset (30 min)

**File:** `safeTimers.h`

**Option A: Restore Random Offset (RECOMMENDED)**
```cpp
#define DECLARE_TIMER_SEC(timerName, ...) \
  static uint32_t timerName##_interval = (getParam(0, __VA_ARGS__, 0) * 1000),\
                  timerName##_due  = millis() + timerName##_interval \
                                    + random(timerName##_interval / 3);
```

**Option B: Document Removal**
```cpp
// Random offset removed in v1.0.0 to ensure predictable timer firing
// All timers now fire synchronously at their exact intervals
// This may cause temporary CPU load spikes when multiple timers fire simultaneously
```

---

### [ ] Issue #6: Archive Deleted Files (15 min)

**Action:**
```bash
# Create archive directory
mkdir -p docs/archive/rc3-rc4-transition

# Restore deleted files from git
git show dev:OTGWSerial_PR_Description.md > docs/archive/rc3-rc4-transition/OTGWSerial_PR_Description.md
git show dev:PIC_Flashing_Fix_Analysis.md > docs/archive/rc3-rc4-transition/PIC_Flashing_Fix_Analysis.md
git show dev:example-api/API_CHANGES_v1.0.0.md > docs/archive/rc3-rc4-transition/API_CHANGES_v1.0.0.md

# Add README
echo "# Archived Documentation from RC3 to RC4 Transition" > docs/archive/rc3-rc4-transition/README.md
echo "These files were removed during v1.0.0-rc4 but preserved for reference." >> docs/archive/rc3-rc4-transition/README.md
```

---

### [ ] Issue #7: Document GPIO Pin Change (30 min)

**File:** `README.md`

**Add section:**
```markdown
## Breaking Changes

### Default GPIO Pin Changed (GPIO 13 → GPIO 10)

**Affects:** Users with Dallas DS18B20 sensors

**Old default:** GPIO 13 (D7 on NodeMCU)  
**New default:** GPIO 10 (SDIO 3)

**Action required:**
- If using GPIO 13: Update settings to explicitly set pin to 13
- If using default: Reconnect sensors to GPIO 10 OR change setting

**Why changed:** GPIO 10 is the OTGW hardware default for sensors.
```

---

### [ ] Issue #8: Add Error Handling (15 min)

**File:** `MQTTstuff.ino`

**In `doAutoConfigure()`:**
```cpp
// After allocations:
char *sLine = new char[MQTT_CFG_LINE_MAX_LEN];
char *sTopic = new char[MQTT_TOPIC_MAX_LEN];
char *sMsg = new char[MQTT_MSG_MAX_LEN];

// ADD null checks:
if (!sLine || !sTopic || !sMsg) {
  DebugTln(F("ERROR: Out of memory in doAutoConfigure"));
  if (sLine) delete[] sLine;
  if (sTopic) delete[] sTopic;
  if (sMsg) delete[] sMsg;
  return;
}
```

---

## Final Verification Checklist

### [ ] Build Test
```bash
python build.py --clean
# Should complete without errors
```

### [ ] Flash Test
```bash
python flash_esp.py --build
# Should flash successfully
```

### [ ] Runtime Tests
- [ ] PIC firmware flashing works
- [ ] MQTT AutoDiscovery works
- [ ] Dallas sensors publish correctly
- [ ] No Exception crashes
- [ ] No memory leaks

### [ ] Code Quality
```bash
python evaluate.py
# Should show no critical issues
```

### [ ] Documentation
- [ ] README.md updated with breaking changes
- [ ] Migration guide complete
- [ ] All changes documented

---

## Estimated Time Investment

| Phase | Time | Tasks |
|-------|------|-------|
| Critical Fixes | 3h | Binary parsing + MQTT buffer |
| High Priority | 3h | PROGMEM + Legacy removal |
| Medium Priority | 2h | SafeTimers + Documentation |
| Testing | 2h | Comprehensive validation |
| **TOTAL** | **10h** | Complete fix implementation |

---

## Success Criteria

### Merge to dev-rc4 branch:
- ✅ All critical issues fixed
- ✅ Build completes successfully
- ✅ Basic runtime tests pass

### Merge to main branch:
- ✅ All critical + high priority issues fixed
- ✅ 24-hour stability test passed
- ✅ All documentation updated
- ✅ Migration guide complete
- ✅ No regressions detected

---

## Notes

- Keep the good changes (#9-14)
- Don't over-engineer solutions
- Test on actual hardware, not just emulator
- Document all design decisions
- Preserve git history

---

**Priority Order:**
1. Fix binary parsing (prevents crashes)
2. Fix MQTT buffer (prevents instability)
3. Apply PROGMEM (ESP8266 requirement)
4. Remove legacy mode (technical debt)
5. Everything else

**Good luck! 🚀**
