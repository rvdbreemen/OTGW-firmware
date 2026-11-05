---
id: TASK-1628
title: Stop hook-triggered builds leaving build-stamp churn in version.h
status: Done
assignee:
  - '@claude'
created_date: '2026-11-04T22:41:00Z'
updated_date: '2026-11-05T11:21:00Z'
labels:
  - process
  - agentic
dependencies: []
priority: medium
ordinal: 755000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Observed 2026-11-04 in a cloud session: the SessionStart toolchain hook (.claude/hooks/session-start-build-toolchain.sh -> build.py) rewrites _VERSION_BUILD and _VERSION_GITHASH in src/OTGW-firmware/version.h and data/version.hash. The Stop hook then reports uncommitted changes and asks to commit them. CLAUDE.md says build-stamp churn must be restored, not committed, so the agent reverts it every turn.
Context: docs/process/agentic-setup.md (finding numbers F1-F7) and docs/process/agentic-improvements-handoff-2026-11-04.md. Parent: TASK-1624.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 After a session start in a clean clone, git status is clean (the stamp is restored by the hook or not written), or the Stop hook ignores a stamp-only diff
- [x] #2 A real _VERSION_PRERELEASE change is still reported by the Stop hook
- [ ] #3 Behaviour shown before and after in a cloud session
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-05. Fix in .claude/hooks/session-start-build-toolchain.sh: before build.py runs, record whether version.h and data/version.hash are clean; after the build (success or failure) restore them only when they were clean before and the diff has no _VERSION_PRERELEASE line. Emulation (Git Bash, the real hook script with CLAUDE_CODE_REMOTE=true, throwaway repo with the real version.h, fake build.py that stamps BUILD/GITHASH like the real one): clean tree -> OLD script leaves ' M version.h, M version.hash', NEW script leaves git status clean. PRERELEASE changed before the session (beta.500) -> kept by both. Build that changes PRERELEASE (beta.999) -> kept by both, so a real bump still reaches the Stop hook. bash -n syntax check OK.

Open: AC3 needs a real cloud session start (CLAUDE_CODE_REMOTE=true with the real build.py and toolchain). Cannot be produced from the local Windows session. Check at the next cloud session on dev: git status right after start must be clean, and the Stop hook must not ask to commit version.h.

2026-11-05 maintainer decision (AskUserQuestion): close TASK-1628 with AC3 deferred to the next real cloud session. AC3 stays unchecked on purpose: it has only the emulation evidence, no cloud-session evidence.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fix: the SessionStart toolchain hook restores version.h and data/version.hash after build.py when they were clean before the build and the diff carries no _VERSION_PRERELEASE line. AC1 and AC2 evidence: Git Bash emulation with the real hook script (CLAUDE_CODE_REMOTE=true, throwaway repo, real version.h, fake build.py that stamps like the real one). Old script leaves version.h and version.hash modified, new script leaves git status clean; a PRERELEASE change before or during the build is kept by both. AC3 (real cloud session) NOT verified: Robert waived it on 2026-11-05 as later evidence. Check at the next cloud session on dev: git status clean right after start, no Stop-hook request to commit version.h.
<!-- SECTION:FINAL_SUMMARY:END -->
