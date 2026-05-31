---
id: TASK-775
title: 'chore: commit boiler tooltip fix and port to 2.0.0 branch'
status: To Do
assignee: []
created_date: '2026-05-31 08:55'
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
