---
id: TASK-1773
title: >-
  device/info reports bootcount 0 for about 1 s after boot (web server starts
  before updateRebootCount())
status: To Do
assignee: []
created_date: '2026-11-10T04:17:00Z'
updated_date: '2026-11-12T17:13:00Z'
labels:
  - bug
  - rest
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 870000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen 2026-11-10 in the TASK-1757 reboot run on .61 (beta.167+e79e495): the first GET /api/v2/device/info after a reboot answered bootcount=0 in boots 3, 4 and 6 (e.g. 04:36:20 bootcount=0, 04:36:25 bootcount=205). Cause in setup(): startWebserver() at src/OTGW-firmware/OTGW-firmware.ino:516 opens the REST API before state.uptime.iRebootCount = updateRebootCount() at :540; restAPI.ino:3396 emits the 0. Same for runtime.reboots (restAPI.ino:2260) and the diag recorder header (restAPI.ino:3806). Impact: a script or Home Assistant that reads bootcount right after a reboot sees a reset to 0. Low.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce: device/info polled from the first HTTP answer after a reboot shows bootcount 0 on the old code
- [ ] #2 Fix: bootcount is correct from the first REST answer (e.g. read the count before startWebserver(), or report null until it is known); same reproduction shows no 0
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-12 bench confirmation (otgw-bench-tester), .61 OTGW32 on 2.0.0-beta.200+f1bb2a0: after POST /ReBoot, a 0.5 s poll of /api/v2/device/info gave (t after the request) 15.1 s -> bootcount 0, uptime 0; 15.8 s -> 433; 16.4 s -> 433. /reboot_count.txt held the real value. So the window is about 0.7 s from the first REST answer; the counter is not reset. The coordinator also saw bootcount 0 once during the 197/200 bench reboots.

Seen again on the beta.217+84c3514 bench (otgw-bench-tester 09, .61, 2026-11-12, 38 reboots under a device/info poller every 5 s): in the first seconds after boot /api/v2/device/info returns "bootcount":0, the real value shows a little later. A client that reads early (HA, scripts) sees a reset counter. Evidence: 09's scratchpad b217/run/boots.jsonl. Relayed by otgw-firmware-95.
<!-- SECTION:NOTES:END -->
