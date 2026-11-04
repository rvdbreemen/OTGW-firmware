---
id: TASK-1275
title: >-
  Generalise implement-next-task into the continuous backlog cycle with
  #alpha-testing reporting
status: Done
assignee:
  - '@claude'
created_date: '2026-10-20 19:04'
updated_date: '2026-10-20 19:29'
labels:
  - tooling
  - skill
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 405000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Maintainer request 2026-10-20: a reusable workflow that keeps picking the next actionable task: announce pickup in #alpha-testing, design, critical self-review with the advisor, announce what will be implemented, implement with old-vs-fix evidence and simulation, reviewer-team review and fix, Done only with all ACs evidenced, report done + build in #alpha-testing, leave the combo flash zip ready. Decisions: implement directly after self-review (Discord is notice only); loop continuously; improve the existing .claude/skills/implement-next-task (currently scoped to TASK-865.x) instead of adding a new skill. Include the gaps found: selection rule, In Progress first, ADR context, open-source check, red-then-green for bugs, gates, bench OTA, bump per task, docs, failure path, Discord rules, build from pushed HEAD, tester questions first, graphify/OpenWolf, port question.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 SKILL.md describes the 9 steps plus the agreed additions, with selection, stop and failure rules
- [x] #2 Skill is general (any 2.0.0 task), not limited to TASK-865.x; old async-specific content kept only where still valid or removed
- [x] #3 Dry run: the selection step on the current backlog names the task it would pick and why (transcript)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-20 dry run of step 1 on the live backlog. Connected: OTGW32 (COM4, 303A:1001) and the 1.x CH340 unit (COM3); no Classic-S3, no DS18B20. In Progress @claude: TASK-1217 (needs Classic + decision) blocked; TASK-1223 epic (AC1 waits on 1223.4/1223.15) blocked; TASK-1223.4 #4 (Classic board) blocked; TASK-1223.15 (needs DS18B20) blocked; TASK-1255 #2 (real PIC flash on Classic) blocked; TASK-1241 #2 'overrides panel still loads on an OT-Direct board (browser capture on the OTGW32)' -> verifiable NOW. To Do 2.0.0 (11): high TASK-1268, TASK-1269; medium 1259, 1270, 1271, 1273; low 1258, 1260, 1267, 1272, 1274. Selection result: resume TASK-1241 first (only open AC verifiable now on the connected OTGW32), then TASK-1268 (high, lowest number, design decisions recorded: thermo-nova parity incl. overshoot_protection gate per Sergeant D). The dry run exposed a gap (6 In Progress tasks, most blocked), fixed in SKILL.md step 1.1: resume only when the remaining ACs are verifiable now, ordered by priority then number; plus fallback to task-file frontmatter when 'backlog task list' is empty and a connected-board check.

Advisor review before commit: fixed (1) plan-approval contradiction: SKILL.md names the exception and CLAUDE.md Task Management + Auto-advance point to it; (2) step 7 carries the one-line/no-sub-branch rule that CLAUDE.md:529 cites; (3) cerebrum entry replaced, no unresolved ADR-PENDING notes found (grep); (5) description shortened so the trigger phrases are visible in the skill listing; (6) TASK-1241 pick confirmed: device/info on .88.61 reports otcommandinterface OT-Direct; (7) opus review agent as advisor fallback. tests/test_adr_governance.py OK.

2026-10-20 maintainer: tasks that change SAT control behaviour wait for Sergeant D's advice and OK after the self-review (skill step 4 exception); meanwhile the cycle continues with tasks that do not change SAT behaviour.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
implement-next-task is now the general continuous backlog cycle: tester questions first, select (resume In Progress tasks whose open ACs are verifiable now, else highest-priority actionable To Do) + announce in #alpha-testing, design with ADR context, critical self-review with the advisor, announce the plan (notice, no approval wait, recorded as a CLAUDE.md exception), implement via a Sonnet agent with red-then-green evidence and haiku runners for gates, reviewer team, Done only with evidence, report + build tag, combo zip from the pushed HEAD, repeat; stop rules included. Old TASK-865 engine archived at .claude/workflows/archive/implement-next-task-865-drain.js; memory and cerebrum updated. AC1: SKILL.md content. AC2: no 865 scope left. AC3: dry run on the live backlog picks TASK-1241 (resume, OT-Direct board connected) then TASK-1268, and exposed the resume-rule gap that is now fixed. Not exercised yet: a full cycle end to end.
<!-- SECTION:FINAL_SUMMARY:END -->
