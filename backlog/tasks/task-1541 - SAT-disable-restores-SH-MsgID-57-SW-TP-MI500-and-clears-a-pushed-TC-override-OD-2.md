---
id: TASK-1541
title: >-
  SAT disable restores SH= (MsgID 57), SW=, TP=, MI=500 and clears a pushed TC=
  override (OD-2)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-01T08:16:00Z'
updated_date: '2026-11-02T23:38:00Z'
labels:
  - sat
dependencies: []
priority: medium
ordinal: 697000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Decision Robert 2026-11-01 on OD-2 (docs/sat/sat-commissioning-profiles-2026-11-01.md, TASK-1412.17; second read confirmed the gap): satDisable() (SATcontrol.ino ~3359-3415) sends CS=0, CH=1, MM=T but does not restore the installer's MsgID 57 max CH setpoint (SH=), the DHW setpoint (SW=), the Immergas TP= parameter, the MI=500 interval, nor clear a TC= override pushed to the thermostat. Plan: remember the pre-SAT values when SAT is enabled (or read them back from the boiler), restore them in satDisable, clear TC=. Callers to cover per second read: normal disable, MQTT-fallback lift (:6170), flashing (:6314), safety trip (:6790, may run while the command interface is down), boot hand-back (:3549-3550 pushes TC=). The order of the hand-back steps is SAT boiler behaviour: put the plan to Sergeant D before implementing (CLAUDE.md SAT rules).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Plan with the hand-back order posted to Sergeant D and his answer recorded as a numbered entry
- [ ] #2 Host test on the real satDisable: after enable + disable, SH=/SW=/TP=/MI= are restored to the pre-SAT values and TC= is cleared (red on HEAD, green on the fix)
- [ ] #3 Every disable path listed in the description covered or explicitly excluded with a reason
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no: satDisable (SATcontrol.ino:3388+) sends only CS=0, CH=1, MM=T; plan with Sergeant D (no Q queued).

2026-11-03: thermo-nova restores nothing on HVAC off (climate.py:571-583, coordinator/mqtt/__init__.py:66-69); SH=/SW=/MI= sent at coordinator/mqtt/opentherm.py:225/202/172. Plan put to Sergeant D as Q-1541a (A: restore SH/SW from pre-SAT values + TC=0, order CS=0, CH=1, MM=T, SH, SW, TC=0; advice A). Implementation waits for his answer.
<!-- SECTION:NOTES:END -->
