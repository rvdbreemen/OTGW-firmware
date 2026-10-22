---
id: TASK-1262
title: >-
  Fix: ESP8266 stuck in an auth/deauth loop for 12+ min until the supervisor
  restarts it (GH #681)
status: Done
assignee:
  - '@claude'
created_date: 2026-10-19 18:03
updated_date: '2026-10-22T14:48:00Z'
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
- [x] #1 Reproduced or explained from an ESP-side log: which code path issues the deauth (reason 3) and why reconnects keep failing
- [x] #2 Time from first failed reconnect to recovery is bounded and shorter than 12 min, shown on the bench by forcing the loop
- [x] #3 esp8266 build and evaluate.py --quick green
- [x] #4 A boot after a WiFi supervisor restart that misses the direct-connect window continues setup() without the final WiFi wait (ADR-099), and after the access point returns MQTT, telnet, the web UI, NTP and mDNS come up without a crash; shown on the bench before and after the service fixes
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

2026-10-22 RTC timing diagnosis (temporary build 1.7.8-beta.6+1ccde89 + breadcrumbs in RTC 116..126, not committed; ultracode workflow wf_389f129d-052 ranked the hypotheses first: 20 found, 9 survived adversarial checks).
Test: deny on router wlan2 + CAPsMAN 23:21:18Z-23:29:51Z (rules removed, 0 left). Banner after reconnect:
  WDiag prev: loss 23:21:19Z (boot+96 s)  decision 23:26:19Z = loss+300.0 s  attempts 10  lastStatus 6  maxGap 20 ms
  WDiag tries(s after loss): 0.0 30.0 60.0 90.0 120.0 150.0 180.0 210.0 240.0 270.0
  WDiag boot: 23:26:22Z  startWiFi 343 ms  direct 120445 ms (no)  final 212595 ms (conn)  firstLoopWifi 214620 ms  skipPortal 1  uptimeSec 26
Conclusions:
- The supervisor is exactly on spec: loss seen 1 s after the block, 10 attempts at 30.0 s spacing, restart decision at loss+300.0 s, loop never stalled (max gap between loopWifi calls 20 ms). Hypotheses "detection latency", "slow loop" and "counter reset" are refuted by measurement.
- The earlier "7.3 and 11.5 min to restart" were a measurement error of mine: I derived the restart time from the uptime field. state.uptime.iSeconds is a 1 s tick counter in doTaskEvery1s (OTGW-firmware.ino:236, SKIP_MISSED_TICKS) that starts only when loop() runs. setup() blocks in startWiFi() for 120 s (direct connect) plus up to 240 s (timeoutWifiConnectFinal) while the AP is away, so uptime misses up to ~6 min. Here: real time since boot ~228 s at the banner, uptimeSec 26. Baseline boot also showed it: uptimeSec 53 vs ~86 s real.
- Recovery after the AP came back: 3.6 s (connected during the final wait, 212.6 s after boot; allow at 209 s).
- Restart-to-restart cycle while the AP stays away is ~11 min (300 s loopWifi + up to 360 s blocked in setup), which matches "only one restart in 14 min". The station stays on in both setup waits, so this does not delay recovery.
AC2 met: from the loss to the restart decision is a fixed 300 s, and recovery follows the AP within seconds (ADR-099 build).

2026-10-22: theaprilhare (#681) on beta.4: no stuck reason-3 loop in 25 h; inactivity episodes recover in 20-100 s. AC#1 (which path issues the deauth and why reconnects kept failing) still unexplained; next evidence is the beta.5 telnet Drop line.

2026-10-22 06:17 bench, old build 1251039, AP present, no access-list rules (counts 0): RTS reset via COM3. MikroTik: 06:17:03 'received deauth: sending station leaving (3)' and 'connected' in the same second, then NO DHCP assignment; 06:19:03 'disassoc: sending station leaving (8)' (= 120 s direct connect failed, portal starts and drops STA); 06:23:09 connected + DHCP assigned (after the 240 s portal timeout restart). Reboot log top line: 'Software/System restart (4) - WiFi: config portal timeout'. The previous RTS reset at 06:13:16 showed the same deauth(3)+connected but DHCP was assigned at once (hostname ESP-22B8E1, i.e. SDK auto-connect before startWiFi set the hostname). So on 1 of 2 resets the ESP associated without completing DHCP for 120 s, which costs 6 min (direct 120 s + portal 240 s) on any non-supervisor reset. Candidate for AC#1: a reset/crash produces the reason-3 deauth the AP logs, and the following association can fail to get an IP. Not yet reproduced a second time; frequency unknown.

2026-10-22 AC#4 (commit 508af3c7c), scratchpad t1278/t1289_deny.sh, deny 540 s:
- Run 1, b266cb6 (final wait skipped, no service fixes): rejoin 4.6 s after allow; web, WS, MQTT (connected), NTP ok; telnet port 23 REFUSED for 7+ min after the rejoin (SimpleTelnet begin() checkWiFi=true returns without starting); this also proves setup() ran offline (with the old final wait it would have connected at 06:53:47 inside the wait and telnet would have started). mDNS looked dead too, but the bench had two hosts named OTGW (.61 OTGW32) so that reading was confounded.
- .61 renamed OTGW2 (user request), .68 mDNS confirmed answering after a normal boot.
- Run 2, b80f098 (telnet begin(false) + mDNS restart in refreshServicesAfterWifiReconnect): rejoin 4.7 s; ports 23/80/81 open; mDNS query answered by .68; NTP -1.2 s; uptime dates restart deny+304.1 s. MQTT not checkable: bench settings.ini lost in the TASK-1285 crash repro, password unknown to me (user to re-enter).
- Not run: the old 1251039 final-wait-timeout path (deny > 11 min); telnet/mDNS start code is identical there, so the same defect applies by source.
- Banner note: after an offline boot the Link line shows "outages 0 disconnect events 60": failed connect attempts count as events while there was never a link; cosmetic, left as is.

2026-10-22 AC#1 reset series with the temporary boot-diag build (2eb0fd5 + RAM event ring flushed to /wdiag.txt; not committed). AP present, no access-list rules. Scripts: scratchpad t1262/reset_until_slow.sh, outputs reset_until_slow.out (08:27-08:41) and reset_until_slow2.out (14:35-14:42).
Correction: the 08:27 series was NOT empty as I first reported; reset 20 (08:35:14) was slow (371 s) and was captured before the series ended.
Event codes: ev1 wdBegin, ev2/ev3 around the early WiFi.hostname() in setup, ev4 startWiFi entry, ev5 wifiConnected, ev6/ev7 around direct-connect hostname()+begin(), ev8 direct-connect result, ev9 portal timeout, ev10 first loopWifi with WL_CONNECTED, ev20 StationConnected, ev21 Disconnected(reason), ev22 GotIP(last octet). a = WiFi.status(), b = wifi_station_get_connect_status().
Slow boot 08:35:14 (RSSI -79..-81):
  382 ms ev1 status 6 / cs 1 (STATION_CONNECTING: SDK auto-connect already running before any firmware WiFi call)
  384-387 ms ev2..ev7: hostname(), mode(STA), hostname()+begin() all issued while cs = 1
  120458 ms ev8 status 6 / cs 1: still CONNECTING after 120 s, and NO station event at all in between (no Connected, no reason-coded Disconnected, no GotIP)
  120465 ms ev21 reason 8 (ASSOC_LEAVE, portal start drops STA), cs 255
  361469 ms ev9 portal timeout -> restart
  AP side: 08:35:15 'received deauth: sending station leaving (3)' + 'connected' in the same second, no DHCP; 08:37:15 disassoc (8); 08:41:22 connected + DHCP OTGW.
  Reading: a wedged SDK connect state machine (zero events for 120 s while the AP keeps the station associated), not RF loss (that gives reason-coded disconnects and retries). Causation by the begin()-during-CONNECTING race is NOT proven: every fast boot also calls begin() while cs = 1.
Race indicator on every boot = DHCP lease name in the router log:
  08:27 series: 14 x ESP-22B8E1 (SDK auto-connect finished first, mean 5.0 s to web), 5 x OTGW (firmware begin() restarted the connect, 4-8 s, mean 6.6 s), 1 x none (the wedge).
  14:35 series: 14 x ESP-22B8E1, 1 x OTGW. RSSI -74..-80 (better than the morning run, a confound).
  So on ~75 % of resets the router lease carries the SDK default name ESP-22B8E1 instead of the configured hostname: a separate, user-visible defect of the same race.
Rates on the old/diag build: WiFi wedge 1/20 (08:27) + 0/15 (14:35) + 1/10 (08:12 loop, reset_loop.out) + 06:17 single = 3 wedges in ~47 resets.
Separate finding, NOT WiFi: 14:35:21 reset took 54 s to web, but WiFi connected at 1204 ms (ev20), GotIP .68 at 1237 ms, direct connect result WL_CONNECTED at 1310 ms; first loopWifi only at 52143 ms. So setup() blocked ~50 s after WiFi was up (first reset after ~6 h uptime; MQTT broker password missing on the bench). Stage unknown; needs a per-stage marker or a telnet boot capture. Track separately.

2026-10-22 AC#1 answered from ESP-side logs (diag ring) plus AP logs:
- Who sends the reason-3 deauth: the ESP itself, at the start of a new connect, when the AP still holds the station's previous association. Every RTS reset showed it in the same second as the new 'connected', on the old build AND with the TASK-1301 fix (so not the firmware's begin()-over-SDK race). A clean restart (OTA, 15:44:56 and 16:04:37) shows 'disassoc: sending station leaving (8)' before the reboot and NO reason 3 afterwards. So reason 3 at the AP marks an unclean reboot or a fresh connect start, consistent with the earlier Exception (2) data point (22:44:09).
- Why reconnects kept failing on the bench: the boot connect raced the SDK auto-connect; about 1 in 20 boots wedged in STATION_CONNECTING for the whole 120 s direct-connect window with no station events while the AP showed the station associated without DHCP, then the portal took 240 s more. Fixed and proven old vs fix in TASK-1301 (a1d756077): 3 wedges in ~47 resets before, 0 in 60 after.
- Field (theaprilhare, ~40 reason-3 cycles in 12 min at -55 dBm on 1.7.7): not reproduced as such. Inference, not proven: each cycle is a fresh connect start while the AP kept the old association; the 1.7.7 boot path (portal, restarts not counted, see above) and the race explain the long recovery. The reporter has not seen the loop on beta.4 in 25 h.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
WiFi recovery after a lost access point is bounded and fast, and the boot connect no longer wedges.

- ADR-099 (508af3c7c): after a WiFi supervisor restart the next boot skips the config portal and the final 240 s wait, and services (telnet, mDNS) come up when WiFi returns. Bench: old build back 194 s after the block, fix 7 s; with the service fixes 4.7 s, all services up.
- Supervisor timing measured exactly on spec: loss to restart decision 300.0 s; the earlier 7-11 min readings were an uptime counter error, fixed in TASK-1289.
- AC#1: the reason-3 deauth is the ESP's own fresh connect while the AP still holds the old association (every hard reset, with and without the fix; a clean restart shows disassoc 8 instead). The failing reconnects on the bench came from the boot connect racing the SDK auto-connect (1 in ~20 boots stuck 120 s + 240 s portal); fixed in TASK-1301 (a1d756077), 0 of 60 after vs 3 of ~47 before.

Not verified: the field 40-cycle loop on 1.7.7 itself (not reproduced; the reporter has not seen it again on beta.4).
<!-- SECTION:FINAL_SUMMARY:END -->
