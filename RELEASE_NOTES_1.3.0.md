# Release Notes — v1.3.0

**Last updated:** 2026-03-16<br>
**Release branch:** `dev`<br>
**Comparison target:** `main` (current stable `v1.2.0`)<br>

---

## ✨ Headline: Safer Upgrades, Better Recovery, and Full `PS=1` Integration

v1.3.0 builds on the current stable `v1.2.0` release. It focuses on OTA and LittleFS reliability, WiFi recovery, MQTT publish control, fuller `PS=1` support, and lower heap pressure.

For users already on `v1.2.0`, this is largely a backward-compatible upgrade: there are no new MQTT topic renames, no new REST API removals, and no settings-format migration.

---

## Overview

**User-visible additions:**
- Configurable MQTT publish gating for OpenTherm and `PS=1` summary data.
- Full `PS=1` summary parsing with MQTT publishing and Home Assistant discovery.
- One-shot OTGW PIC commands from the monitor page.
- Triple-reset WiFi recovery to reopen the captive portal without reflashing.
- Safer OTA / LittleFS flashing with backup, validation, and better logging.
- Richer device-info and Web UI status reporting, including heap visibility.

**Internal improvements:**
- Bounded manual JSON writing in place of ArduinoJson.
- Reduced `String` churn in settings persistence.
- Cleaner OTA reboot handoff and deferred-service handling.
- Larger structural cleanup around settings/state ownership and route handling.

---

## New Features

### Configurable MQTT Publish Gating

OpenTherm values can change quickly enough to flood an MQTT broker or waste WiFi airtime. v1.3.0 adds configurable publish gating so frequent updates can be rate-limited without changing the existing topic layout.

- Normal OpenTherm publishing now uses interval-aware gating.
- `PS=1` summary fields follow the same release philosophy and no longer bypass MQTT discipline.
- The `OTPublishGate` pattern makes the decision explicit and restores previous publish state safely.

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
- The UI now exposes more state feedback, including simulation visibility and richer heap/device status reporting.

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
- Dallas labels cached in the browser are restored after the device reports healthy again.
- OTA XHR uploads now emit detailed telnet logs for start, progress, completion, and abort.

---

## Bug Fixes

### Boot-Time Spurious Service Restarts

Settings initialization could leave the system marked dirty at boot, which triggered avoidable service restarts in the first loop iteration and could drop MQTT soon after connect. That startup path is now cleaned up.

### Hostname Normalization Fix

Dot-stripping in hostname cleanup previously targeted the wrong buffer. v1.3.0 corrects the write target so hostname normalization affects the actual hostname setting.

### Webhook Payload Truncation

Long webhook payloads could be truncated when loaded from settings. The relevant buffer handling has been widened so payloads survive reboot and reload intact.

### Safer Filesystem Flashing

Two OTA-specific corruption paths were fixed:

- WiFi reconnect activity is now suppressed while flash writes are active.
- LittleFS OTA flashes erase the full filesystem partition instead of only the uploaded image size.

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

### Broader Structural Cleanup

This release also includes larger internal cleanup that improves maintainability and reduces state ambiguity:

- clearer split between persistent settings and transient runtime state
- more centralized route and helper handling
- safer status propagation around MQTT and OTA flows

---

## Breaking Changes

There are **no new breaking changes** in v1.3.0 relative to `main` / `v1.2.0`.

| Area | `v1.2.0` -> `v1.3.0` |
|------|-----------------------|
| MQTT topics | No new renames |
| Home Assistant discovery | Additive only (`PS=1` summary coverage) |
| REST API | No new removals beyond the existing v2-only baseline |
| Settings format | No migration required |

The migration items introduced in `v1.2.0` still apply where relevant. See [RELEASE_NOTES_1.2.0.md](RELEASE_NOTES_1.2.0.md) and [docs/BREAKING_CHANGES.md](docs/BREAKING_CHANGES.md).

---

## Upgrade Notes

1. Flash both firmware and filesystem so the Web UI, updater flow, and `PS=1` features stay in sync.
2. Hard-refresh the browser after flashing.
3. If your WiFi environment is unstable or credentials have changed, use the new triple-reset recovery flow instead of reflashing.
4. If you are upgrading from older than `v1.2.0`, review the earlier MQTT and API migration notes first.

---

## Validation Basis

These notes were compiled from the `main..dev` branch delta, excluding CI-only version bumps and merge noise, and then cross-checked against the changed firmware, Web UI, OTA, and documentation files.
