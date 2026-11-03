---
id: TASK-1397
title: >-
  WiFi: connect to the strongest access point of the SSID (and consider
  re-selecting when the signal is poor)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-25T19:48:00Z'
updated_date: '2026-11-03T08:31:00Z'
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

Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no: no setSortMethod/setScanMethod in src. Code doable, proof needs a two-AP bench.

Released by Robert 2026-11-03 (interactive, maintainer): low priority. Bench proof needs two APs on one SSID; any MikroTik write needs Robert's explicit OK first. Expect (re)connect about 1-2 s slower with a full scan.

2026-11-03 (agent, verified by lead in wt-next): Arduino-ESP32 3.3.5 defaults are WIFI_FAST_SCAN + BY_SIGNAL (STA.cpp:231); fast scan stops at the first matching AP. The setters only reach the driver in connect(ssid,...) (STA.cpp:389-390); a bare WiFi.begin() reuses the stored config, so the new shim platformWifiPreferStrongestAp() (platform_esp32.h) sets both and patches scan_method/sort_method in the stored STA config via esp_wifi_get/set_config. Called before all 5 WiFi.begin sites (networkStuff.ino x4 incl. startWiFi top before WiFiManager, Ethernet.ino switchToWiFi). SDK auto-reconnect uses the same stored config. tests/test_wifi_strongest_ap.py 3 tests: old 3 FAIL, fix OK; evaluate 0 failed (abstraction gate clean). AC1 (bench, two APs same SSID, before/after RSSI) open; any MikroTik change needs Robert's OK.

2026-11-03 lead: the first esp32-combo build of the integration failed: platform_esp32.h:135/140 'esp_wifi_get_config/esp_wifi_set_config was not declared' (the agent's change was host-checked only). Fix: #include <esp_wifi.h> in platform_esp32.h. Firmware build then SUCCESS.
<!-- SECTION:NOTES:END -->
