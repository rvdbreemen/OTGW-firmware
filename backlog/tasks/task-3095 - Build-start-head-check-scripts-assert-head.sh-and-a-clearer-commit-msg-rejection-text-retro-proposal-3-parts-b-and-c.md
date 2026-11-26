---
id: TASK-3095
title: >-
  Build-start head check (scripts/assert-head.sh) and a clearer commit-msg
  rejection text (retro proposal 3, parts b and c)
status: To Do
assignee:
  - '@claude'
created_date: '2026-11-26T06:00:00Z'
updated_date: '2026-11-26T07:07:00Z'
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
- [x] #1 assert-head.sh passes for a 7-char and a 9-char form of the same commit and fails for a different commit; a test goes red on a naive string compare and green on the script
- [x] #2 agent-build-and-bench.md has one line: run assert-head.sh before BUILD START
- [x] #3 commit-msg rejection text names both ways out in one line, with a test on the real hook
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Done 2026-11-26 (otgw-webui), pushed e78ec605c: assert-head.sh 6/6 tests (red on a naive string compare, 3 of 6), one line in agent-build-and-bench.md, commit-msg rejection names both ways out in one line (test on the real hook, red on the hook of origin/dev). No [no-src] tag exists in the repo, so it is not in the text.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1 assert-head.sh: tests/test_assert_head.py 6/6, a 7-char and a 9-char form of one commit pass, a different commit fails (exit 1, both full hashes printed), 3 of 6 fail on a naive string compare; pushed e78ec605c (host test). AC2 one line in docs/guides/agent-build-and-bench.md, same commit. AC3 commit-msg rejection names both ways out in one line; tests/test_commit_msg_hook_text.py runs the real hook, red on the hook of origin/dev; no [no-src] tag exists in the repo so it is not in the text. All host-verified; nothing needs a unit.
<!-- SECTION:FINAL_SUMMARY:END -->
