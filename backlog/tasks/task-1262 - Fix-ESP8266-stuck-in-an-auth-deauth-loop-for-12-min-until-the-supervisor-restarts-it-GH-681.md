---
id: TASK-1262
title: >-
  Fix: ESP8266 stuck in an auth/deauth loop for 12+ min until the supervisor
  restarts it (GH #681)
status: In Progress
assignee:
  - '@claude'
created_date: 2026-10-19 18:03
updated_date: '2026-10-21T21:50:00Z'
labels:
  - bug
  - needs-info
dependencies: []
references:
  - https://github.com/rvdbreemen/OTGW-firmware/issues/681
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
- [x] #3 esp8266 build and evaluate.py --quick green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-20 code reading (1.7.7 and current otgw-1.x.x identical here): loopWifi() retries WiFi.begin() every 30 s and restarts after 10 failed attempts, so a link that never comes up restarts after about 5 min, not 12. A 12-min loop therefore needs the retry counter to reset, which happens whenever WiFi.status() reaches WL_CONNECTED even briefly (WIFI_CONNECTING -> WIFI_RECONNECTED sets wifiRetryCount = 0, networkStuff.ino:566-568), so a flapping link (associate + IP, then drop) can loop without bound until something else restarts it. Hypothesis, unproven. Second hypothesis for the reason-3 deauths: WiFi.begin() every 30 s while the SDK auto-reconnect (setAutoReconnect(true)) is mid-association makes the station leave (deauth reason 3 is sent by the station). Both need the ESP-side telnet log the task already asks for.

2026-10-20 maintainer decision: run the MikroTik access-list bench test (deny 84:F3:EB:22:B8:E1, time to doRestart, remove rule) only after 2026-10-21 04:34, so the TASK-1261 24 h window (bootcount 8) finishes first.

2026-10-20: answered mrfox7688 on GH #682 (issuecomment-6299472236): ~2 min self-healing losses are intentionally below the 8-probe threshold; NAS-side ping loss points at the radio link; suggested beta.4 (modem-sleep off) as the next single change after his channel 6->1 test.

2026-10-21 bench (.88.68, 1.7.8-beta.5+09436d6): MikroTik access-list deny 21:50:17-21:58:19 (rule removed, count 0).
- Gateway back only at 22:01:27, 3 min AFTER the AP allowed it again; total outage 11 min 10 s.
- reboot_log: one new line "WiFi: too many reconnect failures"; bootcount 1 -> 2.
- Timeline that fits to the second: loopWifi 10 x 30 s -> restart ~21:55:20; boot direct connect timeOut/2 = 120 s fails (still denied) -> ~21:57:20 WiFiManager config portal, 240 s; portal timeout restart ~22:01:20; connected 22:01:27.
- Code confirms the portal blocks recovery: WiFiManager 2.0.15-rc.1 startConfigPortal() disables STA when not connected (_disableSTAConn = true default, WiFiManager.cpp:719-725), so the gateway cannot rejoin while the portal runs, even when the AP is back.
- The portal-timeout restart is invisible in the logs: bootcount and updateRebootLog() run after startWiFi() (OTGW-firmware.ino:187-188), so a boot that ends in the portal timeout never counts, and storeRebootReasonUnlessPending() drops its reason because the supervisor reason is still pending.
- Fits theaprilhare 12+ min outages ending in a "Software/System restart". Single bench run; the portal phase is inferred from timing + code, not observed directly (no AP scan during the window).
- Reason 3 deauth loop (AC1) not explained yet: WiFi_Disconnect() at portal start sends one deauth per boot, not ~40 in 12 min.

2026-10-21 ADR-099 (Accepted by maintainer) implemented: a WiFi supervisor restart (loopWifi WIFI_FAILED and the dead-link restart) writes an RTC mark (slot 115); the next boot reads and clears it and skips the WiFiManager portal when the saved network is not reachable in the 120 s direct-connect window. WiFiManager v2.0.17 and master have the same _disableSTAConn behaviour (no setter); upstream #706 open since 2018, maintainer on #1752 advises handling it in user code.
Bench evidence (scanner forcing a laptop WiFi scan every 20 s, deny on router wlan2 AND CAPsMAN access-list; the first try only blocked wlan2 and the gateway escaped to CAPsMAN AP "1e verdieping" at 22:51:26, so it did not count):
- beta.5 release (old): deny 23:22:47-23:36:49, portal AP OTGW-84:F3:EB:22:B8:E1 seen 13/41 scans (23:30:06-23:33:56 and again from 23:36:20), back 194 s after the allow; bootcount 1 -> 2 although at least two restarts happened.
- fix 1.7.8-beta.6+9fbafad: deny 23:05:23-23:19:27, portal seen 0/41, back 7 s after the allow.
- fix, no mark (RTS hardware reset while denied, 23:43:23): portal opened 23:45:38 (8/14 scans), so the power-cycle/first-install path still gets the portal.
- build.bat green (globals 53368, unchanged), evaluate.py --quick 37/0.
Still open:
- AC1: the ~40 auth-then-deauth (reason 3) cycles are not explained. One data point: the router logged "received deauth: sending station leaving (3)" plus an immediate reconnect at 22:44:09, the second of an Exception (2) reboot (TASK-1285), so a crash-reboot looks like reason 3 at the AP.
- Supervisor restart came 7.3 to 11.5 min after the AP disappeared, not the 5 min (10 x 30 s) the code suggests; cause not found yet (network handlers are skipped while disconnected, see doBackgroundTasks).
<!-- SECTION:NOTES:END -->
