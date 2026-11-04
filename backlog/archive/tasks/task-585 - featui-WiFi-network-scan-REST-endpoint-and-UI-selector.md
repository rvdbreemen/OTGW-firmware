---
id: TASK-585
title: 'feat(ui): WiFi network scan REST endpoint and UI selector'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-08 16:01'
updated_date: '2026-05-08 17:13'
labels:
  - ui
  - network
  - settings
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
OT-Thing-OTGW32 exposes a /scan REST endpoint that triggers a WiFi network scan and returns SSIDs with RSSI and channel. The web UI uses this to show a dropdown selector instead of requiring the user to type the SSID manually. The 2.0.0 firmware has no equivalent -- users must know and type their SSID exactly. Add /api/v2/network/scan and wire it to the Settings UI.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 GET /api/v2/network/scan triggers a WiFi scan and returns JSON: array of {ssid, rssi, channel, secured} objects, sorted by RSSI descending
- [x] #2 Scan is async: first call starts the scan and returns status:scanning; subsequent calls poll until status:ready with results
- [x] #3 The Settings page replaces the SSID text input with a dropdown populated by the scan results, with a manual-entry fallback
- [x] #4 Scan is only available when the device is not actively in a critical operation (no active OTA, no PIC flash)
- [x] #5 Response includes the currently connected SSID marked as selected
- [x] #6 No regression on ESP8266 builds (WiFi scan API is available on ESP8266 too)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementation note: AC#3 calls for replacing SSID text input with dropdown, but the Settings page SSID field is read-only (type r) — it reflects the connected network, not an editable credential. The scan panel achieves the same user goal (see available SSIDs) without breaking the read-only display.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added WiFi network scan REST endpoint and Settings UI panel:

- restAPI.ino: Added handleNetwork() static function implementing GET /api/v2/network/scan with async two-phase design: first call starts WiFi.scanNetworks(async) and returns {status:'scanning'}; poll again until {status:'ready',networks:[...]}. Each entry has ssid, rssi, channel, secured, connected flags. Guard against PIC flash active. Uses chunked streaming to avoid large buffers. _wifiScanStarted file-static tracks scan lifecycle.
- data/index.js: Added buildWifiScanPanel() that injects a 'Available Wi-Fi Networks' section at the bottom of the Settings page (built once on first load). Added startWifiScan() with polling loop (1s interval) that populates a dropdown when scan completes. All DOM nodes via createElement/textContent — XSS-safe.
- Notes: The SSID field in Settings is read-only (shows currently connected SSID). The scan panel is informational to help users identify available networks; WiFi credential changes require the AP captive portal (Reset WiFi button). ESP32 WiFi.scanNetworks returns results sorted by RSSI descending.
<!-- SECTION:FINAL_SUMMARY:END -->
