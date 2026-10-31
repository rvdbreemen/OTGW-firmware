---
id: TASK-1397
title: >-
  WiFi: connect to the strongest access point of the SSID (and consider
  re-selecting when the signal is poor)
status: To Do
assignee: []
created_date: '2026-10-25T19:48:00Z'
updated_date: '2026-10-30T21:56:00Z'
labels:
  - wifi
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 536000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
maarten_41762 2026-10-25 (#alpha-testing 1563987220115890257, alpha.500): the unit stays on an access point at -84 dBm while better ones of the same SSID are available. The firmware sets no scan/sort method (no setScanMethod/setSortMethod/BSSID in src/OTGW-firmware or Platform); the Arduino-ESP32 default is believed to be a fast scan that joins the first AP found (not verified for this framework version). Option: WIFI_ALL_CHANNEL_SCAN + WIFI_CONNECT_AP_BY_SIGNAL at connect via a platform shim; roaming while connected is a separate, larger question.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Unit joins the strongest AP of its SSID at (re)connect (bench with two APs, before/after RSSI)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-30 maintainer decision (Robert, in session otgw-firmware-ea): released for pickup ('vrijgeven'); label needs-maintainer-ok removed. Priority after beta-1 blockers unless it is one.
<!-- SECTION:NOTES:END -->
