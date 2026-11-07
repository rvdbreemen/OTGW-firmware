---
id: TASK-1627
title: Make the Claude Code hooks portable and their failures visible
status: Done
assignee:
  - '@claude'
created_date: '2026-11-04T22:40:00Z'
updated_date: '2026-11-04T23:44:00Z'
labels:
  - process
  - agentic
dependencies: []
priority: high
ordinal: 754000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Finding F3. The PostToolUse evaluate hook in .claude/settings.json runs: cd 'D:/Users/Robert/Documents/GitHub/RvdB/OTGW-firmware' && python evaluate.py --quick 2>/dev/null || true. In a cloud or macOS session, or another worktree, it silently checks nothing (or the wrong tree). The git hooks only run when core.hooksPath is set per clone, which a fresh cloud session does not do.
Context: docs/process/agentic-setup.md (finding numbers F1-F7) and docs/process/agentic-improvements-handoff-2026-11-04.md. Parent: TASK-1624.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The evaluate hook uses $CLAUDE_PROJECT_DIR (same pattern as the other hooks), not a hardcoded path
- [x] #2 A FAIL from evaluate.py is visible to the agent (non-zero exit or printed summary), no blanket || true; shown with a deliberate PROGMEM violation, then reverted
- [x] #3 The SessionStart hook sets git config core.hooksPath .githooks when it is unset
- [x] #4 Verified on Windows (local tree) and on Linux (cloud session or WSL)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-05. Evaluate hook replaced by .claude/hooks/post-edit-evaluate.py (stdlib), started with the same CLAUDE_PROJECT_DIR prefix as the other hooks. It runs evaluate.py --quick in the git root of the edited file (falls back to CLAUDE_PROJECT_DIR, then cwd), stays silent on pass/warn, exits 2 with the FAIL lines on stderr on FAIL (Claude Code feeds stderr back to the agent; the edit stays), and reports a 25 s timeout without blocking. New SessionStart hook sets core.hooksPath .githooks when unset and leaves an existing value alone.

Evidence (exact settings.json command strings, emulated PostToolUse payloads, CLAUDE_PROJECT_DIR unset). Windows, Git Bash, worktree wt-agentic with a temporary #include <ArduinoJson.h> in helperStuff.ino: OLD command from origin/dev exit 0 and printed 'Failed: 0, Health Score 100%' because it cd'd into the main tree; NEW command exit 2 with '[ArduinoJson] Library ban (ADR-146) ... Failed: 1' for the worktree. Docs file and clean firmware file: exit 0, no output. Linux (WSL2 6.6.87.2, native ext4 copy, no git installed so the pwd fallback was used): FAIL case exit 2 with the same message in 5 s. On /mnt/d (9P) evaluate took over 25 s and the hook reported the timeout without blocking (exit 0). hooksPath hook in a throwaway repo: unset -> .githooks, existing 'custom' kept. Baseline before the change: evaluate.py --quick 0 FAIL on origin/dev, so the hook stays silent for every session until a real violation.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The evaluate hook now works in every tree and on every OS, and a FAIL reaches the agent. AC1: .claude/hooks/post-edit-evaluate.py with the CLAUDE_PROJECT_DIR pattern replaces the inline command with the hardcoded D:/ path. AC2: on a deliberate ArduinoJson include the old command exited 0 and checked the main tree (Failed: 0), the new one exits 2 with the FAIL line; reverted afterwards. AC3: a SessionStart hook sets core.hooksPath .githooks when unset (throwaway repo: unset -> .githooks, existing value kept). AC4: verified on Windows (Git Bash) and Linux (WSL2, native fs). Note: the hooks of a running session are loaded at its start, so sessions pick this up on their next start.
<!-- SECTION:FINAL_SUMMARY:END -->
