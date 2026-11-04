---
id: TASK-1423
title: >-
  SAT PID: round every PID value to 3 decimals and match thermo-nova's Ki-near-0
  clamp (entry 95, Q28a/Q28b)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-28T07:51:00Z'
updated_date: '2026-11-01T08:41:00Z'
labels:
  - sat
  - sergeant-d
milestone: 2.0.0
dependencies:
  - TASK-1368
  - TASK-1414
references:
  - docs/sat/sergeant-d-corrections.md
documentation:
  - docs/sat/sat-low-load-execution-backlog-2026-10-28.md
priority: medium
ordinal: 587000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-28 (entry 95): Q28a 'Round all the values to 3 decimals not only the Output' (deviation: thermo-nova rounds gains to 6 decimals pid.py:71/:82/:93, P/I/D/raw derivative to 3 pid.py:101-119, output to 1 pid.py:132); Q28b Ki-near-0 integral clamp as thermo-nova (SATpid.ino ~187 skips the clamp). ADR-197.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test: gains, P, I, D, raw derivative and output are rounded to 3 decimals (red on HEAD, green on fix)
- [x] #2 Host test: integral clamp with Ki near 0 behaves as pid.py (red on HEAD where it differs)
- [x] #3 ADR-197 updated with entry 95
- [x] #4 build + evaluate green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 Robert requested this execution-graph update. Preserve the earlier reconciliation. Pending PID writers run 1368 -> 1270 -> 1416 -> 1417 -> 1418 -> 1414 -> 1423 -> 1415; 1381 remains a separate branch after 1270 and Q53. Revalidate task premises against preceding commits. Integration also requires the DHW and auto-tune outcomes through their linked verification gates. Approval and bench gates remain in force.

2026-10-30 coordinator: picked up and put back: Robert's SATpid.ino writer order is 1416 -> 1417 -> 1418 -> 1414 -> 1423 -> 1415; TASK-1414 is not Done yet.

2026-10-31 worktree cleanup: unmerged work of this task sits in lane wt-1423 (archived as D:\Users\Robert\Documents\GitHub\RvdB\lane-archive-20261031\wt-1423.uncommitted.patch); the lane is kept and will be merged into dev (Robert: merge lanes where possible).

Correction: the 2026-10-30 note that TASK-1414 is not Done is stale; 1414, 1368, 1416, 1417, 1418 are Done.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1/AC2: tests/test_sat_pid_cadence.py 31/31 OK; bench MQTT sat/pid_attributes shows rounded terms. AC3: ADR-197 records entry 95 (Q28a rounding, Q28b Ki-near-0) at lines 117 and 121; its 'pending in TASK-1423' wording is now stale but ADR-197 is Accepted and not edited. Build: esp32-combo 2.0.0-alpha.597+d458ac3 (build/…-flash.zip 08:51, built and posted by the coordinating session); the task commit is an ancestor of d458ac3 (git merge-base --is-ancestor). evaluate.py --quick 2026-11-01: 0 FAIL, Health 100%. Bench .61 (OT-Direct) runs 2.0.0-alpha.597+d458ac3 (/api/v2/device/info), up 25 min without reboot.
<!-- SECTION:FINAL_SUMMARY:END -->
