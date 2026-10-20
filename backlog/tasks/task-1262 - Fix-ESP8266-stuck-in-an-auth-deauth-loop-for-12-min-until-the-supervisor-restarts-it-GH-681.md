---
id: TASK-1262
title: >-
  Fix: ESP8266 stuck in an auth/deauth loop for 12+ min until the supervisor
  restarts it (GH #681)
status: To Do
assignee: []
created_date: '2026-10-19 18:03'
updated_date: '2026-10-20 02:27'
labels:
  - bug
  - needs-info
dependencies: []
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/681'
priority: medium
ordinal: 249000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
theaprilhare (GH #681, 2026-10-18) on 1.7.7: one 14-min outage did not start with an AP inactivity drop. The OTGW authenticated successfully and immediately left again (Deauth reason 3, station leaving), about 40 times in 12 minutes at -55 dBm, and recovered only when the firmware restarted itself (Software/System restart, ~12 min in). A 24-min outage on 2026-10-16 also ended in a self-restart. Asks for faster recovery: reset the WiFi stack (WiFi.disconnect(true) + WiFi.begin()) before resorting to a reboot, or reboot after 2-3 min of failed reconnects.

Missing: an ESP-side telnet log of the loop (what the supervisor does on each attempt), and the reboot_log reason on 1.7.8-beta.1+ to confirm which path restarted it. Related: TASK-1248 (connected-but-dead link, a different state).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduced or explained from an ESP-side log: which code path issues the deauth (reason 3) and why reconnects keep failing
- [ ] #2 Time from first failed reconnect to recovery is bounded and shorter than 12 min, shown on the bench by forcing the loop
- [ ] #3 esp8266 build and evaluate.py --quick green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-20 code reading (1.7.7 and current otgw-1.x.x identical here): loopWifi() retries WiFi.begin() every 30 s and restarts after 10 failed attempts, so a link that never comes up restarts after about 5 min, not 12. A 12-min loop therefore needs the retry counter to reset, which happens whenever WiFi.status() reaches WL_CONNECTED even briefly (WIFI_CONNECTING -> WIFI_RECONNECTED sets wifiRetryCount = 0, networkStuff.ino:566-568), so a flapping link (associate + IP, then drop) can loop without bound until something else restarts it. Hypothesis, unproven. Second hypothesis for the reason-3 deauths: WiFi.begin() every 30 s while the SDK auto-reconnect (setAutoReconnect(true)) is mid-association makes the station leave (deauth reason 3 is sent by the station). Both need the ESP-side telnet log the task already asks for.
<!-- SECTION:NOTES:END -->
