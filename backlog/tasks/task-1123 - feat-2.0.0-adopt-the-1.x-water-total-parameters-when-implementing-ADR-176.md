---
id: TASK-1123
title: 'feat-2.0.0: adopt the 1.x water-total parameters when implementing ADR-176'
status: To Do
assignee: []
created_date: '2026-09-04 06:30'
labels:
  - 2.0.0
  - parity
dependencies: []
priority: medium
ordinal: 274000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
ADR-176 is Accepted on this line but nothing implements it: no source file under src/ references dhw_water_total, WaterTotal or waterTotal. The 1.x peer shipped in v1.7.5 (TASK-1091, TASK-1093, ADR-093, ADR-094), so the 1.x line is now the reference implementation rather than a parallel plan.\n\nTASK-1094 on the 1.x line compared the two and found the entity contract and the accumulation method identical, with the divergences confined to storage, which ADR-176's own Decision Contract already unbinds ('Storage mechanism is explicitly not bound by that mandate').\n\nTwo things the implementer needs to know, neither of which can be fixed by editing ADR-176, because it is Accepted and therefore immutable.\n\nFirst, the interval cap. ADR-176 states the rule ('the usable interval capped so a long sampling gap cannot invent litres') without a number. The 1.x line chose 60000 ms (dhwWaterMeter.ino:38, DHW_METER_MAX_GAP_MS), matching the firmware's own 60 s publish cadence, and accumulates flow * dtMs / 60000. Adopting a different number would make the two firmwares report different totals for the same boiler, which is exactly what ADR-090 Must #2 forbids.\n\nSecond, a premise in ADR-176 changed after it was accepted. Its answered question on partial regression after an unclean reboot states 'Same answer as the 1.x peer', which was true on 2026-08-24 under ADR-090. ADR-093 then superseded ADR-090 and removed persistence from 1.x entirely: the 1.x counter lives in RAM, is not persisted, and has no reset surface, because a reboot zeroes it. So 2.0.0 persisting while 1.x does not is a real difference in the number a user sees after a reboot, deliberate on both lines but no longer symmetrical the way ADR-176 assumed.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The interval cap is 60000 ms, matching dhwWaterMeter.ino:38 on the 1.x line, and accumulation is flow * elapsed / 60000 so both firmwares report the same total for the same boiler
- [ ] #2 The entity contract matches 1.x exactly: device_class water, unit L, state_class total_increasing, published only after a MsgID 19 frame has decoded
- [ ] #3 The divergence from 1.x on persistence and on the reset surface is restated in the implementing task or a new decision record on this line, rather than by editing the Accepted ADR-176
<!-- AC:END -->
