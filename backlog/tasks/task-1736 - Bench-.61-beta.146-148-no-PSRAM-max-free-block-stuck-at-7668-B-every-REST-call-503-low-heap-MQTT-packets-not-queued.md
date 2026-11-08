---
id: TASK-1736
title: >-
  Bench .61 (beta.146-148, no PSRAM): max free block stuck at 7668 B, every REST
  call 503 'low heap', MQTT packets not queued
status: To Do
assignee: []
created_date: '2026-11-08T15:48:00Z'
updated_date: '2026-11-08T15:54:00Z'
labels:
  - bug
  - heap
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 843000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
2026-11-08 ~16:45 local, bench .61 (OTGW32 MAC 10:20:BA:21:B4:F8, no PSRAM, running beta.146 web UI / firmware from TASK-1717-1730 benches). REST device/info and sat/status answer 503 'low heap' for 3+ minutes; ota_flash --webui failed with 503. Telnet shows free heap 31712 B but max free block 7668 B, below the 8192 B REST gate (restAPI.ino:2221/3776, graphHistoryStuff.ino:92), and every MQTT publish fails 'MQTT packet not queued (connected, maxblk=7668)'. SAT keeps running (SATTICK cs=10.0). So the unit stays alive but becomes unmanageable over REST and MQTT: heap fragmentation, not exhaustion. Unknown: uptime, what fragmented it (BLE on a no-PSRAM board? TASK-1693 beta.148 defaults BLE off), whether it recovers. Telnet excerpt saved by the coordinator session.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce or explain: which allocation pattern leaves the largest block < 8192 B with ~31 KB free (heap trace or telnet heap log over time on a no-PSRAM unit)
- [ ] #2 Fix or gate change so a fragmented heap does not lock out REST and MQTT for minutes; red/green evidence
- [ ] #3 Bench: no-PSRAM unit runs 1 h with REST/MQTT available, maxblk logged
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
toegang 2026-11-08: .61 rebooted via GET /ReBoot at 16:51:29 (that route has no heap gate). Right after boot: freeheap 86608 B, maxfreeblock 42996 B (bootcount 167). After a ~15 min bench with two headless Chrome sessions: maxblk 31732 B, min free heap 18376 B. So the 7668 B state built up over a longer uptime (beta.146-148 web UI sessions, TT clicks); the drift rate is still unknown: log maxblk over hours next.

2026-11-08 17:12 local: passive telnet logger started on .61 (sends nothing; reconnects every 10 min for a fresh banner Heap line; the single debugTelnet slot, AsyncSimpleTelnet<1>, is held). Baseline at uptime 21 min, beta.148+4a00ce2: free 74-78 KB, maxBlk 31732, minFree 18376, frag 58%. All debug toggles ON (1-7, g, n), the same as in the 7668 B state; kept on deliberately to reproduce it. Toggles-on is itself a suspect (telnet TX buffering, MQTT debug publish bursts). Logs: telnet.log and heap.csv in the session scratchpad t1736 folder of the logging session.
<!-- SECTION:NOTES:END -->
