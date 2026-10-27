---
id: TASK-1412
title: >-
  SAT low-load remediation epic: ordered implementation, bench acceptance and
  field proof
status: To Do
assignee: []
created_date: '2026-10-27T23:40:00Z'
updated_date: '2026-10-27T23:48:00Z'
labels:
  - epic
  - sat
  - low-load
milestone: 2.0.0
dependencies:
  - TASK-1412.26
documentation:
  - docs/sat/sat-low-load-remediation-plan-2026-10-28.md
  - docs/sat/sat-low-load-verification-plan-2026-10-28.md
  - docs/sat/sat-low-load-execution-backlog-2026-10-28.md
  - docs/sat/sat-low-load-execution-backlog-2026-10-28.json
  - docs/sat/evidence/2026-10-28-low-load-backlog-order.json
priority: medium
type: feature
ordinal: 550000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Execute the TASK-1411 review programme for R01-R42 using the existing implementation tasks plus bounded missing work. Preserve recorded designer decisions and required approvals. Deliver corrected control, reliable evidence and named equipment/transport validation. Mandatory sequence: contracts and red tests -> scoped fixes -> host integration -> closed-loop simulation -> real-board bench simulation with emulated plant -> bench acceptance -> controlled equipment commissioning -> tester field validation -> independent release evidence. Field validation cannot start before bench acceptance. New work is appended after the existing queue; dependencies, not IDs, determine readiness. This epic does not authorize a controller replacement or contacting testers.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Every issue R01-R42 has an execution owner task and an objective verification gate, with existing fix tasks reused.
- [ ] #2 The task dependency graph is acyclic, preserves existing prerequisites and blocks tester field validation until bench acceptance and equipment preflight pass.
- [ ] #3 LL-001..004 and LL-010..023 have reproducible results tied to the tested revision; expected failures, missing checks and unavailable infrastructure cannot pass release acceptance.
- [ ] #4 PH-PIC, PH-OTD, PH-PROFILE and PH-FIELD are passed for each claimed supported combination or that combination is explicitly excluded from the release.
- [ ] #5 An independent final evidence review verifies the implementation, physical results, rollback and limitations; no unmeasured energy or heat-pump suitability claim remains.
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Created 26 child tasks after the pre-existing maximum ordinal 549000; epic 550000 and children 551000..576000. Existing SAT fix tasks retain their original position/status and gain explicit prerequisites. The parent depends on final independent evidence acceptance; no child depends on the parent. Bench acceptance TASK-1412.22 blocks controlled equipment TASK-1412.23, tester readiness TASK-1412.24 and tester field runs TASK-1412.25. No firmware implementation, bench execution or tester contact has occurred in this planning update.

Sequencing audit passed: 47 graph nodes, zero dependency cycles, all 42 review issues assigned to non-final execution owners, all 26 new children after the previous queue maximum, and every equipment/tester/final path includes TASK-1412.22 bench acceptance. Actual task metadata matches the execution manifest. This audits scheduling only; all children remain To Do and no implementation/physical pass is claimed. evaluate.py --quick: 78 passed, 0 failed, 0 warnings, 7 info.
<!-- SECTION:NOTES:END -->
