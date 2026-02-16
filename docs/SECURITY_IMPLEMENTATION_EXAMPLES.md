# Security Improvement Implementation Examples

**Full Analysis:** [SECURITY_IMPROVEMENT_ANALYSIS.md](SECURITY_IMPROVEMENT_ANALYSIS.md)  
**Quick Reference:** [SECURITY_IMPROVEMENT_OPTIONS.md](SECURITY_IMPROVEMENT_OPTIONS.md)  
**Date:** 2026-02-16

---

## Implementation Code Examples

This document provides copy-paste-ready code examples for implementing the recommended security improvements.

---

## 1. WiFi AP Password (RECOMMENDED - Phase 1)

### Files to Modify
- `src/OTGW-firmware/networkStuff.h` - Add password generation

### Implementation

```cpp
// In networkStuff.h, modify startWiFi() function

void startWiFi(const char* hostname, int timeOut) {
  WiFi.mode(WIFI_STA);
  WiFiManager manageWiFi;
  
  char thisAP[64];
  char apPassword[12];  // NEW: Password buffer
  
  // Generate unique AP SSID (existing code)
  strlcpy(thisAP, hostname, sizeof(thisAP));
  strlcat(thisAP, "-", sizeof(thisAP));
  strlcat(thisAP, WiFi.macAddress().c_str(), sizeof(thisAP));
  
  // NEW: Generate unique AP password from chip ID
  snprintf_P(apPassword, sizeof(apPassword), 
    PSTR("OTGW%04X"), (ESP.getChipId() & 0xFFFF));
  
  // NEW: Display password during setup phase
  SetupDebugln(F("========================================"));
  SetupDebugf(PSTR("WiFi AP SSID:     %s\r\n"), thisAP);
  SetupDebugf(PSTR("WiFi AP Password: %s\r\n"), apPassword);
  SetupDebugln(F("Connect to this AP to configure WiFi"));
  SetupDebugln(F("========================================"));
  
  DebugTln(F("\nStart Wifi ..."));
  manageWiFi.setDebugOutput(true);
  
  manageWiFi.setAPCallback(configModeCallback);
  manageWiFi.setTimeout(timeOut);
  
  // Security improvements (existing)
  std::vector<const char *> wm_menu = {"wifi", "exit"};
  manageWiFi.setShowInfoUpdate(false);
  manageWiFi.setShowInfoErase(false);
  manageWiFi.setMenu(wm_menu);
  manageWiFi.setHostname(hostname);
  
  // NEW: Set AP password
  manageWiFi.setAPPassword(apPassword);
  
  // Existing connection logic
  bool wifiSaved = manageWiFi.getWiFiIsSaved();
  bool wifiConnected = (WiFi.status() == WL_CONNECTED);
  
  DebugTf(PSTR("Wifi status: %s\r\n"), wifiConnected ? "Connected" : "Not connected");
  DebugTf(PSTR("Wifi AP stored: %s\r\n"), wifiSaved ? "Yes" : "No");
  DebugTf(PSTR("Config portal SSID: %s\r\n"), thisAP);
  
  if (wifiConnected) {
    DebugTln(F("Wifi already connected, skipping connect."));
  } else if (wifiSaved) {
    // Existing direct connect code...
  } else {
    // NEW: Pass password to autoConnect
    if (!manageWiFi.autoConnect(thisAP, apPassword)) {
      DebugTln(F("Failed to connect and timeout"));
      ESP.restart();
    }
  }
  
  DebugTln(F("WiFi connected"));
}
```

### Testing Steps

1. Flash firmware with changes
2. Power on device
3. Connect USB cable and open Serial monitor (9600 baud)
4. Look for output:
   ```
   ========================================
   WiFi AP SSID:     OTGW-AA:BB:CC:DD:EE:FF
   WiFi AP Password: OTGW1A2B
   Connect to this AP to configure WiFi
   ========================================
   ```
5. Scan WiFi networks
6. Connect to AP using displayed password
7. Configure WiFi settings

### Documentation Updates

**README.md - Add to "First-Time Setup" section:**

```markdown
### First-Time WiFi Setup

1. Power on the device
2. **Important:** Connect a USB cable and open a Serial monitor at 9600 baud
3. Note the WiFi AP SSID and Password displayed:
   ```
   WiFi AP SSID:     OTGW-AA:BB:CC:DD:EE:FF
   WiFi AP Password: OTGW1A2B
   ```
4. On your phone/laptop, scan WiFi networks
5. Connect to the displayed AP SSID using the displayed password
6. Captive portal should open automatically
7. Select your home WiFi network and enter the password
8. Device will reboot and connect to your home network

**Note:** The AP password is unique to your device and derived from its chip ID.
You can find it printed on the Serial console during first boot.
```

---

## 2. Optional Telnet Disabling (RECOMMENDED - Phase 1)

### Files to Modify
- `src/OTGW-firmware/OTGW-firmware.h` - Add setting
- `src/OTGW-firmware/networkStuff.h` - Modify startTelnet()
- `src/OTGW-firmware/handleDebug.ino` - Add conditional
- `src/OTGW-firmware/settingStuff.ino` - Add to read/write settings
- `src/OTGW-firmware/restAPI.ino` - Add to device settings
- `src/OTGW-firmware/data/index.html` - Add UI toggle

### Implementation

**1. Add setting in OTGW-firmware.h:**
```cpp
// After settingDarkTheme line
bool      settingTelnetEnabled = true;  // Default: enabled for backward compatibility
```

**2. Modify startTelnet() in networkStuff.h:**
```cpp
void startTelnet() 
{
  if (!settingTelnetEnabled) {
    DebugTln(F("Telnet disabled by user setting"));
    return;  // Don't start telnet server
  }
  
  DebugT(F("\r\nUse  'telnet "));
  DebugT(WiFi.localIP());
  DebugTln(F("' for debugging"));
  TelnetStream.begin();
  DebugTln(F("\nTelnet server started .."));
  TelnetStream.flush();
}
```

**3. Conditional in handleDebug.ino (if separate file):**
```cpp
void handleDebug() {
  if (!settingTelnetEnabled) return;  // Skip if disabled
  
  // Existing debug command handling...
}
```

**4. Add to settingStuff.ino - writeSettings():**
```cpp
root[F("TelnetEnabled")] = settingTelnetEnabled;
```

**5. Add to settingStuff.ino - readSettings():**
```cpp
settingTelnetEnabled = doc[F("TelnetEnabled")] | settingTelnetEnabled;
```

**6. Add to settingStuff.ino - updateSetting():**
```cpp
if (strcasecmp_P(field, PSTR("TelnetEnabled"))==0) {
  settingTelnetEnabled = (strcasecmp_P(newValue, PSTR("true")) == 0 || 
                          strcasecmp_P(newValue, PSTR("on")) == 0 || 
                          strcasecmp_P(newValue, PSTR("1")) == 0);
}
```

**7. Add to restAPI.ino - sendDeviceSettings():**
```cpp
sendJsonSettingObj(F("telnetenabled"), settingTelnetEnabled, "b");
```

**8. Add to index.html - Settings section:**
```html
<div class="setting-item">
  <label>
    <input type="checkbox" id="telnetenabled" data-setting="telnetenabled" />
    Enable Telnet Debug Console (Port 23)
  </label>
  <span class="help-text">
    ⚠️ Requires reboot to take effect. Disabling improves security but removes debug console access.
  </span>
</div>
```

---

## 3. Optional OTmonitor Disabling (RECOMMENDED - Phase 1)

### Files to Modify
- `src/OTGW-firmware/OTGW-firmware.h` - Add setting
- `src/OTGW-firmware/OTGW-Core.ino` - Modify startOTGWstream() and handleOTGW()
- `src/OTGW-firmware/settingStuff.ino` - Add to read/write settings
- `src/OTGW-firmware/restAPI.ino` - Add to device settings
- `src/OTGW-firmware/data/index.html` - Add UI toggle

### Implementation

**1. Add setting in OTGW-firmware.h:**
```cpp
// After settingTelnetEnabled line
bool      settingOTmonitorEnabled = true;  // Default: enabled for backward compatibility
```

**2. Modify startOTGWstream() in OTGW-Core.ino:**
```cpp
void startOTGWstream() {
  if (!settingOTmonitorEnabled) {
    DebugTln(F("OTmonitor port disabled by user setting"));
    return;  // Don't start TCP server
  }
  
  OTGWstream.begin();
  DebugTf(PSTR("OTmonitor port started on port %d\r\n"), OTGW_SERIAL_PORT);
}
```

**3. Modify handleOTGW() in OTGW-Core.ino:**
```cpp
void handleOTGW() {
  // Always process Serial -> internal
  while (Serial.available()) {
    int c = Serial.read();
    // Process for MQTT/WebSocket/internal state
    // ... existing code ...
  }
  
  if (!settingOTmonitorEnabled) {
    return;  // Skip network forwarding
  }
  
  // Network forwarding only if enabled
  while (OTGWstream.available()) {
    int c = OTGWstream.read();
    OTGWSerial.write(c);
  }
}
```

**4. Add to settingStuff.ino - writeSettings():**
```cpp
root[F("OTmonitorEnabled")] = settingOTmonitorEnabled;
```

**5. Add to settingStuff.ino - readSettings():**
```cpp
settingOTmonitorEnabled = doc[F("OTmonitorEnabled")] | settingOTmonitorEnabled;
```

**6. Add to settingStuff.ino - updateSetting():**
```cpp
if (strcasecmp_P(field, PSTR("OTmonitorEnabled"))==0) {
  settingOTmonitorEnabled = (strcasecmp_P(newValue, PSTR("true")) == 0 || 
                             strcasecmp_P(newValue, PSTR("on")) == 0 || 
                             strcasecmp_P(newValue, PSTR("1")) == 0);
}
```

**7. Add to restAPI.ino - sendDeviceSettings():**
```cpp
sendJsonSettingObj(F("otmonitorenabled"), settingOTmonitorEnabled, "b");
```

**8. Add to index.html - Settings section:**
```html
<div class="setting-item">
  <label>
    <input type="checkbox" id="otmonitorenabled" data-setting="otmonitorenabled" />
    Enable OTmonitor TCP Socket (Port 25238)
  </label>
  <span class="help-text">
    ⚠️ Requires reboot to take effect. 
    <br><strong>WARNING:</strong> Disabling breaks OTmonitor app and Home Assistant OpenTherm Gateway integration.
    <br>MQTT Auto-Discovery continues to work normally.
  </span>
</div>
```

---

## 4. Optional Web UI Authentication (CONDITIONAL - Phase 2)

**Note:** Only implement this if there is strong community demand. This breaks Home Assistant integration.

### Files to Modify
- `src/OTGW-firmware/OTGW-firmware.h` - Add settings
- `src/OTGW-firmware/restAPI.ino` - Add authentication middleware
- `src/OTGW-firmware/settingStuff.ino` - Add to read/write settings
- `src/OTGW-firmware/data/index.html` - Add UI settings

### Implementation

**1. Add settings in OTGW-firmware.h:**
```cpp
// After settingOTmonitorEnabled line
bool      settingWebAuthEnabled = false;  // Default: disabled
char      settingWebPassword[41] = "";     // Empty = disabled
```

**2. Add authentication helper in restAPI.ino:**
```cpp
bool requireAuth() {
  if (!settingWebAuthEnabled || strlen(settingWebPassword) == 0) {
    return true;  // Auth disabled or no password set
  }
  
  return httpServer.authenticate("admin", settingWebPassword);
}
```

**3. Wrap all handlers in restAPI.ino:**
```cpp
// Example for root handler
httpServer.on("/", []() {
  if (!requireAuth()) {
    return httpServer.requestAuthentication();
  }
  
  // Serve index.html (existing code)
});

// Repeat for ALL other handlers
httpServer.on("/api/v2/settings", []() {
  if (!requireAuth()) {
    return httpServer.requestAuthentication();
  }
  
  // Handle settings (existing code)
});

// ... etc for all endpoints
```

**4. Add to settingStuff.ino - writeSettings():**
```cpp
root[F("WebAuthEnabled")] = settingWebAuthEnabled;
root[F("WebPassword")] = settingWebPassword;
```

**5. Add to settingStuff.ino - readSettings():**
```cpp
settingWebAuthEnabled = doc[F("WebAuthEnabled")] | settingWebAuthEnabled;
strlcpy(settingWebPassword, doc[F("WebPassword")] | "", sizeof(settingWebPassword));
```

**6. Add to settingStuff.ino - updateSetting():**
```cpp
if (strcasecmp_P(field, PSTR("WebAuthEnabled"))==0) {
  settingWebAuthEnabled = (strcasecmp_P(newValue, PSTR("true")) == 0 || 
                           strcasecmp_P(newValue, PSTR("on")) == 0 || 
                           strcasecmp_P(newValue, PSTR("1")) == 0);
}

if (strcasecmp_P(field, PSTR("WebPassword"))==0) {
  if (newValue && strcasecmp_P(newValue, PSTR("notthepassword")) != 0) {
    strlcpy(settingWebPassword, newValue, sizeof(settingWebPassword));
  }
}
```

**7. Add to restAPI.ino - sendDeviceSettings():**
```cpp
sendJsonSettingObj(F("webauthenabled"), settingWebAuthEnabled, "b");
sendJsonSettingObj(F("webpassword"), 
  (strlen(settingWebPassword) > 0) ? "notthepassword" : "", "p", 40);
```

**8. Add to index.html - Settings section:**
```html
<div class="setting-item">
  <label>
    <input type="checkbox" id="webauthenabled" data-setting="webauthenabled" />
    Enable Web UI Authentication
  </label>
  <span class="help-text">
    ⚠️ <strong>WARNING:</strong> Enabling authentication breaks:
    <ul style="margin: 5px 0 5px 20px; color: red;">
      <li>Home Assistant MQTT Auto-Discovery</li>
      <li>OTmonitor app integration</li>
      <li>Home Assistant OpenTherm Gateway integration</li>
    </ul>
    Only enable if you understand the trade-offs.
  </span>
</div>

<div class="setting-item" id="webpassword-container" style="display:none;">
  <label>Web UI Password:</label>
  <input type="password" id="webpassword" data-setting="webpassword" 
         placeholder="Enter password" maxlength="40" />
  <span class="help-text">
    Username is always "admin". Password cannot be empty when auth is enabled.
  </span>
</div>

<script>
// Show/hide password field based on checkbox
document.getElementById('webauthenabled').addEventListener('change', function() {
  document.getElementById('webpassword-container').style.display = 
    this.checked ? 'block' : 'none';
});
</script>
```

---

## Testing Checklist

### WiFi AP Password
- [ ] Password displayed on Serial console during first boot
- [ ] AP requires password to connect
- [ ] Password is unique per device (verify on 2+ devices)
- [ ] Captive portal works after authentication
- [ ] WiFi configuration saves successfully

### Optional Telnet
- [ ] Telnet works when enabled (default)
- [ ] Telnet port closed when disabled (verify with `nmap`)
- [ ] Setting persists across reboots
- [ ] Web UI toggle works
- [ ] Debug output redirected when enabled

### Optional OTmonitor
- [ ] Port 25238 works when enabled (default)
- [ ] Port 25238 closed when disabled (verify with `nmap`)
- [ ] Setting persists across reboots
- [ ] Web UI toggle works with warning
- [ ] OTmonitor app connects when enabled
- [ ] MQTT continues to work when disabled

### Optional Web UI Auth (if implemented)
- [ ] Auth prompt appears when enabled
- [ ] Login with correct password succeeds
- [ ] Login with wrong password fails
- [ ] Auth can be disabled
- [ ] Setting persists across reboots
- [ ] Warning displayed in UI
- [ ] Document that HA integration breaks

---

## Rollback Plan

If issues arise after deployment:

1. **WiFi AP Password Issues:**
   - Revert to open AP: Comment out `manageWiFi.setAPPassword()` line
   - Users can still connect without password

2. **Service Disabling Issues:**
   - Change default to `true` in OTGW-firmware.h
   - Users can manually enable via settings if needed

3. **Web UI Auth Issues:**
   - Disable by default (`settingWebAuthEnabled = false`)
   - Add escape hatch: Allow access without auth if certain GPIO pulled low

---

## Memory Usage Comparison

Before and after implementation:

```
                    Current   +WiFi AP   +Telnet   +OTmonitor   +Web Auth
                             Password    Optional   Optional     (optional)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Settings RAM          ~50      ~50        ~51        ~52           ~93
Runtime Overhead       0        ~12       ~0         ~0            ~500
TelnetStream         ~512      ~512       ~0*        ~512          ~512
OTGWstream           ~256      ~256       ~256       ~0*           ~256
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
TOTAL                ~818      ~830       ~307       ~564          ~1361

* Service not started when disabled by user
```

Best case (all services disabled): Saves ~768 bytes RAM  
Worst case (web auth enabled): Uses +543 bytes RAM

---

## Documentation Updates Required

### README.md
- [ ] Update "First-Time Setup" section with AP password instructions
- [ ] Add "Security Settings" section explaining new options
- [ ] Update "Connectivity options" table with default states
- [ ] Add troubleshooting: "I can't connect to WiFi AP"

### Wiki
- [ ] Create "Security Configuration Guide" page
- [ ] Update setup guide with Serial console instructions
- [ ] Add FAQ: "Should I disable OTmonitor?"
- [ ] Add FAQ: "What breaks when I enable authentication?"

### Code Comments
- [ ] Reference this implementation doc in modified files
- [ ] Add comments explaining security trade-offs
- [ ] Document default settings and rationale

---

## Questions for Review

Before implementing, please answer:

1. **WiFi AP Password:**
   - Approve implementation? ✓ YES / ☐ NO
   - Should password also be displayed on LED screen? ✓ Serial only / ☐ LED
   - Print on device label/sticker? ☐ YES / ✓ User documents it

2. **Service Disabling:**
   - Approve both telnet and OTmonitor toggles? ✓ YES / ☐ NO
   - Default state: ✓ Both enabled / ☐ Both disabled / ☐ Other: _______
   - Require reboot: ✓ YES (simpler) / ☐ NO (runtime toggle)

3. **Web UI Authentication:**
   - Community demand level? ☐ High / ☐ Medium / ✓ Low / ☐ None
   - Worth breaking integrations? ☐ YES / ✓ NO / ☐ Optional only
   - Implement in Phase 2? ☐ YES / ✓ NO / ☐ Wait for requests

4. **Documentation:**
   - Priority updates: ✓ README / ✓ Wiki / ☐ Video tutorial
   - Translation needed? ☐ YES / ✓ English only

---

**For questions or feedback:**  
See [SECURITY_IMPROVEMENT_ANALYSIS.md](SECURITY_IMPROVEMENT_ANALYSIS.md) for full context.
