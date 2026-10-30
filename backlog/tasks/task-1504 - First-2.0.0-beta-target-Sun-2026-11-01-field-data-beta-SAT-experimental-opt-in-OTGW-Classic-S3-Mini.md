---
id: TASK-1504
title: >-
  First 2.0.0 beta (target Sun 2026-11-01): field-data beta, SAT experimental
  opt-in, OTGW Classic + S3 Mini
status: To Do
assignee: []
created_date: '2026-10-30T06:19:00Z'
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
- [ ] #1 TASK-1495, TASK-1500 and TASK-1503 Done with bench/run evidence
- [ ] #2 Capture runs from the release zip on Windows, Linux and macOS (macOS by a named tester), each with a transcript
- [ ] #3 Candidate build soaked 24 h on the bench (Classic + S3 Mini) without an unexplained reboot; crashlog and reboot_log quoted
- [ ] #4 Release notes: SAT experimental/opt-in, BREAKING changes since the last beta/release, known issues (1498, 1438), save settings before flashing, capture instructions
- [ ] #5 Robert's explicit go recorded before the tag; release published with all assets incl. capture tools
<!-- AC:END -->
