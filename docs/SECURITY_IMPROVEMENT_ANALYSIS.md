# Security Improvement Analysis

**Status:** Draft Analysis  
**Date:** 2026-02-16  
**Issue:** [#358](https://github.com/rvdbreemen/OTGW-firmware/issues/358) - Security Improvement Suggestions  
**Related Issue:** #131 - WiFi AP Security  
**Maintainer:** @rvdbreemen  
**Analyst:** GitHub Copilot Advanced Agent

---

## Executive Summary

This document analyzes five security improvement suggestions for the OTGW-firmware, providing implementation options, trade-offs, and recommendations for each. The analysis considers the firmware's existing security model (ADR-003: HTTP-Only, ADR-032: No Authentication), ESP8266 memory constraints (~40KB usable RAM), and target deployment (local home networks).

**Key Findings:**
1. **WiFi AP Security (Issue #131)** - Highest priority, multiple viable solutions available
2. **Service Disabling (Telnet/OTmonitor)** - Easy to implement, low impact, good security improvement
3. **Web UI Authentication** - Moderate complexity, breaks zero-config HA integration
4. **HTTPS Support** - Not recommended due to memory constraints (confirmed by existing ADR-003)

**Quick Recommendation:** Prioritize WiFi AP password protection (#1) and optional service disabling (#2).

---

## Table of Contents

1. [Context & Current Security Model](#context--current-security-model)
2. [Suggestion #1: WiFi AP Mode Security (Issue #131)](#suggestion-1-wifi-ap-mode-security-issue-131)
3. [Suggestion #2: Web UI Authentication](#suggestion-2-web-ui-authentication)
4. [Suggestion #3: HTTPS Support](#suggestion-3-https-support)
5. [Suggestion #4: Optional Telnet Disabling](#suggestion-4-optional-telnet-disabling)
6. [Suggestion #5: Optional OTmonitor Disabling](#suggestion-5-optional-otmonitor-disabling)
7. [Implementation Priority Matrix](#implementation-priority-matrix)
8. [Recommended Implementation Phases](#recommended-implementation-phases)
9. [References](#references)

---

## Context & Current Security Model

### Current Architecture

The OTGW-firmware follows a **"local network trust model"** documented in:
- **ADR-003:** HTTP-Only Network Architecture (No HTTPS)
- **ADR-032:** No Authentication Pattern (Local Network Security Model)

**Security Layers:**
```
Layer 1: Physical Security
         ▼
Layer 2: Network Isolation (WiFi WPA2/WPA3, router firewall)
         ▼
Layer 3: VPN for Remote Access (user responsibility)
         ▼
Layer 4: Application Layer (NO auth, NO TLS - this firmware)
```

### Exposed Network Services

| Port | Service | Purpose | Current Security |
|------|---------|---------|------------------|
| 80 | HTTP | Web UI, REST API | No authentication |
| 81 | WebSocket | Real-time OT messages | No authentication |
| 23 | Telnet | Debug console | No authentication |
| 25238 | TCP | OTmonitor compatibility | No authentication |
| MQTT | MQTT Client | Home Assistant | Broker authentication (external) |

### Memory Constraints

- **Total RAM:** ~80KB
- **Available after core libraries:** ~40KB
- **Typical runtime usage:** ~25-30KB (heap fragmentation risk)
- **Critical threshold:** <10KB free heap triggers emergency mode (ADR-030)

### Current WiFi Behavior

**Initial Setup (No credentials stored):**
1. ESP8266 creates **open WiFi AP** (SSID: `OTGW-<MAC>`)
2. User connects to AP (no password required)
3. Captive portal opens for WiFi configuration
4. User enters home WiFi credentials
5. Device reboots and connects to home network

**Problem:** The open AP is publicly accessible during initial setup (~3-5 minutes).

---

## Suggestion #1: WiFi AP Mode Security (Issue #131)

### Problem Statement

**Current Behavior:**  
During initial setup, the device creates an **open WiFi access point** (no password) with SSID format `OTGW-<MAC>`. Anyone within WiFi range can connect and configure the device.

**Security Risk:**  
- Unauthorized person could configure device to connect to their network
- Attacker could intercept the initial setup and capture home WiFi credentials
- Device could be configured maliciously before legitimate user completes setup

**Community Feedback:**  
> "In my opinion the wifi creating a public AP is the biggest issue as it is public." - @DaveDavenport

### Implementation Options

#### Option 1A: Fixed AP Password (Simple)

**Implementation:**
```cpp
// In networkStuff.h, startWiFi()
manageWiFi.setAPPassword("otgw1234");  // Fixed password
```

**Pros:**
- 1 line of code change
- Minimal memory overhead (~0 bytes)
- Works immediately
- No user configuration needed

**Cons:**
- Same password for all devices (security through obscurity)
- Password must be documented/printed on device
- All OTGW devices share the same password

**Estimated Effort:** 5 minutes  
**Memory Impact:** 0 bytes  
**Security Improvement:** Medium (better than open, but not ideal)

---

#### Option 1B: Unique Per-Device Password (Recommended)

**Implementation:**
```cpp
// Generate password from chip ID or MAC address
char apPassword[12];
snprintf_P(apPassword, sizeof(apPassword), 
  PSTR("OTGW%04X"), (ESP.getChipId() & 0xFFFF));
manageWiFi.setAPPassword(apPassword);

// Display password on Serial during setup phase
SetupDebugf(PSTR("\r\n*** WiFi AP Password: %s ***\r\n\r\n"), apPassword);
```

**Pros:**
- Unique password per device
- Derived from hardware (chip ID or MAC)
- No storage needed (computed on demand)
- User can read password from Serial console during first boot
- Could be printed on sticker with device

**Cons:**
- User needs Serial console access to see password (USB cable + terminal)
- Still somewhat predictable if attacker knows the algorithm

**Estimated Effort:** 30 minutes  
**Memory Impact:** ~12 bytes (temporary buffer)  
**Security Improvement:** High

**User Experience:**
1. Power on device
2. Connect USB cable and open Serial monitor (9600 baud)
3. Read displayed password: `OTGW1A2B`
4. Scan WiFi, connect to `OTGW-xx:xx:xx:xx:xx:xx`
5. Enter password when prompted
6. Complete WiFi setup

---

#### Option 1C: Physical Reset Button (Advanced)

**Implementation:**
```cpp
// Detect GPIO button press during boot
void setup() {
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
  
  if (digitalRead(RESET_BUTTON_PIN) == LOW) {
    // Button pressed during power-on
    DebugTln(F("Reset button detected - forcing AP mode"));
    manageWiFi.resetSettings();  // Clear saved WiFi
  }
  
  startWiFi(...);
}
```

**Additional hardware:** Button connected to GPIO pin (e.g., GPIO0 on NodeMCU)

**Pros:**
- Only enters AP mode when button is pressed
- No accidental AP mode on power loss/reboot
- Standard practice for many IoT devices
- AP can still be password-protected

**Cons:**
- Requires hardware modification or uses existing reset button
- More complex user instructions
- Doesn't solve the open AP problem (still needs password)

**Estimated Effort:** 2-3 hours  
**Memory Impact:** ~50 bytes  
**Security Improvement:** Medium (combines well with Option 1B)

---

#### Option 1D: Web-Based WiFi Reset (Easiest for users)

**Current Status:** Already partially implemented via `/ReBoot` endpoint

**Enhancement:**
```cpp
// Add new REST API endpoint
httpServer.on("/api/v2/system/wifi-reset", HTTP_POST, []() {
  if (httpServer.method() != HTTP_POST) {
    sendApiMethodNotAllowed(F("POST"));
    return;
  }
  
  DebugTln(F("WiFi reset requested via API"));
  WiFiManager manageWiFi;
  manageWiFi.resetSettings();
  
  httpServer.send(200, F("application/json"), 
    F("{\"status\":\"success\",\"message\":\"WiFi reset. Device will reboot into AP mode.\"}"));
  
  delay(1000);
  ESP.restart();
});
```

**Pros:**
- User can force AP mode from Web UI when connected
- No physical access needed for reconfiguration
- Can be combined with AP password protection

**Cons:**
- Doesn't help with initial setup security
- Requires network access to trigger

**Estimated Effort:** 1 hour  
**Memory Impact:** ~100 bytes (handler)  
**Security Improvement:** Low (usability improvement, not security)

---

#### Option 1E: NFC/QR Code Password (Future Enhancement)

**Concept:** Generate a random 8-character password on first boot, display as QR code on Serial console, require NFC tag or QR scan to retrieve password.

**Pros:**
- Very secure (true random password)
- Modern user experience

**Cons:**
- Complex implementation
- Requires external app or NFC reader
- Not practical for DIY project

**Estimated Effort:** 20+ hours  
**Not recommended** for this project.

---

### Recommendation: Option 1B + 1C Combination

**Best Solution:**
1. **Always use a password for AP mode** (Option 1B - unique per device)
2. **Optional:** Add physical reset button support (Option 1C)

**Implementation Plan:**
```cpp
// networkStuff.h - Enhanced startWiFi()

void startWiFi(const char* hostname, int timeOut) {
  WiFi.mode(WIFI_STA);
  WiFiManager manageWiFi;
  
  char thisAP[64];
  char apPassword[12];
  
  // Generate unique AP SSID
  strlcpy(thisAP, hostname, sizeof(thisAP));
  strlcat(thisAP, "-", sizeof(thisAP));
  strlcat(thisAP, WiFi.macAddress().c_str(), sizeof(thisAP));
  
  // Generate unique AP password from chip ID
  snprintf_P(apPassword, sizeof(apPassword), 
    PSTR("OTGW%04X"), (ESP.getChipId() & 0xFFFF));
  
  SetupDebugln(F("========================================"));
  SetupDebugf(PSTR("WiFi AP SSID:     %s\r\n"), thisAP);
  SetupDebugf(PSTR("WiFi AP Password: %s\r\n"), apPassword);
  SetupDebugln(F("========================================"));
  
  manageWiFi.setAPCallback(configModeCallback);
  manageWiFi.setTimeout(timeOut);
  
  // Security improvements
  std::vector<const char *> wm_menu = {"wifi", "exit"};
  manageWiFi.setShowInfoUpdate(false);
  manageWiFi.setShowInfoErase(false);
  manageWiFi.setMenu(wm_menu);
  manageWiFi.setHostname(hostname);
  
  // Set AP password (NEW)
  manageWiFi.setAPPassword(apPassword);
  
  // Existing connection logic...
  if (!manageWiFi.autoConnect(thisAP, apPassword)) {
    DebugTln(F("Failed to connect and timeout"));
    ESP.restart();
  }
  
  DebugTln(F("WiFi connected"));
}
```

**Documentation Required:**
1. Update README.md with setup instructions
2. Add section about reading AP password from Serial console
3. Recommend printing password on device label/sticker
4. Update wiki with troubleshooting steps

---

## Suggestion #2: Web UI Authentication

### Problem Statement

**Current Behavior:**  
Web UI at `http://otgw.local` has no authentication. Anyone on the local network can:
- View all settings (including masked MQTT password)
- Change device configuration
- Trigger firmware updates
- Reboot the device
- Access file system

**Security Risk:**  
- Unauthorized configuration changes
- Accidental misconfiguration by household members
- Malicious changes if local network is compromised

### Implementation Options

#### Option 2A: HTTP Basic Authentication

**Implementation:**
```cpp
// Add to OTGW-firmware.h
char settingWebUsername[32] = "admin";
char settingWebPassword[41] = "";  // Empty = disabled

// Add to restAPI.ino
void setupRestAPI() {
  // Protect all endpoints
  httpServer.on("/", []() {
    if (strlen(settingWebPassword) > 0) {
      if (!httpServer.authenticate(settingWebUsername, settingWebPassword)) {
        return httpServer.requestAuthentication();
      }
    }
    // Serve index.html
  });
  
  // Repeat for all endpoints...
}
```

**Pros:**
- Standard HTTP authentication (RFC 7617)
- Supported by all browsers
- ~200 bytes RAM overhead
- Can be optional (disabled if password empty)

**Cons:**
- **Breaks Home Assistant MQTT Auto-Discovery** (can't authenticate REST API calls)
- **Breaks OTmonitor integration** (no auth support)
- Credentials sent in base64 (easily decoded without TLS)
- Must authenticate on every HTTP request (session management needed)
- **Memory overhead:** ~500 bytes for session management

**Estimated Effort:** 4-6 hours  
**Memory Impact:** ~500-700 bytes  
**Security Improvement:** Medium (without TLS, provides minimal security)

**Compatibility Issues:**
- Home Assistant OpenTherm Gateway integration: Cannot authenticate REST API
- MQTT is unaffected (different protocol)
- OTmonitor TCP socket: No authentication mechanism

---

#### Option 2B: Session-Based Authentication

**Implementation:**
```cpp
struct Session {
  char token[32];
  uint32_t expires;
};

Session sessions[MAX_SESSIONS];  // e.g., 3 concurrent sessions

// Login endpoint
httpServer.on("/api/v2/auth/login", HTTP_POST, []() {
  String username = httpServer.arg("username");
  String password = httpServer.arg("password");
  
  if (username == settingWebUsername && password == settingWebPassword) {
    // Generate session token
    String token = generateToken();
    sessions[nextSlot] = {token, millis() + SESSION_TIMEOUT};
    
    httpServer.send(200, F("application/json"), 
      "{\"token\":\"" + token + "\"}");
  } else {
    httpServer.send(401, F("application/json"), 
      "{\"error\":\"Invalid credentials\"}");
  }
});
```

**Pros:**
- Better UX than Basic Auth (login once per session)
- Can implement "remember me" in browser
- Can whitelist REST API endpoints for HA integration

**Cons:**
- Complex implementation (token generation, validation, expiry)
- **Memory overhead:** ~150 bytes per session × 3 = ~450 bytes
- Still sends credentials in cleartext without TLS
- Requires frontend changes (login page)

**Estimated Effort:** 12-16 hours  
**Memory Impact:** ~600-800 bytes  
**Security Improvement:** Medium

---

#### Option 2C: Optional Authentication (Recommended approach if implementing)

**Concept:** Add authentication as an **optional feature**, disabled by default.

**Implementation:**
```cpp
// Settings
bool settingWebAuthEnabled = false;  // Default: OFF
char settingWebPassword[41] = "";

// Middleware
bool requireAuth() {
  if (!settingWebAuthEnabled || strlen(settingWebPassword) == 0) {
    return true;  // Auth disabled
  }
  
  return httpServer.authenticate("admin", settingWebPassword);
}

// Use in handlers
httpServer.on("/", []() {
  if (!requireAuth()) {
    return httpServer.requestAuthentication();
  }
  // Serve content
});
```

**Pros:**
- Backward compatible (disabled by default)
- Users can enable when needed
- Maintains zero-config HA integration for most users
- Simple implementation

**Cons:**
- Still breaks integrations when enabled
- Limited security without TLS

**Estimated Effort:** 6-8 hours  
**Memory Impact:** ~400-500 bytes  
**Security Improvement:** Medium (when enabled)

---

### Recommendation: **Not Recommended**

**Reasoning:**

1. **Violates existing security model (ADR-032)**  
   The firmware explicitly chose "no authentication" with network-level security.

2. **Breaks Home Assistant integration**  
   HA cannot authenticate REST API calls for Auto-Discovery.

3. **Breaks OTmonitor compatibility**  
   OTmonitor has no authentication support for TCP socket.

4. **Minimal security without TLS**  
   Without HTTPS, credentials are sent in cleartext (easily intercepted).

5. **Memory concerns**  
   Session management adds ~500-800 bytes RAM overhead.

6. **User experience degradation**  
   Adds friction to setup and daily use.

**Alternative:** Focus on network-level security:
- Document proper network segmentation (VLAN)
- Recommend VPN for remote access
- Emphasize WiFi WPA2/WPA3 encryption

**If authentication is absolutely required:**
- Implement Option 2C (optional, disabled by default)
- Clearly document that enabling it breaks HA/OTmonitor integration
- Wait for TLS support (if ESP8266 successor with more RAM is adopted)

---

## Suggestion #3: HTTPS Support

### Problem Statement

**Request:** Add HTTPS support for Web UI and REST API, even with self-signed certificates.

**Current Behavior:**  
All HTTP traffic is unencrypted (`http://`, not `https://`).  
All WebSocket traffic is unencrypted (`ws://`, not `wss://`).

### Analysis

**Existing Decision: ADR-003 (HTTP-Only Network Architecture)**

This suggestion has already been extensively analyzed and **explicitly rejected** in ADR-003.

**Key findings from ADR-003:**

1. **Memory Constraints:**
   - TLS handshake requires 20-30KB RAM (50-75% of available heap)
   - Even "lightweight" TLS (wolfSSL, mbedTLS) needs 10-15KB (25-37% of heap)
   - This leaves insufficient memory for normal operations

2. **Performance Impact:**
   - TLS handshake: ~500ms-2s delay per connection
   - Encryption/decryption: CPU overhead impacts responsiveness
   - ESP8266 @ 80MHz struggles with TLS crypto operations

3. **Stability Risk:**
   - Multiple concurrent WebSocket clients (3) + TLS = heap exhaustion
   - OTA updates may fail due to insufficient memory
   - Watchdog resets likely under load

4. **Certificate Management:**
   - Self-signed certificates: Browser warnings on every connection
   - Let's Encrypt: Requires internet access, DNS, renewal logic
   - Certificate storage: 1-2KB flash per cert

5. **Testing:**
   - Community has attempted HTTPS on ESP8266 with poor results
   - Arduino ESP8266 core documentation recommends against TLS for memory-constrained applications

**Alternatives considered in ADR-003:**
- HTTPS with self-signed certificates: ❌ Memory constraints
- HTTPS with certificate pinning: ❌ Memory + complexity
- Lightweight TLS (mbedTLS, wolfSSL): ❌ Still too memory-intensive
- Application-level encryption: ❌ Security through obscurity

### Test Data (from community)

Users have reported ESP8266 TLS experiences:
- Successful HTTPS on ESP32 (much more RAM)
- ESP8266 HTTPS requires disabling features to free memory
- Frequent crashes under load with TLS enabled

### Recommendation: **NOT RECOMMENDED - Already Rejected in ADR-003**

**Reasons:**
1. **Violates existing architectural decision** (ADR-003)
2. **Insufficient RAM** for stable operation
3. **Better alternatives exist** (VPN for remote access)
4. **Self-signed certs** provide minimal security improvement (user must accept warnings)

**Alternative Solutions:**
- **VPN:** Use WireGuard or OpenVPN for remote access (encrypts all traffic)
- **Reverse Proxy:** Run nginx/Apache on home server with valid TLS cert, proxy to OTGW
- **Network Segmentation:** Isolate OTGW on IoT VLAN, control access via firewall

**If HTTPS is absolutely required:**
- Consider ESP32 platform (ADR-001 revisited) - 8x more RAM
- This would be a **major architectural change** requiring new ADR to supersede ADR-001 and ADR-003

---

## Suggestion #4: Optional Telnet Disabling

### Problem Statement

**Request:** Allow users to disable the telnet debug console (port 23) when not needed.

**Current Behavior:**  
Telnet server is always running on port 23 for debug output.

**Security Risk:**  
- Anyone on local network can connect to port 23
- View real-time debug output (sensor readings, settings, commands)
- Not typically sensitive, but increases attack surface

### Implementation Options

#### Option 4A: Simple On/Off Toggle

**Implementation:**
```cpp
// Add to OTGW-firmware.h
bool settingTelnetEnabled = true;  // Default: ON (current behavior)

// Add to networkStuff.h
void startTelnet() {
  if (!settingTelnetEnabled) {
    DebugTln(F("Telnet disabled by user setting"));
    return;  // Don't start telnet server
  }
  
  DebugT(F("\r\nUse 'telnet "));
  DebugT(WiFi.localIP());
  DebugTln(F("' for debugging"));
  TelnetStream.begin();
  DebugTln(F("\nTelnet server started .."));
  TelnetStream.flush();
}

// Add to loop() or handleDebug()
void handleDebugCommands() {
  if (!settingTelnetEnabled) return;  // Skip if disabled
  
  // Existing debug command handling...
}
```

**Web UI Addition:**
```html
<!-- In settings page -->
<div class="setting-item">
  <label>
    <input type="checkbox" id="telnetEnabled" />
    Enable Telnet Debug Console (Port 23)
  </label>
  <span class="help-text">
    Disable to prevent debug console access. Requires reboot.
  </span>
</div>
```

**Pros:**
- Very simple implementation (2-3 hours)
- Minimal memory overhead (~1 byte)
- Clear security improvement
- Doesn't affect other functionality

**Cons:**
- Requires reboot to apply (TelnetStream can't be stopped dynamically)
- Loses debug output if disabled (but that's the point)

**Estimated Effort:** 2-3 hours  
**Memory Impact:** 1 byte (setting) + ~0 (TelnetStream not started)  
**Security Improvement:** Low-Medium (reduces attack surface)

---

#### Option 4B: Runtime Enable/Disable

**Implementation:**
```cpp
bool telnetCurrentlyRunning = false;

void setTelnetEnabled(bool enabled) {
  if (enabled && !telnetCurrentlyRunning) {
    TelnetStream.begin();
    telnetCurrentlyRunning = true;
    DebugTln(F("Telnet server started"));
  } else if (!enabled && telnetCurrentlyRunning) {
    TelnetStream.stop();  // Note: May not be supported by library
    telnetCurrentlyRunning = false;
    // Debug output lost after this point
  }
}
```

**Pros:**
- Can enable/disable without reboot
- Better user experience

**Cons:**
- TelnetStream library may not support `stop()` method
- Requires library investigation/modification
- More complex implementation

**Estimated Effort:** 4-6 hours (depends on library support)  
**Memory Impact:** 1 byte  
**Security Improvement:** Low-Medium

---

### Recommendation: **Option 4A (Simple On/Off Toggle)**

**Reasoning:**
1. Simple to implement and maintain
2. Clear security benefit (reduces attack surface)
3. Telnet is for debugging anyway (not needed in production)
4. Reboot requirement is acceptable for security setting

**Default Setting:** `true` (telnet enabled)  
**Rationale:** Backward compatible, doesn't break existing user workflows.

**Implementation Priority:** Medium (nice-to-have, low risk)

---

## Suggestion #5: Optional OTmonitor Disabling

### Problem Statement

**Request:** Allow users to disable the OTmonitor TCP socket (port 25238) when not needed.

**Current Behavior:**  
TCP server on port 25238 is always running, forwarding serial traffic to/from PIC.

**Security Risk:**  
- Anyone on local network can connect to port 25238
- Send commands to OTGW PIC controller
- Read raw OpenTherm messages
- Potentially send malicious commands (e.g., change boiler setpoints)

**Use Case:**  
Many users don't use OTmonitor app and only use Home Assistant via MQTT.

### Implementation Options

#### Option 5A: Simple On/Off Toggle

**Implementation:**
```cpp
// Add to OTGW-firmware.h
bool settingOTmonitorEnabled = true;  // Default: ON (current behavior)

// Modified startOTGWstream() in OTGW-Core.ino
void startOTGWstream() {
  if (!settingOTmonitorEnabled) {
    DebugTln(F("OTmonitor port disabled by user setting"));
    return;  // Don't start TCP server
  }
  
  OTGWstream.begin();
  DebugTf(PSTR("OTmonitor port started on port %d\r\n"), OTGW_SERIAL_PORT);
}

// Modified handleOTGW()
void handleOTGW() {
  if (!settingOTmonitorEnabled) {
    // Only handle Serial -> processOT, skip network forwarding
    while (Serial.available()) {
      int c = Serial.read();
      // Process for internal use, don't forward to network
    }
    return;
  }
  
  // Existing bidirectional forwarding logic...
}
```

**Web UI Addition:**
```html
<div class="setting-item">
  <label>
    <input type="checkbox" id="otmonitorEnabled" />
    Enable OTmonitor TCP Socket (Port 25238)
  </label>
  <span class="help-text">
    Disable if you don't use OTmonitor app or Home Assistant's 
    OpenTherm Gateway integration. Reduces attack surface. Requires reboot.
  </span>
</div>
```

**Pros:**
- Improves security (closes attack vector)
- Simple implementation
- Frees ~256 bytes RAM when disabled (no TCP server)
- Doesn't affect MQTT functionality

**Cons:**
- Breaks Home Assistant OpenTherm Gateway integration (uses TCP socket)
- Breaks OTmonitor app
- Users must understand what they're disabling

**Estimated Effort:** 2-3 hours  
**Memory Impact:** Saves ~256 bytes when disabled  
**Security Improvement:** Medium-High (prevents unauthorized OTGW commands)

---

#### Option 5B: Authentication on TCP Socket

**Implementation:**
```cpp
// Require password before accepting commands
bool otmonitorAuthenticated = false;

void handleOTGWcommands() {
  if (OTGWstream.available()) {
    String cmd = OTGWstream.readStringUntil('\n');
    
    if (!otmonitorAuthenticated) {
      if (cmd == settingOTmonitorPassword) {
        otmonitorAuthenticated = true;
        OTGWstream.println("OK - Authenticated");
      } else {
        OTGWstream.println("ERR - Authentication required");
      }
      return;
    }
    
    // Process command...
  }
}
```

**Pros:**
- Keeps functionality while adding security
- Compatible with OTmonitor (can send password first)

**Cons:**
- Complex to implement (~8-10 hours)
- Breaks existing OTmonitor integration (requires manual password entry)
- Breaks Home Assistant integration (no way to configure password)
- Password sent in cleartext (no TLS)

**Not recommended** - breaks too many integrations.

---

### Recommendation: **Option 5A (Simple On/Off Toggle)**

**Reasoning:**
1. **Highest security improvement** of all suggestions
   - Prevents unauthorized access to OTGW PIC
   - Closes potential attack vector for boiler control
   
2. **Simple to implement** (2-3 hours)

3. **Memory benefit** (saves ~256 bytes when disabled)

4. **Clear user choice:**
   - Users who don't use OTmonitor/HA OpenTherm integration: Disable it
   - Users who do use these features: Keep it enabled

**Default Setting:** `true` (OTmonitor enabled)  
**Rationale:** Backward compatible, doesn't break existing workflows.

**Documentation Required:**
- Update README.md to explain setting
- Add warning in Web UI: "Disabling breaks OTmonitor app and HA OpenTherm Gateway integration. MQTT continues to work."

**Implementation Priority:** Medium-High (good security improvement, low complexity)

---

## Implementation Priority Matrix

| Suggestion | Priority | Complexity | Security Gain | Memory Impact | User Impact |
|------------|----------|------------|---------------|---------------|-------------|
| **#1: WiFi AP Password** | **HIGH** | Low | High | 0 bytes | Low (one-time setup) |
| **#4: Optional Telnet** | Medium | Low | Low-Medium | Saves ~512 bytes | None (optional) |
| **#5: Optional OTmonitor** | Medium-High | Low | Medium-High | Saves ~256 bytes | Medium (breaks integrations if disabled) |
| **#2: Web UI Auth** | Low | High | Medium | +500-800 bytes | High (breaks integrations) |
| **#3: HTTPS** | **REJECTED** | Very High | Low (without valid cert) | +10-30KB | Medium |

**Priority Scores:**
- Priority = (Security Gain × 2) + (Ease of Implementation) - (User Impact)

1. **WiFi AP Password:** (2×2) + 4 - 1 = 7 ⭐⭐⭐
2. **Optional OTmonitor:** (2×1.5) + 4 - 2 = 5 ⭐⭐
3. **Optional Telnet:** (2×1) + 4 - 1 = 5 ⭐⭐
4. **Web UI Auth:** (2×1.5) + 2 - 3 = 2 ⭐
5. **HTTPS:** REJECTED (ADR-003)

---

## Recommended Implementation Phases

### Phase 1: Quick Wins (Recommended for v1.2.0)

**Target:** Low-hanging fruit with high security impact

1. **WiFi AP Password (Option 1B)**
   - Effort: 30 minutes
   - Memory: 0 bytes
   - Security: High
   - Implementation: Unique password derived from chip ID
   - Documentation: Update README, add Serial console instructions

2. **Optional Service Disabling (Options 4A + 5A)**
   - Effort: 4-6 hours
   - Memory: Saves ~768 bytes when both disabled
   - Security: Medium-High
   - Implementation: Add two boolean settings + Web UI toggles
   - Documentation: Clear warnings about integration impacts

**Total Effort:** 1 day  
**Deliverables:**
- Secure WiFi AP by default
- Users can disable unnecessary services
- Clear documentation of trade-offs

---

### Phase 2: Enhanced Security (Optional, v1.3.0+)

**Target:** Additional security for advanced users

3. **Physical Reset Button Support (Option 1C)**
   - Effort: 2-3 hours
   - Memory: ~50 bytes
   - Security: Medium
   - Implementation: GPIO button detection on boot
   - Documentation: Hardware setup guide

4. **Web UI Authentication (Option 2C - Optional)**
   - **Only if strongly requested by community**
   - Effort: 6-8 hours
   - Memory: +400-500 bytes
   - Security: Medium (limited without TLS)
   - Implementation: Optional Basic Auth (disabled by default)
   - Documentation: **MUST** document integration breakage

**Total Effort:** 8-11 hours  
**Deliverables:**
- Physical reset button support
- Optional Web UI authentication (if needed)

---

### Phase 3: Architecture Review (Future)

**Target:** Long-term security improvements

5. **ESP32 Migration Evaluation**
   - Supersedes ADR-001 (ESP8266 Platform Selection)
   - Would enable: TLS/HTTPS, more concurrent connections, better performance
   - Effort: 100+ hours (major migration)
   - Would require: Hardware design change, testing, documentation rewrite

6. **mTLS Certificate-Based Authentication**
   - Only viable on ESP32 (sufficient RAM for TLS)
   - Industry-standard security
   - Compatible with Home Assistant certificate authentication

---

## References

### Existing ADRs
- **ADR-003:** HTTP-Only Network Architecture (No HTTPS) - Rejects HTTPS
- **ADR-017:** WiFiManager Initial Configuration - Current WiFi AP behavior
- **ADR-024:** Debug Telnet Command Console - Telnet design
- **ADR-032:** No Authentication Pattern - Security model

### Related Code
- `src/OTGW-firmware/networkStuff.h` - WiFi setup, telnet
- `src/OTGW-firmware/OTGW-Core.ino` - OTmonitor TCP socket
- `src/OTGW-firmware/OTGW-firmware.h` - Settings definitions
- `src/OTGW-firmware/restAPI.ino` - Web UI endpoints

### Issues
- Issue #358: Security Improvement Suggestions (this analysis)
- Issue #131: WiFi AP Security (referenced in #358)

### External References
- ESP8266 Arduino Core: https://github.com/esp8266/Arduino
- WiFiManager Library: https://github.com/tzapu/WiFiManager
- TelnetStream Library: https://github.com/jandrassy/TelnetStream

---

## Conclusion

**Summary:**

This analysis evaluated five security improvement suggestions for OTGW-firmware. The recommendations prioritize practical security improvements that respect the ESP8266's memory constraints and the firmware's "local network trust model."

**Top Recommendations:**

1. ✅ **Implement WiFi AP Password (Option 1B)** - Quick, effective, no downsides
2. ✅ **Add Service Disabling Options (4A + 5A)** - Good security/usability balance
3. ❌ **Reject HTTPS Support** - Already rejected in ADR-003, insufficient RAM
4. ⚠️ **Consider Web UI Auth (2C) only if strongly requested** - Breaks integrations

**Next Steps:**

1. Maintainer review and prioritization decision
2. If approved, create implementation plan for Phase 1
3. Update ADRs as needed (e.g., supersede ADR-017 for WiFi changes)
4. Implement changes with thorough testing
5. Update documentation (README, wiki)

**Maintainer Questions:**

@rvdbreemen - Please provide feedback on:
1. Do you approve Phase 1 implementation (WiFi AP password + service disabling)?
2. Should service disabling be enabled or disabled by default?
3. Is there community demand for Web UI authentication (Phase 2)?
4. Any concerns about the proposed implementations?

---

**Document Version:** 1.0  
**Last Updated:** 2026-02-16  
**Status:** Awaiting maintainer review
