---
id: TASK-1879
title: 'Docs: compare Versatile Thermostat with SAT and record transferable lessons'
status: Done
assignee:
  - '@claude'
created_date: '2026-11-13T06:13:00Z'
updated_date: '2026-11-13T06:14:00Z'
labels:
  - sat
  - docs
dependencies: []
priority: low
ordinal: 952000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert asked (2026-11-13) what OTGW/SAT can learn from Versatile Thermostat (jmcollin78/versatile_thermostat). Analysis in docs/sat/vtherm-comparison-2026-11-13.md, with follow-up tasks and one queued Sergeant D question.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 docs/sat/vtherm-comparison-2026-11-13.md exists with pinned commits and file:line citations for VTherm and thermo-nova
- [x] #2 Follow-up backlog tasks exist for lessons 1-5
- [x] #3 Integral question queued as Q-VT1 in docs/sat/open-questions.md and the live queue refreshed (tests/test_open_questions_status.py passes)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Sources read in this session: VTherm main a45ec37 (cloned read-only to scratchpad), thermo-nova pin 7beaaf5 (fresh fetch, git log verified). Core citations reopened by the main session. Corrections during the work: the tuner already logs SatTuneReason per hour (SATcontrol.ino:6424) and a closed-loop low-load sim exists (TASK-1412.19), so lessons 2 and 5 were narrowed to the remaining gap. VTherm's sign-change shrink of the integral is gated by overheat_protection and divides by 2*max(dt,0.5) (pi_algorithm.py:98-99); doc wording follows the code.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: docs/sat/vtherm-comparison-2026-11-13.md written with both pinned commits and file:line citations (status: internal, second read pending before any publication). AC2: follow-up tasks TASK-1880 (per-PWM-phase realized output telemetry), TASK-1881 (tuner exclusion counters), TASK-1882 (heating-failure/no-ignition event, needs-maintainer-ok), TASK-1883 (offline identification research + tuner convergence test) created via backlog CLI. AC3: Q-VT1 added to docs/sat/open-questions.md, live queue refreshed by scripts/sat_open_questions.py --update ('3 open rows'), tests.test_open_questions_status: Ran 6 tests, OK.
<!-- SECTION:FINAL_SUMMARY:END -->
