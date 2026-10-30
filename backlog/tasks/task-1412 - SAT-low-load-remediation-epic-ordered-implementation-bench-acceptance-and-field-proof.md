---
id: TASK-1412
title: >-
  SAT low-load remediation epic: ordered implementation, bench acceptance and
  field proof
status: To Do
assignee: []
created_date: '2026-10-27T23:40:00Z'
updated_date: '2026-10-30T06:19:00Z'
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
  - docs/sat/sat-low-load-agent-handoff-2026-10-28.md
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

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
2026-10-28 Robert requested graph update and builder communication: preserve the already committed reconciliation; link TASK-1422/1424 into post-DHW verification and TASK-1413 into model-baseline verification; serialize pending SATpid.ino writers after 1368/1270 without putting Q53 or persistence-cadence decisions in front of independent fixes; refresh execution markdown/JSON and graph audit from actual task metadata; publish handoff on active tasks 1368/1409 and the epic; verify acyclicity and mandatory bench-to-field gates; commit only these task/document changes. No firmware implementation or status completion in this update.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Created 26 child tasks after the pre-existing maximum ordinal 549000; epic 550000 and children 551000..576000. Existing SAT fix tasks retain their original position/status and gain explicit prerequisites. The parent depends on final independent evidence acceptance; no child depends on the parent. Bench acceptance TASK-1412.22 blocks controlled equipment TASK-1412.23, tester readiness TASK-1412.24 and tester field runs TASK-1412.25. No firmware implementation, bench execution or tester contact has occurred in this planning update.

Sequencing audit passed: 47 graph nodes, zero dependency cycles, all 42 review issues assigned to non-final execution owners, all 26 new children after the previous queue maximum, and every equipment/tester/final path includes TASK-1412.22 bench acceptance. Actual task metadata matches the execution manifest. This audits scheduling only; all children remain To Do and no implementation/physical pass is claimed. evaluate.py --quick: 78 passed, 0 failed, 0 warnings, 7 info.

## Re-sequencing 2026-10-28 (after Sergeant D answered Q15-Q51)

Sergeant D's answers (entries 67-94) removed most of the approval blockers, and gruijter's alpha.514 capture
(entry 92) shows the defect testers feel today: after hot water the PWM OFF wait restarts on every hot-water
flame-off (~23.5 min CH delay), and an ON phase that starts flame-off ends after one tick. The dependency graph
stays; the order inside it changes so that this defect is fixed first, with old-vs-fix proof.

- TASK-1412.2 is split. A thin PWM-anchor contract (entries 43, 80, 89-91, 92) is written first and unblocks a
  PWM slice of TASK-1412.3. The full OT low-load crosswalk (low-load permission, measurement validity) runs in
  parallel and does not gate the PWM slice.
- Wave 1 (critical path): TASK-1412.1 (records, mostly done 2026-10-28) -> PWM contract slice -> TASK-1412.3
  PWM slice (real-code harness as tests/test_sat_hp_pwm.py; replay of the 2026-10-27 hot-water draws and the
  slow-ignition case, red on HEAD) -> TASK-1382 + TASK-1380 (one candidate) -> TASK-1383 -> TASK-1407
  (pwm_on/pwm_off in the SAT log, so the fix is visible) -> bench old vs fix on the Classic.
- Wave 2: TASK-1421 (count at the ON start, never a hot-water flame; hard dependency on TASK-1382) ->
  TASK-1419 (heat pumps) -> TASK-1420 (sustained switches).
- Wave 3: TASK-1368 -> TASK-1270 -> TASK-1381, then the PID parity tasks one at a time (they share SATpid.ino):
  TASK-1416, TASK-1417, TASK-1418, TASK-1414, TASK-1415 (write cadence is Robert's call).
- Wave 4: TASK-1413 (auto-tune removal; API removal recorded in ADR-198, Proposed), TASK-1409, TASK-1412.4/.5/.6,
  TASK-1365 close-out.
- Blocked on Sergeant D: TASK-1373 (Q15b, Q41), TASK-1375 (Q16), TASK-1355/ADR-199 (Q20b), Q28 rounding.
- Validation sequence TASK-1412.18 -> .26 is unchanged.

2026-10-28 maintainer decision (Robert, in session): alphas keep reaching testers after a bench old-vs-fix proof per fix; TASK-1412.22 gates only the formal field-validated claim and the release, not alpha builds.

2026-10-28 dependency reconciliation (Robert's go: reconcile the graph alongside the wave-3 work; scout+verifier report): removed TASK-1412.3 from TASK-1420 (Done), 1368, 1270, 1381, 1413, 1409; replaced it by TASK-1368/1270 on 1417/1414. Reason (precedent TASK-1419): these tasks carry their own red/green host tests and recorded decisions; TASK-1412.3 absorbs them into LL-003/LL-004/LL-020 later. Kept on 1412.4-.6 (LL-010/011/017 PENDING and contracts missing). Added 1428/1429 to 1412.8, 1423 to 1412.10, 1425 to 1412.15, 1426 to 1412.18, 1412.5 to 1428. Not changed: Done tasks 1380/1382/1383/1407 still list 1412.3 (history). docs/sat/sat-low-load-execution-backlog-2026-10-28.md and the backlog-order JSON are now stale on these edges. Real blockers outside the graph: TASK-1381 AC#2 (Sergeant D OK, Q53 queued), TASK-1426 (entry 21, Q54 queued), TASK-1415 AC#3 (Robert: write cadence, entry 74).

2026-10-28 Codex execution-graph update completed at Robert's request: preserved reconciliation 6dda1e15a, linked 1422/1424 into 1412.7 and 1413 into 1412.16, encoded the serial PID writer chain without Q53 blocking independent fixes, refreshed markdown/JSON and audit. Audit: 63 nodes, zero cycles, 42 issue owners, all relevant SAT follow-ups in the epic closure, bench required for formal field stages; injected cycle and bench-bypass controls rejected. Builder handoff published in comments on 1368/1409/1270 and this epic, linked from AGENTS.md and CLAUDE.md. No direct active builder endpoint was available and no acknowledgement is claimed. Active builders retain ownership of 1368/1409 and all firmware edits. No control change or task completion claimed.

Robert 2026-10-30 (session, coordinator otgw-firmware-b8): WAIVER of the field-validation order for the first 2.0.0 beta (target Sun 2026-11-01). SAT ships experimental / opt-in (default off); SAT field data may come from named testers who enable it knowingly, before 1412.21/.22 bench and equipment acceptance. The epic stays the route to 'SAT stable'; this waiver covers field-data collection only, not a stable claim.
<!-- SECTION:NOTES:END -->

## Comments

<!-- COMMENTS:BEGIN -->
author: @codex
created: 2026-10-28T20:08:00Z

---

Robert requested this builder handoff from Codex. Read docs/sat/sat-low-load-execution-backlog-2026-10-28.md (current schedule) and docs/sat/sat-low-load-agent-handoff-2026-10-28.md before the next task. Keep current 1368 and 1409 work; this update changes only metadata/docs. Main PID chain: 1368 -> 1270 -> 1416 -> 1417 -> 1418 -> 1414 -> 1423 -> 1415. 1381 branches after 1270 and still waits on Q53; 1415 waits on Robert for write cadence. Q55 is answered by entry 111. 1409 is independent, followed by telemetry verification with 1425. Include 1422/1424 and 1413 in their integration gates. Do not resolve the ID6/MaxTSet contract or any open question by assumption. Per-fix bench old-vs-new proof is required before tester alphas; formal 1412.18 -> .26 validation remains required. Do not stage or overwrite another worker's source/task changes; coordinate shared SATpid.ino edits one at a time.

---
<!-- COMMENTS:END -->
