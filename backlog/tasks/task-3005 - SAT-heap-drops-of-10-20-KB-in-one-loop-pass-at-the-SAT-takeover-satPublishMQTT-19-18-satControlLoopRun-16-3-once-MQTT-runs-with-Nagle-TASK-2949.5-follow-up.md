---
id: TASK-3005
title: >-
  SAT: heap drops of 10-20 KB in one loop pass at the SAT takeover
  (satPublishMQTT 19/18, satControlLoopRun 16/3) once MQTT runs with Nagle
  (TASK-2949.5 follow-up)
status: To Do
assignee: []
created_date: '2026-11-21T12:55:00Z'
updated_date: '2026-11-21T12:56:00Z'
labels:
  - mqtt
  - heap
  - sat
milestone: 2.0.0
dependencies:
  - TASK-2949.5
priority: medium
ordinal: 1126000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-2949.5 bench on .61 (2026-11-21, beta.410+eedf081, n=3 per arm, loopback, SAT on, satsimulation off): with Nagle on the MQTT socket the largest drop inside MQTTclient.loop() (probe at=13 from=12) falls from ~43 KB to 7.3/9.0/13.5 KB, but the largest kept drops move to the SAT path: at=19 from=18 (CPROBE_SAT_PUB_END after CPROBE_SAT_PUB_BEGIN, i.e. inside satPublishMQTT) 10320/15644/18544 B at the takeover, and at=16 from=3 (CPROBE_SAT_RUN after CPROBE_DISCOVERY, inside satControlLoopRun) 16456-19744 B. hd_min_free_heap stays >= 14.6 KB with Nagle. Evidence: OTGW-field-data\bench-otgw32\2026-11-21_task2949-5-takeover\ and TASK-2949.5 notes. Goal: find what allocates in those two steps at the takeover (outbox copies of the SAT always-on set, JsonEmit/stream buffers, discovery work inside the SAT run) and bring the largest one-pass drop under 10 KB without changing SAT control behaviour (publish timing/pacing only; ADR-088 burst windowing, ADR-266 per-loop cap as backstop).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Per-step breakdown of the 19/18 and 16/3 drops at the SAT takeover on .61 (probe records with outbox depth and mqttTx), naming the allocations
- [ ] #2 Change (pacing or cap) with host test red/green and the reason in the code comment; no SAT control behaviour change
- [ ] #3 Bench on .61 n=3: largest drop at 19/18 and 16/3 under 10 KB at the takeover, hd_min_free_heap not lower than the TASK-2949.5 Nagle arm
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Related: TASK-1995 (loop stalls of 180-430 ms during the SAT status MQTT burst, added the CPROBE_SAT_PUB_BEGIN/END probes) - same publish path, check it first. Probe tags from MQTTstuff.h enum: 3 DISCOVERY, 16 SAT_RUN, 17 SAT_CORE, 18 SAT_PUB_BEGIN, 19 SAT_PUB_END, 12/13 MQTT_PRE/POST_LOOP.
<!-- SECTION:NOTES:END -->
