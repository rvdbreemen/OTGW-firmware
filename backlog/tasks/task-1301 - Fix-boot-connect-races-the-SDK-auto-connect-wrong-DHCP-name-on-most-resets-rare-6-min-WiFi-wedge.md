---
id: TASK-1301
title: >-
  Fix: boot connect races the SDK auto-connect (wrong DHCP name on most resets,
  rare 6-min WiFi wedge)
status: To Do
assignee: []
created_date: '2026-10-22T12:50:00Z'
labels:
  - bug
  - wifi
dependencies: []
references:
  - backlog task TASK-1262
priority: high
ordinal: 255000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found on the bench while chasing TASK-1262 AC#1 (boot-diag build, 35 RTS resets with the AP present).

The SDK auto-connect flag is on (persistent WiFi), so the SDK is already in STATION_CONNECTING at 382 ms, before setup() touches WiFi. startWiFi() then calls WiFi.hostname() and WiFi.begin() (wifi_station_connect + dhcpc_start) on top of that attempt. Two outcomes seen:
- ~75 % of resets: the SDK attempt wins and the router lease carries the SDK default name ESP-22B8E1 instead of the configured hostname (28 of 34 fast resets). The early WiFi.hostname() call in setup() does not prevent this, contrary to its comment.
- 1 of 20 resets (08:35:14, plus 06:17 and 08:12 earlier): the SDK stays in STATION_CONNECTING for 120 s with zero station events while the AP shows the station associated without DHCP; direct connect fails, the config portal runs 240 s, and the gateway is offline ~6 min.

Prior art: ESPHome (wifi_component_esp8266.cpp) turns SDK auto-connect off (wifi_station_set_auto_connect(0)) and runs disconnect -> config -> hostname -> wifi_station_connect on every connect; Tasmota (support_wifi.ino WifiBegin) runs persistent(false), disconnect(true), mode, hostname, begin. Neither builds on an SDK attempt already in progress.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Every reset in a series of at least 60 on the bench gets its DHCP lease under the configured hostname, and none ends in the config portal or a WiFi wedge; old build vs fix shown on the same rig with RSSI recorded
- [ ] #2 Saved credentials survive the change (no portal after the first and later boots), and loopWifi reconnects after an AP outage still work (deny test)
- [ ] #3 esp8266 build and evaluate.py --quick green
<!-- AC:END -->
