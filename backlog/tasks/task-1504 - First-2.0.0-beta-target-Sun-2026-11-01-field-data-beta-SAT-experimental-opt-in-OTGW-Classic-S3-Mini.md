---
id: TASK-1504
title: >-
  First 2.0.0 beta (target Sun 2026-11-01): field-data beta, SAT experimental
  opt-in, OTGW Classic + S3 Mini
status: To Do
assignee: []
created_date: '2026-10-30T06:19:00Z'
updated_date: '2026-10-30T17:21:00Z'
labels:
  - release beta
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 660000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-10-30 decisions: (1) SAT experimental, opt-in, default off, named so in the release notes; (2) TASK-1412 field-validation order waived for named testers (note in TASK-1412); (3) macOS capture run by a tester from #alpha-testing; (4) target Sunday 2026-11-01. Purpose: field data from more users. Blockers: TASK-1495 (settings after an interrupted FS upload, bench old-vs-fix), TASK-1500 (Linux/macOS portable-Python launcher), TASK-1503 (raw OT frame log with timing + replay-format file), capture works on Windows/macOS/Linux from the release zip (exec bits after unzip), a 24 h bench soak of the candidate without an unexplained reboot. Known open, to state in the notes: TASK-1498 (undecodable IRAM crash, bench), TASK-1438 (unknown reboots, alpha.519), Sergeant D open questions (Q60, Q64, Q65 confirmation, Q69-Q71, entry 129 follow-up). Release mechanics: version word alpha->beta by hand in version.h; CHANGELOG [Unreleased] renamed before the tag; immutable release burns the tag; beta workflow ships the capture assets; /release skill uses an old branch model (1.x -> otgw-1.x.x); the release itself needs Robert's explicit go.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 TASK-1495, TASK-1500 and TASK-1503 Done with bench/run evidence
- [ ] #2 Capture runs from the release zip on Windows, Linux and macOS (macOS by a named tester), each with a transcript
- [ ] #3 Candidate build soaked 24 h on the bench (Classic + S3 Mini) without an unexplained reboot; crashlog and reboot_log quoted
- [ ] #4 Release notes: SAT experimental/opt-in, BREAKING changes since the last beta/release, known issues (1498, 1438), save settings before flashing, capture instructions
- [ ] #5 Robert's explicit go recorded before the tag; release published with all assets incl. capture tools
- [x] #6 Sergeant D's beta-1 musts done with evidence (entry 146): TASK-1371, TASK-1501, TASK-1370, TASK-1373, TASK-1375, TASK-1376
- [x] #7 TASK-1505 (zip exec bits) and TASK-1507 (SAT beta notice) Done
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-30 Sergeant D entry 146 (msg 1565620504503517256): beta 1 must include TASK-1371, 1501, 1370, 1373, 1375, 1376 (priorities raised to high). Robert: added to the beta.1 task list; goal set: implement all beta.1 tasks. Full beta.1 list: TASK-1495 (Done), 1500, 1503, 1505, 1507, 1371, 1501, 1370, 1373, 1375, 1376 + 24 h soak + macOS capture run by a tester. (--dep could not be set: the CLI dependency lookup finds no tasks, the known list bug.)

2026-10-30 (coordinator): no v2.* tag or GitHub release exists (git tag -l, git ls-remote --tags, gh release list); the CHANGELOG [2.0.0-beta] - 2026-04-11 heading was never published. Proposed beta tag v2.0.0-beta.1 (maintainer decides at release). Release-notes draft being written to docs/releases/RELEASE_NOTES_2.0.0-beta.1.md (AC#4).

2026-10-30 coordinator (session 80): AC1 (1495, 1500, 1503), AC6 (1371 alpha.556, 1501 alpha.551, 1370 alpha.553, 1373 alpha.554, 1375 alpha.552, 1376 alpha.555, each bench-tested on .63 and published) and AC7 (1505, 1507) all Done with evidence in their final summaries. Also in: TASK-1513 (alpha.557, entry 152; AC2 entry 155 in progress) and TASK-1514 (alpha.558, entry 153). Remaining: AC2 macOS capture by a tester, AC3 24 h soak of the candidate (starts after the TASK-1513 AC2 build), AC4 release notes update (in progress), AC5 Robert's go.

AC4 draft updated (docs/releases/RELEASE_NOTES_2.0.0-beta.1.md): today's SAT items, three new BREAKING items (cycles 3/4, max_setpoint meaning + underfloor default 50, cycles_this_hour), known issues 1498/1438/1480 + 1371 not field-run. Open for Robert's review: crash-log endpoint claim (only in the old 2.0.0-beta CHANGELOG section), port 25238 bridge opt-in and ADR-084 topics (only in RELEASE_GITHUB_2.0.0.md), older [Unreleased] items whose cycle is unclear, release date, macOS result, soak result.

AC3 soak started: alpha.559+ccda6eb (beta-1 candidate) on .63 Classic+S3 Mini, 2026-10-30 17:20:43 UTC, bootcount 26, detached monitor %LOCALAPPDATA%/OTGW-capture/soak/soak.py (PID 14212) logging every 300 s to %LOCALAPPDATA%/OTGW-capture/soak/soak-559.log; ends 2026-10-31 17:20 UTC. Do not flash .63 until then.
<!-- SECTION:NOTES:END -->
