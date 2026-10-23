---
id: TASK-1314
title: >-
  PIC boards: refresh the return water temperature from a wired sensor every
  minute instead of every ~5 minutes
status: To Do
assignee: []
created_date: '2026-10-23T09:26:00Z'
labels:
  - pic
  - opentherm
  - sensor
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 437000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Maintainer directive (number3nl, #alpha-testing 2026-10-23 08:46): 'do as gruijter ask, go from 5 min injection to 1 min injection of Tret when using a wired sensor that you read out ... once a minute'. Context: gruijter (OTGW Classic + PIC, Intergas + Honeywell Round) sees Tret (MsgID 28) only every ~5 minutes and the DHW setpoint even less often; his boiler does not send Tret, the value comes from a temperature sensor wired to the OTGW board (PIC temperature-sensor function, TS=R). Earlier the maintainer suggested checking whether the PIC priority-message command PM= can force a refresh every minute. Not a SAT control-behaviour change (data freshness), but Tret feeds SAT, so faster updates help it.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Root cause of the ~5 min cadence documented from the PIC reference (other-projects/otgw-6.6 source/docs, cited) and from gruijter's capture (frame types and intervals of MsgID 28)
- [ ] #2 On PIC boards with the sensor configured as return water, the firmware gets a fresh Tret about every 60 s (mechanism chosen from the evidence, e.g. PM=28 once a minute), without adding PIC commands when the board has no such sensor
- [ ] #3 Bench or replay evidence old vs fix of the MsgID 28 interval; no regression in the PIC command queue (no extra traffic on boards without the sensor)
<!-- AC:END -->
