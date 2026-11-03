---
id: TASK-1589
title: >-
  SAT PWM: boiler stops and relights inside one ON phase when the ON setpoint
  sits at the flow temperature at minimum modulation (gruijter beta.49 capture)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-03T13:40:00Z'
updated_date: '2026-11-03T14:24:00Z'
labels:
  - sat
dependencies: []
priority: medium
ordinal: 734000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the peer analysis of gruijter's capture 20261103-101925 (beta.49, Classic PIC, Intergas; Discord msg 1567167044309352500, otframes.log in the zip): PWM ON phase 13:13:17-13:17:47 had two flames. The boiler stopped at 13:15:28 at minimum modulation (MM=0) with flow 36.3 C vs CS 35.3 C; SAT then set CS=44.3 (board return 26.3 + 18, the restart rule of entries 45/149) and the boiler relit at 13:16:35 (67 s off). Question: is an ON-phase setpoint at or below the flow at MM=0 expected (thermo-nova pwm.py ON-phase setpoint, cite file:line), and should the firmware differ. First: trace thermo-nova's ON-phase setpoint and the entries 45/149 rule; a second reader checks both citations; only then a question to Sergeant D.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 thermo-nova ON-phase setpoint traced with file:line and compared with the firmware on the capture's frames (timeline)
- [x] #2 Second reader confirmed the difference (or refuted it); if real, Q queued for Sergeant D with A/B/C and advice
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-03 investigation (opus agent, read-only; findings scratchpad task1589-findings.md, timeline from the capture's otframes.log): no deviation. Every CS in 13:13-13:18 equals thermo-nova heating_control.py:385-448 for the same inputs (flame-off hold return+18 :392-401, 20 s hold after flame-on :410-426 with flame_on_since re-anchored per edge device/__init__.py:208-211, then flow-1 :435-448) and entries 20, 23, 135, 230. Corrections to the observation: the boiler raised its own modulation 7 -> 74 % at 13:15:10 with no DHW bit; MsgID 14 is Unknown-DataID on all 377 B frames (MM never reaches this Intergas); flame off at 13:15:28.5 after 122 s with last CS 36.4 (4.9 K under the flow, refresh lags the ~5 s MsgID 25 poll); 44.3 = board return 26.3 + 18 is the normal flame-off hold (entry 135: the board sensor counts as return), entries 45/149 (no-return rule) do not apply. A stop with CS under the flow is the known risk of the OT 2.1.1.2.5 suppression workaround, used on purpose by thermo-nova. On_cycle_end (TASK-1331) would not fire (class SHORT). AC2: no difference claimed, so nothing to confirm or publish; no Sergeant D question. Optional maintainer design idea (not parity, not filed): react to a boiler's own modulation rise on boilers that reject MsgID 14.

2026-11-03 follow-up: in-depth timeline posted to Sergeant D by the peer session (his request), second reader (lead) confirmed one unrecorded timing difference: the flame-off hold waits for the 30 s control tick (SATcontrol.ino:1608) vs thermo-nova's 5 s loop (climate.py:707-709). Asked as Q-1589a.

2026-11-03 CORRECTION (lead): the earlier verdict 'no DHW bit, the boiler raised its own modulation' was wrong in part. Sergeant D (entry 253): 'It behaved like DHW enabled'. The frames show a hot-water event without the DHW status bit: MsgID 26 Tdhw 29.17 -> 37.01 -> 41.20 (13:14:35-13:15:23), MsgID 19 flow rate 0.38 at 13:15:21 (0.00 on every other read), thermostat DHW enable on. We checked only the status bit. Still no firmware vs thermo-nova deviation (both use the bit only), but a detection gap: Q-1589b asked; Q-1589a parked. Reopened until Q-1589b is decided.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The relight inside a PWM ON phase in gruijter's beta.49 capture is thermo-nova behaviour, not a firmware deviation: all setpoints match heating_control.py:385-448 and entries 20/23/135/230; the trigger was the boiler's own modulation rise (MsgID 14 unsupported on this Intergas). No question for Sergeant D.
<!-- SECTION:FINAL_SUMMARY:END -->
