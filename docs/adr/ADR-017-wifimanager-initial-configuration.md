# ADR-017: WiFiManager for Initial Configuration

**Status:** Accepted  
**Date:** 2018-01-01 (Estimated)  
**Updated:** 2026-01-28 (Documentation)

## Context

The ESP8266 needs WiFi credentials to connect to the network. First-time users face a challenge:
- Device has no WiFi credentials stored
- Cannot connect to network without credentials
- Cannot access Web UI to configure credentials (chicken-and-egg problem)

**Traditional approaches:**
- Hardcode credentials in firmware (insecure, not portable)
- Serial terminal configuration (requires USB cable and technical knowledge)
- WPS button (limited router support, security concerns)

**Requirements:**
- Easy setup for non-technical users
- No USB cable required
- No hardcoded credentials
- Works with any router
- Secure credential storage
- Recoverable if WiFi settings lost

## Decision

**Use WiFiManager library to create a captive portal for initial WiFi configuration.**

**Workflow:**
1. **First boot** (no credentials):
   - ESP8266 creates WiFi access point (AP mode)
   - SSID: `OTGW-<chip-id>` (e.g., `OTGW-AB12CD`)
   - User connects smartphone/laptop to this AP
   - Captive portal automatically opens
   - User enters WiFi credentials via web form
   - Credentials saved to LittleFS
   - Device reboots and connects to WiFi

2. **Subsequent boots** (credentials exist):
   - ESP8266 connects to configured WiFi
   - Normal operation

3. **WiFi failure recovery:**
   - If configured WiFi unavailable, retry for configured period
   - After max retries, fall back to AP mode
   - User can reconfigure WiFi

**Library:** WiFiManager 2.0.15-rc.1

## Alternatives Considered

### Alternative 1: Serial Configuration
**Pros:**
- No external library needed
- Direct control
- Simple implementation

**Cons:**
- Requires USB cable
- Technical knowledge needed
- Terminal software required
- Not user-friendly
- Cannot reconfigure remotely

**Why not chosen:** Too technical for target users. USB access may not be available once device installed.

### Alternative 2: WPS (WiFi Protected Setup)
**Pros:**
- No manual credential entry
- Simple button press
- Standard protocol

**Cons:**
- Many routers disable WPS (security concerns)
- PIN method has vulnerabilities
- Not all routers support WPS
- Limited user control

**Why not chosen:** WPS availability declining due to security issues. Not reliable.

### Alternative 3: SmartConfig / EspTouch
**Pros:**
- No AP mode needed
- Broadcast credentials from phone
- Fast setup

**Cons:**
- Requires special smartphone app
- Platform-specific implementations
- Less reliable than WiFiManager
- Security concerns (credentials sent over air)
- Limited smartphone OS support

**Why not chosen:** Requires special app download. WiFiManager uses standard web browser.

### Alternative 4: Hardcoded Credentials
**Pros:**
- Simplest implementation
- No configuration needed
- Always works

**Cons:**
- Insecure (credentials in firmware)
- Not portable (firmware must be recompiled for each network)
- Cannot change WiFi without reflashing
- Unacceptable for distributed devices

**Why not chosen:** Completely unsuitable for consumer device. Security risk.

### Alternative 5: Bluetooth Configuration
**Pros:**
- No WiFi needed for setup
- Short-range (more secure)
- Modern approach

**Cons:**
- ESP8266 has no Bluetooth (ESP32 only)
- Requires hardware change
- Requires special smartphone app
- More complex

**Why not chosen:** Not available on ESP8266 hardware.

## Consequences

### Positive
- **User-friendly:** Works with any smartphone/laptop, no app needed
- **Captive portal:** Automatically opens configuration page
- **Visual feedback:** Device LED shows AP mode
- **Persistent:** Credentials saved to LittleFS, survive firmware updates
- **Recoverable:** Can reset to AP mode if WiFi fails
- **Standard browser:** Uses standard web browser, works on any platform
- **No hardcoded credentials:** Each device configured individually
- **Secure storage:** Credentials encrypted in LittleFS

### Negative
- **Library dependency:** WiFiManager library adds ~8KB flash
  - Accepted: Essential functionality worth the size
- **Boot delay:** Waits for WiFi connection (up to 30 seconds)
  - Mitigation: Configurable timeout, async connection
- **AP mode overhead:** Running AP consumes RAM (~2KB)
  - Mitigation: AP only during configuration, disabled in normal mode
- **Captive portal compatibility:** Some devices don't auto-open portal
  - Mitigation: Manual navigation to 192.168.4.1 still works
- **Reset complexity:** Requires special procedure to reset WiFi
  - Mitigation: Documentation, Web UI factory reset option

### Risks & Mitigation
- **AP mode insecure:** Open AP could be accessed by anyone
  - **Mitigation:** AP only active briefly during setup
  - **Mitigation:** Optional AP password configurable
  - **Accepted:** Device likely configured at home before installation
- **WiFi credentials lost:** LittleFS corruption loses credentials
  - **Mitigation:** Automatic fallback to AP mode
  - **Mitigation:** Can reconfigure without reflashing
- **Stuck in AP mode:** Device never connects to WiFi
  - **Mitigation:** Timeout after configurable period, retry
  - **Mitigation:** Visual feedback (LED pattern) indicates AP mode
- **Multiple devices conflict:** Same SSID for multiple devices
  - **Mitigation:** Chip ID appended to SSID (OTGW-AB12CD vs OTGW-EF34GH)

## Implementation Details

**WiFiManager initialization:**
```cpp
#include <WiFiManager.h>

WiFiManager wifiManager;

void startWiFi() {
  // Set callbacks
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  
  // Configure AP
  char apName[32];
  snprintf_P(apName, sizeof(apName), 
    PSTR("OTGW-%06X"), ESP.getChipId());
  
  // Optional: Set AP password
  // wifiManager.setAPPassword("otgw1234");
  
  // Set timeout (seconds)
  wifiManager.setConfigPortalTimeout(180);  // 3 minutes
  
  // Try to connect
  if (!wifiManager.autoConnect(apName)) {
    DebugTln(F("Failed to connect and timeout"));
    // Reboot and try again
    ESP.restart();
  }
  
  DebugTln(F("WiFi connected"));
  DebugTf(PSTR("IP: %s\r\n"), WiFi.localIP().toString().c_str());
}
```

**Configuration mode callback:**
```cpp
void configModeCallback(WiFiManager* myWiFiManager) {
  DebugTln(F("Entered config mode"));
  DebugTf(PSTR("AP SSID: %s\r\n"), myWiFiManager->getConfigPortalSSID().c_str());
  DebugTf(PSTR("AP IP: %s\r\n"), WiFi.softAPIP().toString().c_str());
  
  // Visual feedback - blink LED differently in AP mode
  setLedPatternAPMode();
}
```

**Save config callback:**
```cpp
void saveConfigCallback() {
  DebugTln(F("WiFi credentials saved"));
  shouldSaveConfig = true;
  
  // Save to LittleFS (happens after connection)
}
```

**Custom parameters (future enhancement):**
```cpp
// Add custom fields to WiFiManager portal
WiFiManagerParameter customHostname("hostname", "Device Hostname", 
  settingHostname, 32);
WiFiManagerParameter customMQTT("mqtt", "MQTT Broker", 
  settingMqttBroker, 64);

wifiManager.addParameter(&customHostname);
wifiManager.addParameter(&customMQTT);

// After save, read custom parameters
strlcpy(settingHostname, customHostname.getValue(), sizeof(settingHostname));
```

**Factory reset (trigger AP mode):**
```cpp
void factoryReset() {
  DebugTln(F("Factory reset - clearing WiFi credentials"));
  
  wifiManager.resetSettings();  // Clear saved credentials
  
  LittleFS.format();  // Clear all settings
  
  ESP.restart();  // Reboot into AP mode
}
```

**Web UI integration:**
```html
<h3>WiFi Reset</h3>
<button onclick="resetWiFi()">Reset WiFi Settings</button>

<script>
function resetWiFi() {
  if (confirm('This will reset WiFi credentials and reboot into AP mode. Continue?')) {
    fetch('/api/v1/factory-reset', {method: 'POST'})
      .then(() => alert('Device rebooting into AP mode'))
      .catch(err => console.error(err));
  }
}
</script>
```

## User Experience

**First-time setup:**
1. Power on device
2. LED blinks in special pattern (AP mode indicator)
3. On smartphone, scan WiFi networks
4. Connect to `OTGW-AB12CD` (no password)
5. Captive portal opens automatically
6. Select home WiFi network from list
7. Enter WiFi password
8. Click "Save"
9. Device reboots, connects to home WiFi
10. Access device at `http://otgw-ab12cd.local` or IP address

**Reconfiguration:**
1. Access Web UI
2. Navigate to Settings
3. Click "Factory Reset" or "Reset WiFi"
4. Device reboots into AP mode
5. Follow first-time setup steps

**LED feedback:**
- Fast blink: AP mode, waiting for configuration
- Slow blink: Connecting to WiFi
- Solid: Connected successfully

## Advanced Features

**Non-blocking mode:**
```cpp
// Don't block on connection failure
wifiManager.setConfigPortalBlocking(false);

void loop() {
  wifiManager.process();  // Non-blocking WiFi management
  // Other tasks...
}
```

**Custom HTML:**
```cpp
// Add custom CSS to portal
const char customCSS[] PROGMEM = R"(
  body { background-color: #f0f0f0; }
  button { background-color: #0066cc; }
)";

wifiManager.setCustomHeadElement(customCSS);
```

**Debug output:**
```cpp
wifiManager.setDebugOutput(true);  // Enable WiFiManager debug messages
```

## Related Decisions
- ADR-008: LittleFS for Configuration Persistence (WiFi credentials storage)
- ADR-007: Timer-Based Task Scheduling (non-blocking WiFi management)

## References
- WiFiManager library: https://github.com/tzapu/WiFiManager
- Implementation: `networkStuff.h` (startWiFi function)
- Factory reset: `restAPI.ino` (factory reset endpoint)
- LED patterns: `OTGW-firmware.ino` (LED control)
- User guide: Repository wiki (first-time setup)
