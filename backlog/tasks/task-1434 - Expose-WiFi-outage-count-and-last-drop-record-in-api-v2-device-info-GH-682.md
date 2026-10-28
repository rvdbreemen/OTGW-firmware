---
id: TASK-1434
title: 'Expose WiFi outage count and last-drop record in /api/v2/device/info (GH #682)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-28T21:06:00Z'
updated_date: '2026-10-28T22:35:00Z'
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
- [x] #1 esp8266 build and evaluate.py --quick green
- [ ] #2 /api/v2/device/info returns outage count, disconnect events and last drop (reason code, name, channel, duration), matching the telnet banner on the bench after a forced AP drop
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 implemented: sendWifiLinkInfoJson() in networkStuff.ino (same record as printWifiDropBanner), called at the end of sendDeviceInfoV2(). Keys: wifi_ap_bssid, wifi_ap_channel, wifi_outages, wifi_disconnect_events, wifi_drop_bssid, wifi_drop_channel, wifi_drop_reason(_name), wifi_drop_last_reason(_name), wifi_drop_events, wifi_drop_down_sec, wifi_drop_ago_sec, wifi_drop_self; always present, 0/empty before the first outage. Reason names copied from PROGMEM to RAM with strncpy_P before the RAM-string sendJsonMapEntry overload. openapi.yaml + CHANGELOG updated.
Gates: build.bat 1.7.8-beta.8+628cb88 (ino 23:25:53, littlefs 23:25:55, "Build completed successfully"), evaluate.py --quick 38/0, host tests 7/7.
Baseline on .16 (1.7.5-beta.6, before): no wifi_outages / wifi_drop_* keys in device/info (saved under %LOCALAPPDATA%\OTGW-capture\bench-16-*).
NOT verified on hardware: bench .68 offline and no ESP8266 bench available (maintainer, 2026-10-28); AC #2 stays open. Ships in beta.8 for field verification by mrfox7688 (GH #682).
<!-- SECTION:NOTES:END -->
