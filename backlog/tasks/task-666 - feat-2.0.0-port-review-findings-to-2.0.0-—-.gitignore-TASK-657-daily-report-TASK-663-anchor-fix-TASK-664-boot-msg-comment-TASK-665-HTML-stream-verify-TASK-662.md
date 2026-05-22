---
id: TASK-666
title: >-
  feat-2.0.0: port review findings to 2.0.0 — .gitignore (TASK-657),
  daily-report (TASK-663), anchor fix (TASK-664), boot-msg comment (TASK-665),
  HTML stream verify (TASK-662)
status: To Do
assignee: []
created_date: '2026-05-22 06:24'
labels:
  - port
  - 2.0.0
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Consolidated 2.0.0 port task for the dev review (TASK-653) — small/docs-only ports. Resetgateway hardening (TASK-661 port) is intentionally split into TASK-667 because it's a real firmware code change.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 2.0.0 /.gitignore extended with /OTGW_*.txt, /OTGW_*.log, /plan/, docs/daily-issue-report.md (mirror of dev TASK-657)
- [ ] #2 2.0.0 git rm --cached docs/daily-issue-report.md if tracked (mirror of dev TASK-663)
- [ ] #3 2.0.0 docs/guides/OPENHAB.md + docs/guides/DOMOTICZ.md anchor fix (mirror of dev TASK-664)
- [ ] #4 2.0.0 src/OTGW-firmware/OTGW-Core.ino comment added at first-OT-message branches 4067/4074/4081/4112 (mirror of dev TASK-665)
- [ ] #5 2.0.0 HTML streaming audit completed (mirror of dev TASK-662); if any handler loads into String/char buffer, fix + bump prerelease
- [ ] #6 Each change commits separately on claude/2.0.0-port-from-review-xOxi4 with the right TASK-NNN ref
- [ ] #7 python build.py --firmware passes on 2.0.0 worktree (only if a firmware-code change was needed for L1/L4)
- [ ] #8 python evaluate.py --quick passes on 2.0.0 worktree (only if firmware-code touched)
<!-- AC:END -->
