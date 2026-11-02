---
id: TASK-1570
title: >-
  SAT: send MM, TP=11:12 and CH only on change, for every boiler; CS keeps the 5
  s resend (Sergeant D entry 237)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-02T20:56:00Z'
updated_date: '2026-11-02T20:57:00Z'
labels:
  - sat
dependencies: []
priority: high
ordinal: 717000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D entry 237 (2026-11-02, answers to Q-1537c): MM= (and for an explicit Immergas its TP=11:12=min(MM,80)) and CH= go to the PIC only when the value changes, for every boiler; CS= keeps the 5 s resend and the PIC keepalive. Changes entry 44 / ADR-203 for MM and CH. Firmware today: satSendMM()/satSendCH() enqueue on every control tick (SATcontrol.ino ~6862), on the 5 s satCmdResend() (:694-702) and on the hot-water/cutoff paths (:6615/6645/6661). PIC keeps MM= and CH= until a new command (gateway.asm:4752-4764; only SC/CS/C2 expire, :493-496, :1619-1641), so on-change is safe as long as a lost value is resent: SAT taking control, OT link back, PIC reset, reboot (satResetCmdCache).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test on the real satSendMM/satSendCH/satCmdResend: an unchanged MM or CH is not enqueued again by the tick or the 5 s resend; a changed value is enqueued once; Immergas TP=11:12 follows MM; CS is still resent every 5 s (red on beta.17, green)
- [ ] #2 After a command-cache reset (SAT takes control, OT link back, PIC reset detected, reboot) the next send enqueues MM, CH (and the Immergas TP) once even if unchanged; the PIC reset path is identified in the code and covered by the test
- [ ] #3 A Proposed ADR superseding ADR-203 for MM and CH (CS unchanged), citing entry 237; the maintainer accepts it
- [ ] #4 Bench .63 telnet: MM/CH/TP enqueue counts per minute before (beta.17) and after; CS still every 5 s; settings kept, stable 5 min
<!-- AC:END -->
