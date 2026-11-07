---
id: TASK-1707
title: >-
  OT-Direct gateway mode: own reads and commands only in alternative slots, as
  on the PIC (follow-up of TASK-1701)
status: To Do
assignee: []
created_date: '2026-11-07T17:50:00Z'
labels:
  - otdirect
dependencies: []
priority: medium
ordinal: 819000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-1701 stopped the gateway's own MsgID 0 while a thermostat is connected (hotfix, Robert 2026-11-07). The rest of the master scheduler still runs between the thermostat's frames (tjfs log: 634 own frames against 369 thermostat frames in 6 min). The PIC sends exactly one boiler frame per thermostat frame and uses its own reads and commands only in slots where the thermostat asks an ID the boiler does not support, answering the thermostat itself (gateway.asm:2384-2414 CreateRequest/SendAltRequest, 3390-3442 Alternative/SendPriorityMsg); frames of its own only without a thermostat (gateway.asm:1770-1816 StandAloneMsg). Make OT-Direct gateway mode follow that model. Consequence to weigh: values the thermostat never reads arrive slower, as on the PIC.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 In gateway mode with a thermostat connected the boiler receives exactly one frame per thermostat frame (host test over a recorded thermostat stream)
- [ ] #2 Gateway reads and queued commands go out only in slots of thermostat requests for IDs the boiler does not support, and the thermostat gets the PIC's reply for such a slot
- [ ] #3 Without a thermostat the gateway runs its own schedule as today
- [ ] #4 Bench with a thermostat on an OT-Direct unit, and the MQTT values that depend on gateway reads checked
<!-- AC:END -->
