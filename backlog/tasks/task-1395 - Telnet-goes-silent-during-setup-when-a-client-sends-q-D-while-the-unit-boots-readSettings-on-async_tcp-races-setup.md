---
id: TASK-1395
title: >-
  Telnet goes silent during setup() when a client sends q/D while the unit boots
  (readSettings on async_tcp races setup)
status: To Do
assignee: []
created_date: '2026-10-25T18:39:00Z'
updated_date: '2026-11-05T05:51:00Z'
labels:
  - telnet
  - debug
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 534000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From Sergeant D's alpha.505 capture 2026-10-25 19:28 (TASK-1339 notes; analysis wf_a5432f10, verified). The capture script reconnects telnet ~9 s after a reboot and sends 'q' (readSettings(true) + 54 KB settings dump, run on async_tcp holding the telnet mutex, AsyncSimpleTelnet.h:417-431 -> networkStuff.ino:615-617 -> handleDebug.ino:368-370) while setup() still runs; setup() blocks on the telnet mutex (AsyncSimpleTelnet.h:175) and the static _bol prefix is overwritten (spliced line at u.txt:2831). Telnet output then stopped for the rest of the run while the unit kept working. Also a W5500 was found on his unit (not on the bench). Two candidate causes, neither proven. Separate race: 'q' runs readSettings(true) on async_tcp concurrently with setup() (settings struct + _bol).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Bench repro: reboot with the capture script attached so q/D arrive during setup(); telnet silent or not (without a W5500)
- [ ] #2 If reproduced: telnet keeps streaming after a q during setup (red/green), and q does not run readSettings concurrently with setup()
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no: 'q' runs readSettings(true) from async_tcp (handleDebug.ino:364); no setup gate. Repro first.

Bench .61 beta.87+2a9201f 2026-11-05 05:50-06:43 (sonnet agent, evidence scratchpad/bench-b87/, router exports b87-before/after, rules left 0): PASS. GET /ReBoot 05:54:24.8, telnet connected 15.2 s later, 'r' -> 'RX Key 'r' ignored: setup still running, retry after 'Setup finished!'' (device 05:54:38.624); after setup 'r' accepted ('Wifi is connected', 'MQTT is connected').
<!-- SECTION:NOTES:END -->
