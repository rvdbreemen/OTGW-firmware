---
id: TASK-1304
title: >-
  ADR-022 lists the GPIO output trigger bits as master bits 0-7; the code tests
  the slave byte at 0-7 (erratum or superseding ADR)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-22T20:27:00Z'
updated_date: '2026-10-24T18:18:00Z'
labels:
  - docs
  - adr
dependencies: []
priority: low
ordinal: 427000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the TASK-1295 review (2026-10-22). docs/adr/ADR-022-gpio-output-bit-flag-control.md lines 35-43 and 61-67 describe 'OpenTherm status flags available' as bit 0 = CH enable .. bit 6 = DHW blocking (the master byte) and explain the default 0 as 'CH enable'. The firmware has always tested OTcurrentSystemState.Statusflags = (MasterStatus << 8) | SlaveStatus (OTGW-Core.ino:2461, outputs_ext.ino:85), so bits 0-7 are the slave (boiler) bits: 0 fault, 1 CH active, 2 DHW active, 3 flame; the master bits are 8-15. Accepted ADRs are immutable, so this needs an erratum note or a short superseding ADR via adr-kit (unreachable in the 2026-10-22 session). The 1.x copy of ADR-022 has the same text. c4-code-sensors.md and the outputs_ext.ino comment were corrected under TASK-1295.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 An ADR (erratum or superseding) states the real bit layout with citations to OTGW-Core.ino and the OT spec MsgID 0
- [ ] #2 ADR-022 is marked Superseded or carries the erratum reference per adr-kit rules
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-24: ADR-200 (Proposed) supersedes ADR-022 on acceptance: the trigger bit indexes Statusflags = (MasterStatus<<8)|SlaveStatus; bits 0-7 = boiler (slave) byte, 8-15 = thermostat (master) byte (CH enable = bit 8, DHW blocking = bit 14). Citations: outputs_ext.ino:87 (test), OTGW-Core.ino:2943/:2534/:4665/:4792 (writers), spec v4.2 5.3.1 lines 1431-1452 and 5.4 line 2341. Second reader: 3 corrections, fixed. adr-lint: ADR-200 no FAIL (ADVISORY only). AC2 (ADR-022 marked Superseded) waits for acceptance = Robert's explicit OK.
<!-- SECTION:NOTES:END -->
