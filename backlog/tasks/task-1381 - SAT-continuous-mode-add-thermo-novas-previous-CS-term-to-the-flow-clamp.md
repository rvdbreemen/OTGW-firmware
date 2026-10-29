---
id: TASK-1381
title: 'SAT continuous mode: add thermo-nova''s previous-CS term to the flow clamp'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-25T10:38:00Z'
updated_date: '2026-10-29T12:19:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies:
  - TASK-1270
references:
  - TASK-1412
ordinal: 520000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From TASK-1273 (gruijter field run 2026-10-20). thermo-nova clamps the continuous setpoint with the previous control setpoint: rising request -> max(requested, flow - offset); falling -> max(requested, min(flow - offset, previous)) (heating_control.py:484-488). Firmware satApplyContinuous (SATcontrol.ino ~1353-1392 at HEAD) is flow-offset only, so CS follows a rising flow. Changes how SAT controls the boiler: needs a queued question + Sergeant D's OK.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Continuous CS follows heating_control.py:484-488 incl. the previous-CS term (host test red on HEAD, green on fix)
- [x] #2 Sergeant D's OK recorded in docs/sat/sergeant-d-corrections.md before implementation (CLAUDE.md SAT rules)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Entry 112 (Q53 A). thermo-nova heating_control.py:450-489 _compute_continuous_control_setpoint: previous = stored control setpoint; no flow -> request; flow <= request -> request; flame off -> request; else min_allowed = flow - offset; request > previous -> max(request, min_allowed) else max(request, min(min_allowed, previous)). Firmware satApplyContinuous (SATcontrol.ino:1572-1611) has the bypasses but not the previous term. Change: satApplyContinuous(pidOutput, previousCS) with previous = _sat_lastControlCS (set to finalSetpoint each tick :5649; 0 after satDisable :2727 = enable/boot; must be SAT_MIN_SETPOINT 10 after heater-off / cold cutoff: verify every path that sends CS outside the tick and make it write _sat_lastControlCS; entry 112 interpretation: thermal-safe fallback and summer simmer write the CS they send). No restore after reboot. Test: host test on the real satApplyContinuous: falling request with flow above -> CS holds at min(flow-offset, previous) (red on HEAD: HEAD returns flow-offset), rising request -> max(request, flow-offset), the three bypasses unchanged; plus previous after enable = 0 and after cold cutoff = 10. Docs: firmware-sat-spec, sat-comparison.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 execution sequencing: reused by TASK-1412. Required prerequisites are encoded as dependencies; existing status, queue ordinal, decisions and approval requirements are preserved. Do not resume affected control changes before the scoped approved contract and red tests. Later integration, simulation, bench acceptance and field evidence are tracked by the epic; this metadata update is not implementation or validation.

2026-10-28 dependency reconciliation (Robert's go: reconcile the graph alongside the wave-3 work; scout+verifier report): TASK-1412.3 dependency removed (own red/green AC#1). Real blocker not in the graph: AC#2 needs Sergeant D's recorded OK; no question was queued -> queued in open-questions.md (Q53).

2026-10-28 Q53 answered A (entry 112): follow thermo-nova heating_control.py:485-489; previous = last CS SAT sent (10 after heater-off/cold cutoff, 0 after enable/boot), no restore after reboot; thermal-safe and simmer write their CS as previous (our sub-choice). AC#2 has his OK.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Continuous CS now follows thermo-nova heating_control.py:484-489 including the previous-CS term (entry 112, Q53 A). satApplyContinuous(pidOutput, previousCS): rising request -> max(request, flow - offset); otherwise max(request, min(flow - offset, previous)); the three bypasses (no flow, flow <= request, flame off) unchanged. previous = _sat_lastControlCS (0 after enable/boot, no restore after reboot); thermal-safe fallback writes the CS it sends, summer simmer writes SAT_MIN_SETPOINT (entry 112 sub-choice).

Evidence per AC:
- AC#1: tests/test_sat_previous_cs_clamp.py calls the real satApplyContinuous: 9/9 OK on the fix (re-run 2026-10-29), FAILED on HEAD 8176c49 (53.0 != 50.0, falling request held at flow-offset instead of min(flow-offset, previous)). test_sat_pwm_override.py and test_sat_pwm_start_limit.py OK (stubs updated for the new signature). evaluate.py --quick: no FAIL, health 100%. esp32-combo build: see the commit/tag (alpha.530).
- AC#2: Sergeant D's OK recorded as entry 112 in docs/sat/sergeant-d-corrections.md before implementation.
Review: opus correctness review found no defect; follow-up on PWM exit timing filed as TASK-1468. Docs: firmware-sat-spec.md and sat-comparison.md updated.
Not verified: closed-loop behaviour on a real boiler (TASK-1412 bench/field gates).
<!-- SECTION:FINAL_SUMMARY:END -->
