---
id: TASK-1244
title: 'settings.ini writer: helpers own the comma separator (no per-call withComma)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-17 21:34'
updated_date: '2026-10-17 21:35'
labels:
  - settings
  - refactor
dependencies: []
ordinal: 382000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1242. writeSettings() passes withComma per call (~185 calls), so appending a key, or a build whose last key sits in an #if block, can silently produce invalid JSON; the line-based reader never notices. The TASK-1242 guard test runs nowhere automatically. Fix: the writeJson*KV helpers write the separator before every entry except the first (static first-entry flag reset by writeSettings), and the withComma parameter is removed. Output must stay byte-identical.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Guard check in evaluate.py fails on the current code (withComma still present) and passes on the fix
- [ ] #2 settings.ini written by the fix is byte-identical to the alpha.431 file for the same settings (bench, combo)
- [ ] #3 json.load passes and REST settings are unchanged (176/176) after the update
<!-- AC:END -->
