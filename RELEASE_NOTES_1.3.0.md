# Release Notes — v1.3.0

**Last updated:** 2026-03-24<br>
**Release branch:** `dev`<br>
**Comparison target:** `main` (current stable `v1.2.0`)<br>

---

## ✨ Headline: Safer Upgrades, Better Recovery, Optional Admin Protection, Full `PS=1` Integration, and Web UI Polish

v1.3.0 builds on the current stable `v1.2.0` release. It focuses on OTA and LittleFS reliability, WiFi recovery, optional protection for admin endpoints, MQTT publish control, fuller `PS=1` support, Web UI improvements and fixes, and lower heap pressure.

For users already on `v1.2.0`, this is largely a backward-compatible upgrade: there are no new MQTT topic renames, no new REST API removals, and no settings-format migration.

---

## Overview

**User-visible additions:**
- Optional HTTP Basic Auth for protected settings and maintenance endpoints.
- Configurable MQTT publish gating for OpenTherm and `PS=1` summary data.
- Full `PS=1` summary parsing with MQTT publishing and Home Assistant discovery.
- One-shot OTGW PIC commands from the monitor page.
- Triple-reset WiFi recovery to reopen the captive portal without reflashing.
- Safer OTA / LittleFS flashing with backup, validation, Dallas label auto-preservation, and better logging.
- File System Explorer now hides the firmware-update button on touch devices (smartphones and tablets).
- Richer device-info and Web UI status reporting, including heap visibility and improved settings tooltips.

**Bug fixes:**

- Settings page blank on iOS Safari.
- Boot-time spurious service restarts.
- Hostname normalization writing to wrong buffer.
- File Explorer delete handling.
- Webhook payload truncation after reboot.
- Unsafe LittleFS OTA flashing (WiFi activity during write, partial partition erase).
- IP validation incorrectly rejecting valid addresses with a `255` octet.

**Internal improvements:**
- Bounded manual JSON writing in place of ArduinoJson.
- Reduced `String` churn in settings persistence.
- Cleaner OTA reboot handoff and deferred-service handling.
- CSS vendor prefix cleanup across all four stylesheets.
- Larger structural cleanup around settings/state ownership and route handling.

---

## New Features

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

### File System Explorer: Firmware-Update Button Hidden on Touch Devices

The "Update Firmware" button in the File System Explorer is now hidden on smartphones and tablets. Detection uses the CSS media query `(pointer: coarse) and (hover: none)`, which matches finger/stylus input devices regardless of screen size — more reliable than a viewport-width breakpoint and avoids false-positives on resized desktop windows.

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
