---
id: TASK-1259
title: >-
  SAT target follows the thermostat setpoint (MsgID 16 TrSet), two-way with
  push-setpoint
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-19 17:33'
updated_date: '2026-10-21 17:49'
labels:
  - feature
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 391000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Feature request by gruijter 2026-10-19, endorsed by Sergeant D. Today SAT only pushes its target to the thermostat (satpushsetpoint, TC= command, SATcontrol.ino ~407-415); a setpoint change on the wall thermostat is ignored, so the thermostat is useless as UI. Request: opt-in 'SAT target follows thermostat setpoint' that takes MsgID 16 TrSet as SAT target when it changes. Must avoid a feedback loop with satpushsetpoint (TC= echo). Sergeant D's rule: thermostat syncs only the room setpoint; with SAT active SAT drives the boiler; on SAT inactive/malfunction fall back to the thermostat. Open: thermostat may send CH enable off when it thinks the room is warm, decide who wins.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Design agreed (option name, loop guard, CH-enable arbitration)
- [ ] #2 Thermostat setpoint change is adopted as SAT target within one TrSet cycle, and a SAT target change pushed via TC= does not bounce back (bench or simulator evidence)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. New opt-in setting SATfollowthermostat / settings.sat.bFollowThermostat (default OFF), wired like overshoot_protection: settings.ini, REST GET/POST + knownSettings, SAT REST route follow_thermostat, MQTT sat/follow_thermostat (+ _enable state), HA switch, v2 UI, docs.
2. Source: the thermostat's own room setpoint, MsgID 16 TrSet Write-Data from the thermostat (T frame; PIC and OT-Direct), already stored in OTcurrentSystemState.TrSet. Valid range 5-30 C (same as satHandleTargetTemp).
3. Edge-triggered (Sergeant D 2026-10-19): remember the last TrSet seen; when a new value differs by >= 0.1 C, adopt it through satHandleTargetTemp() (same path as REST/MQTT target, so presets/PID handling stay as they are). The first TrSet after boot or after enabling the option is only recorded (no jump to an old thermostat value).
4. Loop guard with push-setpoint (TC=): a TrSet equal (+-0.05) to the last TC= value SAT pushed is SAT's own echo and is ignored, so a SAT target change does not bounce back. TC= keeps working as today.
5. CH enable (maintainer 2026-10-20): while SAT is active and healthy SAT keeps control (its own CS/CH commands); when SAT is off or faulted, SAT releases CS (CS=0, existing) and the thermostat applies again. No new code expected; verify on the bench.
6. Out of scope: syncing the SAT target back to a thermostat that rejects TC= (TASK-1272).
Verification: host test of the edge/echo logic compiled from the real function (red on HEAD: no follow); bench replay with thermostat T frames TrSet 20.0 -> 21.5 (SAT target follows within one frame cycle) and with push-setpoint on, SAT target 21.0 -> TC=21.0 -> replayed TrSet 21.0 not adopted again.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-19 Sergeant D (#alpha-testing): adopt the thermostat setpoint only on a room setpoint CHANGE (edge-triggered), not by periodic sync. CH-enable arbitration question still open.

2026-10-20 maintainer decision: CH enable from the thermostat is ignored while SAT is active and healthy (SAT wins); when SAT is off or faulted the thermostat applies again.

2026-10-21 maintainer: pick it up now (gruijter calls it essential for the Wife Acceptance Factor, #alpha-testing 2026-10-21). Plan goes to Sergeant D first (SAT control behaviour rule).

2026-10-21 plan posted to Sergeant D in #alpha-testing (message 1562523233524121862) with three open points: default on with push-setpoint?, clear an active preset on a thermostat change?, minimum time between adopted changes? Waiting for his OK before implementing; cycle continues with other tasks.
<!-- SECTION:NOTES:END -->
