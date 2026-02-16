# Security Improvement Options - Quick Reference

**Full Analysis:** See [SECURITY_IMPROVEMENT_ANALYSIS.md](SECURITY_IMPROVEMENT_ANALYSIS.md)  
**Date:** 2026-02-16  
**Issue:** [#358](https://github.com/rvdbreemen/OTGW-firmware/issues/358)

---

## TL;DR - Quick Decision Guide

### ✅ Recommended to Implement

| Option | Effort | Impact | Default |
|--------|--------|--------|---------|
| **WiFi AP Password** | 30 min | 🔒🔒🔒 High | Always ON |
| **Disable Telnet (optional)** | 2-3 hrs | 🔒🔒 Medium | ON (users can disable) |
| **Disable OTmonitor (optional)** | 2-3 hrs | 🔒🔒🔒 High | ON (users can disable) |

### ⚠️ Consider Carefully

| Option | Effort | Impact | Issues |
|--------|--------|--------|--------|
| **Web UI Authentication** | 6-8 hrs | 🔒🔒 Medium | Breaks HA Auto-Discovery |

### ❌ Not Recommended

| Option | Reason |
|--------|--------|
| **HTTPS Support** | Already rejected in ADR-003 (insufficient RAM) |

---

## Option 1: WiFi AP Password (HIGH PRIORITY)

### Current Problem
Initial setup creates **open WiFi AP** - anyone can connect and configure device.

### Solution
```cpp
// Generate unique password from chip ID
char apPassword[12];
snprintf_P(apPassword, sizeof(apPassword), 
  PSTR("OTGW%04X"), (ESP.getChipId() & 0xFFFF));
manageWiFi.setAPPassword(apPassword);
```

### User Experience
1. Power on device
2. Connect USB cable, open Serial monitor (9600 baud)
3. Read displayed password: `OTGW1A2B`
4. Connect to WiFi AP using that password
5. Complete setup

### Pros/Cons
- ✅ Unique password per device
- ✅ No memory overhead (0 bytes)
- ✅ 30 minutes to implement
- ⚠️ Requires Serial console access (one time)

**Recommendation:** **IMPLEMENT** - Highest security gain for minimal effort.

---

## Option 2: Optional Service Disabling

### A) Disable Telnet (Port 23)

**What it does:** Stops debug console from starting.

**When to disable:**
- You don't use telnet for debugging
- Want to reduce attack surface
- Don't need real-time debug output

**Impact when disabled:**
- ✅ Saves ~512 bytes RAM
- ✅ Closes port 23
- ❌ Loses debug console access

**Implementation:**
```cpp
bool settingTelnetEnabled = true;  // Default: ON

void startTelnet() {
  if (!settingTelnetEnabled) return;
  TelnetStream.begin();
}
```

### B) Disable OTmonitor (Port 25238)

**What it does:** Stops TCP serial bridge.

**When to disable:**
- You only use MQTT (not OTmonitor app)
- You don't use Home Assistant OpenTherm Gateway integration
- Want to prevent unauthorized OTGW commands

**Impact when disabled:**
- ✅ Saves ~256 bytes RAM
- ✅ Closes port 25238
- ✅ Prevents direct OTGW PIC access
- ❌ **BREAKS OTmonitor app**
- ❌ **BREAKS Home Assistant OpenTherm Gateway integration**
- ✅ **MQTT still works** (unaffected)

**Implementation:**
```cpp
bool settingOTmonitorEnabled = true;  // Default: ON

void startOTGWstream() {
  if (!settingOTmonitorEnabled) return;
  OTGWstream.begin();
}
```

**Recommendation:** **IMPLEMENT BOTH** - Let users choose based on their needs.

---

## Option 3: Web UI Authentication

### What it adds
Username/password protection on Web UI (`http://otgw.local`).

### Implementation Options

**A) Optional Basic Authentication (Recommended if implementing)**
```cpp
bool settingWebAuthEnabled = false;  // Default: OFF
char settingWebPassword[41] = "";

bool requireAuth() {
  if (!settingWebAuthEnabled) return true;
  return httpServer.authenticate("admin", settingWebPassword);
}
```

### Pros/Cons
- ✅ Protects Web UI from unauthorized access
- ✅ Optional (disabled by default)
- ❌ **BREAKS Home Assistant MQTT Auto-Discovery** (can't auth REST API)
- ❌ **BREAKS OTmonitor** (no auth support)
- ⚠️ Credentials sent in cleartext (no TLS)
- ⚠️ +500 bytes RAM overhead

### Who needs this?
- Users with untrusted devices on local network
- Shared network environments (apartments, offices)
- Users who don't use HA/OTmonitor integrations

**Recommendation:** **OPTIONAL** - Only implement if community strongly requests it. Document integration breakage clearly.

---

## Option 4: HTTPS Support

### Status: ❌ **NOT RECOMMENDED (Already Rejected)**

**Reason:** Analyzed and rejected in **ADR-003: HTTP-Only Network Architecture**

**Why not viable:**
- TLS requires 20-30KB RAM (50-75% of available heap)
- Even "lightweight" TLS needs 10-15KB (25-37% of heap)
- Insufficient memory for stable operation
- Self-signed certs provide minimal security benefit
- Better alternatives exist (VPN, reverse proxy)

**Alternative Solutions:**
1. **VPN for remote access** (WireGuard, OpenVPN)
2. **Reverse proxy** (nginx with valid TLS cert on home server)
3. **Network segmentation** (IoT VLAN with firewall rules)

**Future:** Only viable with ESP32 migration (8x more RAM).

---

## Implementation Recommendation

### Phase 1: Essential Security (v1.2.0)

**Implement these:**

1. ✅ **WiFi AP Password** (30 min)
   - Always enabled
   - Unique per device
   - Displayed on Serial console

2. ✅ **Optional Service Disabling** (4-6 hrs)
   - Telnet enable/disable toggle
   - OTmonitor enable/disable toggle
   - Both enabled by default (backward compatible)
   - Clear warnings in Web UI

**Total effort:** 1 day  
**Benefits:**
- Secure initial setup
- User control over services
- ~768 bytes RAM savings (if both disabled)
- No integration breakage (defaults unchanged)

### Phase 2: Enhanced Security (Future - v1.3.0+)

**Consider if requested:**

3. ⚠️ **Optional Web UI Auth** (6-8 hrs)
   - Only if community wants it
   - Disabled by default
   - Document integration breakage
   - +500 bytes RAM overhead

---

## Decision Matrix

Use this to decide which options to implement:

| If you... | Then implement... |
|-----------|-------------------|
| Want highest security with lowest effort | WiFi AP Password |
| Don't use OTmonitor app | Optional OTmonitor disabling |
| Don't need debug console | Optional Telnet disabling |
| Have untrusted devices on network | Optional Web UI Auth (with caveats) |
| Want remote HTTPS access | Use VPN or reverse proxy (not HTTPS on device) |

---

## Configuration Examples

### Example 1: Maximum Security (MQTT-only user)

```
Settings:
  - WiFi AP Password: OTGW1A2B (auto-generated, always on)
  - Telnet Enabled: NO
  - OTmonitor Enabled: NO
  - Web Auth Enabled: YES (password: strongpassword123)

Result:
  - Secure initial setup ✅
  - MQTT works ✅
  - Web UI protected ✅
  - No OTmonitor ✅
  - No telnet debug ✅
  - HA Auto-Discovery: ❌ BROKEN
  - HA OpenTherm integration: ❌ BROKEN
```

### Example 2: Balanced (Home Assistant user)

```
Settings:
  - WiFi AP Password: OTGW2B3C (auto-generated, always on)
  - Telnet Enabled: NO
  - OTmonitor Enabled: YES
  - Web Auth Enabled: NO

Result:
  - Secure initial setup ✅
  - MQTT works ✅
  - HA Auto-Discovery works ✅
  - HA OpenTherm integration works ✅
  - OTmonitor app works ✅
  - No telnet debug ✅
```

### Example 3: Default (Backward Compatible)

```
Settings:
  - WiFi AP Password: OTGW3C4D (auto-generated, always on)
  - Telnet Enabled: YES
  - OTmonitor Enabled: YES
  - Web Auth Enabled: NO

Result:
  - Secure initial setup ✅
  - Everything works as before ✅
  - All integrations work ✅
  - Maximum compatibility ✅
```

---

## Questions for Maintainer

@rvdbreemen - Please review and answer:

1. **Approve Phase 1 implementation?**
   - WiFi AP password (always enabled)
   - Optional service disabling (default: both enabled)

2. **Default settings preference?**
   - Telnet enabled by default? (YES recommended for backward compat)
   - OTmonitor enabled by default? (YES recommended for backward compat)

3. **Web UI authentication demand?**
   - Is there strong community demand for this?
   - Worth the integration breakage?

4. **Documentation concerns?**
   - Any additional warnings needed?
   - Where to document AP password (README, label, wiki)?

---

**For detailed analysis, pros/cons, code examples, and full reasoning:**  
👉 See [SECURITY_IMPROVEMENT_ANALYSIS.md](SECURITY_IMPROVEMENT_ANALYSIS.md)
