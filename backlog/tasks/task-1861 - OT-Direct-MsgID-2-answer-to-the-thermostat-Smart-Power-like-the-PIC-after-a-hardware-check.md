---
id: TASK-1861
title: >-
  OT-Direct: MsgID 2 answer to the thermostat (Smart Power) like the PIC, after
  a hardware check
status: To Do
assignee: []
created_date: '2026-11-12T06:02:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 937000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Split out of TASK-1844 (PIC 6.8 parity N10). The PIC always acknowledges the thermostat's MsgID 2 (master configuration) with its own MemberID and only the Smart Power bit (MessageID2, OTGW-other-projects/otgw-6.8/gateway.asm:2837-2844, 'The gateway will provide Smart Power support, even if the boiler doesn't'; tests/thermostat-3.stc AD002010B). OT-Direct passes the boiler's answer. Acknowledging Smart Power commits the slave interface to medium/high power line levels; the vendored library already sets smartPowerEnabled from the thermostat's request (src/libraries/OpenTherm/src/OpenTherm.cpp:445-446) and has getSmartPowerState() (:540-544), but whether the OT-Direct slave hardware drives those levels was not checked. Decide after that check: answer like the PIC, answer with the bit cleared, or keep the boiler's answer.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The OT-Direct slave hardware's Smart Power capability is established (schematic or bench measurement) and recorded
- [ ] #2 Decision recorded (PIC answer with Smart Power, answer without it, or keep the boiler's answer) with the reason
- [ ] #3 If changed: host test on the real reply path with the PIC frame of thermostat-3.stc; red on the old code
<!-- AC:END -->
