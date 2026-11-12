---
id: TASK-1864
title: >-
  Heap low-water on beta.214 (3.9-4.6 KB) against about 7 KB on beta.211: rule
  out the TASK-1757 boot race before calling it a regression
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-12T11:01:00Z'
updated_date: '2026-11-12T21:26:00Z'
labels:
  - heap
  - investigation
  - ot-direct
milestone: 2.0.0
dependencies:
  - TASK-1757
priority: medium
ordinal: 940000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Observation (otgw-bench-tester 09, .61 OTGW32, 2026-11-12): hd_min_free_heap on beta.214+5d03c17 fell to 4,612 B, then 4,100 B (bench of TASK-1842/1844/1860, after a pyotgw session and with a Firefox WS client), then 3,948 B in a SAT simulation run (SAT on, satsimulation, no boiler, one extra Firefox WS client). CRITICAL is 3,072 B. In 09's earlier beta.211 run the minimum was about 7 KB. No reset, no crash, bootcount stable.

Not yet a regression. hd_min_free_heap is a run-wide watermark since boot, so each value likely includes the boot connect window. That is TASK-1757's finding: with one browser tab, boots are bimodal (18-19 KB vs about 7 KB in the boot window), with a 40-55 KB drop at the first MQTT write when a page load lands in it; TASK-1757's AC2 run on beta.167 already saw 4,244 B in the boot window and 3,600 B overall with two tabs. Robert's Firefox tab was open in every run here. Alternative explanation to rule out first: the same boot race landing worse, not a change in 211..214. Related: TASK-1757.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Closing run on .61 with beta.218 (TASK-1757 fix) in the 214 scenario: SAT on, satsimulation, no boiler, the Firefox tab, one pyotgw session, 30 min after a /ReBoot; hd_min_free_heap at +60 s and at +30 min, plus device/info free and maxblk every 60 s; close as the TASK-1757 boot race when the +30 min watermark stays at or near the +60 s value, well above 3,072 B, with no reset
- [x] #2 Static review of src/ changes 203..214 per prerelease tag for new allocations, buffers, task stacks and connection handling, recorded in the notes
- [ ] #3 Only if AC1 shows the watermark falling during steady state well below the +60 s value: bisect over the tags (208, 211, 214) by the bench-tester and name the commit with the measurement; otherwise close as the TASK-1757 boot race with the numbers
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC2 static review (95, 2026-11-12, no build; scratch scan of every src/ commit per segment for alloc/free, new/delete, String, std containers, reserve, task create/stack, queues/ringbuffers, AsyncClient, static arrays, char/uint8_t buffers >= 100, *_SIZE/LEN/CAP/MAX defines; added and removed lines):

beta.203 (e418ead3d) -> beta.208 (b1486c859): 204-207 are web UI only (TASK-1843, 1838, 1837, 1841: v2.js, v2.html, v2.css). Asset sizes 203 -> 208: v2.js 525,953 -> 529,032 B (+3,079), v2.html 65,091 -> 64,611 (-480), v2.css 67,414 -> 67,171 (-243); unchanged after 208. beta.208 b1486c859 TASK-1757 (MQTTstuff.ino): the connect burst leaves in portions and the drip waits on a deep outbox; scan hits are only a [HEAP] debug line per connect stage. This is the one segment that touches the boot connect window itself.

beta.208 -> beta.211 (b2e473f87): 9aa81609c TASK-1846 and 2216ea923 TASK-1855, OTDirect.ino only, no scan hits; b2e473f87 docs (version.hash only).

beta.211 -> beta.214 (47f9edd84): 8f6924552 TASK-1842, 60762c994 TASK-1844, 47f9edd84 TASK-1860: OTDirect.ino plus one emitted field in restAPI.ino (device/info otdpiccompat, a const char* from a flash string); no scan hits. New code is a few pure functions and one entry in the existing otOverrides table.

Static conclusion: no change in 203..214 adds a heap allocation, buffer, task or stack. The only change near the boot heap window is beta.208's TASK-1757 connect-burst change (by design it lowers the burst). So the static review gives no cause for a 211 -> 214 drop; AC1 (like-for-like measurement) decides. Not checked: libraries (no src/libraries change in the range), runtime behaviour.

AC1 recipe from ba (TASK-1757 owner, 2026-11-12), works on every 2.0.0 build since beta.171, no new firmware: hd_min_free_heap in device/info is a run-wide, non-resettable watermark, so it always contains the boot dip and cannot separate boot from steady state. Read telnet from right after /ReBoot instead: each '[HEAP] boot win=<phase> dur=.. minWin=..' line gives the windowed minimum per boot phase (setup-early, setup-net, setup-services, setup-late, wait-mqtt, connect, drip ...) until '[HEAP] boot done'; the connect phase also prints '[HEAP] connect-trace ...' lines and 'connect-trace end ... traceMin= phaseMin= match=1'. Boot-window minimum = the minimum over those minWin values. Steady-state minimum = sample device/info after 'boot done' and track free / maxblk (or the windowed dip fields), not hd_min_free_heap.

Existing runners: 09's b211 run (telnet per boot + info60.json) and ba's t1757/boots1757ac2.py + trace_summary.py (ba's scratchpad). Like-for-like conditions (ba): the Firefox tab adds one WS client to every boot, so keep it identical on 211 and 214 (or closed; Robert's DM answer on the tab is pending), same mode (C / one tab / two tabs), at least 4 boots per build, because one-tab boots are bimodal (18-19 KB vs about 7 KB on 211).

Attribution build (ba): TASK-1757 instrumentation is prepared on lane/ba-1757 (connProbe() at 17 call sites, including right before and after satPublishCoreState; ba's offline read of the 211 boot-3 log shows about 14 sat/* topics published in the first pass after onMqttConnect, then '[loop-stall] 288 ms'). It takes the beta.215 slot, on top of 214, so it attributes the dip on the 214 code base but does not compare 211 with 214.

WS client baseline (ba, 2026-11-12; Robert's choice B on the desktop Firefox tab at .61): the tab stays open as a constant extra /ws client, so every run on beta.208/211/214/215 has one client more than its mode says (C = 1, one-tab = 2, two-tab = 3); unknown for the beta.167/171 baselines. The 211-vs-214 comparison stays like-for-like as long as both sides have the tab open (they had); do not compare against the 167/171 numbers without that caveat. Same note in TASK-1757.

Correction (coordinator and 09, 2026-11-12): the three 214 values 4,100 / 3,948 / 3,896 B all come from ONE boot (bootcount 466). hd_min_free_heap is a since-boot low-water mark and can only fall within a boot, so a falling watermark is not a trend. The ~7 KB on 211 was a per-boot watermark over a boot window without pyotgw or a SAT run, so the 211-vs-214 comparison does not hold either. What stands: one boot under bench load with a lowest point of 3,896 B (about 0.8 KB above CRITICAL), no reset. AC1 must use current free heap and maxblk sampled per phase (boot window per '[HEAP] boot win=' phase, then steady state), not the watermark; 09's TASK-1757 probe bench on 215 (12 boots, the same per-boot window) gives comparable numbers.

2026-11-12 re-read after TASK-1757 landed (ba, no build, numbers from 09's bench files as reported in TASK-1757): beta.215 (214 code base + probes), 12 boots on .61 with the Firefox tab: the boot-window probe minimum was 8,184-35,944 B, and in every boot the largest drop sat inside one MQTTclient.loop() after onMqttConnect (23,452-43,516 B, outbox 4), with the SAT core-state publish landing on an outbox of 18. beta.218 (TASK-1757 fix, same scenarios): probe minimum 31,816-52,528 B, largest loop() drop 7,024-19,976 B, maxblk minimum 19,444-31,732 B, hd_min_free_heap at +60 s 29,216-52,368 B. Reading: the 214 observation (one boot, bootcount 466, since-boot watermark 3,896 B under pyotgw + SAT sim + Firefox) fits the TASK-1757 boot race: the dip that race caused was 23-43 KB per boot on the 214 code base, enough to reach about 4 KB from a 30-50 KB start, and the 218 fix removes it. The static review (AC2) found no new allocation in 203..214. What is NOT shown: steady-state heap under the 214 load (pyotgw session + SAT sim + Firefox WS client), on any build; all 215/218 numbers are boot-window or +60 s. So the boot race explains the low point plausibly but not with a measurement of the same load.

Proposed closing run instead of AC1's 211-vs-214 comparison (which the correction note already shows does not hold): beta.218 on .61, the 214 scenario (SAT on, satsimulation, no boiler, Firefox tab, one pyotgw session), 30 min after a /ReBoot. Record hd_min_free_heap at +60 s (boot window) and at +30 min, plus device/info free and maxblk every 60 s. Pass to close as the TASK-1757 boot race: the +30 min watermark stays at or near the +60 s value and well above 3,072 B (CRITICAL), with no reset. If the watermark falls during steady state well below the +60 s value, a steady-state cause remains and AC3's bisect applies. Needs a bench slot from 09, no new firmware. AC1 wording should change to this run; that is the coordinator's or Robert's call.

2026-11-12 AC1 reworded by the coordinator's call (measurement detail): the 211-vs-214 comparison is replaced by the beta.218 closing run in the 214 scenario. AC2 stays ticked (95's static review). The coordinator books the run with 09 after the beta and 219's bench.
<!-- SECTION:NOTES:END -->
