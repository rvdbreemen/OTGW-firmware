---
id: TASK-1298
title: >-
  SAT follow-thermostat adopts a thermostat setpoint change late and misses the
  next one (Honeywell Round, alpha.454)
status: To Do
assignee: []
created_date: '2026-10-22T12:00:00Z'
labels:
  - sat
  - bug
  - thermostat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 423000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Report gruijter 2026-10-22 08:03 + log bundle 09:25 (#alpha-testing; %TEMP%\discord-1022, mqtt.log line numbers as clock, no timestamps). Follow thermostat and push setpoint both on. TrSet (MsgID 16, the Honeywell's own setpoint) went 15.00 (l.1250) -> 19.50 (13166) -> 20.00 (14396) -> 19.50 (16082) -> 20.00 (26545). sat/target changed once: 20.5 -> 20.0 at l.17049, i.e. after the 20.00 at 14396 and after the 19.50 at 16082, and never adopted the 19.50. Right after, SAT pushed TC=20.0 (TrOverride 20.00 at 17095). The thermostat clears the override repeatedly (TrOverride 20.00 -> 0.00 at 15409, 19407, 24044, 61163; 20 TC: event_reports), the TASK-1272 pattern. The user saw: dial change on the Honeywell shows in the monitor graph but the SAT dial did not move (it moved once, to 20.0, not to 19.5); a target set in the SAT GUI does not stick on the Honeywell. Analyse satFollowThermostatDecide() (TASK-1259: two-frame debounce + the synced rule |prev - lastTC| <= 0.25 while push is on) against this sequence with a host test that replays exactly these MsgID 16/9 values; decide with Sergeant D (this is our own feature, not in thermo-nova) how follow and push should interact on a thermostat that drops the override, together with TASK-1272. Note: the telnet part of the capture stopped at the upgrade reboot, so no SAT debug lines exist for this run.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A host test replays the logged MsgID 16/9 sequence through the real decide function and shows which changes are adopted and why (red = the 19.50 is missed)
- [ ] #2 Behaviour agreed with Sergeant D and recorded in the task notes, with the spec citation if thermo-nova has an equivalent
- [ ] #3 Bench or replay evidence old vs fix; gruijter confirms on the Honeywell Round
<!-- AC:END -->
