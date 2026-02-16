# Security Improvement Analysis - README

**Issue:** [#358](https://github.com/rvdbreemen/OTGW-firmware/issues/358) - Security Improvement Suggestions  
**Status:** Analysis Complete - Awaiting Maintainer Decision  
**Date:** 2026-02-16

---

## 📚 Document Navigation

This directory contains a comprehensive security improvement analysis for the OTGW-firmware. Choose the document that best fits your needs:

### For Decision-Makers

**[SECURITY_IMPROVEMENT_OPTIONS.md](SECURITY_IMPROVEMENT_OPTIONS.md)** (8 pages)  
Quick reference guide with TL;DR recommendations, comparison tables, and user personas.  
📖 **Read this if:** You need to make a quick decision about what to implement.

### For Technical Review

**[SECURITY_IMPROVEMENT_ANALYSIS.md](SECURITY_IMPROVEMENT_ANALYSIS.md)** (40 pages)  
Complete technical analysis with implementation options, trade-offs, pros/cons, and detailed reasoning.  
📖 **Read this if:** You want to understand the full context and all options.

### For Implementation

**[SECURITY_IMPLEMENTATION_EXAMPLES.md](SECURITY_IMPLEMENTATION_EXAMPLES.md)** (16 pages)  
Copy-paste-ready code examples, testing checklists, and rollback plans.  
📖 **Read this if:** You're ready to implement the approved changes.

### For Quick Comparison

**[security_comparison.txt](security_comparison.txt)** (ASCII charts)  
Visual comparison charts showing effort vs. security gain, memory impact, and integration compatibility.  
📖 **Read this if:** You want a visual overview of all options.

---

## 🎯 Executive Summary

### The Problem
Community requested five security improvements:
1. Fix WiFi AP mode security (Issue #131) - open AP during setup
2. Add username/password authentication to Web UI
3. Support HTTPS (even with self-signed certificates)
4. Allow disabling telnet (port 23) when not needed
5. Allow disabling OTmonitor (port 25238) when not needed

### The Recommendations

#### ✅ Phase 1: Implement These (Recommended)

| Feature | Effort | Security Gain | Memory | User Impact |
|---------|--------|---------------|--------|-------------|
| WiFi AP Password | 30 min | 🔒🔒🔒 HIGH | 0 bytes | None |
| Optional Telnet Disable | 2-3 hrs | 🔒🔒 MEDIUM | Saves 512 bytes | Optional |
| Optional OTmonitor Disable | 2-3 hrs | 🔒🔒🔒 HIGH | Saves 256 bytes | Optional* |

*Breaks OTmonitor app and HA OpenTherm integration when disabled by user choice.

**Total Phase 1 Effort:** ~1 day  
**Total Memory Impact:** 0 to -768 bytes (improvement)  
**Total Security Improvement:** HIGH

#### ⚠️ Phase 2: Consider Carefully (Optional)

| Feature | Effort | Security Gain | Memory | User Impact |
|---------|--------|---------------|--------|-------------|
| Web UI Authentication | 6-8 hrs | 🔒🔒 MEDIUM | +500 bytes | **BREAKS integrations** |

**Recommended:** Only implement if there's strong community demand. Breaks HA Auto-Discovery and OTmonitor.

#### ❌ Rejected

| Feature | Reason |
|---------|--------|
| HTTPS Support | Already rejected in ADR-003 (insufficient RAM: requires 10-30KB) |

---

## 🔍 Key Findings

### 1. WiFi AP Password (Top Priority)

**Current Problem:**  
Device creates **open WiFi AP** during initial setup. Anyone within range can connect and configure the device.

**Solution:**  
Generate unique password from chip ID (e.g., `OTGW1A2B`), display on Serial console during first boot.

**Implementation:** 30 minutes, 0 bytes RAM, HIGH security improvement.

**User Impact:** Minimal - one-time setup change. User needs USB cable and Serial monitor to read password.

---

### 2. Optional Service Disabling (Recommended)

**Current Problem:**  
All network services always running, even if not used. Increases attack surface.

**Solution:**  
Add optional toggles for telnet (port 23) and OTmonitor (port 25238). Default: both enabled (backward compatible).

**Use Cases:**
- **Disable Telnet:** User doesn't debug, wants to reduce attack surface → Saves 512 bytes
- **Disable OTmonitor:** User only uses MQTT, doesn't need OTmonitor app → Saves 256 bytes + prevents unauthorized OTGW commands

**Implementation:** 4-6 hours total, saves RAM when disabled.

**User Impact:** Optional. Clear warnings when disabling OTmonitor (breaks integrations).

---

### 3. Web UI Authentication (Conditional)

**Current Problem:**  
Anyone on local network can access Web UI and change settings.

**Solution:**  
Optional HTTP Basic Authentication. Default: disabled (backward compatible).

**Trade-offs:**
- ✅ Protects Web UI from unauthorized access
- ❌ **BREAKS Home Assistant MQTT Auto-Discovery** (can't authenticate REST API)
- ❌ **BREAKS OTmonitor app** (no authentication support)
- ❌ Credentials sent in cleartext without TLS (limited security benefit)

**Recommendation:** Only implement if there's strong community demand. Document that enabling it breaks integrations.

---

### 4. HTTPS Support (Rejected)

**Request:** Add HTTPS to Web UI and REST API.

**Existing Decision:** Already analyzed and **rejected in ADR-003** (HTTP-Only Network Architecture).

**Reason:** ESP8266 has insufficient RAM:
- TLS handshake requires 20-30KB (50-75% of available heap)
- Even "lightweight" TLS needs 10-15KB (25-37% of heap)
- Leaves insufficient memory for stable operation
- Self-signed certificates provide minimal security benefit (browser warnings)

**Alternative Solutions:**
- Use VPN for remote access (WireGuard, OpenVPN)
- Use reverse proxy with valid TLS cert on home server
- Network segmentation (IoT VLAN)

**Future:** Only viable with ESP32 migration (8x more RAM) - would require superseding ADR-001 and ADR-003.

---

## 📊 Integration Compatibility

| Feature | HA MQTT Auto-Discovery | HA OpenTherm Gateway | OTmonitor App | MQTT Publishing |
|---------|------------------------|----------------------|---------------|-----------------|
| WiFi AP Password | ✅ Works | ✅ Works | ✅ Works | ✅ Works |
| Disable Telnet | ✅ Works | ✅ Works | ✅ Works | ✅ Works |
| Disable OTmonitor | ✅ Works | ❌ Breaks | ❌ Breaks | ✅ Works |
| Web UI Auth (enabled) | ❌ Breaks | ❌ Breaks | ❌ Breaks | ✅ Works |
| HTTPS | N/A (rejected) | N/A | N/A | N/A |

---

## 💾 Memory Impact Summary

```
ESP8266 Available RAM: ~40KB
Current Usage: ~25-30KB
Critical Threshold: <10KB

Implementation Options:

┌────────────────────────┬─────────────────┬─────────────┐
│ Option                 │ RAM Impact      │ Net Result  │
├────────────────────────┼─────────────────┼─────────────┤
│ WiFi AP Password       │ 0 bytes         │ No change   │
│ Disable Telnet         │ -512 bytes      │ MORE RAM ✅ │
│ Disable OTmonitor      │ -256 bytes      │ MORE RAM ✅ │
│ Web UI Auth (optional) │ +500 bytes      │ LESS RAM ⚠️ │
│ HTTPS (rejected)       │ +10-30KB        │ NOT VIABLE ❌│
└────────────────────────┴─────────────────┴─────────────┘

Best Case (all services disabled): +768 bytes free RAM
Worst Case (web auth enabled):     -500 bytes free RAM
```

---

## 🎭 User Personas

### Persona 1: "Maximum Security User"
- Doesn't use OTmonitor or HA OpenTherm integration
- Willing to use MQTT only
- **Recommended Settings:**
  - ✅ WiFi AP Password (always on)
  - ✅ Disable Telnet
  - ✅ Disable OTmonitor
  - ✅ Enable Web UI Auth (optional)
- **Security Level:** 🔒🔒🔒🔒🔒 Excellent

### Persona 2: "Home Assistant User"
- Uses HA MQTT Auto-Discovery
- May use HA OpenTherm Gateway integration
- **Recommended Settings:**
  - ✅ WiFi AP Password (always on)
  - ✅ Disable Telnet (if not debugging)
  - ✅ Keep OTmonitor enabled
  - ❌ Don't enable Web UI Auth (breaks HA)
- **Security Level:** 🔒🔒🔒🔒 Very Good

### Persona 3: "OTmonitor User"
- Uses OTmonitor desktop app
- Doesn't use Home Assistant
- **Recommended Settings:**
  - ✅ WiFi AP Password (always on)
  - ✅ Disable Telnet (if not debugging)
  - ✅ Keep OTmonitor enabled
  - ⚠️ Web UI Auth (optional, doesn't affect OTmonitor)
- **Security Level:** 🔒🔒🔒 Good

### Persona 4: "Default User"
- Wants everything to work
- Minimal configuration
- **Recommended Settings:**
  - ✅ WiFi AP Password (always on)
  - ✅ Keep Telnet enabled
  - ✅ Keep OTmonitor enabled
  - ❌ Don't enable Web UI Auth
- **Security Level:** 🔒🔒 Acceptable (local network)

---

## 📝 Implementation Checklist

### Before Implementation
- [ ] Maintainer reviews all analysis documents
- [ ] Decision made on which options to implement
- [ ] Community feedback collected (optional)
- [ ] Default settings decided

### Phase 1 Implementation (Recommended)
- [ ] WiFi AP password generation implemented
- [ ] Serial console password display implemented
- [ ] Optional telnet toggle implemented
- [ ] Optional OTmonitor toggle implemented
- [ ] Web UI settings page updated
- [ ] Settings persistence implemented
- [ ] All changes tested on hardware
- [ ] Documentation updated (README, wiki)

### Phase 2 Implementation (If Requested)
- [ ] Community demand assessed
- [ ] Web UI authentication implemented
- [ ] Integration breakage documented
- [ ] Warning messages added to UI
- [ ] Testing completed

### Documentation Updates
- [ ] README.md first-time setup section
- [ ] README.md security settings section
- [ ] Wiki setup guide
- [ ] Wiki troubleshooting guide
- [ ] Release notes prepared

### Testing
- [ ] WiFi AP password tested on multiple devices
- [ ] Telnet enable/disable tested
- [ ] OTmonitor enable/disable tested
- [ ] Settings persistence verified
- [ ] Integration compatibility verified
- [ ] Memory usage measured
- [ ] Performance impact assessed

---

## ❓ Questions for Maintainer

@rvdbreemen - Please review and provide feedback:

### Phase 1 Approval
1. **Do you approve Phase 1 implementation?**
   - WiFi AP password (always enabled)
   - Optional telnet toggle
   - Optional OTmonitor toggle

2. **Default settings for optional features?**
   - Telnet: ☐ Enabled (backward compat) / ☐ Disabled (security)
   - OTmonitor: ☐ Enabled (backward compat) / ☐ Disabled (security)

3. **WiFi AP password display:**
   - ☐ Serial console only (current plan)
   - ☐ Also on LED screen
   - ☐ Print on device label/sticker

### Phase 2 Decision
4. **Web UI authentication demand?**
   - ☐ High demand - implement it
   - ☐ Medium demand - wait and see
   - ☐ Low demand - don't implement
   - ☐ Need community poll

5. **Integration breakage acceptable?**
   - ☐ YES - if optional and well-documented
   - ☐ NO - breaks too many things
   - ☐ UNSURE - need more discussion

### Timeline
6. **Target release?**
   - ☐ v1.2.0 (next minor release)
   - ☐ v2.0.0 (next major release)
   - ☐ No timeline yet

### Other
7. **Additional concerns or requirements?**
   - (Your feedback here)

---

## 📚 Related Documents

### Existing ADRs
- **ADR-003:** HTTP-Only Network Architecture (rejects HTTPS)
- **ADR-017:** WiFiManager Initial Configuration (current WiFi setup)
- **ADR-024:** Debug Telnet Command Console (telnet design)
- **ADR-032:** No Authentication Pattern (current security model)

### Code References
- `src/OTGW-firmware/networkStuff.h` - WiFi setup, telnet
- `src/OTGW-firmware/OTGW-Core.ino` - OTmonitor TCP socket
- `src/OTGW-firmware/OTGW-firmware.h` - Settings definitions
- `src/OTGW-firmware/restAPI.ino` - Web UI endpoints

### Issues
- **Issue #358:** Security Improvement Suggestions (this analysis)
- **Issue #131:** WiFi AP Security (referenced in #358)

---

## 📞 Contact & Feedback

- **Issue Discussion:** [#358](https://github.com/rvdbreemen/OTGW-firmware/issues/358)
- **Discord:** [OTGW-firmware Discord](https://discord.gg/zjW3ju7vGQ)
- **Maintainer:** @rvdbreemen

---

**Document Status:** ✅ Analysis Complete - Awaiting Decision  
**Last Updated:** 2026-02-16  
**Next Step:** Maintainer review and decision on Phase 1 implementation
