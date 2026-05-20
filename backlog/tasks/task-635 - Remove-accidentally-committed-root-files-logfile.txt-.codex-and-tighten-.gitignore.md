---
id: TASK-635
title: >-
  Remove accidentally committed root files (logfile.txt, .codex) and tighten
  .gitignore
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-20 09:30'
updated_date: '2026-05-20 09:31'
labels: []
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Two non-source files were inadvertently committed to the repo root since v1.5.0-fix2:

- `logfile.txt` (1.6 MB, ~10,643 lines) — a serial-log dump, committed via the GitHub web UI as `f863aebe Create logfile.txt`. Looks like an accidental drag-and-drop in the web editor.
- `.codex` — zero-byte file, slipped in via `b0515f6a Update task TASK-590`.

Both are unrelated to firmware behaviour, increase the repo footprint (the logfile alone bloats clones by ~1.6 MB on every checkout), and the `.gitignore` does not currently prevent the same mistake on the next round. Clean them up and add patterns that block recurrence at the root.

Why not history-rewrite: removing from the active tree is the KISS option. History rewrite would invalidate every contributor checkout and CI history reference; the file is not a secret (just a debug log). Live with the historical blob; prevent future ones.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 logfile.txt removed from working tree (git rm)
- [ ] #2 .codex removed from working tree (git rm)
- [ ] #3 .gitignore extended with /logfile.* and /.codex root-anchored patterns to prevent recurrence
- [ ] #4 Repository builds and existing CI still pass (docs/tooling-only path; no firmware source touched, no version bump required per project policy)
- [ ] #5 Commit pushed to development branch and a draft PR opened against dev
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. git rm logfile.txt .codex (working-tree removal; history retained)
2. Extend .gitignore with /logfile.* and /.codex root-anchored patterns
3. Commit on current branch (claude/review-commits-refactor-plan-Hho3Q) — docs/tooling-only path, no version bump
4. Push branch and open draft PR against dev
5. Check ACs, add final summary, mark Done
<!-- SECTION:PLAN:END -->
