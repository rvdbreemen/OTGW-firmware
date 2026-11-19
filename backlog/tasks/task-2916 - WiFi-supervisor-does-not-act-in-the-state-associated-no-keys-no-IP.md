---
id: TASK-2916
title: WiFi supervisor does not act in the state associated, no keys, no IP
status: To Do
assignee: []
created_date: '2026-11-19T13:47:00Z'
labels:
  - wifi
  - needs-maintainer-ok
milestone: 2.1.0
dependencies: []
priority: medium
ordinal: 1054000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in TASK-2026 bench (09, .61 OTGW32 no PSRAM, BLE on, beta.391+ebc0cf9, 'H' heap hold 14:13:08-14:43:08, MQTT off, no REST). Evidence: OTGW-field-data/bench-otgw32/20261119-task2026-391/ (steps.log, tel.log, telnet.raw, router-log-61.txt, info-after.json, reboot_log-after.txt). Router log for 10:20:BA:21:B4:F8 over the day: 52 connected, 53 disconnected, 26 'unicast key exchange timeout' (4-way handshake), 9 'group key exchange timeout'; no DHCP between 14:13:56 and 14:43:08 (deassigned+assigned exactly at the hold timer release). Telnet unreachable 14:15:10-14:43:30. Unit drop_reason 16, last_reason 2 (AUTH_EXPIRE); two own disassocs (reason 8) 11 min apart; bootcount 1070 unchanged, no restart. Baseline outside a hold: 0 kicks. Code (dev 0f788017e): the dead-link check wifiLinkProbeTick() runs only in WIFI_IDLE (networkStuff.ino:964-971); arduino-esp32 3.3.5 sets WL_CONNECTED only on STA_GOT_IP (STA.cpp:175) and WL_IDLE_STATUS on STA_CONNECTED (STA.cpp:120), so while associated without keys/IP loopWifi never reaches IDLE and neither the dead-link escalation nor the TASK-2026 starved-heap counting (networkStuff.ino:821-834) applies. What does act: the Arduino core auto-reconnect (reason 15 reconnectable, STA.cpp:63, :156-163; setAutoReconnect(true) networkStuff.ino:458) and loopWifi: prerelease builds enter BETA AP fallback after 2 failed 30 s attempts (networkStuff.ino:982-987) and then retry every 5 min with a 30 s wait and WiFi.disconnect() (:1052-1072, the reason-8 disassocs), never restarting; release builds go to WIFI_FAILED after 10 attempts (~5 min) and restart (:990-994, :1037-1046). The AP fallback also starts a softAP (AP+STA) on a unit whose heap is already starved. Source research on why the handshakes fail under a starved heap: docs/diagnostics/task-2026-group-rekey-starved-heap.md (IDF 9bb7aa84fe: every rekey/handshake heap allocation visible in source is small; the drop path is in the closed WiFi driver; no cheap reservation). Open: decide whether 'associated, no IP for N minutes' should count toward the dead-link escalation, and whether the prerelease AP fallback should apply when the cause is a starved heap rather than a missing AP.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Decision recorded (Robert): which state 'associated without keys or IP' should trigger, and the prerelease vs release behaviour
- [ ] #2 A reproduction (the 'H' hold on a no-PSRAM unit) fails on the old code and recovers on the fix without waiting for the hold release
<!-- AC:END -->
