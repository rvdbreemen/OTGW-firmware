---
id: TASK-1298
title: >-
  SAT follow-thermostat adopts a thermostat setpoint change late and misses the
  next one (Honeywell Round, alpha.454)
status: To Do
assignee: []
created_date: '2026-10-22T12:00:00Z'
updated_date: '2026-10-22T12:46:00Z'
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-22 12:05 correction from gruijter: the sat/target 20.5 -> 20.0 at mqtt.log l.17049 was HIS manual change in the SAT web GUI, not follow-thermostat. So in this run follow-thermostat adopted nothing: TrSet 15.00 -> 19.50 -> 20.00 -> 19.50 -> 20.00 produced no target change at all (follow_thermostat_enable true at l.12370, push_setpoint_enable true at l.131). The earlier 'followed once, late' reading in the description is wrong; the defect is stronger: no follow while push is on and the thermostat keeps dropping the override.

2026-10-22 root cause (code, SATcontrol.ino:1064-1081, TASK-1259 rule): with push on, a confirmed edge is adopted only when synced = (a TC= was sent since boot) AND |previous TrSet - lastSentTC| <= 0.25, and not an echo (|new - lastSentTC| <= 0.25). The rule assumes a thermostat that mirrors an accepted override in its own MsgID 16. The Honeywell Round does not: in the log TrSet stayed 15.00/19.50/20.00 while TrOverride was 20.50 (l.13030-15409) and it dropped the override repeatedly. Replay of the log against the rule: 15->19.5 prev 15 vs TC 20.5 (not synced); 19.5->20.0 prev 19.5 (not synced); 20.0->19.5 prev 20.0 (0.5, not synced); after the manual 20.0 (TC 20.0): 19.5->20.0 prev 19.5 (not synced) and new == TC (echo). So nothing is ever adopted on this thermostat while push is on. Proposal (SAT behaviour, needs Sergeant D / maintainer OK, this feature is firmware-only): keep the boot guard (a TC= sent since boot) and the echo guard, drop the 'previous value synced' precondition, so an edge that differs from the last push by more than 0.25 C is adopted even on a thermostat that never mirrors the override; the PIC-reset case (override dropped, thermostat back on its schedule) then adopts the schedule value once, which is what the user sees on the dial anyway. Alternative: detect 'mirrors override' per thermostat and apply the synced rule only there (more state, same outcome on the Round).
<!-- SECTION:NOTES:END -->
