## v1.3.0 — PIC Settings, One-Click OTA, Admin Protection, and Major Hardening

A major feature release building on v1.2.0. **No breaking changes** — backward-compatible upgrade for existing v1.2.0 users.

### Highlights

- **PIC Gateway Settings Panel** — All 15 PIC configuration registers now exposed via REST API (`/api/v2/pic/settings`), MQTT (`otgw-pic/settings/<key>`), and a new Web UI section with human-readable formatting, color-coded live/cached values, and browser localStorage caching (7-day TTL).
- **Single-Click GitHub Release OTA** — The update page lists GitHub releases with semver-aware Installed/Update/Rollback badges. One-click download and flash.
- **Optional Admin Endpoint Protection** — HTTP Basic Auth for settings, file management, reboot, and OTA routes. Disabled by default; existing setups unchanged.
- **Configurable MQTT Publish Gating** — Rate-limit OpenTherm and `PS=1` publishing to reduce broker load and WiFi chatter. Better status republish after boot and reconnect.
- **Full `PS=1` Summary Integration** — `PS=1` output now parsed into the data pipeline, published to MQTT, and exposed to Home Assistant discovery.
- **Light/Dark Theme Toggle** — Persistent per-browser theme switching via toggle button.
- **Triple-Reset WiFi Recovery** — Three quick hardware resets within 10 seconds clear stored WiFi credentials and reopen the captive portal.
- **Non-Blocking WiFi Reconnect** — State machine replaces blocking 30-second reconnect loop, preventing main-loop freezes.
- **OTGW Simulation Mode** — Test firmware and Web UI without physical hardware.
- **Crash Log Endpoint** — `GET /api/v2/device/crashlog` for ESP8266 diagnostics.
- **OTGW Event Reporting** — PIC restart, serial errors forwarded over MQTT and WebSocket.

### OTA & Flashing Hardening

- Reboot verification via `/api/v2/health`
- Browser backups for `settings.ini` and `dallas_labels.ini` before filesystem flash
- Dallas labels auto-preserved through localStorage — survives full LittleFS wipe
- WiFi activity suppressed during flash writes; full partition erase for LittleFS OTA
- Detailed XHR upload progress in telnet debug output

### Security Hardening

- Centralized HTTP Basic Auth enforcement for all POST/PUT API endpoints
- CORS wildcard replaced with dynamic origin validation
- Webhook hostname SSRF prevention via DNS resolution
- XSS fix in statistics table
- Boot command and MQTT payload validation
- ~450 lines of dead code removed

### Memory & Stability

- ArduinoJson dependency removed; bounded manual JSON handling
- Settings and state reorganized into `OTGWSettings` / `OTGWState` structs
- `String` class eliminated from hot paths (protocol, settings, HTTP, CSRF)
- MQTT autodiscovery memory reduced via streaming template rendering
- ~1,400 bytes of stack pressure removed through static/shared buffers
- `millis()` wraparound bug fixed, f8.8 negative value encoding fixed, OT message parse validation added

### Bug Fixes

- ESP hostname reverting to `ESP-XXXXXX` after reboot
- Settings page blank on iOS Safari
- Boot-time spurious service restarts
- Hostname normalization writing to wrong buffer
- MQTT subscription topic truncation
- IP validation incorrectly rejecting valid addresses with `255` octet
- WiFi portal triggered by stale RTC data after USB flash
- File Explorer delete handling
- Webhook payload truncation after reboot

### Web UI Polish

- One-shot OTGW PIC commands from the monitor page
- Gateway mode and WebSocket connection status indicators with tooltips
- Heap memory info in device status and footer
- GPIO conflict detection at boot
- Richer settings tooltips
- Firmware-update button hidden on touch devices
- CSS vendor prefix cleanup

### Upgrade Notes

1. Flash **both firmware and filesystem** so the Web UI, updater, and `PS=1` features stay in sync.
2. Hard-refresh the browser after flashing (Ctrl+F5).
3. If upgrading from older than v1.2.0, review the earlier MQTT and API migration notes in [RELEASE_NOTES_1.2.0.md](RELEASE_NOTES_1.2.0.md).

Full release notes: [RELEASE_NOTES_1.3.0.md](RELEASE_NOTES_1.3.0.md)
