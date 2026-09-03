---
id: TASK-1119
title: >-
  Express the release sync to main as an explicit merge, without moving the
  release worktree
status: To Do
assignee: []
created_date: '2026-09-03 19:32'
labels: []
dependencies: []
ordinal: 209000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up to TASK-1118. The maintainer wants the sync into main expressed as a real git merge of otgw-1.x.x, not the ref push (git push origin otgw-1.x.x:main) that TASK-1118 introduced as a safety measure.\n\nTwo constraints have to be honoured at the same time. First, main is not checked out in any worktree, so a real merge needs a working tree; running it in the release worktree would temporarily move that worktree off otgw-1.x.x, which is the exact hazard the Phase 0 preflight exists to prevent, and an interruption would leave it parked on main. Second, the merge must be allowed to fast-forward and must never use --no-ff: a merge commit on main would be a commit otgw-1.x.x does not contain, so the Phase 3 guard (main strictly behind) would trip at the next release, and the only way to clear it would be merging main back into the 1.x line, which is the wrong direction for a one-way release sync.\n\nResolution: perform the merge in a throwaway worktree created for main, then remove it. That gives a genuine merge operation while the release worktree never leaves its branch, and it keeps main a strict subset of otgw-1.x.x.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The sync step performs an actual git merge of otgw-1.x.x into main rather than a ref push
- [ ] #2 The release worktree stays on otgw-1.x.x throughout, including if the merge fails
- [ ] #3 The merge is allowed to fast-forward and --no-ff is explicitly ruled out, with the reason recorded
- [ ] #4 The throwaway worktree is removed again, and the skill says what to do if it is left behind
<!-- AC:END -->
