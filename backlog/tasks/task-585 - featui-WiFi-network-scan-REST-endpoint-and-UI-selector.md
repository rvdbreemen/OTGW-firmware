---
id: TASK-585
title: 'feat(ui): WiFi network scan REST endpoint and UI selector'
status: To Do
assignee: []
created_date: '2026-05-08 16:01'
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
- [ ] #1 GET /api/v2/network/scan triggers a WiFi scan and returns JSON: array of {ssid, rssi, channel, secured} objects, sorted by RSSI descending
- [ ] #2 Scan is async: first call starts the scan and returns status:scanning; subsequent calls poll until status:ready with results
- [ ] #3 The Settings page replaces the SSID text input with a dropdown populated by the scan results, with a manual-entry fallback
- [ ] #4 Scan is only available when the device is not actively in a critical operation (no active OTA, no PIC flash)
- [ ] #5 Response includes the currently connected SSID marked as selected
- [ ] #6 No regression on ESP8266 builds (WiFi scan API is available on ESP8266 too)
<!-- AC:END -->
