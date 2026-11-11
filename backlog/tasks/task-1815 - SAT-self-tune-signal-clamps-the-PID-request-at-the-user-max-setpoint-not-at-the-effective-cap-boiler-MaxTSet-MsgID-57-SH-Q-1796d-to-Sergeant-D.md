---
id: TASK-1815
title: >-
  SAT self-tune signal clamps the PID request at the user max setpoint, not at
  the effective cap (boiler MaxTSet MsgID 57 / SH=): Q-1796d to Sergeant D
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-11T20:05:00Z'
updated_date: '2026-11-11T22:26:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 908000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The self-tune signal is clamp(pidRaw, SAT_MIN_SETPOINT, sysMax) - curve with sysMax = satGetMaxSetpoint(), the user MaxTSet (65 default) (SATcontrol.ino:7548-7551 on beta.184, :382-385). The control setpoint SAT sends is capped by satEffectiveMaxSetpoint(), which also applies the boiler MaxTSet (MsgID 57) and the hard ceiling (:419-431, used at :7365). ADR-205 decision 1 says only 'the system maximum'; no Sergeant D entry names which maximum. Field case (coordinator, gruijter 10 Nov, beta.167): SH=48, user max 65, so warm-up ticks above 48 count a signal the boiler cannot deliver and push the coefficient up in hours capped by the boiler. Question Q-1796d via comms 92: A clamp at the user max (as now) / B clamp at the effective cap (recommended). Implementation waits for his answer; do not change before it is recorded in docs/sat/sergeant-d-corrections.md.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Sergeant D's answer to Q-1796d is recorded in docs/sat/sergeant-d-corrections.md
- [x] #2 The tuner signal uses the maximum he chose; a host test (tests/test_sat_curve_selftune.py or the SATcontrol harness) fails on the old clamp and passes on the fix when the answer is B
- [ ] #3 Build, evaluate --quick and a bench run on .61 with a boiler MaxTSet below the user max show the clamped signal in SATTUNEH
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Entry 317 (Q-1796d B) settles it: clamp the tuner signal at satEffectiveMaxSetpoint(), the ceiling SAT applies to the sent setpoint, instead of satGetMaxSetpoint(). In the control tick the variable maxSetpoint already holds satEffectiveMaxSetpoint() (SATcontrol.ino, Clamp to valid range) and is not reassigned before the tuner block; use it and drop the then unused sysMax. Our tuner, no thermo-nova code (entry 298). Test: tests/test_sat_tune_signal_ceiling.py resolves which function fills the clamp variable (red on origin/dev, green on the fix); test_sat_curve_selftune.py source pin updated. Docs: sat-log-format.md self-tune signal line names the ceiling.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Q-1796d answered B by Sergeant D, entry 317 (20:11:32 UTC, msg 1570169126947655914): the tuner clamps its signal at satEffectiveMaxSetpoint() (SATcontrol.ino:419-431) instead of satGetMaxSetpoint() (:7548-7551). Unblocked once entry 317 is on origin/dev (wt-comms a84a61825); after the 183/184 push.

Implemented locally (lane-1815 on origin/dev 52fab1fb7): SATcontrol.ino tuner block clamps sigPid at maxSetpoint (= satEffectiveMaxSetpoint()), sysMax removed, signal comment cites entry 317. tests/test_sat_tune_signal_ceiling.py 3 OK on the fix; on origin/dev SATcontrol.ino it fails: 'satGetMaxSetpoint' != 'satEffectiveMaxSetpoint'. test_sat_curve_selftune.py 74 OK (source pin moved to maxSetpoint), test_sat_narrate_fits 1 OK, evaluate --quick 86/0. docs/sat/sat-log-format.md:34 names the ceiling. AC2 (host test red/green) ticks with this; AC3 (build + bench by the bench tester with a boiler MaxTSet below the user max) waits for the build slot after ba's 186.
<!-- SECTION:NOTES:END -->
