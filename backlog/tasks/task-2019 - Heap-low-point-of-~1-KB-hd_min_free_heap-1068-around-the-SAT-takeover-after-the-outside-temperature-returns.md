---
id: TASK-2019
title: >-
  Heap low point of ~1 KB (hd_min_free_heap 1068) around the SAT takeover after
  the outside temperature returns
status: In Progress
assignee:
  - '@otgw-dev'
created_date: '2026-11-18T10:04:00Z'
updated_date: '2026-11-19T04:25:00Z'
labels:
  - heap
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1036000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
09 on .61, beta.369 (2026-11-18, TASK-2013 bench: GW=L, SAT on, preset, 'q', outdoor_temp): hd_min_free_heap = 1068 B (368 smoke: 20,900). hd_min is the minimum since boot (platformMinFreeHeap(), restAPI.ino:3178), so the time is not known from it. Boot-window [HEAP] lines: at 10:13:13.564 'SAT: outside temperature valid again, SAT resumes' + 'SATBOOT phase=takeover', then minWin 4236 / 6348 / 2536 at 10:13:14-16 (drip id=1, 1, 14), while free heap read 54.7-55.0 k and maxblk 31,732 at each print: short spikes of ~50 KB. No info.jsonl sample between boot and 10:36, so the 1068 itself is not placed (inferred: takeover window). otgw-docs: the TASK-2013 diff allocates nothing (settingStuff.ino:583, :616-618). Candidates (not verified): the first SAT control pass forced by the takeover (SATcontrol.ino:4486-4491, timerSATControl_due = now) and its full SAT publish overlapping the boot discovery drip in the espMqttClient outbox (SAT_MQTT_OUTBOX_LIMIT 24, SATmqttPublish.h:89; CONN_STAGE_MAX_OUTBOX, MQTTstuff.ino:1851). Read-only so far; no firmware change.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The dip is reproduced on the bench and placed in time: reboot, wait for 'outside temperature missing', inject outdoor_temp, sample hd_min every 1-2 s (09's recipe)
- [ ] #2 The allocation behind the dip is named with file:line and evidence (heap trace, outbox count or a host reproduction)
- [ ] #3 Fix or a decision recorded; the same repro shows the dip gone (red/green)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Hypothesis H1 (from code + 09's lines, NOT measured): on that boot the SAT takeover (SATcontrol.ino:4486-4491, 'since_sat_start_ms=210053') is the FIRST SAT control pass since boot, so its satPublishMQTT() is the first full SAT publish: every SAT topic is first-seen. That is the TASK-1339 path (126 packets, ~82 -> 8.7-14 KB). Since TASK-1339 the helpers stop at SAT_MQTT_OUTBOX_LIMIT = 24 packets (SATmqttPublish.h:89, publishJsonAttrIfChanged() SATmqttPublish.cpp:299-317) and satControlLoopRun() repeats the pass every 500 ms while deferred. The gate counts packets, not bytes: 24 packets with JSON blobs (climAttrBuf[512] SATcontrol.ino:5780, cycles buf[960] SATcycles.ino:1592) plus topic and espMqttClient packet overhead hold up to ~25-30 KB per portion, and the boot discovery drip and the per-OT-ID publishes add their own packets in the same window (drip id=1/1/14 in 09's lines). Three portions at 500 ms fit the three seconds of low minWin (10:13:14-16). H2 (open): a non-MQTT allocation of the first control pass (state-file loads, LittleFS file buffers). H1 predicts outbox= near or above 24 at each minWin drop; H2 predicts a low outbox.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Read-only so far (otgw-dev, 2026-11-18, origin/dev 83648a4e9). (1) TASK-2013 diff ruled out by otgw-docs: no allocation (settingStuff.ino:583, :616-618), the 'q' reload does less with a held preset. (2) Telnet output ruled out: AsyncSimpleTelnet has a static TX ring per client (AsyncSimpleTelnet.h:165-168), AsyncClient only takes what fits in the TCP send buffer. (3) The takeover itself (SATcontrol.ino:4476-4491) only resets the CS/MM/CH cache, narrates one line and makes the control pass due at once. (4) The first full SAT publish was the TASK-1339 heap killer (126 packets, one malloc each, ~82 KB -> 8.7-14 KB); since then publishIfChanged*() skips a due publish while the outbox holds more than SAT_MQTT_OUTBOX_LIMIT = 24 packets (SATmqttPublish.h:80-89); the gate counts packets, not bytes. The boot discovery drip only adds a portion while the outbox holds at most CONN_STAGE_MAX_OUTBOX = 2 packets (MQTTstuff.ino:1794, :1851). On the code alone neither explains a ~50 KB spike for 3 s; not settled without a measurement.

Next step (needs the bench, after the TASK-1995 run on .61): 09's recipe (reboot, wait for 'outside temperature missing', inject outdoor_temp) with hd_min sampled every 1-2 s, plus the [HEAP] burst/drip lines, which already print outbox= and minWin (MQTTstuff.ino:1449, :1491). The outbox count at the minWin drop separates an MQTT outbox burst (count high) from a non-MQTT allocation (count low). Compare with the same recipe on 368 to answer 'new in 369 or old'.

Host repro plan: compile the REAL publishIfChanged*/publishJsonAttrIfChanged/satOutboxFull path (SATmqttPublish.cpp) with a stub MQTT client that keeps every queued packet (no drain) and counts the bytes espMqttClient would allocate per packet (fixed header + topic + payload). Drive the first full SAT publish (all shadows first-seen) as in the existing SAT MQTT state harness and record the bytes held when the 24-packet gate stops. Red when those bytes exceed a budget (e.g. 16 KB), green after a fix that gates on bytes (or a smaller packet limit during the boot drip). Bench check: 09's recipe with the [HEAP] burst/drip lines (outbox= at each minWin drop) and hd_min every 1-2 s; first, the outbox= values of the three lines 10:13:14-16 already in 09's telnet.raw separate H1 from H2 without a new run.

Design constraint for the fix (coordinator, 2026-11-18): run /adr-kit:context first; a byte-based outbox gate must fit ADR-088 (binding: MQTT status-burst windowing + post-burst cooldown; gates check_status_publishers_wrap_burst, check_status_burst_cooldown_bound, check_drip_consults_deferred in evaluate.py), not bypass it. Also check TASK-1339's design of SAT_MQTT_OUTBOX_LIMIT and ADR-167 (no heap tier gating). No firmware change before the 370 push.

H1 REFUTED (09, fix369A-telnet.raw): outbox=1 at all three dips (10:13:14.455 minWin 4236, :15.462 6348, :16.459 2536), sburst=0, connburst=0, no [HEAP] burst lines in 10:13:13-17; minWin 51,444 before and 50,752 after. Discovery drip ruled out on the code: its only malloc is the transient payload in measureMallocPublish() (MQTTHaDiscovery.cpp:2309, ~900 B, freed after publish). H3 (open): the SAT weather fetch on the webhook task (weatherWorkerPoll(), OTGW-firmware.h:395-396; open-meteo current + 24 h hourly, SATweather.ino:99-106): stream-parsed without malloc (SATweather.ino:114-122), but WiFiClient/HTTPClient/DNS and the lwIP pbufs of the response allocate; size depends on our lwIP TCP window (not looked up). Needs a fetch in those seconds: asked 09 for weather/HTTP lines 10:12:40-10:13:20 and the satweather settings.

H3 REFUTED (09, fix369A-telnet.raw 10:12:40-10:13:20): 0 weather/Weather/open-meteo/HTTP lines while SATweatherenable=true with lat/lon set and interval 900; the outdoor value came from 09's MQTT injection. H1 and H3 out; the allocation is still unnamed. Next (diagnostic firmware, after the 370 push): heap-window samples (platformHeapWindowStart/min, the connect-probe pattern of MQTTstuff.ino:1235-1249) around each step of the first SAT control pass after the takeover (satControlLoopRun, satPublishMQTT, state loads), printed on the loop task; then 09's recipe (reboot, outside missing, inject outdoor_temp) with SAT on. Note: SAT is off on .61 now (restored after TASK-2013).

Control boot without SAT (09, .61, 370+41a9e44 app-only 11:27:48, satenabled false): hd_min_free_heap 42,020 at uptime 0, 23,236 at +1 min, 22,260 at +3 min, 18,220 at +5 min (freeheap 42.6-49.6 k, maxblock 31,732). So a boot WITHOUT a SAT takeover bottoms out around 18-23 KB, the 369 boot WITH the takeover at 1068 B: supports the takeover window as the place of the dip (correlation, one boot per arm, not proof). The setup() phase before telnet connects (11:28:17) is not visible in either.

Control run, SAT off (09, .61, 41a9e44, 62 min): hd_min_free_heap 14,180 from ~11:45 to the end (after 18,220 at +5 min), freeheap 43.7-52.7 k flat; hd_max_loop_gap_ms 481 from the boot phase. So without SAT the since-boot low point settles near 14 KB after about 15 min; the 369 boot with the SAT takeover reached 1068 B.

Diagnostics (lane-2019, 372): satTakeoverHeapTraceBegin() arms the TASK-1757 connect probes for 5 s at the SAT takeover and at 'outside temperature valid again' (SATcontrol.ino), keeping drops of 1 KB or more with the step before it; satTakeoverHeapTraceTick() in loop() prints '[HEAP] sat-takeover-probe ...' plus begin/end lines. No behaviour change. Second reader (otgw-webui) F1 fixed: a boot connect trace that starts inside the 5 s first ends the takeover trace (satTakeoverHeapTraceYield() in connTraceBegin()), so the takeover records are not wiped and not mislabelled; host test red on a mutant without the yield, green on the fix. F2 KNOWN LIMITATION (accepted by the coordinator): once another owner closes the shared heap window after boot (drip tick end, burst end), platformHeapWindowMin() reads 0 and the trace keeps no drop; at boot the boot watch keeps the window open, so 09's repro is covered. Tests: test_mqtt_connect_probe 9/9; sat_loop_stubs.h gets the satTakeoverHeapTraceBegin() stub (the SAT loop harnesses include MQTTstuff.h); test_loop_step_watch matches the extended trace gate. Full suite before F1: 333/337 with exactly those 4 harness failures, all green after the stub and the gate regex.

2026-11-18 bench observation (otgw-bench-tester 09, TASK-2020 AC3 run on .61, 373+9e6636d, 16:26:52-18:28:53): hd_min_free_heap fell 16,868 -> 6,044 between samples 17:43:52 and 17:44:52 (uptime ~78 min); maxblock dipped once to 25,588 at 17:57:52. No telnet line explains it (no [HEAP] burst/boot line, no MQTT reconnect; heap line 17:44:40 shows 52,960 free). One short dip, freeheap flat 45.9-52.8 k, no reboot. 370 arm hd_min was 12,508. Cause unknown, NOT investigated. Evidence: 09 scratchpad b2020\arm373\ (telnet.raw, info.jsonl).

Loopback exception (maintainer, 2026-11-18, relayed by the coordinator: Robert's DM 1572707965889159331, reply 'A'): a one-off exception to the 15 Nov no-loopback rule for bench boards. Scope: loopback (GW=L) ONLY on .61, ONLY for this TASK-2019 heap diagnosis (make SAT really take over and capture the 372 sat-takeover heap trace of the passes after it), switched off again afterwards with the settings verified as restored. It does not extend to other tasks, other boards or later runs. .61 state before the run: 2.0.0-beta.375+ff73985 (contains satTakeoverHeapTraceBegin, MQTTstuff.ino:1283), bootcount 1045, satenabled false, satsimulation false, satbleenable false, satweatherenable true, hd_min_free_heap 18,056 at 2 h 51 min uptime.

Starting evidence on current code (otgw-bench-tester 09, 2026-11-19), 2.0.0-beta.382+e562f82 on .61 (OTGW32, no PSRAM, SAT off, no loopback, no simulation), OTA app+fs from 375 at 04:20-04:22 ('settings: 165 keys, changed: none'), 60-min soak 04:22-05:23 with device/info every 60 s: 62/62 HTTP 200, bootcount 1051 throughout (no restart), 0 panic, 2 loop-stalls only right after the boot (04:22:12/13, ~420 ms, step 3->16), freeheap 39.2-44.9k, maxblk 23.5-31.7k, MQTT connected. hd_min_free_heap steps: 30,120 -> 24,356 -> 18,560 -> 15,532 -> 13,748 in the first 5 min (boot), then 13,748 -> 10,660 between 04:47:12 and 04:48:12 (uptime ~25 min) and 10,612 -> 6,444 between 05:02:12 and 05:03:12 (uptime ~40 min); a small step 10,660 -> 10,612 at 04:57-04:58. Telnet in those minutes shows only an [fs-slow] mount-check (123/116 ms, writer) at :11 of the minute and the poller's device/info GET; freeheap stayed 39.8-44.9k. So these dips occur WITHOUT a SAT takeover (SAT off). Same 382 on .63 (PSRAM, SAT enabled, handed back, no boiler): 60 min flat, hd_min 14,980 unchanged. Logs: 09 scratchpad soak382\61 and \63 (telnet.raw, info.jsonl).

2026-11-19 input 09 (FYI, no conclusion): on .61 with 382+e562f82, SAT OFF, no loopback, the low-water mark hd_min_free_heap stepped down twice in the soak: 13748 -> 10660 (04:47:12-04:48:12) and 10612 -> 6444 (05:02:12-05:03:12), ~25 and ~40 min after the 04:22 OTA. Polled freeheap 39.8-44.9k, maxblk 23.5-31.7k the whole time; telnet in those minutes only an [fs-slow] mount-check (123/116 ms, writer) at :11 and 09's device/info GET. Reading (ours): the low-water mark only falls, so these are short transient dips of ~34-38k below the polled level, not a trend, and they occur without a SAT takeover: the 2019 plan must not attribute such dips to the takeover. Logs: 09 scratchpad soak382/61/ (telnet.raw, info.jsonl).

2026-11-19 follow-up 09: the [fs-slow] mount-check runs every minute at :10-:11 on .61 (65 lines in 61 min, 109-146 ms), also in minutes without a step in hd_min (04:42 144 ms, 04:58 146 ms); the poller GET runs every minute at :12. Neither explains the two dips on its own; nothing else distinctive in telnet. Cause of the two transient dips: open, not found in the logs. For 2019: compare the takeover run against this SAT-off baseline (two dips in ~60 min, low-water 13.7k -> 6.4k), not against zero.
<!-- SECTION:NOTES:END -->
