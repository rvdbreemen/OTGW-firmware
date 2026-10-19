---
id: TASK-1261
title: 'Fix: ESP8266 stays in modem-sleep, AP drops it for inactivity (GH #681)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-19 18:02'
updated_date: '2026-10-19 18:15'
labels:
  - bug
dependencies: []
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/681'
priority: high
ordinal: 248000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
theaprilhare (GH #681, 2026-10-18, comment on the closed issue) on 1.7.7 + ASUS RT-AX86U guest SSID: every WiFi outage starts with the AP disassociating the OTGW for inactivity (reason 4), then the 4-way handshake fails for 1-3 min. wl sta_info shows the PS flag set and a high retry rate; scb_activity_time is the Broadcom default 5 s. Raising it to 60 s on the router cut >=60 s drops from ~0.3/h to 2 in ~26 h.

Verified in code: the 1.x source and vendored libraries never call WiFi.setSleepMode()/wifi_set_sleep_type(), so the ESP8266 runs in its default modem-sleep. The dev line has platformWifiDisableSleep() (TASK-817), called after connect and re-asserted after reassociation; that never reached an ESP8266 release. Reporter offers to test a build.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 1.x calls WiFi.setSleepMode(WIFI_NONE_SLEEP) after every successful WiFi connect, including reconnects, verified on the bench unit via telnet log or WiFi.getSleepMode()
- [ ] #2 Bench gateway current/heap impact noted (free heap before/after, no new instability over 24 h)
- [ ] #3 theaprilhare reports AP inactivity disassociations with router scb_activity_time back at 5 s, on a beta carrying the change
- [ ] #4 esp8266 build and evaluate.py --quick green
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. networkStuff.ino ~174: WiFi.setSleepMode(WIFI_NONE_SLEEP) after initial connect, log the mode.
2. WIFI_RECONNECTED: same call after a supervisor reconnect.
3. WIFI_IDLE: if WiFi.getSleepMode() != WIFI_NONE_SLEEP, re-assert and log (covers SDK auto-reconnect).
4. build.bat + evaluate.py --quick.
5. Bench .88.68 OTA: old build logs mode 1, new logs mode 0, also after a forced reconnect; free heap before/after.
Approved by Robert 2026-10-19. No push (check_otgw_issues rule).
<!-- SECTION:PLAN:END -->
