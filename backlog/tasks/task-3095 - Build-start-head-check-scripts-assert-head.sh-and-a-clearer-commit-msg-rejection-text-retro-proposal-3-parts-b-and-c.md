---
id: TASK-3095
title: >-
  Build-start head check (scripts/assert-head.sh) and a clearer commit-msg
  rejection text (retro proposal 3, parts b and c)
status: To Do
assignee:
  - '@claude'
created_date: '2026-11-26T06:00:00Z'
labels: []
dependencies: []
ordinal: 1161000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert adopted retro proposal 3 (DM 1575390148747857941 A). (b) scripts/assert-head.sh <hash>: compare full hashes via rev-parse, exit 1 with both values on mismatch; run before BUILD START (agent-build-and-bench.md). (c) .githooks/commit-msg rejection prints both legal ways out in one line (stage the task file or drop the TASK-NNN reference; docs-only commits use [no-task]; [no-src] for backlog-only commits).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 assert-head.sh passes for a 7-char and a 9-char form of the same commit and fails for a different commit; a test goes red on a naive string compare and green on the script
- [ ] #2 agent-build-and-bench.md has one line: run assert-head.sh before BUILD START
- [ ] #3 commit-msg rejection text names both ways out in one line, with a test on the real hook
<!-- AC:END -->
