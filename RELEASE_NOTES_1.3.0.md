# Release Notes — v1.3.0

**Last updated:** 2026-03-26<br>
**Release branch:** `dev`<br>
**Comparison target:** `main` (current stable `v1.2.0`)<br>

---

## ✨ Headline: PIC Settings Visibility, One-Click OTA, Safer Upgrades, Optional Admin Protection, Full `PS=1` Integration, and Major Memory Optimization

v1.3.0 is a major feature release building on `v1.2.0`. It exposes all PIC gateway settings through the Web UI, REST API, and MQTT; adds one-click GitHub release OTA updates; hardens OTA/LittleFS reliability; adds WiFi recovery; optionally protects admin endpoints; delivers fuller `PS=1` support; significantly reduces RAM pressure; and polishes the Web UI with theme toggling, better status indicators, and richer formatting.

For users already on `v1.2.0`, this is a backward-compatible upgrade: there are no new MQTT topic renames, no new REST API removals, and no settings-format migration.

---

## Overview

**User-visible additions:**
- PIC gateway settings exposed via REST API, MQTT, and a new Web UI panel with human-readable formatting, color-coded live/cached values, and browser localStorage caching.
- Single-click GitHub release OTA with version comparison, rollback support, and Installed/Update/Rollback badges.
- Optional HTTP Basic Auth for protected settings and maintenance endpoints.
- Configurable MQTT publish gating for OpenTherm and `PS=1` summary data.
- Full `PS=1` summary parsing with MQTT publishing and Home Assistant discovery.
- One-shot OTGW PIC commands from the monitor page with command/response/error feedback.
- Light/dark theme toggle button with persistent preference.
- Triple-reset WiFi recovery to reopen the captive portal without reflashing.
- Safer OTA / LittleFS flashing with backup, validation, Dallas label auto-preservation, and better logging.
- OTGW simulation mode for testing without physical hardware.
- Crash log endpoint for ESP8266 diagnostics.
- OTGW event reporting (PIC restart, serial errors) via MQTT and WebSocket.
- Heap memory info in device status and Web UI footer.
- GPIO conflict detection at boot.
- Gateway mode and WebSocket connection status indicators with tooltips.
- File System Explorer now hides the firmware-update button on touch devices.
- Richer device-info and settings tooltips throughout the Web UI.

**Bug fixes:**

- ESP hostname reverting to `ESP-XXXXXX` — deep audit of all hostname code paths.
- Settings page blank on iOS Safari.
- Boot-time spurious service restarts.
- Hostname normalization writing to wrong buffer.
- File Explorer delete handling.
- Webhook payload truncation after reboot.
- Unsafe LittleFS OTA flashing (WiFi activity during write, partial partition erase).
- IP validation incorrectly rejecting valid addresses with a `255` octet.
- NTP hostname not applied in all code paths.
- Numeric settings accepted out-of-range values.
- MQTT subscription topic truncation.
- WiFi portal triggered by stale RTC data after USB flash.
- PIC settings buffer truncation for fields returning longer-than-expected text.

**Internal improvements:**
- ArduinoJson dependency completely removed; bounded manual JSON handling.
- Global variables reorganized into `OTGWSettings` and `OTGWState` structs.
- `String` class eliminated from hot paths (protocol, settings, HTTP handlers).
- MQTT autodiscovery memory reduced via streaming template rendering.
- Non-blocking WiFi reconnect state machine replaces blocking 30-second loop.
- Non-blocking webhook with retry and backoff.
- REST API v2 migration completed; dispatch table routing.
- CSS vendor prefix cleanup across all four stylesheets.
- WiFiManager upgraded to stable 2.0.17.

---

## New Features

### PIC Gateway Settings Panel

All 15 PIC configuration registers are now exposed through the firmware, giving full visibility into the OTGW gateway's internal settings.

- **REST API**: `GET /api/v2/pic/settings` returns all cached PIC settings as JSON. Calling this endpoint also triggers a fresh readout cycle from the PIC.
- **MQTT**: Each setting is published to `otgw-pic/settings/<key>` when its value changes.
- **Web UI**: A new "Gateway Settings" section on the PIC firmware page displays all settings in grouped tables with human-readable formatting:
  - Setpoint override, setback temperature, DHW override decoded into readable text.
  - GPIO functions, LED assignments, and tweaks shown per-pin with descriptive labels.
  - Smart power, thermostat detection, reset cause, and voltage reference decoded with lookup tables (voltage reference shows actual voltage levels for both PIC16F88 and PIC16F1847).
  - Build date, clock speed, and standalone interval shown as-is.
- **On-demand readout**: Settings are read from the PIC one PR= command every 3 seconds (full cycle ~45 seconds). A cycle runs automatically at boot, and is re-triggered when the REST API is called or when any setting-change command is sent to the PIC. Multiple rapid triggers are coalesced.
- **Browser caching**: Values are cached in browser `localStorage` per hostname for up to 7 days. Live values display in green, cached values in amber, and undiscovered values in gray.
- **WebSocket feedback**: Each successful PR= read during a cycle sends a WebSocket event so the browser can show discovery progress in real time.

### Single-Click GitHub Release OTA

The OTA update page now connects directly to GitHub releases for one-click firmware updates.

- Lists available GitHub releases with semver-aware version comparison including pre-release tags.
- Shows Installed/Update/Rollback badges for each release.
- One-click download and flash without manual file management.
- Intel HEX integrity validation for PIC firmware downloads.

### Optional Protection for Admin Endpoints

v1.3.0 adds optional HTTP Basic Authentication for sensitive admin operations while keeping the device usable out of the box on trusted local networks.

- Authentication is disabled by default when the Protected Endpoints Password is empty.
- When configured, settings read/write, file-management, reboot, reset, and OTA update endpoints require authentication.
- The Web UI now exposes this password in the Settings tab as a masked field, and unchanged saves preserve the stored value.
- Username is fixed to `admin`, keeping the ESP8266-side implementation lightweight.

This is an additive security feature, not a breaking change: existing setups continue to work unchanged until a password is configured.

### Configurable MQTT Publish Gating

OpenTherm values can change quickly enough to flood an MQTT broker or waste WiFi airtime. v1.3.0 adds configurable publish gating so frequent updates can be rate-limited without changing the existing topic layout.

- Normal OpenTherm publishing now uses interval-aware gating.
- `PS=1` summary fields follow the same gating behavior and no longer bypass MQTT rate limiting.
- MQTT connection status is republished more predictably after boot and reconnect, reducing stale availability state in consumers.

### Full `PS=1` Summary Translation

Previous releases detected `PS=1` mode but did not fully turn the summary output into first-class firmware data. v1.3.0 now parses `PS=1` summary lines into the normal OTGW data path.

- Summary fields are published to MQTT.
- Home Assistant discovery can expose `PS=1`-derived values.
- Status-bit handling stays aligned with the normal OT mode behavior.
- OTGW events and command-style responses are surfaced more clearly through MQTT and WebSocket logging.

### Monitor-Page Command Bar and Better Status Visibility

The main Web UI monitor page now allows direct one-shot OTGW PIC commands from the browser.

- Send commands such as `TT=20.5`, `SH=60`, `PR=A`, or `GW=R` without leaving the UI.
- Responses remain visible in the monitor/log view.
- The UI now exposes more state feedback, including simulation visibility, richer heap/device status reporting, and clearer field descriptions in the settings screen.

### Light/Dark Theme Toggle

The Web UI now includes a theme toggle button to switch between light and dark themes. The user's preference is persisted across sessions.

### OTGW Simulation Mode

A new simulation mode allows testing the firmware and Web UI without a physical OTGW gateway connected. When active, a SIMULATION badge is shown in the monitor header. Controllable via the REST API.

### Crash Log Endpoint

A new `/api/v2/device/crashlog` endpoint exposes ESP8266 crash information (stack trace, exception cause) for diagnostics. Also displayed in the Device Info page.

### OTGW Event Reporting

PIC restart events, non-processed serial lines, serial overrun, and serial RX errors are now forwarded as events over MQTT and WebSocket, improving visibility into gateway health.

### Heap Memory in Device Status

Free heap and fragmentation percentage are now shown in the Web UI footer and available via the device info API endpoint.

### GPIO Conflict Detection

The firmware now detects conflicting GPIO pin assignments at boot (e.g., Dallas sensor pin conflicting with S0 counter or output pins) and logs a warning.

### Gateway Mode and WebSocket Status Indicators

The OpenTherm Monitor header now shows compact gateway mode ("Gateway" / "Monitor") and WebSocket connection status indicators with descriptive tooltips explaining what each means.

### File System Explorer: Firmware-Update Button Hidden on Touch Devices

The "Update Firmware" button in the File System Explorer is now hidden on smartphones and tablets. Detection uses the CSS media query `(pointer: coarse) and (hover: none)`, which matches finger/stylus input devices regardless of screen size.

### Triple-Reset WiFi Recovery

When saved WiFi credentials are no longer valid, you no longer need to reflash the ESP8266 just to recover network access.

- Three quick hardware resets within 10 seconds clear stored WiFi credentials.
- The device immediately reopens the WiFiManager captive portal for reconfiguration.
- Normal reboot behavior remains unchanged outside that reset pattern.

Detailed guide: [docs/guides/WIFI_RECOVERY_TRIPLE_RESET.md](docs/guides/WIFI_RECOVERY_TRIPLE_RESET.md)

### OTA / LittleFS Hardening

The firmware and filesystem updater received a substantial reliability pass in this release.

- Reboot verification now consistently uses `GET /api/v2/health`.
- Before a LittleFS flash, the browser can download `settings.ini` and `dallas_labels.ini` backups.
- After a successful filesystem flash, settings are rewritten to the fresh filesystem and the reboot handoff is cleaned up before restart.
- **Dallas labels survive a full filesystem wipe:** Immediately before a LittleFS flash, the updater fetches `/api/v2/sensors/labels` and saves the result to `localStorage`. After the device reports healthy, the labels are automatically restored via `POST /api/v2/sensors/labels` — no user action required and no data lost even though LittleFS is fully erased. This path runs whether or not the optional browser-backup checkbox is checked.
- Health-check polling was tightened to prevent a race where the timeout handler and the success handler could both fire on the final tick.
- OTA XHR uploads now emit detailed telnet logs for start, progress, completion, and abort.

---

## Bug Fixes

### Settings Page Blank on iOS Safari

The Settings page did not render on iPhone Safari; Firefox on iPhone was unaffected. The root cause was a DOM-timing issue: `setActivePageSection` was called after the async `refreshSettings()` fetch started, and Safari's stricter DOM scheduling meant the active section was not accessible when the `active` class needed to be applied. The fix reorders the call — show the page section first, set a loading indicator, then start the fetch. A secondary issue was also fixed: the error handler was appending an error node to a detached DOM element (silently discarding the error); it now displays the message inline in the settings panel.

### Boot-Time Spurious Service Restarts

Settings initialization could leave the system marked dirty at boot, which triggered avoidable service restarts in the first loop iteration and could drop MQTT soon after connect. That startup path is now cleaned up.

### Hostname Normalization Fix

Dot-stripping in hostname cleanup previously targeted the wrong buffer. v1.3.0 corrects the write target so hostname normalization affects the actual hostname setting.

### File Explorer Delete Handling

Filesystem delete handling in the browser path was corrected so file-removal actions behave consistently again.

### Webhook Payload Truncation

Long webhook payloads could be truncated when loaded from settings. The relevant buffer handling has been widened so payloads survive reboot and reload intact.

### Safer Filesystem Flashing

Two OTA-specific corruption paths were fixed:

- WiFi reconnect activity is now suppressed while flash writes are active.
- LittleFS OTA flashes erase the full filesystem partition instead of only the uploaded image size.

### IP Validation Correction

IP validation was tightened so only `255.255.255.255` is rejected as the broadcast address; valid addresses containing an octet of `255` are no longer incorrectly blocked.

### GPIO Conflict Detection

The settings path now detects conflicting GPIO assignments earlier, reducing the chance of overlapping sensor, S0-counter, and output-pin usage making it into runtime behavior unnoticed.

---

## Memory and Performance Improvements

### Manual JSON Writer Instead of ArduinoJson

ArduinoJson has been removed from the firmware-side data path in favor of bounded manual JSON writing helpers. This reduces dependency weight and gives tighter control over RAM use.

### Lower Heap Churn in Settings Persistence

The settings write path was reworked to avoid repeated `String` allocation and reallocation. Static scratch buffers now handle the hot path more predictably.

### Cleaner OTA Reboot Window

`settingsMarkClean()` and related cleanup ensure that OTA-triggered writes do not leave unnecessary MQTT, NTP, mDNS, or similar service restarts pending during the short reboot handoff.

### CSS Vendor Prefix Cleanup

Obsolete `-moz-transition`, `-ms-transition`, and `-o-transition` prefixes (unused since 2012–2013) were removed from all four stylesheets (`FSexplorer.css`, `FSexplorer_dark.css`, `index.css`, `index_dark.css`). `-webkit-transition` and `-webkit-appearance` are retained for older iOS Safari and Android WebView compatibility. Dead selectors from a previous layout era (`.outer-div`, `.inner-div`, `.container-card`, `.container-box`, `.div1`) were also removed.

### Broader Structural Cleanup

This release also includes larger internal cleanup that improves maintainability and reduces state ambiguity:

- Clearer split between persistent settings and transient runtime state.
- More centralized route and helper handling.
- Safer status propagation around MQTT and OTA flows.

---

## Breaking Changes

There are **no new breaking changes** in v1.3.0 relative to `main` / `v1.2.0`.

| Area | v1.2.0 -> v1.3.0 |
| ---- | ---------------- |
| MQTT topics | No new renames |
| Home Assistant discovery | Additive only (`PS=1` summary coverage) |
| REST API | No new removals beyond the existing v2-only baseline |
| Settings format | No migration required |
| Protected endpoints auth | New optional feature, disabled by default |

The migration items introduced in `v1.2.0` still apply where relevant. If you are upgrading from older than `v1.2.0`, review the earlier MQTT and API migration notes first. See [RELEASE_NOTES_1.2.0.md](RELEASE_NOTES_1.2.0.md) and [docs/BREAKING_CHANGES.md](docs/BREAKING_CHANGES.md).

---

## Upgrade Notes

1. Flash both firmware and filesystem so the Web UI, updater flow, and `PS=1` features stay in sync.
2. Hard-refresh the browser after flashing.
3. If your WiFi environment is unstable or credentials have changed, use the new triple-reset recovery flow instead of reflashing.
4. If you are upgrading from older than `v1.2.0`, review the earlier MQTT and API migration notes first.

---

## Validation Basis

These notes were compiled from the `main..dev` branch delta, excluding CI-only version bumps and merge noise, and then cross-checked against the changed firmware, Web UI, OTA, MQTT, and documentation files.
