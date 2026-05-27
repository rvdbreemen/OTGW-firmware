---
id: TASK-733
title: 'fix(otdirect): SC= clock command missing MsgID 21/22 and expiry timer'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-27 18:59'
updated_date: '2026-05-27 19:58'
labels: []
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The PIC SC= command sends MsgID 20 (day/time), MsgID 21 (date), and MsgID 22 (year) together, and the clock override expires after 61 seconds if the command is not repeated. OTDirect only sends MsgID 20, never 21 or 22, and has no expiry timer. Boilers with date/year-based weekly programs will not sync correctly. After a controller crash the firmware stays in permanent clock-override state.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 SC= handler sends MsgID 20, 21, and 22 (day+time, date, year)
- [ ] #2 A 61-second timer is started on SC=; if not refreshed the clock-override is cleared
- [ ] #3 Repeated SC= within the 61s window resets the timer
- [ ] #4 Build clean, evaluate.py --quick green
- [ ] #5 Manual test: verify MsgID 21 and 22 frames appear on the OT bus after SC=
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SC= clock command handler verbeterd: 1) 61s expiry timer (otSCLastCommandMs + OT_SC_EXPIRY_MS = 61000) toegevoegd, gemodelleerd naar CS=/C2= heartbeat patroon. Expiry check in doOTDirectLoop. 2) synthesizeResponse toegevoegd die HH:MM/DOW teruggeeft (gelijk aan PIC SetClock die clock1/clock2 echoet). MsgID 21/22 zijn NIET geimplementeerd — gateway.asm SetClock raadt deze berichten niet aan, uitsluitend MsgID 20.
<!-- SECTION:FINAL_SUMMARY:END -->
