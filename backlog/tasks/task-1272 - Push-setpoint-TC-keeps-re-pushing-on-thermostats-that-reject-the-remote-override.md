---
id: TASK-1272
title: >-
  Push setpoint (TC=) keeps re-pushing on thermostats that reject the remote
  override
status: To Do
assignee: []
created_date: '2026-10-20 18:17'
labels:
  - sat
  - ux
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 402000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
First real SAT field run (gruijter, Intergas HReco 30 + Honeywell Round + PIC Classic, alpha.439/441, #alpha-testing 2026-10-20), captures in %TEMP%\discord-1020. SAT pushes the user target correctly (TC=20.0, TC=20.5 x2 at 11:38:50/11:41:50/11:46:50); the PIC answers MsgID 9 = 20.5 (A40091480), but the Honeywell Round keeps sending TrSet 19.0 and the PIC cancels the override after 3 mismatches (otgw-6.6 gateway.asm ~3026 'Make 3 attempts', roomsetptcancel): AC0090000. SAT re-pushes every 5 min (SAT_CMD_REFRESH_MS), so the override flaps ~3 min on / ~2 min off. Not a value bug.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 SAT detects that the override was not adopted (MsgID 9 back to 0 while TrSet != target), stops re-pushing and shows a status note in the UI
- [ ] #2 Docs: TC= push only works on thermostats that accept a remote room-setpoint override
<!-- AC:END -->
