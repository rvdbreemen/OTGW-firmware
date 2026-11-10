---
id: TASK-1756
title: Heap steps down at about +5 and +10 min after boot (to 12664 B), cause unknown
status: Done
assignee:
  - '@claude'
created_date: '2026-11-09T17:03:00Z'
updated_date: '2026-11-10T18:10:00Z'
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
- [x] #3 Decide with Robert: bound or fix it, or accept it with the numbers recorded
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Measurement plan (otgw-firmware-80, 2026-11-10; builds on 030509's analysis in comment #1). Goal: name the cause of the ~305 s and ~610 s steps with A/B evidence, not timing alone. Bench .61 only after 4c's TASK-1757 run (~06:20), current dev build, clean boots (reboot via REST, no browser open).
Logging per run (all read-only): (a) telnet, debug flags for heap + SAT weather + MQTT, capture '[HEAP]' lines, 'Weather: fetching', 'Heap: ... bytes free' (logHeapStats every 60 s) with PC timestamps; (b) device/info hd_min_free_heap / hd_min_max_block / freeheap / maxfreeblock. To rule out the poll itself (AC2): runs B use device/info every 60 s instead of 5 s, and one run polls nothing except telnet.
Runs (2 clean boots each, 12 min per boot):
A0 baseline: weather on, MQTT on, poll 5 s (reproduces 030509's data).
A1 weather off (satweatherenable false): if the ~305 s step disappears -> weather fetch confirmed (candidate 1).
A2 weather on, poll 60 s / no poll: steps unchanged -> the test's own poll is ruled out.
A3 MQTT off (broker unreachable or mqttenable false): the ~310/610 s birth re-assert and the timer5min burst go away; isolates candidates 2/3 and the SAT-sim publish burst (candidate 5; also SAT sim off in A3b).
A4 window artifact (candidate 4): read hd_min_free_heap just before and just after the '[HEAP] boot done' / 600 s timeout line with no other activity; a drop with no allocation = merge artifact, not a leak.
Old-vs-fix: once a cause is named, the fix (e.g. weather fetch with a bounded RX / earlier stream drain, or reporting the window-merged minimum separately) is measured with the same A0 run before and after. Decision on bound/fix/accept goes to Robert (AC3) with the numbers.
Settings saved before and restored after each run (A1/A3 change settings).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-10 05:17-07:20 runs on .61 (beta.167+e79e495, otgw-firmware-80; scripts and raw data in the session scratchpad t1756/): A0 baseline, A1 satweatherenable=false, A2 no poll, A3 mqttenable=false, A3b satsimulation=false; 2 clean boots x 12 min each; every run's settings restored and verified (0 differences). Results from device/info every 5 s (hd_min_free_heap / hd_min_max_block, seconds after the first answer, about millis +15 s): A0: NO step at ~300 s or ~600 s in either boot (minimum flat after the first seconds: 31,720 B and 39,244 B). A1: early step -11,764 B at +11 s in boot2 (min 15,844 B), nothing late. A3 (MQTT off): late steps +294 s -1,688 B, +604 s -5,468 B (boot1), +247 s -8,200 B (boot2). A3b (SAT sim off): +264 s -3,268 B (boot1). A2: no time series (no poll and no telnet, see below), end values only.

Limits, honest: (1) the telnet capture FAILED in every run: after the reboot the script stayed on the dead pre-reboot socket (the device sends no FIN; recv only timed out, no reconnect), so there are no 'Weather: fetching' / [HEAP] lines to time the steps (030509's boots.py has the same pattern); (2) 2 boots per run is too few: late steps appear in some runs and not in others, so no variable is shown to cause or remove the step yet. Verdict so far: on beta.167 the +5/+10 min step did not reproduce in the baseline (2/2 boots), the cause is NOT named. Next: fix the telnet reconnect (reconnect when bootcount changes or after 20 s without data) with the heap + weather + MQTT debug flags on, 4 boots per run for A0 and A1 first, then A3; needs another .61 slot.

2026-11-10 telnet A/B on .61 (beta.170, UI-only change vs 167), 4 boots each of 720 s, device/info every 5 s, settings restore 0 differences both runs. Outputs: session scratchpad t1756/runT1-telnet-170 (14:12-15:01) and runT0-notelnet-170 (15:01-15:50), analyze.py. Result: no step at ~300 s or ~600 s after boot in any of the 8 boots (with the 2 A0 boots of this morning: 0 of 10 on beta.167/170, against 4/4 on beta.162 in detectie's runs). The minimum is set within ~5 s after 'up' (boot burst), 22.7-39.4 KB, both arms. One later step: telnet arm boot 4 at +617 s, hd_min_free_heap 35156 -> 27328 (-7.8 KB); it sits exactly on 15:00:00 wall clock, where the hourly block (OTGW-firmware.ino:960-962, hourFlag) runs sendMQTTheapdiag(): a burst of about 30 otgw-firmware/stats/* publishes, each also printed to the live telnet client. No other boot crossed a full hour (telnet boots 1-3 ran 14:12-14:49, all no-telnet boots 15:01-15:50), so this does NOT show that telnet matters: the no-telnet arm never met the hourly burst. Hypothesis 'live telnet causes the ~5/10 min step' is not supported (no such step seen at all). New lead: the hourly heapdiag burst costs ~8 KB of free-heap minimum with a telnet reader attached. Next measurement: span a full hour, with and without telnet (and with debug MQTT logging off), 2-3 hours each.

2026-11-10 ~17:30 Robert's decision (via the coordinator's AskUserQuestion): close as not reproducible. The hourly sendMQTTheapdiag() step stays an observation; no new task (min free heap after it 27.3 KB, far above the 3072/5120/8192 ladder of ADR-167; it only showed with a telnet reader attached in 1 boot).
<!-- SECTION:NOTES:END -->

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

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Closed as not reproducible (Robert, 2026-11-10). AC1 attempted, not met: 10 clean boots on .61 (2 A0 boots on beta.167 in the morning, 8 on beta.170: 4 with a live telnet reader, 4 without, 720 s each, device/info every 5 s, settings restored with 0 differences) showed no step at ~+5 or ~+10 min; detectie saw it 4/4 on beta.162 and the code between 162 and 167 differs only in banners. The windowed minimum always falls in the boot burst (first ~5 s after up), 22.7-39.4 KB. AC2 not met, consequently: no step to attribute. One observation: a -7.8 KB drop of hd_min_free_heap (35156 -> 27328) at exactly 15:00:00 wall clock, the hourly sendMQTTheapdiag() burst (OTGW-firmware.ino:960-962, ~30 stats publishes, each also printed to telnet); only that boot crossed a full hour, so telnet vs no telnet is not settled. AC3: Robert decided to close (accept), no follow-up task. Evidence: session scratchpad t1756/runT1-telnet-170, runT0-notelnet-170, analyze.py.
<!-- SECTION:FINAL_SUMMARY:END -->
