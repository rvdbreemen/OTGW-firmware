---
id: TASK-1261
title: 'Fix: ESP8266 stays in modem-sleep, AP drops it for inactivity (GH #681)'
status: Done
assignee:
  - '@claude'
created_date: 2026-10-19 18:02
updated_date: '2026-10-29T05:16:00Z'
labels:
  - bug
dependencies: []
references:
  - https://github.com/rvdbreemen/OTGW-firmware/issues/681
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
- [x] #2 Bench gateway current/heap impact noted (free heap before/after, no new instability over 24 h)
- [x] #3 esp8266 build and evaluate.py --quick green
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

2026-10-19 22:19 AC #2 baseline (bench .88.68, fix build 1.7.8-beta.4+0ba8e65 + TASK-1261): bootcount=3, device uptime 1h48m, lastreset "Software/System restart" (OTA), freeheap 17720, RSSI -64. MikroTik wlan2 registration uptime=1h45m (signal -70 dBm). Pass rule at >= 2026-10-20 22:19: bootcount still 3 AND registration uptime >= 25h45m (no reboot, no disassociation).

2026-10-20 04:07 interim: before the TASK-1253 OTA the bench had run 7h33m with bootcount 3 unchanged (no reboot). The 24 h window restarted twice for other bench work (OTA 04:05, OTA ~04:25, RTS reset 04:32). New AC #2 baseline 2026-10-20 04:33:50: build 1.7.8-beta.5+d92c03f (carries the TASK-1261 fix), bootcount 8, MikroTik registration uptime 57s, freeheap 17608, RSSI -64. Pass at >= 2026-10-21 04:34: bootcount 8 and registration uptime >= 24h.

2026-10-21 07:3x AC #2 PASS (bench .88.68, 1.7.8-beta.5+d92c03f): bootcount 8 (unchanged since baseline 2026-10-20 04:33:50), device uptime 1d02:54, MikroTik wlan2 registration uptime 1d02:56:31 (>= 24h; last disconnect in /log is the 04:32:54 RTS reset, deauth reason 3), freeheap 16800 (baseline 17608, range seen 16608-18072), RSSI -72. Remaining open: AC #3 (theaprilhare field report, not self-verifiable).

2026-10-22: theaprilhare (#681, 2026-10-21 12:02Z) ran beta.4 25 h with router scb_activity_time still at 60 (not 5, so AC#3 stays open): sleep mode 0 confirmed via telnet r; 7 AP episodes in 25 h (4 during a disturbed-laptop window) vs ~2-4/day on 1.7.7; MQTT disconnects 0.84/h -> 0.44/h; no stuck WiFi ending in a self-reboot (was twice in 3 days). Episodes still start with 'Disassociated due to inactivity (4)' + '(8)' in the same second, then a few '(2)' handshake failures, now recovering in 20-100 s. Separate 20-min ARP-expiry stalls split off to TASK-1290.

2026-10-29: maintainer decision (relayed via the 2.0.0 session otgw-firmware-c5): AC#3 is dropped. The reporter will not set scb_activity_time back to 5 s; the fix is closed on bench evidence (AC#1, AC#2 24 h PASS) plus the field report on beta.4 (sleep mode 0 confirmed, AP episodes and MQTT disconnects roughly halved, no stuck-WiFi self-reboots). Remaining inactivity drops are tracked in TASK-1262 and TASK-1290.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
1.x now calls WiFi.setSleepMode(WIFI_NONE_SLEEP) after every successful connect and reconnect, so the ESP8266 no longer sits in modem-sleep and the AP has less reason to drop it for inactivity (GH #681). Shipped in v1.7.8-beta.4.

Verification: bench .68 readout sleep mode 2 (MODEM_SLEEP) on the old logic, 0 with the fix, also after two AP kicks; heap unchanged (16.6-18.1 KB); 24 h bench run without reboot or disassociation (registration uptime 1d02:56). Field (theaprilhare, beta.4, 25 h): sleep mode 0 confirmed, AP episodes and MQTT disconnects roughly halved, no stuck-WiFi self-reboots.

Dropped AC: a field run with the router's scb_activity_time back at 5 s (maintainer decision). Remaining inactivity drops: TASK-1262, TASK-1290.
<!-- SECTION:FINAL_SUMMARY:END -->
