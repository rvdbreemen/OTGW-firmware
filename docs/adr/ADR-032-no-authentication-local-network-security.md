# ADR-032: No Authentication Pattern (Local Network Security Model)

**Status:** Accepted  
**Date:** 2026-02-07  
**Decision Maker:** Copilot Agent based on codebase analysis and ADR-003

## Context

The OTGW-firmware provides multiple network-accessible interfaces:
- **HTTP Server:** REST API, Web UI, file system explorer
- **WebSocket Server:** Real-time OpenTherm message streaming (port 81)
- **MQTT Client:** Home Assistant integration
- **TCP Server:** OTmonitor compatibility (port 25238)
- **Telnet Server:** Debug console (port 23)

**Security question:** Should these interfaces require authentication (username/password, API keys, JWT tokens)?

**Key considerations:**
1. **Target deployment:** Home local network (not internet-facing)
2. **Memory constraints:** ESP8266 has ~20-25KB available RAM
3. **Primary use case:** Home automation integration (Home Assistant)
4. **User experience:** Ease of setup and use
5. **Security model:** Network isolation vs application-level authentication

## Decision

**Do NOT implement authentication on any network interface. Rely on network-level security (WiFi encryption, network segmentation) instead of application-level authentication.**

**Rationale:**

1. **Local Network Deployment:** Device is designed for trusted home networks, not internet exposure
2. **Memory Constraints:** Authentication libraries (TLS, JWT, session management) consume precious RAM
3. **User Experience:** Zero-configuration integration with Home Assistant via MQTT Auto-Discovery
4. **Security by Design:** Network isolation provides stronger security than application authentication
5. **Explicit Documentation:** "No auth" is a deliberate choice, not an oversight

**Security Model:**

```
┌──────────────────────────────────────────────────────────┐
│ Security Layers (Defense in Depth)                       │
└──────────────────────────────────────────────────────────┘

Layer 1: Physical Security
- Device inside home
- Physical access controlled
                ▼

Layer 2: Network Isolation
- Device on isolated local network
- WiFi WPA2/WPA3 encryption
- Router firewall blocks internet access
- No port forwarding to device
                ▼

Layer 3: VPN for Remote Access (User Responsibility)
- Remote access via VPN tunnel
- VPN provides authentication and encryption
- Device not directly exposed to internet
                ▼

Layer 4: Application Layer (OTGW-firmware)
- NO authentication on HTTP/WebSocket/Telnet
- NO TLS/SSL encryption
- MQTT password for broker (optional, external)
- Trust model: Network provides security
```

**Implementation:**
- HTTP endpoints: No authentication required
- WebSocket: No authentication required
- REST API: No authentication required
- Telnet: No authentication required
- File uploads: No authentication required
- Firmware updates: No authentication required (blank username/password accepted)

**MQTT Exception:**
- MQTT broker may require username/password
- Credentials stored in OTGW settings
- Authentication handled by broker, not OTGW
- Password masked in Web UI ("notthepassword" placeholder)

## Alternatives Considered

### Alternative 1: HTTP Basic Authentication

**Pros:**
- Simple to implement
- Supported by all browsers
- Standard protocol (RFC 7617)
- Username/password protection

**Cons:**
- Credentials sent in base64 (easily decoded without TLS)
- Every HTTP request requires authentication header
- Breaks MQTT Auto-Discovery (Home Assistant can't auto-auth)
- Breaks OTmonitor integration (no auth support)
- RAM overhead for session management
- User must configure credentials

**Why not chosen:** Breaks zero-configuration integration and doesn't provide meaningful security without TLS.

### Alternative 2: API Keys

**Pros:**
- Token-based access control
- Can revoke keys
- Modern pattern
- Better than passwords

**Cons:**
- Requires key generation and storage
- Key management complexity
- Breaks MQTT Auto-Discovery
- Breaks OTmonitor integration
- Users must manage keys
- No encryption (keys sent in clear)

**Why not chosen:** Adds complexity without solving the TLS problem. Network isolation is more effective.

### Alternative 3: JWT Tokens

**Pros:**
- Stateless authentication
- Industry standard
- Can include claims
- Supports expiration

**Cons:**
- Complex implementation
- Requires TLS for security
- Significant RAM overhead
- Breaks integrations
- Token management complexity
- Overkill for local network

**Why not chosen:** Too complex for ESP8266, breaks integrations, requires TLS to be secure.

### Alternative 4: TLS Client Certificates

**Pros:**
- Strong cryptographic authentication
- Mutual TLS provides encryption + auth
- No password management

**Cons:**
- Extremely complex setup
- Certificate management nightmare
- 20-30KB RAM for TLS (prohibitive)
- Breaks all integrations
- User configuration complexity
- Performance impact

**Why not chosen:** Memory constraints make TLS impossible, complexity is prohibitive.

### Alternative 5: Network Segmentation with Firewall Rules

**Pros:**
- Strong security boundary
- No application changes needed
- Router-level control
- Industry best practice
- Protects all services simultaneously

**Cons:**
- Requires network configuration
- User responsibility
- Not enforced by device

**Why CHOSEN:** Most effective security approach for local network devices. Recommended in documentation.

## Consequences

### Positive

1. **Zero-Configuration Setup** ✅
   - Home Assistant MQTT Auto-Discovery works out of box
   - No credential configuration needed
   - OTmonitor connects without setup
   - Web UI immediately accessible

2. **Maximum Memory Available** ✅
   - No RAM consumed by authentication libraries
   - No TLS overhead (20-30KB saved)
   - More memory for application features
   - Better stability and performance

3. **Integration Compatibility** ✅
   - MQTT Auto-Discovery works seamlessly
   - OTmonitor TCP protocol unmodified
   - REST API accessible from scripts
   - WebSocket streaming works in all browsers

4. **Simplified Codebase** ✅
   - No authentication logic
   - No session management
   - No credential storage
   - Easier to maintain and audit

5. **Better User Experience** ✅
   - No password fatigue
   - No locked-out users
   - No password resets
   - Immediate access for troubleshooting

### Negative

1. **No Application-Level Access Control** ⚠️
   - Anyone on local network can access device
   - **Mitigation:** Network segmentation recommended
   - **Mitigation:** WiFi encryption (WPA2/WPA3)
   - **Mitigation:** Router firewall rules
   - **Accepted:** Trust model assumes trusted local network

2. **Vulnerable if Exposed to Internet** 🔴 CRITICAL
   - Device has NO protection against internet attacks
   - **Mitigation:** Documentation explicitly warns against internet exposure
   - **Mitigation:** No port forwarding to device
   - **Mitigation:** VPN for remote access
   - **Mitigation:** README prominently warns users
   - **CRITICAL:** Users MUST NOT expose device to internet

3. **No Audit Trail** ⚠️
   - Cannot track who accessed device
   - Cannot identify unauthorized access
   - **Accepted:** Local network assumption means less concern
   - **Mitigation:** Network-level logging on router

4. **Password Visibility Risk** ⚠️
   - MQTT password stored in settings.ini
   - **Mitigation:** Password masked in Web UI ("notthepassword")
   - **Mitigation:** settings.ini readable only via authenticated LittleFS access
   - **Accepted:** Local network trust model

### Risks & Mitigation

**Risk 1:** User exposes device to internet
- **Impact:** CRITICAL - Full compromise, malware installation, boiler control
- **Likelihood:** Medium - Some users may enable port forwarding
- **Mitigation:** Prominent warning in README.md
- **Mitigation:** Documentation emphasizes VPN for remote access
- **Mitigation:** Web UI footer shows "Local network only"
- **Mitigation:** No documentation of internet exposure patterns
- **Monitoring:** Cannot prevent user misconfiguration

**Risk 2:** Malicious client on local network
- **Impact:** High - Can control boiler, modify settings, upload malware
- **Likelihood:** Low - Requires local network access
- **Mitigation:** Network segmentation isolates IoT devices
- **Mitigation:** Router firewall rules
- **Mitigation:** WiFi client isolation mode
- **Monitoring:** No application-level defense

**Risk 3:** Compromised local network
- **Impact:** High - Attacker has access to all devices
- **Likelihood:** Low - Requires WiFi password compromise
- **Mitigation:** WPA3 encryption recommended
- **Mitigation:** Strong WiFi password
- **Mitigation:** Regular router firmware updates
- **Mitigation:** Guest network for untrusted devices
- **Monitoring:** Router-level intrusion detection

## Security Recommendations

**Documented in README.md and ADR-003:**

### For Users

**MUST DO:**
1. ❌ **NEVER expose device directly to internet** (no port forwarding)
2. ✅ **Use VPN for remote access** (WireGuard, OpenVPN)
3. ✅ **Keep device on trusted local network only**
4. ✅ **Use strong WiFi password (WPA2/WPA3)**

**SHOULD DO:**
1. ✅ **Network segmentation:** Separate IoT VLAN
2. ✅ **Router firewall rules:** Limit device access
3. ✅ **WiFi client isolation:** Prevent device-to-device attacks
4. ✅ **Regular firmware updates:** Keep device patched

**COULD DO:**
1. ✅ **Reverse proxy with authentication** (for advanced users)
   - Note: WebSocket features may not work via HTTPS proxy (see ADR-003)
2. ✅ **Network monitoring:** IDS/IPS on router
3. ✅ **MAC address filtering:** Restrict WiFi access

### For Developers

**MUST NOT:**
1. ❌ **Do NOT add internet-facing features**
2. ❌ **Do NOT document port forwarding patterns**
3. ❌ **Do NOT create cloud integration without explicit user consent**

**SHOULD:**
1. ✅ **Maintain local-network-only design**
2. ✅ **Document security model clearly**
3. ✅ **Review network code for vulnerabilities**
4. ✅ **Validate all user inputs** (prevent injection attacks)

## Explicit Trust Model

**Assumptions:**
1. Local network is trusted
2. Physical access is controlled
3. WiFi is encrypted (WPA2/WPA3)
4. Router provides firewall
5. Users follow security recommendations
6. Device is NOT exposed to internet

**Out of Scope:**
- Defense against local network attackers
- Protection against compromised local devices
- Audit trail of device access
- User authentication and authorization

## MQTT Password Handling

**Special Case:** MQTT broker authentication

```cpp
// Settings storage
String settingMQTTpasswd;  // MQTT broker password

// Web UI display (masked)
sendJsonSettingObj(F("mqttpasswd"), "notthepassword", "p", 100);

// Settings update (only if not placeholder)
if (newValue && strcasecmp_P(newValue, PSTR("notthepassword")) != 0) {
  settingMQTTpasswd = newValue;  // Update password
}

// MQTT connection
if (settingMQTTuser.length() > 0) {
  mqtt.connect(clientId, settingMQTTuser.c_str(), settingMQTTpasswd.c_str());
} else {
  mqtt.connect(clientId);  // Anonymous connection
}
```

**Rationale:**
- Password is for external MQTT broker
- OTGW doesn't validate the password
- Password masked in UI to prevent shoulder surfing
- Placeholder "notthepassword" prevents accidental clearing

## Comparison with Alternative Architectures

### If TLS Were Possible (ESP32)

**Hypothetical ESP32 Implementation:**
- Could support HTTPS (20-30KB RAM available)
- Could use TLS client certificates
- Could implement OAuth2/JWT
- Would break OTmonitor compatibility
- Would break MQTT Auto-Discovery
- Would add significant complexity

**Still recommend no auth:**
Even with TLS capability, network isolation is more effective than application authentication for local devices.

## Related Decisions

- **ADR-003:** HTTP-Only Network Architecture (No HTTPS/TLS)
- **ADR-001:** ESP8266 Platform Selection (establishes RAM constraints)
- **ADR-006:** MQTT Integration Pattern (external broker authentication)
- **ADR-005:** WebSocket Real-Time Streaming (no authentication on port 81)
- **ADR-010:** Multiple Concurrent Network Services (all use same security model)

## References

- **README.md:** Security recommendations section
- **ADR-003:** HTTP-Only Network Architecture
- **Code:** `src/OTGW-firmware/webSocketStuff.ino:35` (comment: "no built-in authentication")
- **Code:** `src/OTGW-firmware/networkStuff.h:78` (comment: "no authentication on WebSocket")
- **Code:** `src/OTGW-firmware/OTGW-ModUpdateServer-impl.h:90` (blank username/password accepted)
- **Code:** `src/OTGW-firmware/settingStuff.ino:244` (MQTT password handling)
- **Documentation:** Home network deployment section

## Future Considerations

**If Requirements Change:**

If future requirements demand authentication (e.g., cloud integration, multi-user support):

1. **Create new ADR** that supersedes this one
2. **Explain why requirements changed**
3. **Choose authentication appropriate for new threat model**
4. **Maintain backwards compatibility** (auth optional, default off)
5. **Update security documentation**

**Likely triggers:**
- Cloud integration feature requested
- Multi-tenant deployment needed
- Regulatory compliance required
- User community requests auth option

**Unlikely to change:**
Current local network use case is stable and well-suited to no-auth model.

---

**This ADR explicitly documents the deliberate decision to omit authentication in favor of network-level security. The "no auth" pattern is a conscious architectural choice based on deployment model, memory constraints, and integration requirements - not an oversight or future TODO item.**
