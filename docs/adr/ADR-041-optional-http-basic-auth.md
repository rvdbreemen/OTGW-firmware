# ADR-041: Optional HTTP Basic Authentication for Settings

**Status:** Accepted  
**Date:** 2026-02-24  
**Supersedes:** ADR-032 (partially — ADR-032 remains the baseline; this adds opt-in auth for settings)

## Context

ADR-032 established a no-authentication model for all OTGW network interfaces, relying on network-level security (WiFi WPA2/WPA3, router firewall). This was the correct choice for a zero-configuration local-network device.

However, users on networks with multiple clients, shared environments, or wanting defense-in-depth have requested the ability to optionally password-protect sensitive operations (settings read/write, file upload, reboot, OTA update). The issue tracker received a request: "Consider adding authentication (security) on the settings of the OTGW."

**Key constraints:**
1. **Backward compatibility:** Existing deployments must continue working without any configuration change (empty password = no auth)
2. **Memory:** ESP8266 has ~40KB RAM — solution must be lightweight
3. **Integration:** MQTT Auto-Discovery and OT data API must remain unauthenticated (Home Assistant expects open access to sensor data)
4. **No TLS:** HTTP-only (ADR-003), so credentials travel in base64 — acceptable on trusted local networks

## Decision

**Add optional HTTP Basic Authentication to sensitive endpoints only.** Authentication is disabled by default (empty password). When a password is configured, it protects:

- `GET/POST /api/*/settings` — settings read/write (exposes MQTT credentials)
- `POST /upload` — filesystem file upload
- `GET /ReBoot` — device reboot
- `GET /ResetWireless` — WiFi credential reset
- File delete via `?delete=` query parameter
- `GET/POST /update` — OTA firmware/filesystem update

**Authentication is NOT required for:**
- Health/status endpoints (`/api/*/health`, `/api/*/flash/status`)
- OpenTherm sensor data (`/api/*/otgw/*`)
- Device info and time (`/api/*/device/*`)
- WebSocket stream (port 81)
- MQTT data publishing

**Credentials:** Username is fixed as `admin`; only the password is user-configurable. This simplifies the UI and matches embedded device conventions.

**Implementation:** One new setting `settingHTTPpasswd` (char[41], default empty). A `checkHttpAuth()` helper function returns `true` immediately if no password is set, else calls `httpServer.authenticate("admin", settingHTTPpasswd)`. The OTA update server (`httpUpdater`) receives credentials via `updateCredentials()` immediately when the password is changed.

The password is stored in `settings.ini` and masked in the settings API response as `"notthepassword"` (same pattern used for MQTT password).

## Alternatives Considered

### Alternative 1: No change (current state)
- **Pros:** Zero complexity, no RAM overhead
- **Cons:** Any device on the local network can modify settings and upload firmware
- **Why not chosen:** The issue request and user feedback indicate this is an inadequate security posture for some deployments

### Alternative 2: Protect ALL endpoints (including sensor data)
- **Pros:** Stronger security boundary
- **Cons:** Breaks Home Assistant MQTT Auto-Discovery polling; breaks OTmonitor integration; requires auth header in every HA data fetch
- **Why not chosen:** Would break primary use case of HA integration

### Alternative 3: Separate username + password fields
- **Pros:** More flexibility per standard HTTP Basic Auth
- **Cons:** Uses 82 extra bytes of RAM for the username buffer; complicates UI; most embedded devices use a fixed username
- **Why not chosen:** Marginal benefit; fixed "admin" username is the established convention for embedded devices

## Consequences

### Positive
1. **Optional protection:** Users who want security can set a password; existing deployments are unaffected
2. **Minimal RAM overhead:** One 41-byte buffer added (~0.1% of available RAM)
3. **Covers high-risk operations:** Settings, file upload, reboot, OTA update
4. **Consistent with embedded device pattern:** Fixed "admin" username + configurable password
5. **Integration preserved:** HA sensor data and OT monitoring remain unauthenticated

### Negative
1. **Credentials in cleartext:** HTTP Basic Auth sends base64-encoded credentials without TLS — acceptable on trusted local networks, not for internet exposure
2. **Fixed username:** Advanced users cannot change the username
3. **No session management:** Every browser request to a protected page requires the credentials header (browser caches automatically)
4. **Partial protection:** The WebSocket stream (port 81) and TCP socket (port 25238) remain unauthenticated — consistent with ADR-032

### Risks & Mitigation
- **Risk:** User sets a password and gets locked out  
  **Mitigation:** Reset by editing `settings.ini` via FSexplorer (itself protected when password set — use physical reset or direct serial access)
- **Risk:** Credentials intercepted on network  
  **Mitigation:** Device is designed for local trusted networks only (ADR-003); not suitable for internet exposure regardless of auth
- **Risk:** Confusion about partial protection  
  **Mitigation:** Documentation clarifies which endpoints are protected and which are open

## Related Decisions

- **ADR-032:** No Authentication Pattern (Local Network Security Model) — baseline decision, remains valid; this ADR adds opt-in layer
- **ADR-003:** HTTP-Only Network Architecture — explains why TLS is not feasible
- **ADR-001:** ESP8266 Platform Selection — establishes RAM constraints
- **ADR-006:** MQTT Integration Pattern — MQTT data endpoints remain unauthenticated

## References

- Issue: "Consider adding authentication (security) on the settings of the OTGW"
- Code: `src/OTGW-firmware/restAPI.ino` — `checkHttpAuth()` helper function
- Code: `src/OTGW-firmware/settingStuff.ino` — `settingHTTPpasswd` read/write/update
- Code: `src/OTGW-firmware/FSexplorer.ino` — protected upload/reboot/delete endpoints
- Code: `src/OTGW-firmware/OTGW-firmware.h` — `settingHTTPpasswd` declaration
