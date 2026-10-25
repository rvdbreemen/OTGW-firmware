---
id: TASK-1375
title: 'SAT: gas boiler PWM cycles per hour 3 or 4 only, default 3 (entries 30, 52)'
status: To Do
assignee: []
created_date: '2026-10-25T07:21:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
ordinal: 510000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D entry 52 (2026-10-25, answer to Q16): default 3 cycles per hour for gas boilers, 2 for heat pumps. Entry 30: gas boilers use 3 or 4. Firmware today: iCyclesPerHour default 3 (SATtypes.h:548), accepted 2..6 (settingStuff.ino:1107). Change: for gas boilers accept 3 or 4 only (clamp stored values outside into range on load), UI offers 3/4; heat pump values are TASK-1366. Hybrids: open (entry 52).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Setting accepts only 3 or 4 for gas boilers; other values clamp to the nearest; default stays 3
- [ ] #2 v2 UI offers 3 and 4 for gas boilers
- [ ] #3 Host test shows HEAD accepts 2/5/6 and the fix rejects them
<!-- AC:END -->
