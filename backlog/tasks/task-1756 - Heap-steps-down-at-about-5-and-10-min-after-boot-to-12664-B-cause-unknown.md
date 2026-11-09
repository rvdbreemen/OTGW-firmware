---
id: TASK-1756
title: Heap steps down at about +5 and +10 min after boot (to 12664 B), cause unknown
status: To Do
assignee: []
created_date: '2026-11-09T17:03:00Z'
updated_date: '2026-11-09T22:08:00Z'
labels:
  - heap
  - bug
  - beta.1
dependencies: []
priority: medium
ordinal: 858000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in TASK-1528 step b (bench .61, beta.162+5786928, 2026-11-09, 10 reboots, run-20261109). Clean boots stay at 22-33 KB in the boot window (setup-early .. drip), so the old 4.1/16.8 KB boot dips are not reproduced. After the boot window the windowed heap minimum still steps down at about +5 min and +10 min after boot, lowest 12664 B. The cause is unknown: candidates are a timer-driven job that starts later (MQTT discovery/republish, NTP, SAT log, OTA check, web-file cache), not a request from the test (clean boots had no HTTP load besides device/info every 5 s, which itself is a candidate). Related TASK-1528 (Done) and TASK-1675.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce on the bench with clean boots: the windowed heap minimum around +5 and +10 min after boot, with timestamps, on the current dev build
- [ ] #2 Name the job or allocation that causes each step, with evidence (telnet [HEAP] lines or an instrumented build), and rule out the device/info poll of the test itself
- [ ] #3 Decide with Robert: bound or fix it, or accept it with the numbers recorded
<!-- AC:END -->

## Comments

<!-- COMMENTS:BEGIN -->
created: 2026-11-09T22:08:00Z

---

Code + data analysis, read-only (030509, 2026-11-09; data: TASK-1528 step b run-20261109 on .61 beta.162, device/info every 5 s, telnet where captured; code at origin/dev 3c1353581). Time base: the first weather fetch log line (millis 300 000, see below) is 11:14:37.76 in boot1, so the run's 'up' sample is at about millis 12.8 s; offsets below are millis.
Measured steps (hd_min_free_heap / hd_min_max_block, clean boots):
- millis ~229 s: -170..-720 B in every boot (end of the MQTT discovery drip, last '[HEAP] drip id=255' at 12:06:15 in boot7). Small.
- millis ~305-308 s: boot1 -2436, boot3 -6356 (block 31732->22516), boot7 -960, boot9 -10012 (block 31732->10228). In every boot with data.
- millis ~610-615 s: boot9 -6264, boot7 block 31732->24564 (min unchanged). boot1/3 have no samples that late.
Candidates, file:line:
1. Weather first fetch (most likely for the 5-min step): SATweather.ino:611 'if (millis() < (WEATHER_POLL_MIN_SEC * 1000UL)) return;' (WEATHER_POLL_MIN_SEC 300, :28); then weatherRequestFetch() on the persistent webhook worker (webhook.ino:285 weatherWorkerPoll, SATweather.ino:437), weatherFetchWork() :385-433: WiFiClient + HTTPClient (begin with a ~400-char URL: HTTPClient copies host/uri into String, several hundred B), lwIP socket/pcb, then the HTTP/1.0 JSON response arriving into lwIP pbufs and WiFi dynamic RX buffers while the stream parser (no malloc, :114-191) drains it. Telnet shows 'Weather: fetching' at exactly 300 s in boot1 (11:14:37.76), boot7 (12:07:32.75) and boot10 (12:34:24.59), and the step falls 2-8 s later. Estimate (not measured per allocation): 1-10 KB transient, depending on how many RX pbufs/buffers queue before the parser reads them (TCP window up to ~5.7 KB plus buffer overhead). Next fetch 900 s later (timerWeatherPoll :82), or a retry after 60/120/... s if the first fails (:28, :558-563).
2. MQTT availability re-assert: MQTTstuff.ino:1302 DECLARE_TIMER_SEC(timerMQTTbirthreassert, 300) (function-local, declared on the first MQTT pass ~10 s after boot -> fires at ~310 s and ~610 s), :1452 -> publishBirthOnline() :2083: one small retained publish. Timing matches both steps; size small (one topic, ~100-200 B). Unlikely to explain KB steps alone.
3. timer5min: OTGW-firmware.ino:1139 DECLARE_TIMER_MIN(timer5min, 5, ..., 30000, 60000) -> first fire 5 min + 30-60 s jitter after the first loop pass (~335-375 s, ~635-675 s), :1157 do5minevent() :993 (sendMQTTuptime MQTTstuff.ino:1908, version info, sendMQTTstateinformation :2067, publishAllPICsettings OTGW-Core.ino:1467): an MQTT publish burst. Timing does NOT match the measured steps (30-60 s later).
4. Boot heap-watch failsafe: MQTTstuff.ino:1198-1200 bootHeapTick() at millis > 600 000 -> bootHeapFinish('timeout') :1185 -> platformHeapWindowStop() (platform_esp32.h:331): the IDF merges the per-region window minima into the since-boot minimum, so platformMinFreeHeap() (= hd_min_free_heap) can drop at that moment without any new allocation (platform_esp32.h:305-313: the in-window value can sit above IDF's per-region merge). This can make a part of the 10-min step a measurement artifact. Caveat: in boot7 no '[HEAP] boot done' line is in the telnet log although the drip ended at ~222 s, so whether the window was still open at 600 s is not settled from the logs.
5. SAT-sim event: boot7 at 12:12:47 (millis ~614) SAT went 'Heating off' (cold cutoff) with a burst of ~20 MQTT publishes incl. sat/pid_attributes JSON; the block minimum dropped in the same 5 s sample. State-driven (the sim room reaching target after ~10 min), not periodic; MQTT outbox entries of topic+payload.
6. Not candidates at 5/10 min: healthFsWriteProbe restAPI.ino:3578 (only on /api/v2/health, declared at its first call), timer15min OTGW-firmware.ino:1140, timerAPRetry networkStuff.ino:916 (AP mode only), SAT timers (60 s and the control interval).
Conclusion (opinion, to be verified): the 5-min step is the first weather fetch (SATweather.ino:611 gate). The 10-min step is not one job: candidates are the windowed-minimum merge at the 600 s failsafe (artifact), the 300 s birth re-assert and a SAT state change in the sim. Verify on the bench with weather disabled (or Toutside present) for the 5-min step, and with '[HEAP]' telnet captured through 620 s for the 10-min step; per-allocation sizes are not measured.

---
<!-- COMMENTS:END -->
