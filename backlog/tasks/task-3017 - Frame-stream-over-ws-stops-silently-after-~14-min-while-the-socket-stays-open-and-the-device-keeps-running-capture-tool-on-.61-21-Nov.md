---
id: TASK-3017
title: >-
  Frame stream over /ws stops silently after ~14 min while the socket stays open
  and the device keeps running (capture tool on .61, 21 Nov)
status: To Do
assignee: []
created_date: '2026-11-21T16:41:00Z'
updated_date: '2026-11-21T16:46:00Z'
labels:
  - tooling
  - bug
dependencies: []
priority: high
ordinal: 1131000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the TASK-3006 stream watch (09, .61 beta.413+da42aca, capture tool 1c16a65 on the NAS): last frame 16:51:44 (device 16:51:42.734), the /ws connection stayed open until 'disconnected from /ws' 17:38:20, the device kept its OT traffic (telnet processOT ~720 lines/min 16:38-17:38) and reported WS_drops=0 every minute; no WS line on the device around 16:51:4x. Either the firmware stops sending to this client or the tool's WS reader stalls. Possibly the same as will_85903's 08:04 silence on 20 Nov (both device streams). Evidence: OTGW-field-data/bench-otgw32/2026-11-21_task3006-capture-1c16a65/.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduced: a capture on .61 shows the /ws frame stall again, with a device-side and a tool-side trace that tell which side stops
- [ ] #2 Root cause fixed with a test that fails on the old code
- [ ] #3 A capture of >= 2 h on .61 has no /ws silence while the device runs
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-21: deferred to 2026-11-25 (token budget), like the rest of this lane.

Bench check 2026-11-21 17:45 (otgw-bench-tester), .61 beta.413+da42aca, no reflash/reboot since 15:42: (1) fresh /ws connection (stdlib client from the NAS): HTTP 101, 296 frames in 30 s, first after 0.29 s, last at 30.09 s -> a new client gets frames. (2) the stalled connection no longer exists: the capture tool closed it at stop ('disconnected from /ws' 17:38:20), not testable. (3) device/info: uptime 2h02, bootcount 1168 (no restart), freeheap 48028, maxfreeblock 31732, hd_min_free_heap 116 B; for comparison .63 (beta.421+464a188, 25 h soak) hd_min_free_heap 20884 B. When the 116 B minimum happened is not visible (capture telnet logHeapStats HEALTHY every minute, WS_drops=0; device state at capture start showed low/warn/crit 13/3/1). Hypothesis, untested: a WS send dropped under heap pressure without WS_drops counting. .61 left as is (no reflash/reboot) to keep the evidence. Evidence: OTGW-field-data/bench-otgw32/2026-11-21_task3006-capture-1c16a65/.
<!-- SECTION:NOTES:END -->
