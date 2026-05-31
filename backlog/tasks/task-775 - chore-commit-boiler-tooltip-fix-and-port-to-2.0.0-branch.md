---
id: TASK-775
title: 'chore: commit boiler tooltip fix and port to 2.0.0 branch'
status: Done
assignee:
  - '@codex'
created_date: '2026-05-31 08:55'
updated_date: '2026-05-31 09:07'
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
- [x] #1 The dev branch has a commit containing the boiler unsupported badge tooltip fix, AGENTS.md guidance, and relevant Backlog metadata, with unrelated .claude changes left unstaged.
- [x] #2 The dev branch is pushed to its configured upstream.
- [x] #3 The same boiler unsupported badge tooltip/readout behavior is implemented in the 2.0.0 feature worktree/branch.
- [x] #4 The 2.0.0 feature branch changes are committed and pushed to its configured upstream, without staging unrelated worktree changes.
- [x] #5 Validation is run for both worktrees where practical, and any unavailable checks are documented.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Update AGENTS.md in dev with explicit completed-task commit/push guidance.
2. Stage only the dev UI fix, AGENTS.md, and task metadata; commit and push dev.
3. Apply the same UI fix to the 2.0.0 feature worktree, preserving local unrelated changes.
4. Validate, commit, and push the feature branch.
5. Update TASK-775 with exact commit/push/validation results and close it.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Dev branch result: committed and pushed cb2eeac4 fix(webui): expose boiler tooltip (TASK-775). Commit included AGENTS.md completion guidance plus the dev WebUI files src/OTGW-firmware/data/index.html, index.js, index.css, and index_dark.css. Unrelated local .claude and scripts changes were left unstaged. Dev validation before commit: node --check src/OTGW-firmware/data/index.js passed; git diff --check passed; python evaluate.py --quick reported 36 checks, 34 passed, 0 warnings, 0 failures, 2 info.

2.0.0 feature branch result: committed and pushed 0955f39b fix(webui): expose boiler tooltip (TASK-692) to origin/feature-dev-2.0.0-otgw32-esp32-sat-support. The branch uses components.css instead of index.css/index_dark.css, so the tooltip styling was ported with design-system variables. AGENTS.md completion guidance was added there too. The feature branch commit also includes the required prerelease bump alpha.113 -> alpha.114 and generated version metadata because the branch commit hook blocks firmware/WebUI changes without a bump. Unrelated existing .claude, .wolf, backlog, and test changes in that worktree were left unstaged.

Feature branch validation: node --check src/OTGW-firmware/data/index.js passed; git diff --check for AGENTS.md and src/OTGW-firmware passed; python evaluate.py --quick reported 74 checks, 66 passed, 1 warning, 0 failures, 7 info. The single warning is the existing ESP abstraction boundary baseline warning, not caused by this UI change. The Bash bump helper bin/bump-prerelease.sh could not run directly in the Windows worktree because Git Bash mangled the worktree gitdir path; I ran the underlying autoinc-semver.py command with --prerelease alpha.114 instead.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Committed and pushed the boiler unsupported tooltip fix on both requested branches. On dev, commit cb2eeac4 adds the hover/focus tooltip readout, preserves the compact ellipsis badge, and documents the Codex completed-task commit/push rule in AGENTS.md. On feature-dev-2.0.0-otgw32-esp32-sat-support, commit 0955f39b ports the same UI behavior to the components.css design-system layout, adds the same AGENTS.md guidance, and bumps prerelease metadata to 2.0.0-alpha.114 as required by the branch hook. Both branches were pushed to origin. Validation passed with node --check and git diff --check on both worktrees; evaluate.py --quick passed with 0 failures on both, with the 2.0.0 branch retaining its known ESP abstraction warning.
<!-- SECTION:FINAL_SUMMARY:END -->
