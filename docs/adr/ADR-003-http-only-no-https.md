# ADR-003: HTTP-Only Network Architecture (No HTTPS)

**Status:** Accepted  
**Date:** 2018-01-01 (Estimated)  
**Updated:** 2026-01-28 (Documentation)

## Context

The OTGW-firmware provides web-based interfaces including:
- HTTP REST API for configuration and monitoring
- WebSocket server for real-time OpenTherm message streaming
- Web UI for device management
- File system browser

Security considerations arose regarding whether to implement HTTPS/TLS and WSS (WebSocket Secure) to protect these interfaces.

**Key constraints:**
- ESP8266 has limited RAM (~40KB usable)
- TLS/SSL requires significant memory for handshake and certificates (~20-30KB)
- Target deployment: Home local network (not internet-facing)
- Primary use case: Home automation integration via Home Assistant

## Decision

**Use HTTP-only protocols. Do NOT implement HTTPS or WSS (WebSocket Secure).**

**Implementation:**
- All web traffic uses `http://` protocol
- WebSocket connections use `ws://` protocol (never `wss://`)
- No TLS/SSL certificate management
- No encrypted transport layer
- No authentication mechanisms on HTTP endpoints

**Security model:**
- Device accessible only on trusted local networks
- Remote access via VPN or secure tunnel (external to device)
- Network-level security via WiFi encryption (WPA2/WPA3)
- Physical security assumed (device in home)

## Alternatives Considered

### Alternative 1: HTTPS with Self-Signed Certificates
**Pros:**
- Encrypted transport
- Protection against passive eavesdropping on local network
- Industry best practice for web services

**Cons:**
- Requires 20-30KB RAM for TLS handshake (50-75% of available heap)
- Certificate management complexity
- Browser warnings for self-signed certificates
- Performance overhead on ESP8266 CPU
- WebSocket over TLS adds additional complexity
- OTA updates may fail due to insufficient memory

**Why not chosen:** Memory constraints. TLS would consume the majority of available heap, leaving insufficient memory for normal operations, especially with multiple concurrent WebSocket clients.

### Alternative 2: HTTPS with Certificate Pinning
**Pros:**
- Better security than self-signed
- Client can validate server identity

**Cons:**
- Same memory constraints as Alternative 1
- Certificate rotation requires firmware updates
- User complexity to configure clients
- Still prohibitive memory overhead

**Why not chosen:** Memory overhead remains prohibitive, with added complexity.

### Alternative 3: Lightweight TLS (e.g., wolfSSL, mbedTLS)
**Pros:**
- Reduced memory footprint vs full TLS (~10-15KB)
- Encryption without full TLS overhead
- Some ESP8266 Arduino cores include mbedTLS

**Cons:**
- Still requires 10-15KB heap (25-37% of available memory)
- Reduced heap leads to crashes under load
- CPU overhead impacts responsiveness
- Limited cipher suite support
- Complex integration with WebSocket library

**Why not chosen:** Even "lightweight" TLS consumes too much memory for stable operation with multiple concurrent services.

### Alternative 4: Application-Level Encryption
**Pros:**
- Custom encryption scheme for sensitive data
- Control over memory usage
- No transport-level overhead

**Cons:**
- Security through obscurity (bad practice)
- Difficult to implement correctly
- No protection for WebSocket streams
- Browser incompatibility
- Maintenance burden

**Why not chosen:** Poor security practice and incompatible with browser-based access.

## Consequences

### Positive
- **Memory available:** 100% of heap available for application functionality
- **Simplicity:** No certificate management, renewal, or distribution
- **Performance:** No TLS handshake or encryption overhead
- **Compatibility:** Works with all browsers and HTTP clients without warnings
- **Development velocity:** Faster iteration without TLS debugging
- **Multiple concurrent services:** Sufficient memory for HTTP, MQTT, WebSocket simultaneously

### Negative
- **No transport encryption:** Network traffic visible to anyone on local network
  - Mitigation: Local network deployment only, VPN for remote access
- **No authentication:** Anyone with network access can control device
  - Accepted: Assumption of trusted local network
- **Credentials in clear text:** MQTT passwords, WiFi passwords visible in settings
  - Mitigation: Web UI masks passwords, local network trust model
- **Man-in-the-middle vulnerable:** Attacker on local network can intercept/modify traffic
  - Accepted: Physical/network security assumed

### Risks & Mitigation
- **Internet exposure:** If device accidentally exposed to internet, no protection
  - **Mitigation:** Documentation explicitly states "local network only," no port forwarding
- **Malicious insider:** Attacker with local network access can compromise device
  - **Accepted:** Home network threat model assumes trusted users
- **WiFi eavesdropping:** Attacker could capture traffic if WiFi is weak (WEP) or open
  - **Mitigation:** Documentation recommends WPA2/WPA3 for WiFi network

## Documentation Requirements

Per this decision, the following **MUST** be documented:

1. **User Documentation:**
   - Device is for local network use only
   - Use VPN for remote access (never port forwarding)
   - Ensure WiFi network uses WPA2/WPA3 encryption

2. **Code Comments:**
   - WebSocket implementation explicitly notes "local network only, no WSS"
   - HTTP server comments reference this ADR

3. **Copilot Instructions:**
   - **CRITICAL:** Never add HTTPS or WSS protocol detection or support
   - Always use `http://` and `ws://` protocols
   - Reject any PR attempting to add TLS/SSL

## Related Decisions
- ADR-001: ESP8266 Platform Selection (memory constraints)
- ADR-004: Static Buffer Allocation Strategy (memory management)

## References
- ESP8266 TLS memory requirements: https://github.com/esp8266/Arduino/issues/4826
- Local network security model: Repository README.md "Network Architecture"
- Copilot instructions: `.github/copilot-instructions.md` (Network Architecture section)
- WebSocket implementation: `webSocketStuff.ino` (comments note HTTP-only)
