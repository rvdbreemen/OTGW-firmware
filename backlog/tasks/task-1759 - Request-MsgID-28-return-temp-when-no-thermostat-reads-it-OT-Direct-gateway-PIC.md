---
id: TASK-1759
title: >-
  Request MsgID 28 (return temp) when no thermostat reads it (OT-Direct gateway
  + PIC)
status: To Do
assignee: []
created_date: '2026-11-09T17:08:00Z'
labels:
  - sat
  - opentherm
  - ot-direct
  - pic
dependencies: []
priority: high
ordinal: 860000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D, 2026-11-09 16:09 UTC, #dev-sat-mqtt msg 1569383329470488710: "We need to ask for return temperature" (docs/sat/sergeant-d-corrections.md entry 287). tjfs's capture of 2026-11-09 (beta.161, OT-Direct gateway mode, Ideal Halo Lite thermostat, Ideal Logic boiler, SAT on) holds no MsgID 28 frame: the thermostat reads only IDs 0, 1, 16, 17, 24, 25, the boiler supports all of them, and since TASK-1707 (beta.132) the gateway sends its own reads only in the slot of an unsupported thermostat request, so it never reads ID 28; SAT's tick shows ret=0.0. thermo-nova needs it: the PWM flame-off hold sets CS = return temperature + flame-off offset and skips the override with a warning when the return temperature is missing (heating_control.py:392-399); the cycle tracker uses flow minus return (cycles/tracker.py:262-265). Counterpart of entry 276 (MsgID 25 on PIC boards). Related TASK-1707.

Open questions before code, to Sergeant D per the SAT rules (sat-ground-truth, open-questions.md): the method (on OT-Direct gateway mode which slot; on PIC the equivalent of entry 276's own request), the interval, and SAT-only or always.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Open questions (method, interval, SAT-only or always) asked to Sergeant D and his answers recorded as entries in docs/sat/sergeant-d-corrections.md before any code
- [ ] #2 OT-Direct gateway mode: the gateway reads MsgID 28 when no thermostat reads it, as decided; shown in a test and in a bench or tester capture
- [ ] #3 PIC boards: the same, as decided; shown in a test and in a capture
- [ ] #4 SAT sees a valid return temperature on such an installation (tick ret != 0.0) and the flame-off hold uses it
<!-- AC:END -->
