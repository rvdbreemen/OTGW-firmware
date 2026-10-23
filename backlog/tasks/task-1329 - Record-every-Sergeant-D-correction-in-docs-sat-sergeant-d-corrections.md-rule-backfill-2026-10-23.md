---
id: TASK-1329
title: >-
  Record every Sergeant D correction in docs/sat/sergeant-d-corrections.md (rule
  + backfill 2026-10-23)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-23T19:05:00Z'
updated_date: '2026-10-23T19:24:00Z'
labels:
  - sat
  - docs
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 452000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Maintainer (session 2026-10-23): make sure CLAUDE.md says that every time Sergeant D corrects or decides something, it is documented in the docs/sat document where his decisions are kept (docs/sat/sergeant-d-corrections.md). Today the CLAUDE.md SAT ground truth block only says the corrections are 'kept in docs/sat/' and the file holds 4 entries from 2026-10-22; none of the 2026-10-23 decisions (TASK-1316 CH rule, 1322 mode buttons, 1323 solar/thresholds/freeze, 1327 cutoff, CS follows BT, Tret only at PWM start-up) are in it.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 CLAUDE.md and AGENTS.md (SAT ground truth block) name docs/sat/sergeant-d-corrections.md and make recording mandatory: same session, before implementing, one numbered entry per correction/decision with date, quote, Discord link, thermo-nova and firmware comparison, task id and open points
- [x] #2 implement-next-task SKILL.md step 4/backlog_discord reference the same rule
- [x] #3 The 2026-10-23 decisions are backfilled as entries, each quote checked against the Discord message by a second reader
- [x] #4 tests/test_sat_ground_truth_block.py (and the CLAUDE.md/AGENTS.md sync test if any) pass
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-23 21:35 evidence: CLAUDE.md + AGENTS.md SAT ground truth block (identical): item 1 names docs/sat/sergeant-d-corrections.md; new mandatory rule (same session, before implementing; quote, link, thermo-nova + firmware comparison, task, open points; interpretations labelled; old entries never rewritten). tests/test_sat_ground_truth_block.py: new test_block_requires_recording_sergeant_d_decisions (HEAD CLAUDE.md lacks both strings: red; now OK); blocks identical test OK; test_adr_governance OK. .claude/skills/implement-next-task/SKILL.md step 4 and .claude/commands/backlog_discord.md carry the same rule. Backfill via workflow wf_e8999ecc-c52 (draft + 2 independent verifiers + fix): entries 5-12 for 2026-10-23 (10:21 CH with CS=10 incl. our wrong claim and correction; Tret at PWM start-up; per-system cold setpoint + cold-start hold; CS decoupled from duty; 15:08 read as yes; mode buttons + active mode visible; solar/28.2-21/freeze; cutoff as thermo-nova). Quote verifier: all quotes verbatim vs Discord, ids/times match, no decision missing (10:24/10:26 are not decisions). Citation verifier: all thermo-nova and firmware citations hold; 2 minor wording issues in entry 6 fixed (context not from him moved out of 'Settled'; source order 10:28 before gruijter's 10:30 question). No em dashes.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
CLAUDE.md and AGENTS.md now make it mandatory to record every correction or decision by Sergeant D as a numbered entry in docs/sat/sergeant-d-corrections.md, in the same session and before implementing (quote, Discord link, comparison with thermo-nova and the firmware, task, open points; interpretations labelled). The implement-next-task skill and the backlog_discord command carry the same step, and a test guards the rule. The 2026-10-23 decisions are backfilled as entries 5-12, each quote checked against Discord and each citation against the code by independent reviewers.
<!-- SECTION:FINAL_SUMMARY:END -->
