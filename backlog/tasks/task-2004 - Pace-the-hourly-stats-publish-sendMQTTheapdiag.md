---
id: TASK-2004
title: Pace the hourly stats publish (sendMQTTheapdiag)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-16T13:26:00Z'
updated_date: '2026-11-16T14:17:00Z'
labels:
  - mqtt
  - heap
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1025000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
At every wall-clock hour (ADR-086, doTaskMinuteChanged -> if (hourFlag) -> sendMQTTheapdiag(), OTGW-firmware.ino:998-1001) the firmware publishes 26 retained otgw-firmware/stats/* topics back to back (publishStatU32 -> sendMQTTData, MQTTstuff.ino ~2305-2330), with no burst window. 09 on .61 (OTGW32, no PSRAM, beta.351 bench build, 2026-11-16): hd_min_free_heap fell from 19,876 B to 2,180 B (below CRITICAL 3,072) at 14:00:00.002-.040, followed by '[drip] slowed to 10s (heap pressure)'; no reboot, free heap back at once. .63 (Classic with PSRAM, beta.350) showed no dip (hd_min 35.5 KB). The ADR-088 burst window only keeps the discovery drip out (MQTTstuff.ino:108-160); it does not reduce the 26 queued payloads. canPublishMQTT() blocks at CRITICAL (ADR-167), so the tail of the burst may be dropped. Fix: pace the stats like the discovery drip (a cursor, N per loop pass or one per ~100 ms), flat topics stay (ADR-101). Coordinator 2026-11-16: priority medium, high if 09's 15:00 mark on .61 with plain dev (352) shows the same dip; after the 20:00 beta tag.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduction red on the old code: hd_min drop at hh:00 on a board without PSRAM, plus the count of the 26 stats/* topics that reach the broker
- [ ] #2 Fix green on the same reproduction: paced, all 26 topics arrive, hd_min stays above ~10 KB
- [ ] #3 Host test on the real pacer (order, all topics once, respects canPublishMQTT)
- [ ] #4 Bench on .61 by the bench-tester
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-16 (95) taken over from ba (coordinator). Status of the reproduction (ba): 14:00 dip to 2,180 B on .61 with ba's 351+597c163 and SAT enabled/inactive; 15:00 on 352 with SAT off: no dip, all 26 topics arrived. 09 runs the 16:00 mark on 353 with SAT enabled (inactive, no sim). Per the bug rule (reproduce, then prove) no code before that result: if 16:00 shows no dip, the 14:00 one is not reproduced and AC1 is open. Planned fix if it reproduces: a table/switch of the 26 stats driven by a cursor, one publish per ~100 ms from the loop (like the discovery drip), skipped while !canPublishMQTT(); flat topics stay (ADR-101); host test on the real pacer (order, every topic once, respects canPublishMQTT).
<!-- SECTION:NOTES:END -->
