---
id: TASK-1434
title: 'Expose WiFi outage count and last-drop record in /api/v2/device/info (GH #682)'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-28T21:06:00Z'
updated_date: '2026-10-29T05:13:00Z'
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
- [x] #2 /api/v2/device/info returns outage count, disconnect events and last drop (reason code, name, channel, duration), matching the telnet banner on the bench after a forced AP drop
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 implemented: sendWifiLinkInfoJson() in networkStuff.ino (same record as printWifiDropBanner), called at the end of sendDeviceInfoV2(). Keys: wifi_ap_bssid, wifi_ap_channel, wifi_outages, wifi_disconnect_events, wifi_drop_bssid, wifi_drop_channel, wifi_drop_reason(_name), wifi_drop_last_reason(_name), wifi_drop_events, wifi_drop_down_sec, wifi_drop_ago_sec, wifi_drop_self; always present, 0/empty before the first outage. Reason names copied from PROGMEM to RAM with strncpy_P before the RAM-string sendJsonMapEntry overload. openapi.yaml + CHANGELOG updated.
Gates: build.bat 1.7.8-beta.8+628cb88 (ino 23:25:53, littlefs 23:25:55, "Build completed successfully"), evaluate.py --quick 38/0, host tests 7/7.
Baseline on .16 (1.7.5-beta.6, before): no wifi_outages / wifi_drop_* keys in device/info (saved under %LOCALAPPDATA%\OTGW-capture\bench-16-*).
NOT verified on hardware: bench .68 offline and no ESP8266 bench available (maintainer, 2026-10-28); AC #2 stays open. Ships in beta.8 for field verification by mrfox7688 (GH #682).

2026-10-29 bench .68 (Classic ESP8266, PIC gateway 6.8), released beta.8 assets (+65762e7, SHA256SUMS OK) flashed via web OTA. Before the drop device/info had all wifi_* keys at 0 (wifi_ap_bssid 74:4D:28:C7:07:40 ch 1). Forced AP drop 06:12:19 with MikroTik '/interface wireless registration-table remove [find mac-address=84:F3:EB:22:B8:E1]' (wlan2). After reconnect, telnet banner and /api/v2/device/info agree field by field: outages 1, disconnect events 1, reason 2 AUTH_EXPIRE (last 2 AUTH_EXPIRE, 1 event), drop AP 74:4D:28:C7:07:40 ch 1, down 1 s, 12 s ago, drop_self false. Evidence: C:\Users\rvdbr\AppData\Local\OTGW-capture\bench-68-beta8-20261029-0559\banner_kick1.log, after_kick1_device_info.json, before_device_info.json. Only one short (1 s) outage tested; a longer outage was not forced.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added the WiFi outage record from the telnet banner to /api/v2/device/info (wifi_ap_bssid/channel, wifi_outages, wifi_disconnect_events, wifi_drop_* incl. reason code and name, down and age seconds, self-initiated flag), so a capture can read it at the end without a telnet session (GH #682).

Changes: sendWifiLinkInfoJson() in networkStuff.ino called from sendDeviceInfoV2(); docs/api/openapi.yaml; CHANGELOG. Shipped in v1.7.8-beta.8.

Verification: build.bat + evaluate.py green (38/0), host tests 7/7. Bench .68 on the released beta.8 binary: forced AP deauth via the MikroTik registration table; device/info matched the telnet banner on every field (outages 1, events 1, AUTH_EXPIRE, AP/channel, down 1 s, 12 s ago). Evidence in C:\Users\rvdbr\AppData\Local\OTGW-capture\bench-68-beta8-20261029-0559.

Not verified: a long outage (only a 1 s drop was forced); field confirmation by the GH #682 reporter is pending.
<!-- SECTION:FINAL_SUMMARY:END -->
