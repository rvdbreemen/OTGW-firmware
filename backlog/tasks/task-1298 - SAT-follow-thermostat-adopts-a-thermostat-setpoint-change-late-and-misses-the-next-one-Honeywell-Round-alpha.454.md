---
id: TASK-1298
title: >-
  SAT follow-thermostat adopts a thermostat setpoint change late and misses the
  next one (Honeywell Round, alpha.454)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-22T12:00:00Z'
updated_date: '2026-10-22T16:51:00Z'
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
- [x] #1 A host test replays the logged MsgID 16/9 sequence through the real decide function and shows which changes are adopted and why (red = the 19.50 is missed)
- [x] #2 Behaviour agreed with Sergeant D and recorded in the task notes, with the spec citation if thermo-nova has an equivalent
- [ ] #3 Bench or replay evidence old vs fix; gruijter confirms on the Honeywell Round
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1) SATcontrol.ino satFollowThermostatDecide(): with push on, return (lastSentTC > 0) && !echo instead of synced && !echo; update the comment block (TASK-1259 rule text). 2) tests/test_sat_follow_thermostat.py: replay gruijter's logged sequence (TrSet 15 -> 19.5 -> 20.0 -> 19.5 -> 20.0 with TC 20.5 then 20.0, two frames each) through the real function: red on HEAD (nothing adopted), green after (19.5, 20.0, 19.5 adopted; the 20.0 after the manual TC=20.0 stays an echo); adjust the existing cases that encoded the synced rule (boot-before-first-push, PIC reset) to the new rule and keep the echo and debounce cases. 3) docs: ch05/h05 follow-thermostat paragraph, docs/sat/firmware-sat-spec.md area 6 row, sat-comparison.md row if it cites the rule, CHANGELOG Fixed. 4) Bench: /simulate replay fixture with push on where the thermostat T frames change MsgID 16 while the override is not mirrored; sat/target must follow on the new build and not on alpha.457 (MQTT capture). 5) Report to gruijter.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-22 12:05 correction from gruijter: the sat/target 20.5 -> 20.0 at mqtt.log l.17049 was HIS manual change in the SAT web GUI, not follow-thermostat. So in this run follow-thermostat adopted nothing: TrSet 15.00 -> 19.50 -> 20.00 -> 19.50 -> 20.00 produced no target change at all (follow_thermostat_enable true at l.12370, push_setpoint_enable true at l.131). The earlier 'followed once, late' reading in the description is wrong; the defect is stronger: no follow while push is on and the thermostat keeps dropping the override.

2026-10-22 root cause (code, SATcontrol.ino:1064-1081, TASK-1259 rule): with push on, a confirmed edge is adopted only when synced = (a TC= was sent since boot) AND |previous TrSet - lastSentTC| <= 0.25, and not an echo (|new - lastSentTC| <= 0.25). The rule assumes a thermostat that mirrors an accepted override in its own MsgID 16. The Honeywell Round does not: in the log TrSet stayed 15.00/19.50/20.00 while TrOverride was 20.50 (l.13030-15409) and it dropped the override repeatedly. Replay of the log against the rule: 15->19.5 prev 15 vs TC 20.5 (not synced); 19.5->20.0 prev 19.5 (not synced); 20.0->19.5 prev 20.0 (0.5, not synced); after the manual 20.0 (TC 20.0): 19.5->20.0 prev 19.5 (not synced) and new == TC (echo). So nothing is ever adopted on this thermostat while push is on. Proposal (SAT behaviour, needs Sergeant D / maintainer OK, this feature is firmware-only): keep the boot guard (a TC= sent since boot) and the echo guard, drop the 'previous value synced' precondition, so an edge that differs from the last push by more than 0.25 C is adopted even on a thermostat that never mirrors the override; the PIC-reset case (override dropped, thermostat back on its schedule) then adopts the schedule value once, which is what the user sees on the dial anyway. Alternative: detect 'mirrors override' per thermostat and apply the synced rule only there (more state, same outcome on the Round).

2026-10-22 14:01 Sergeant D (#alpha-testing 1562828251364069377): he owns no OpenTherm thermostat and defers the follow/push decision to Robert. Decision needed from the maintainer: (A) drop the 'previous value synced' precondition (keep the since-boot push guard and the echo guard), or (B) per-thermostat 'mirrors override' detection. Until then the workaround for gruijter is push off + follow on.

2026-10-22 maintainer decision (Robert, in session): option A. Drop the 'previous value synced' precondition; keep the since-boot push guard (lastSentTC > 0) and the 0.25 C echo guard. Sergeant D deferred (no OT thermostat).

2026-10-22 review round 1 (21 agents): my first fix added a 'previous push' echo guard (fPrevSentTC) beyond the recorded decision. Three reviewers and six verifiers reproduced through the compiled function that the control loop pushes every adopted value as TC= on the next tick (SATcontrol.ino ~4999), so after adopting 19.5 and 20.0 the dial's return to 19.5 equals the previous push and was swallowed: the exact AC1 miss, hidden by a test that omitted those pushes (also an A->B->A regression vs HEAD on mirroring thermostats, and an untested write in satEnqueueIfChangedTC). Fixed by removing fPrevSentTC entirely (pure option A): the late-reply case is no edge anyway, because the older mirrored value is already the recorded one. Tests now model the loop's pushes (gruijter replay with tc= after each adoption: 19.5, 20.0, 19.5 adopted), add A->B->A and the realistic mirroring late reply; 35/35 on the fix, HEAD copy fails the gruijter replay and scenario 3b. Docs/test-header finding refuted (already updated); dead 'prev' local removed. Bench RED (alpha.456, old rule): replayed TrSet 15 -> 19.5 -> 20.0 -> 19.5, sat/target stayed 20.5 (%TEMP%\bench-1298\red-mqtt.txt). Bench run of the first fix in progress as the bench-side reproduction of the review finding.

2026-10-22 bench, first fix (alpha.457+a0e0936 with the fPrevSentTC guard), same replay: sat/target 20.5 -> 19.5 -> 20.0 -> (19.5 missed) -> 15.0 (%TEMP%\bench-1298\green-mqtt.txt renamed firstfix): the review finding reproduced on the device. The 'pushed since boot' guard was met (adoptions happened), so SAT did push TC= on enable; the 45 s telnet capture after enable shows no new TC= because the refresh is 300 s.

2026-10-22 GREEN (final build alpha.457+a0e0936 without fPrevSentTC, safeboot OTA, same replay fixture %TEMP%\bench-1298\trset_replay.log: TrSet 15 -> 19.5 -> 20.0 -> 19.5 -> 15 held 12 quads each, push on, follow on, satsimulation on, target 20.5): sat/target 20.5 -> 19.5 -> 20.0 -> 19.5 -> 15.0, every change followed (%TEMP%\bench-1298\green-mqtt.txt). RED (alpha.456, old rule): sat/target stayed 20.5 (red-mqtt.txt). First fix: 19.5 missed (firstfix-mqtt.txt). Host test: 35/35 on the fix, HEAD copy fails the gruijter replay + scenario 3b. Gates: evaluate 72/0, 11 python suites OK, build SUCCESS. Bench restored, settings diff vs baseline empty, original simulation log back. AC3: the old-vs-fix half is done; gruijter's confirmation on the real Honeywell Round is still open, so the task stays In Progress after the push.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
With push setpoint on, SAT now follows a thermostat setpoint change once it has pushed at least once since boot, unless the change echoes SAT's own last push (0.25 C). The earlier precondition that the thermostat's previous value matched the last push is gone: thermostats such as the Honeywell Round never mirror an accepted override in MsgID 16, so it blocked every dial change (gruijter's log: 15 -> 19.5 -> 20 -> 19.5 -> 20 adopted nothing). Decision: Robert, option A, after Sergeant D deferred; recorded in docs/sat/sergeant-d-corrections.md entry 2. Evidence: host test on the compiled function (35/35, HEAD red on the replay of gruijter's sequence), bench replay old (target stuck at 20.5) vs fix (20.5 -> 19.5 -> 20.0 -> 19.5 -> 15.0). The review caught my first fix adding a previous-push guard that swallowed the return to 19.5 (reproduced on the bench too); removed. Open: AC3 second half, gruijter confirming on his Honeywell Round.
<!-- SECTION:FINAL_SUMMARY:END -->
