---
id: TASK-1753
title: >-
  Docs: replace the outdated April 2026 RELEASE_NOTES_2.0.0.md and
  RELEASE_GITHUB_2.0.0.md before the 2.0.0 stable release
status: Done
assignee:
  - '@claude'
created_date: '2026-11-09T10:17:00Z'
updated_date: '2026-11-10T18:13:00Z'
labels:
  - docs
  - release
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 855000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
docs/releases/RELEASE_NOTES_2.0.0.md (507 lines, 'Last updated 2026-04-25') and docs/releases/RELEASE_GITHUB_2.0.0.md describe a unified ESP8266/ESP32 build that 2.0.0 no longer is, and name the board publicly (14 and 8 occurrences of 'OTGW32'). Checked 2026-11-09: no v2.0.0 GitHub release exists; README and the docs index do not link either file; the only link is RELEASE_GITHUB_2.0.0.md -> RELEASE_NOTES_2.0.0.md (on main); .claude/skills/update-docs-2x/SKILL.md:204 already says not to start from them. Both are still readable in the public repo on dev. The stable notes draft is docs/releases/RELEASE_NOTES_2.0.0-draft.md (TASK-1740). Medium, not high: not linked from a user-facing entry point.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 RELEASE_NOTES_2.0.0.md and RELEASE_GITHUB_2.0.0.md no longer hold the April text: replaced by the reviewed stable notes (TASK-1740) or removed, with Robert's choice recorded
- [x] #2 grep -ri otgw32 docs/releases/RELEASE_NOTES_2.0.0.md docs/releases/RELEASE_GITHUB_2.0.0.md finds nothing
- [x] #3 No link in the repo points at a removed file (grep for the two file names)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Robert's choice (2026-11-10, relayed by the coordinator session, his words: "taak 1753 verwijder de bordnaam, voer taak 1753 uit"): remove. Interpretation of 'verwijder de bordnaam' (remove the board name) as removing both April 2026 files by the coordinator; removing them takes the board name out of the public repo on dev.

Done on dev: git rm docs/releases/RELEASE_NOTES_2.0.0.md and docs/releases/RELEASE_GITHUB_2.0.0.md. Mentions updated: .claude/skills/update-docs-2x/SKILL.md (now says removed by TASK-1753, start from RELEASE_NOTES_2.0.0-draft.md) and the HTML comment in docs/releases/RELEASE_NOTES_2.0.0-draft.md. Left as is: archived tasks and TASK-1740 notes (history), ADR-084:108 (Accepted ADR, immutable, plain mention, not a link). main is NOT touched: the RELEASE_GITHUB file and its link on main stay until the release flow.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: both April 2026 files removed (git rm) with Robert's choice recorded in the notes (relayed by the coordinator; 'remove' is the coordinator's reading of 'verwijder de bordnaam'). AC2: grep -ri otgw32 on both paths exits 2 (No such file or directory), so it finds nothing. AC3: git grep for a Markdown link '](...RELEASE_NOTES_2.0.0.md|RELEASE_GITHUB_2.0.0.md)' outside backlog exits 1 (no match); the only live mentions (update-docs-2x SKILL.md, draft notes comment) were updated; ADR-084:108 is an immutable plain mention. evaluate.py --quick: 92 checks, 0 failed, 0 warnings. main untouched: its RELEASE_GITHUB_2.0.0.md link stays until the release flow.
<!-- SECTION:FINAL_SUMMARY:END -->
