---
id: TASK-1658
title: Verify TASK-1628 build-stamp restore in a real cloud session
status: Done
assignee:
  - '@claude'
created_date: '2026-11-05T13:09:00Z'
updated_date: '2026-11-05T17:46:00Z'
labels:
  - process
  - agentic
dependencies: []
priority: low
ordinal: 782000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Moved out of TASK-1628 (AC3) by Robert's decision on 2026-11-05 (AskUserQuestion). The fix in .claude/hooks/session-start-build-toolchain.sh restores version.h and data/version.hash after the SessionStart build when they were clean before it. So far it has been verified only by emulation in Git Bash. Verify it at the next cloud session on dev.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 In a fresh cloud session on dev, git status straight after the session start is clean (no version.h or data/version.hash change)
- [ ] #2 The Stop hook does not ask to commit version.h in that session
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-05 (session otgw-firmware-54): RED evidence from a real cloud session on the pre-fix hook, read with RemoteTrigger get_run_log on run cse_01PxtYRVPWLK2z2awCijUtXr. At 2026-11-04 22:02:11Z and 22:47:27Z the Stop hook said 'There are uncommitted changes in the repository. Please commit and push these changes to the remote branch.'; git status showed M src/OTGW-firmware/data/version.hash and M src/OTGW-firmware/version.h, with 0 _VERSION_PRERELEASE lines changed (build stamp only). The fix c67efd11a landed on dev at 2026-11-04 23:58Z, after both runs. GREEN evidence still open: that sandbox reuses its old clone on branch claude/amazing-babbage-j1mdcm, so it cannot prove the fix. Note for the green run: when ~/.cache/otgw-toolchain-ready exists the hook skips the build, and a clean status then proves nothing; the run must show the build path ran (fresh /tmp/otgw-toolchain-setup.log), or rerun the old hook (git show c67efd11a^:...) and the current hook in that container after removing the marker. Prompt ready in the session scratchpad (cloud_1658_prompt.txt). Starting the cloud session from this local session (claude --cloud) was blocked by the auto-mode classifier; Robert has to start it.

AC1 and AC2 deviation (Robert 2026-11-05, session otgw-firmware-54, AskUserQuestion: 'Done op emulatie'): no fresh cloud session was run, because starting one from the local session was blocked by the auto-mode classifier. Both ACs stay unchecked. Closed on the red cloud evidence (run cse_01PxtYRVPWLK2z2awCijUtXr, 2026-11-04 22:02Z and 22:47Z: version.h + data/version.hash dirty, Stop hook asked to commit) plus the Git Bash emulation of the fix from TASK-1628. Accepted risk: if the restore behaves differently in a real container, the next cloud session on dev shows the Stop-hook message again; then reopen this task.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Closed by maintainer decision without the green cloud run. AC1 and AC2 NOT verified in a fresh cloud session (unchecked, deviation in notes). Evidence: red cloud run on the pre-fix hook (cse_01PxtYRVPWLK2z2awCijUtXr: Stop hook asked to commit the version.h build stamp, 0 _VERSION_PRERELEASE lines changed); the fix c67efd11a was verified only by Git Bash emulation (TASK-1628). Reopen if a cloud session on dev shows the Stop-hook version.h message again.
<!-- SECTION:FINAL_SUMMARY:END -->
