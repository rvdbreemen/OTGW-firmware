---
id: TASK-1600
title: >-
  OT-Direct gateway mode: apply the CH override to the relayed thermostat MsgID
  0 (implement ADR-208)
status: To Do
assignee: []
created_date: '2026-11-04T05:48:00Z'
updated_date: '2026-11-04T05:57:00Z'
labels:
  - otdirect
  - sat
milestone: 2.0.0
dependencies:
  - TASK-1238
priority: medium
ordinal: 741000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Implements ADR-208 (Proposed, TASK-1238 option B): bit 0 of a relayed thermostat READ_DATA MsgID 0 gets otdEffectiveMasterFlags() (OTDirect.ino:761-785), parity recomputed, low byte passes; buildOverriddenReply() gets a MsgID 0 case that returns the boiler's slave status with the thermostat's original high byte (as the PIC statusreturn, gateway.asm:2745-2750).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test in the style of tests/test_otd_ch_override.py: relayed MsgID 0 with CH=0 while a CS override is held goes out with CH=1 (red on today's applyOverrides), reply carries the boiler low byte and the thermostat's original high byte; without an override the frame passes unchanged
- [ ] #2 ADR-208 accepted by Robert before merge (open question: any CS override vs SAT-only answered)
- [ ] #3 esp32-combo build, OTA on the OT-Direct bench unit, no regression in gateway-mode polling
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-04 Robert: ADR-208 accepted (any CS override, entry 264); implementation may start.
<!-- SECTION:NOTES:END -->
