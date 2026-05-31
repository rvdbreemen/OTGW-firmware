---
id: TASK-775
title: 'chore: commit boiler tooltip fix and port to 2.0.0 branch'
status: In Progress
assignee:
  - '@codex'
created_date: '2026-05-31 08:55'
updated_date: '2026-05-31 08:56'
labels:
  - webui
  - process
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Commit and push the completed boiler unsupported badge tooltip fix on dev, add Codex guidance to AGENTS.md that completed tasks should be committed and pushed, then apply the same WebUI fix to the 2.0.0 feature worktree/branch and commit/push it there as well. Keep staging narrow and leave unrelated local files untouched.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The dev branch has a commit containing the boiler unsupported badge tooltip fix, AGENTS.md guidance, and relevant Backlog metadata, with unrelated .claude changes left unstaged.
- [ ] #2 The dev branch is pushed to its configured upstream.
- [ ] #3 The same boiler unsupported badge tooltip/readout behavior is implemented in the 2.0.0 feature worktree/branch.
- [ ] #4 The 2.0.0 feature branch changes are committed and pushed to its configured upstream, without staging unrelated worktree changes.
- [ ] #5 Validation is run for both worktrees where practical, and any unavailable checks are documented.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Update AGENTS.md in dev with explicit completed-task commit/push guidance.
2. Stage only the dev UI fix, AGENTS.md, and task metadata; commit and push dev.
3. Apply the same UI fix to the 2.0.0 feature worktree, preserving local unrelated changes.
4. Validate, commit, and push the feature branch.
5. Update TASK-775 with exact commit/push/validation results and close it.
<!-- SECTION:PLAN:END -->
