---
id: TASK-1753
title: >-
  Docs: replace the outdated April 2026 RELEASE_NOTES_2.0.0.md and
  RELEASE_GITHUB_2.0.0.md before the 2.0.0 stable release
status: To Do
assignee: []
created_date: '2026-11-09T10:17:00Z'
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
- [ ] #1 RELEASE_NOTES_2.0.0.md and RELEASE_GITHUB_2.0.0.md no longer hold the April text: replaced by the reviewed stable notes (TASK-1740) or removed, with Robert's choice recorded
- [ ] #2 grep -ri otgw32 docs/releases/RELEASE_NOTES_2.0.0.md docs/releases/RELEASE_GITHUB_2.0.0.md finds nothing
- [ ] #3 No link in the repo points at a removed file (grep for the two file names)
<!-- AC:END -->
