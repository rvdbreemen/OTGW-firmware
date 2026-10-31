---
id: TASK-1528
title: >-
  Heap low-water mark sinks to ~1.7 KB under a long-run capture (free stays ~71
  KB): find the transient large allocation
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-31T13:54:00Z'
updated_date: '2026-10-31T14:07:00Z'
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-31 15:06 bench .61 alpha.570, TASK-1523 run 2: planned network drop 14:58:33-15:01:34 (MikroTik bridge filter, rules removed: count 0). After the drop the unit stayed associated (CAPsMAN uptime 36 min, DHCP bound) but answered no ARP/HTTP until 15:06:19 (~5 min; in run 1 the same drop recovered in 1 s). No reboot (uptime continuous). runtime.heap_min_free now 524 B (was 1752 at 13:48Z). So the reconnect wave after a network outage (telnet + /ws + MQTT + snapshot + crashlog clients, MQTT republish) drives the heap to within ~0.5 KB of exhaustion. Related: TASK-1450 (2.0.0 port of 'recover from a WiFi link that stays associated but passes no traffic') is still To Do.

Response sizes measured 15:07 (one at a time): sat?detail=full 413 B, device/info 2767, device/crashlog 58, debug 6335, settings 9544, graph/history 2602. None explains a ~69 KB transient, so the snapshot requests are not the cause (suspect 1 dropped). Dips coincide with (re)connect moments: boot 14:28 (2240 B) and the network-drop recovery (524 B). Hypotheses for the next bench step, in order: (a) espMqttClient outbox mallocs per queued publish while TCP is slow or down (ADR-186), plus the HA discovery republish burst after reconnect; (b) AsyncSimpleTelnet TX buffering with all capture debug toggles on (~750 lines/min) while the client is slow/away; (c) AsyncTCP/lwIP pbufs for the /ws frame stream. Method: windowed heap minimum (platformHeapWindowStart) per phase with one stream enabled at a time (capture flags --skip-frame-capture, no broker, telnet toggles off), same network-drop fault, old vs fix.
<!-- SECTION:NOTES:END -->
