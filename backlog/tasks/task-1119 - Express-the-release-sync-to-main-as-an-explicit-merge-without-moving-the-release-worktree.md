---
id: TASK-1119
title: >-
  Express the release sync to main as an explicit merge, without moving the
  release worktree
status: Done
assignee:
  - '@claude'
created_date: '2026-09-03 19:32'
updated_date: '2026-09-03 19:35'
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
- [x] #1 The sync step performs an actual git merge of otgw-1.x.x into main rather than a ref push
- [x] #2 The release worktree stays on otgw-1.x.x throughout, including if the merge fails
- [x] #3 The merge is allowed to fast-forward and --no-ff is explicitly ruled out, with the reason recorded
- [x] #4 The throwaway worktree is removed again, and the skill says what to do if it is left behind
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented in .claude/skills/release/SKILL.md, Phase 6 step 5, with Phase 7 step 4 and the Important rules pointing at the same form.

The merge runs as `git -C ../wt-release-main merge otgw-1.x.x` in a worktree created for main and removed afterwards, so the release worktree never leaves its branch. A stale worktree from an interrupted run is called out with the command to clear it, because it holds the branch and blocks the retry.

The --no-ff prohibition is stated twice, in the step and in the Important rules, with the consequence spelled out rather than asserted: a merge commit lives only on main, main then sits ahead of the release branch, the Phase 3 guard trips at the next release, and clearing it would require merging main back into the 1.x line against the one-way direction of the sync.

Worth recording for whoever reads this later: with main strictly behind, the ref push, the worktree merge and a checkout-in-place merge all produce the identical commit. The choice between them is about safety and readability, not about the resulting history. Only --no-ff would have produced a materially different and worse result.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Expressed the release sync to main as a real merge without exposing the release worktree.

TASK-1118 synced main with `git push origin otgw-1.x.x:main`, chosen because it needs no working tree. The maintainer wants a merge. The merge now runs in a throwaway worktree created for main and removed afterwards, so it is a genuine git merge while the release worktree stays on otgw-1.x.x even if the run is interrupted.

The substantive part is the constraint that came with it: the merge must fast-forward and --no-ff is ruled out in both the step and the Important rules. A merge commit would exist only on main, putting it ahead of the release branch, tripping the Phase 3 guard at the next release, and forcing a merge of main back into the 1.x line to clear it. That is the wrong direction for a one-way release sync, and it would permanently cost the invariant that main is a strict subset of otgw-1.x.x.
<!-- SECTION:FINAL_SUMMARY:END -->
