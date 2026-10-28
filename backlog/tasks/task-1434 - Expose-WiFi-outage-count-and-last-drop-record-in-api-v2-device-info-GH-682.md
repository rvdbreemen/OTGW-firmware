---
id: TASK-1434
title: 'Expose WiFi outage count and last-drop record in /api/v2/device/info (GH #682)'
status: To Do
assignee: []
created_date: '2026-10-28T21:06:00Z'
labels:
  - enhancement
dependencies: []
references:
  - https://github.com/rvdbreemen/OTGW-firmware/issues/682
priority: medium
ordinal: 259000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
mrfox7688 asked on GH #682 (2026-10-22) to read the WiFi outage counters without a telnet reconnect. Today they exist only in the telnet banner (networkStuff.ino: wifiOutages, wifiDisconnectEvents, last drop reason/BSSID/channel/duration). Adding them to device/info lets a capture read the final count at the end.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 C:/Program Files/Git/api/v2/device/info returns outage count, disconnect events and last drop (reason code, name, channel, duration), matching the telnet banner on the bench after a forced AP drop
- [ ] #2 esp8266 build and evaluate.py --quick green
<!-- AC:END -->
