---
id: TASK-659
title: >-
  Add commit-msg hook enforcing TASK-NNN reference in firmware/docs commit
  subjects
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-22 05:52'
updated_date: '2026-05-25 22:00'
labels:
  - ci
  - process
  - backlog
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Roughly 70% of the 50 dev commits reference a PR number (#576, #594, #596, #598, etc.) but not a TASK-NNN. The CLAUDE.md policy 'every piece of work must have a backlog task before any code is written. No exceptions.' is currently auditable only by walking each PR. A commit-msg hook (paired with the existing pre-commit bump-check) would catch missing TASK references at commit time.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 .githooks/commit-msg script added that scans the staged paths and fails if the subject lacks a TASK-NNN reference for any commit touching src/OTGW-firmware/**, src/libraries/**, docs/**, or backlog/**
- [x] #2 Hook recognises an explicit exemption tag (e.g. [no-task] or chore(housekeeping):) for legitimate housekeeping commits (daily reports, gitignore tweaks)
- [x] #3 Hook reads bypass env-var OTGW_TASK_HOOK_DISABLE=1 (matches existing bump-hook bypass convention)
- [x] #4 core.hooksPath documented in CLAUDE.md / .githooks/README so new clones pick the hook up
- [x] #5 Self-test commit: a commit with no TASK ref is blocked; same commit with TASK ref or exemption tag passes
<!-- AC:END -->
