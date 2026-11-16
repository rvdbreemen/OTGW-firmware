---
id: TASK-2000
title: >-
  SAT: PWM ignores MsgID 3 HB bit 4 (master low-off) again; the bit-4 veto of
  entries 213/214 is removed (Sergeant D entry 392)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-16T11:08:00Z'
updated_date: '2026-11-16T11:18:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1022000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D entry 392 (2026-11-16, to be recorded verbatim by 92 before the build; relayed by the coordinator: 'Don't look at bit 4 at all ... boiler should be able to enter PWM') reverses entries 213 (bit 4 = 1 blocks every PWM entry path) and 214 (bit 4 turning 1 leaves PWM in that tick). Firmware today: satPwmPermitted() = bSlaveCfgValid && !bSlaveLowOffDenied (SATcycles.ino:1302-1336), refusal reason 2 'the boiler does not allow master low-off', satCycleCheckLowLoadVeto() leaves PWM (path 4). thermo-nova only shows the bit (coordinator/serial/binary_sensor.py:54) and never acts on it, so Part 1 returns to thermo-nova. Field case: indigo_light's Vitodens reports HB bit 4 = 1, so SAT never enters PWM there. Part 1 (now): drop the bit-4 condition from satPwmPermitted() and the reason-2 path; keep the bit recorded and shown (REST slave_low_off_denied) as information. Part 2 (entry 215, reason 1: no PWM until a valid MsgID 3 reply) waits on Q-392a (A remove / B keep). Superseded by this task: entries 213 and 214 (by entry 392); firmware-sat-spec.md 'Slave low-load permission gate' section; ADR-249 says 'the boiler's MsgID 3 permission comes first (entry 213)' - after Part 1 that permission means 'a valid MsgID 3 reply' (entry 215), so its wording stays true but its citation is outdated; no Accepted ADR states the bit-4 rule itself (grep docs/adr for TASK-1412.4/satPwmPermitted/bit 4 = 1: only ADR-249's clause), so no new ADR is needed unless Part 2 removes entry 215 too. Milestone 2.0.0 pending Sergeant D's scope answer in Q-392a.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Entry 392 is recorded verbatim in docs/sat/sergeant-d-corrections.md (with link) before the code change
- [ ] #2 satPwmPermitted() no longer reads MsgID 3 HB bit 4; a valid reply with bit 4 = 1 allows PWM on every entry path, and bit 4 turning 1 no longer leaves PWM; the bit stays recorded and shown
- [ ] #3 Host test on the real code: a Vitodens-like profile (MsgID 3 HB bit 4 = 1) enters PWM; red on the old code (refused, reason 2), green on the fix; the no-valid-reply case is unchanged until Q-392a
- [ ] #4 firmware-sat-spec.md and the REST/UI veto text updated; ADR-249's entry-213 citation noted as superseded by entry 392
- [ ] #5 Part 2 per Q-392a (entry 215): implemented or recorded as kept
- [ ] #6 Field check by indigo_light on his Vitodens: PWM entered (before capture on beta.339, after capture on the fix)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Entry 392 recorded verbatim by 92 (origin 6c9edca6b) before the code; Q-392a asked 11:05:27 UTC (no answer yet). Field test task by the coordinator: TASK-1998 (indigo_light).
<!-- SECTION:NOTES:END -->
