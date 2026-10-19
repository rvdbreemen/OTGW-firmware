---
id: TASK-1261
title: 'Fix: ESP8266 stays in modem-sleep, AP drops it for inactivity (GH #681)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-19 18:02'
updated_date: '2026-10-19 20:17'
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
- [x] #1 1.x calls WiFi.setSleepMode(WIFI_NONE_SLEEP) after every successful WiFi connect, including reconnects, verified on the bench unit via telnet log or WiFi.getSleepMode()
- [ ] #2 Bench gateway current/heap impact noted (free heap before/after, no new instability over 24 h)
- [ ] #3 theaprilhare reports AP inactivity disassociations with router scb_activity_time back at 5 s, on a beta carrying the change
- [x] #4 esp8266 build and evaluate.py --quick green
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-19 commit 328e8638c (local, not pushed: check_otgw_issues rule).
- Bench .88.68, Core 2.7.4, readout via telnet r: readout-only build on old WiFi logic = sleep mode 2 (WIFI_MODEM_SLEEP); fix = 0 (WIFI_NONE_SLEEP), also after two AP kicks via MikroTik registration-table remove (reassoc 4 s, no reboot, uptime continuous).
- Not shown: which path kept it at 0 after reassoc (SDK persistence or the WIFI_IDLE re-check); telnet drops during the kick and lines without a client are lost.
- Heap: 16608-17296 before, 16760-18072 after, no measurable change. 24 h stability (AC #2) not yet run. Ping RTT did not discriminate (old max 81 ms, new max 106 ms, network noise).
- Side finding, not touched: reboot_log.txt entries after OTA are dated 2106-02-07 (clock not yet synced when written).
- Plan correction: MODEM_SLEEP is 2 in the core enum, not 1.

2026-10-19: shipped in v1.7.8-beta.4 (release commit c686e9310, CI run 39317735107 completed success, 10 assets). Discord #beta-testing msg 1561835525084549191; GH #681 issuecomment-6276392696 asks theaprilhare to test (AC #3). AC #2 24h bench run still not started (curl to .88.68 blocked by auto mode).
<!-- SECTION:NOTES:END -->
