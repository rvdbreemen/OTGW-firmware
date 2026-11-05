---
id: TASK-1528
title: >-
  Heap low-water mark sinks to ~1.7 KB under a long-run capture (free stays ~71
  KB): find the transient large allocation
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-31T13:54:00Z'
updated_date: '2026-11-05T21:35:00Z'
labels:
  - alpha
  - bug
  - heap
  - beta.1
dependencies: []
priority: high
ordinal: 684000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Bench OTGW32 .61 alpha.570, 2026-10-31, TASK-1523 long-run capture running (telnet, /ws frames, MQTT, 5-min snapshots GET /api/v2/device/info + /api/v2/sat?detail=full, crashlog poll every 30 s) with SAT simulation and OT replay. soak-570-otgw32.log: minfree 2240 (13:38Z) -> 2180 (13:43Z) -> 1752 (13:48Z) while free stays ~71 KB and hd_min_max_block 7668; entered_critical stays 0 (sampling misses it, TASK-1386). The alpha.559 soak on .63 without capture had minfree 29564. A transient allocation of ~69 KB happens repeatedly; first suspects: the snapshot requests (sat?detail=full JSON), the crashlog poll, the /ws frame stream at reconnect, telnet reconnect replay. Risk: an allocation failure or crash for testers who run the capture or poll the API.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce on the bench: watermark (or windowed heap minimum, platformHeapWindowStart) around each candidate request in isolation; name the allocation that drops the heap below 5 KB with evidence
- [ ] #2 Fix or bound it (stream instead of buffer, cap, or capture-tool change) with old-vs-fix watermark numbers under the same capture load
- [ ] #3 Capture-tool snapshot interval and endpoints reviewed against the result
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Plan (2026-11-05, after the 21:29-22:00 bench; design choice open for Robert).

Facts from the bench (t1528b/run1-telnet.txt, run2-telnet.txt): on both boots the native minimum drops in the first 5 s after MQTT CONNACK. onMqttConnect (MQTTstuff.ino:1552-1615) queues birth, versioninfo, publishAllPICsettings, unsupported-MsgID CSV and the subscribes: outbox=56 at the end of the connect window (free 42 KB). Two seconds later the first discovery drip tick (loopMQTTDiscovery, MQTTstuff.ino:2410-2500, 2 s cadence) publishes OT ID 0 with the device block: 39 topics in one tick while those 56 packets are still queued. The windowed minimum for that drip window is 16824 B (run 1) and 4076 B (run 2). After that the minimum stays flat for 15 min.

Two contributors, kept apart:
(1) Boot connect burst plus the first drip tick: shown on 2 boots, depth varies 16.8 KB vs 4.1 KB.
(2) The night run step to 548 B at uptime 1:05 (no boot, no drop): suspected weather worker, NOT proven (the telnet 'w' never reached the firmware). This plan only addresses (1); (2) needs its own repro with a working trigger.

Design options for (1):
A. Outbox-depth gate on the drip: loopMQTTDiscovery skips a tick while mqttOutboxDepth() is above a limit (start: the SAT_MQTT_OUTBOX_LIMIT value in SATmqttPublish.cpp:51), counted in a new heapdiag skip counter. Reuses the TASK-1339 primitive, sits next to the ADR-088 isStatusBurstActive/isDripDeferred skips (gate check_drip_consults_deferred stays green), a few lines. Discovery finishes a few seconds later at boot.
B. Split big discovery jobs (OT ID 0, 39 topics) over several ticks. Touches the streaming discovery path (ADR-077, doAutoConfigureMsgid); larger change, also helps when the outbox is empty.
C. Fixed delay of the first drip tick after CONNACK (e.g. 10 s). Simplest, but time-based: does not adapt to a slow broker.
D. No change; 4 KB stays above the 3072 CRITICAL tier, collect field data first.
Recommendation: A, because it bounds the overlap that the bench shows, adapts to broker speed and reuses an existing gate. Risk: a broker that never drains keeps discovery waiting; mitigate with the existing slow-mode cadence and the skip counter in device/info.

Verification (when chosen): host test on the gate decision (red on the old drip, green on the fix), then bench .61 with 5 boots old vs 5 boots fix, windowed minimum of the connect and first drip windows from telnet, same settings; AC2 numbers from that. AC1 stays open until (2) is reproduced or excluded with a weather trigger through REST (no telnet key).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-31 15:06 bench .61 alpha.570, TASK-1523 run 2: planned network drop 14:58:33-15:01:34 (MikroTik bridge filter, rules removed: count 0). After the drop the unit stayed associated (CAPsMAN uptime 36 min, DHCP bound) but answered no ARP/HTTP until 15:06:19 (~5 min; in run 1 the same drop recovered in 1 s). No reboot (uptime continuous). runtime.heap_min_free now 524 B (was 1752 at 13:48Z). So the reconnect wave after a network outage (telnet + /ws + MQTT + snapshot + crashlog clients, MQTT republish) drives the heap to within ~0.5 KB of exhaustion. Related: TASK-1450 (2.0.0 port of 'recover from a WiFi link that stays associated but passes no traffic') is still To Do.

Response sizes measured 15:07 (one at a time): sat?detail=full 413 B, device/info 2767, device/crashlog 58, debug 6335, settings 9544, graph/history 2602. None explains a ~69 KB transient, so the snapshot requests are not the cause (suspect 1 dropped). Dips coincide with (re)connect moments: boot 14:28 (2240 B) and the network-drop recovery (524 B). Hypotheses for the next bench step, in order: (a) espMqttClient outbox mallocs per queued publish while TCP is slow or down (ADR-186), plus the HA discovery republish burst after reconnect; (b) AsyncSimpleTelnet TX buffering with all capture debug toggles on (~750 lines/min) while the client is slow/away; (c) AsyncTCP/lwIP pbufs for the /ws frame stream. Method: windowed heap minimum (platformHeapWindowStart) per phase with one stream enabled at a time (capture flags --skip-frame-capture, no broker, telnet toggles off), same network-drop fault, old vs fix.

Lead triage 2026-11-01: not actively worked; back to To Do, priority stays high (known issue in the beta.1 notes). Needs a dedicated multi-hour bench slot (windowed heap minimum per stream + network-drop fault); possibly linked to TASK-1450.

Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no: cause not found. Multi-hour bench run.

2026-11-04 20:49 overnight bench run started on .61 (beta.79, SAT simulation + built-in OT replay, MQTT on): device/info every 10 s (freeheap, hd_min_free_heap watermark, maxfreeblock, hd_min_max_block), continuous telnet log, 180 s network drop every 30 min (MikroTik bridge filter on the unit MAC, router-side :delay 200s auto-removal). Output: scratchpad t1528-night/ (samples.log, events.log, telnet.txt). Goal AC1: time each watermark step against the drop/reconnect events and the telnet context.

2026-11-05 night run .61 (beta.79 line? no: the unit ran the build flashed before 20:49; 9 h, replay running, 18 bridge-filter drops of 180 s every 30 min, sampler device/info every 10 s, telnet log): bootcount 46 for all 2830 samples (0 restarts, uptime 9:29 at the end), mqtt_drops 0, max loop gap 489 ms, router rules left 0. Watermark steps (hd_min_free_heap = native ESP minimum, not resettable): 15156 at start; 14896 at 21:03:04 (6 s after drop 1 ended); 548 at 21:21:49 (between drops, uptime 1:05; 10 s before it freeheap 71748, telnet prefixes in 21:21:39-21:21:49 never below 72 KB, max block stayed 31732, so a burst of many allocations, not one big block); 536 at 21:33:06 (6 s after drop 2 ended, reconnect). No further steps in the remaining 16 drops. Open: what allocated ~71 KB in one burst at 21:21:4x (candidate: an event about 1 h after boot, or a client request); next step: telnet heap trace around uptime 1:05 on a fresh boot, and check hourly jobs. Data local: scratchpad/t1528-night/.

2026-11-05 ~19:30 6c (host analysis of the night run data from 6b: scratchpad 9604259c.../t1528-night, build 2.0.0-beta.79+a9dd048, evening of 2026-11-04). The watermark step 14896 -> 548 falls between samples 21:21:39 and 21:21:49 (samples.log:181-182). The telnet prefixes show exactly one dip there: 21:21:44.563 free 64264 / max block 21492 (telnet.txt:17931), against 78160 / 31732 at 21:21:43.815, and back at 72444 / 31732 at 21:21:45.024. No loop-task log line between 43.815 and 44.563, so the allocation runs off the loop task. At 21:21:30.34 the weather worker started (telnet.txt:17796). It runs on the 15-min cadence from 20:51:30 as a one-shot FreeRTOS task: 8 KB stack (WEATHER_WORKER_STACK_BYTES) + WiFiClient + HTTPClient + DNS/connect (SATweather.ino at a9dd048, weatherRequestFetch / weatherFetchWork). That fetch logged no weatherApply line (neither did 21:06, 21:36, 22:21, 22:36 and others), and the dip came 14 s after the start, which fits the connect bound (2 s) plus read bounds (5 s). Ruled out with log evidence: the thermal coeff save (only at 02:16 and 03:36, telnet.txt:118187/145836), the hourly energy save (21:16:35, outside the window), wall-clock hourly jobs (21:21 is not on the hour), and the snapshot requests (sizes measured earlier). Open gap: the visible dip is ~14 KB, the watermark says ~71 KB. Hypothesis only, not evidence: the ESP32 WiFi dynamic RX buffers (many ~1.6 KB allocations during a packet burst) match the 'many allocations, max block stays' signature; the 21:03:04 and 21:33:06 steps both follow a drop end. Bench recipe for AC1 when .61 is free: fresh boot, SAT sim + replay as in the night run, platformHeapWindowStart windows around (a) the weather worker (telnet 'w' key forces a fetch; also with the internet blocked for the timeout path) and (b) a 180 s drop end; plus a weather-off run with the same drops as control. AC1 not ticked: needs this bench reproduction.

Bench 2026-11-05 21:29-22:05 on .61 (2.0.0-beta.101+5781def, satsimulation on), from /ReBoot each run, device/info every 5 s + telnet. Evidence: scratchpad t1528b/ (run1-*, run2-*, events.log). Run 1 (internet up): hd_min_free_heap 76868 -> 16824 within 5 s of boot, then flat 15 min. Run 2 (firewall forward drop src .61 out ether1, TASK-1528-2, scheduler removal, left 0): 75640 -> 4076 within 5 s of boot, then flat. Telnet places both dips in the boot MQTT connect window (outbox=56, discovery drip id=0 topics=39, free 42 KB at window end; burst minWin 16824 resp. 4076), before any weather fetch. 4076 B is 1 KB above the CRITICAL 3072 B tier.

Not verified: the forced weather fetch. The telnet 'w' sent by the script (raw socket, char mode) produced no 'Weather: manual Open-Meteo fetch triggered' line in run 1 or run 2, so no fetch ran; cause of the lost keypress unknown (to test with 'h'). Run 3 (weather off, 180 s drop control) was cut at 22:00 after boot: the runner was killed by Claude Code's low-memory reaper before the drop; no drop rule was added. Cleanup checked: firewall/bridge/scheduler TASK-1528 count 0, router export t1528-runs-after, satweatherenable restored true, settings diff vs before = {}. AC1 still open: needs a working weather trigger and several boots to see whether the boot connect burst depth (16.8 KB vs 4.1 KB) is random.

Robert 2026-11-05 ~22:30 (AskUserQuestion in session): option A chosen, outbox-depth gate on the discovery drip. Next: host test red/green on the gate decision, build, bench 5 boots old vs 5 boots fix on .61. Not started tonight: commit charge 83%, lead 6b asked for no long runs tonight.
<!-- SECTION:NOTES:END -->
