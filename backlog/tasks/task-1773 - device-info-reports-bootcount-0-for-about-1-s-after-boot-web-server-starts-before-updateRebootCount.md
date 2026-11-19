---
id: TASK-1773
title: >-
  device/info reports bootcount 0 for about 1 s after boot (web server starts
  before updateRebootCount())
status: Done
assignee:
  - '@claude'
created_date: '2026-11-10T04:17:00Z'
updated_date: '2026-11-16T08:38:00Z'
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
- [x] #1 Reproduce: device/info polled from the first HTTP answer after a reboot shows bootcount 0 on the old code
- [x] #2 Fix: bootcount is correct from the first REST answer (e.g. read the count before startWebserver(), or report null until it is known); same reproduction shows no 0
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-12 bench confirmation (otgw-bench-tester), .61 OTGW32 on 2.0.0-beta.200+f1bb2a0: after POST /ReBoot, a 0.5 s poll of /api/v2/device/info gave (t after the request) 15.1 s -> bootcount 0, uptime 0; 15.8 s -> 433; 16.4 s -> 433. /reboot_count.txt held the real value. So the window is about 0.7 s from the first REST answer; the counter is not reset. The coordinator also saw bootcount 0 once during the 197/200 bench reboots.

Seen again on the beta.217+84c3514 bench (otgw-bench-tester 09, .61, 2026-11-12, 38 reboots under a device/info poller every 5 s): in the first seconds after boot /api/v2/device/info returns "bootcount":0, the real value shows a little later. A client that reads early (HA, scripts) sees a reset counter. Evidence: 09's scratchpad b217/run/boots.jsonl. Relayed by otgw-firmware-95.

Research 2026-11-14 (ba, read-only, origin/dev 753d45581): the cause is gone on current code as a side effect of TASK-1909 (b14cce850, beta.231, in v2.0.0-beta.237 and beta.250): setup() now runs state.uptime.iRebootCount = updateRebootCount() (OTGW-firmware.ino:546) and updateRebootLog() (:547) before setupFSexplorer() and startWebserver() (:548-549). startWebserver() is the first place the REST API can answer, so device/info bootcount (restAPI.ino:3413), runtime.reboots (:2261) and the diag recorder header (:3823) all read the counted value from the first answer on. Old-vs-fix: the old reproduction is 09's (beta.200: 15.1 s bootcount 0, 15.8 s 433; beta.217: 38 reboots, 0 seen early); the green half is a bench run on a build with TASK-1909 (the same 0.5 s device/info poll after POST /ReBoot, about 10 reboots, no bootcount 0). Added to the 1937/1850/1778/1947 stack bench as item 7, no code change needed unless that run still shows a 0.

2026-11-15 ba: verified on current origin/dev that the TASK-1909 order still holds: OTGW-firmware.ino:553 state.uptime.iRebootCount = updateRebootCount(), :555 setupFSexplorer(), :556 startWebserver(); the port-80 listener starts only in startWebserver() (FSexplorer.ino:240) and its retry handleWebserverListener() (:261, from loop()). Regression gate added: tests/test_bootcount_before_webserver.py reads the REAL setup() and FSexplorer.ino: OK 2/2 on the current code; against b14cce850^ (the order of the beta.200 bench) FAILED 'count line 297 not less than startWebserver line 272'. No firmware change. AC1 (old reproduction) is 09's beta.200/beta.217 evidence above; AC2 needs the green bench run (0.5 s device/info poll after POST /ReBoot, about 10 reboots, no bootcount 0) on any build with TASK-1909, which is every current build.

2026-11-16 09:19-09:26 bench 09 on .61 (OT-Direct), 2.0.0-beta.341+3cb72c3 (origin/dev 82f355917 code, contains b14cce850): 10 x POST /ReBoot, device/info polled every 0.5 s from the first HTTP answer for 20 s (31-33 samples per cycle). First answer 15.1-15.9 s after the request; bootcount values per cycle exactly one, the new count (816, 817, ... 825), bootcount 0 seen in 0 of 10 cycles (0 of ~325 samples); lastreset 'Software reset' each time. Evidence: 09 scratchpad closeout/reboot/ (reboot-192.168.88.61.log, cycle-192.168.88.61-*.json, rebootrun.py).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1 (old code): 09 on .61 beta.200+f1bb2a0 (2026-11-12): a 0.5 s poll after /ReBoot read bootcount 0 at 15.1 s, 433 at 15.8 s; beta.217 showed the same over 38 reboots. AC2 (fix): cause removed by TASK-1909 (b14cce850): setup() sets iRebootCount = updateRebootCount() before startWebserver() (OTGW-firmware.ino:553 vs :556); gate tests/test_bootcount_before_webserver.py passes on the current code, fails on b14cce850^. Same reproduction on beta.341+3cb72c3 (.61, 2026-11-16): 10 reboots, 0.5 s poll from the first answer, bootcount 0 in 0 of 10 cycles, each cycle showed only the new count. Not checked on the Classic (.63) with this exact cadence; its 5 reboot cycles (TASK-1877 run, 0.2 s poll) also showed no 0.
<!-- SECTION:FINAL_SUMMARY:END -->
