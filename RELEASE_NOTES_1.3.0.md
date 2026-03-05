# Release Notes — v1.3.0

**Last updated:** 2026-03-05<br>
**Release branch:** dev<br>
**Comparison target:** v1.2.0<br>

---

## Summary

v1.3.0 builds on v1.2.0 with focused reliability, usability, and memory improvements. There are no breaking changes: no MQTT topic renames, no API removals, and no settings format changes.

**New features:** 
- Configurable MQTT publishing interval to reduce broker load.
- Triple-reset WiFi recovery.
- One-shot OTGW PIC commands from the web UI.
- Full PS=1 summary parsing with MQTT/HA discovery.
- OTGW events reported via MQTT and WebSocket.
- Heap status included directly in device info.

**Bug fixes:** 
- Boot-time spurious service restarts.
- Hostname dot-stripping targeting the wrong buffer.
- Webhook payload truncation on settings load.

**Memory / performance:** 
- String heap fragmentation eliminated from the settings save path.
- ArduinoJson library fully removed in favor of a custom JSON writer.
- REST API v2 migration officially completed for the OTA updater.

---

## New Features

### ⏱️ Configurable MQTT Publishing Interval
**Problem solved**: Certain OT values update very frequently. Previously, every change immediately pushed an MQTT message, sometimes flooding the broker and consuming excess WiFi airtime.
**Solution**: Introduced OTPublishGate to configure a minimum publishing interval (in seconds) for OpenTherm variables. This throttles rapid state changes to a manageable rate without missing critical updates, vastly reducing MQTT broker load.

### 📢 OTGW Events reporting via MQTT and WebSocket
OpenTherm Gateway specific events and diagnostic messages from the PIC are now appropriately published via MQTT and WebSocket, allowing Home Assistant and other smart home integrators to trace internal hardware notifications dynamically.

### 🔁 Triple-Reset WiFi Recovery
**Problem solved**: when WiFi credentials become invalid (router replaced, SSID changed, password changed, device moved), the only previous recovery path was to physically re-flash the firmware.
**Solution**: triple-clicking the hardware reset button within 10 seconds now clears all stored WiFi credentials and restarts the WiFiManager captive portal for reconfiguration. Normal single resets and double resets remain unaffected.
See [docs/guides/WIFI_RECOVERY_TRIPLE_RESET.md](docs/guides/WIFI_RECOVERY_TRIPLE_RESET.md) for detailed instructions.

### 💻 One-Shot OTGW PIC Commands from the Web UI
A command bar has been added to the OpenTherm Monitor page. You can now type raw OTGW PIC commands directly in the browser and see the response in the log in real time — without needing a telnet or serial connection.
**Examples**:
- TT=20.5 — set room temperature target
- SH=60 — set hot water temperature setpoint
- PR=A — print firmware revision
- GW=R — reset the gateway PIC

### 📊 PS=1 Summary Parsing with MQTT/HA Discovery
Previously, PS=1 mode (Print Summary) was detected and the firmware would stop streaming raw OT frames — but the PS=1 summary lines themselves were not parsed.
v1.3.0 fully parses all PS=1 summary fields and:
- Publishes each field to MQTT (using the same topic structure as normal OT message publishing).
- Registers Home Assistant auto-discovery entries per field, so PS mode users now get proper HA sensor entities.

### 🧠 Heap Status in Device Info
Free heap, max contiguous block size, and heap health level are now included in GET /api/v2/device/info. These values are also surfaced on the firmware flash utility page to help diagnose low-memory conditions before flashing.

---

## Bug Fixes

### Boot-Time Spurious Service Restarts
readSettings() used to flag the configuration as dirty initially, forcing a service restart over WiFi during the first loop() iteration. This caused MQTT drops almost immediately after connecting. Addressed by clearing dirty flags post-initialization.

### Hostname Dot-Stripping Targeting Wrong Buffer
The hostname formatting fix inadvertently stripped periods from the wrong buffer pointer, now corrected to cleanly address settingHostname.

### Webhook Payload Truncation on Settings Load
The settings loader payload buffer was previously constrained. It has been widened to ensure long webhook payloads load cleanly without trailing character amputation upon reboot.

---

## Memory and Performance Improvements

### String Heap Fragmentation Eliminated
Removed heavy String logic from writeSettings(). Each call historically dropped and reallocated strings causing heap fragmentation. The saving path now uses a global static scratch buffer to sidestep dynamic memory entirely.

### REST API v2 Migration Complete
The OTA update page was officially moved from legacy endpoints to /api/v2. The REST API v1 handles are now entirely obsolete inside the WebUI layer.

### ArduinoJson Removed
To squeeze out even more free RAM, ArduinoJson was stripped from the build. All serialization has been migrated to a bounded manual JSON writing architecture.

---

## Breaking Changes

There are **no breaking changes** in v1.3.0 regarding external behaviors, topics, or endpoints.

| Area | v1.2.0 → v1.3.0 |
|------|-----------------|
| MQTT topics | No changes |
| HA discovery | Additive only (PS=1 fields added) |
| REST API | Legacy removed internally; v2 untouched |
| Settings keys | No changes |

---

## Validation Basis
Compiled from the git delta between v1.2.0 and the current 1.3.0 release. Changes derived from commit history across firmware, Web UI, settings handling, and documentation.
