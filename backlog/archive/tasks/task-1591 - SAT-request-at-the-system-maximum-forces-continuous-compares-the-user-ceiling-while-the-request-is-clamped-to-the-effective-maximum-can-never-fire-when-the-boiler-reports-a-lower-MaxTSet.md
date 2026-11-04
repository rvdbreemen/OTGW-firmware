---
id: TASK-1591
title: >-
  SAT: 'request at the system maximum forces continuous' compares the user
  ceiling while the request is clamped to the effective maximum (can never fire
  when the boiler reports a lower MaxTSet)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-03T14:02:00Z'
updated_date: '2026-11-03T19:35:00Z'
labels:
  - sat
dependencies: []
priority: low
ordinal: 736000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by TASK-1412.7 (2026-11-03): the rule that ends PWM when the request reaches the system maximum compares against the user ceiling sysMax, but the request is clamped to the effective maximum (also bounded by the boiler's reported MaxTSet, MsgID 49/57). They differ only when the boiler reports a lower MaxTSet (e.g. a rejected SH=, TASK-1412.13); then the rule cannot fire. thermo-nova uses one value (heating_control.py:203-210). No recorded decision covers it. First: trace both with file:line, second reader, then a question to Sergeant D if real.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Both sides traced with citations and a second reader confirmed or refuted the difference
- [x] #2 If real: decision recorded (Sergeant D) and implemented with a host test red on the old code
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-03 investigation (opus, read-only) + second reader (lead): CORRECTED and confirmed. effective <= sysMax always (MsgID 49 bounds are inside sysMax, so not a cause); they differ only through the boiler MsgID 57 term or the 75 fallback in satEffectiveMaxSetpoint() (SATcontrol.ino:417-429); clamp at ~:7089 feeds the compare pidOutput >= sysMax at ~:7095, so the at-max rule can never fire when the boiler/installer MaxTSet is below the user/default ceiling (most common: installer 55 vs default 62, no SH= involved). thermo-nova uses one configured value for clamp and compare (climate.py:394; coordinator/__init__.py:275-283; heating_control.py:203-204); MsgID 57 there only feeds the number entity (number.py:103-113). No entry decides which value the rule compares against. Asked as Q-1591a.

2026-11-03 Q-1591a decided A by Robert (entry 256): compare with the clamped value satEffectiveMaxSetpoint(). Curve-tune clamp unchanged (not asked).

2026-11-03 implemented (sonnet agent) + opus adversarial review (code SHIP; docs fixed per review: 8 spec places, sat-comparison rows ported; harness argc fix; debug text). SATcontrol.ino:7095 compares pidOutput >= maxSetpoint (satEffectiveMaxSetpoint). test_sat_max_tset: OLD 1 failure (mode 1 at clamped 55 vs default 62), FIX 37 OK; ceiling_paths 9 OK, ground_truth_block 6 OK, classifier_parity OK, evaluate 0 failed. Known effect (review, matches thermo-nova heating_control.py:203-208): at the boiler's own limit PWM and continuous can alternate. Tag beta.57; bench check pending in the shared build.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SAT's at-max rule (end PWM, go continuous) compares with the clamped maximum satEffectiveMaxSetpoint() instead of the user ceiling (entry 256, maintainer decision A, Sergeant D informed). Evidence: tests/test_sat_max_tset.py old 1 failure (stays PWM at 55 vs default 62), fix 37 OK; ceiling_paths 9, ground_truth_block 6, classifier_parity OK; opus adversarial review: code SHIP, docs fixed; beta.57 built and running on both bench units (beta.60) without crash. Not bench-verified: the switch with a real boiler at its MaxTSet (no boiler on the bench). Known effect: PWM and continuous can alternate at the boiler's own limit, as thermo-nova.
<!-- SECTION:FINAL_SUMMARY:END -->
