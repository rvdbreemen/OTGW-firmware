---
id: TASK-1436
title: >-
  SAT: thermo-nova pin 7beaaf5 vs develop: settle the ancestry and list per
  point what develop does differently, then report to Sergeant D
status: Done
assignee:
  - '@claude'
created_date: '2026-10-28T21:27:00Z'
updated_date: '2026-11-01T01:46:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 597000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Maintainer decision 2026-10-28: keep the pin at 7beaaf5 for now. Entries 64 and 66 contradict each other on ancestry (entry 64: develop is an ancestor of the pin, 294 behind; entry 66: the pin is an ancestor of develop, develop 294 ahead). Sergeant D wants develop followed for some points (duty temperature, reignition; entry 66 Open). Investigate on upstream https://github.com/Alexwijn/SAT (git merge-base / rev-list between releases/thermo-nova@7beaaf5 and develop), then list per SAT area what develop changes (file:line on both). Read-only for other-projects/. Moving the pin is a separate, deliberate commit per CLAUDE.md (update the SAT ground truth block, re-diff docs/sat/thermo-nova-spec.md).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Ancestry settled with commands and output recorded; the wrong entry (64 or 66) is corrected by a new entry in docs/sat/sergeant-d-corrections.md
- [x] #2 Per-area list of develop differences vs 7beaaf5 with file:line on both, in docs/sat/
- [x] #3 Result reported to Sergeant D in #alpha-testing (maintainer: 'leg het resultaat ook bij sergeant terug'), one question per message if a choice follows
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-01: ancestry settled (entry 191): develop is an ancestor of the pin (GitHub compare develop...7beaaf5: ahead_by 294, behind_by 0, merge base = develop HEAD 7d43560); entry 64 right, entry 66 wrong. Per-area differences with file:line both sides in docs/sat/thermo-nova-pin-vs-develop-2026-11-01.md (flame.py, area.py, config_flow, coordinator not compared in full, marked so). Questions queued Q-1436a..j.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ancestry settled (entry 191): develop is an ancestor of the pin; 7beaaf5 is 294 commits ahead of develop (GitHub compare ahead_by 294, behind_by 0; local git merge-base --is-ancestor develop pin = 0). Entry 64 right, entry 66 reversed. Per-area differences with file:line on both sides: docs/sat/thermo-nova-pin-vs-develop-2026-11-01.md (duty temperature, reignition, PWM ON timing, PWM start rule, minimum setpoint, smoothing, COLD_SETPOINT, PID update, classifier). Reported to Sergeant D in #alpha-testing (msg 1566267196571787296) with one question, Q-1436j (stay on the pin or move to a newer thermo-nova commit); the per-area questions Q-1436a..i are queued only if he wants develop behaviour. Not compared in full: flame.py, area.py, config_flow.py, coordinator/OpenTherm modules (marked in the doc).
<!-- SECTION:FINAL_SUMMARY:END -->
