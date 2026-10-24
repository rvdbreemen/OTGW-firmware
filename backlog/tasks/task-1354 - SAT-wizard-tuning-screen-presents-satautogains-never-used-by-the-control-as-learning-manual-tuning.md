---
id: TASK-1354
title: >-
  SAT wizard: tuning screen presents satautogains (never used by the control) as
  learning / manual tuning
status: To Do
assignee: []
created_date: '2026-10-24T16:29:00Z'
labels:
  - sat
  - webui
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 489000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SAT port audit 2026-10-24 (docs/sat/sat-port-audit-2026-10-24.md) finding #67: data/v2.js:5440,5531,5540 present satautogains, which SATpid.ino:120-141 never reads, as 'SAT learns ... adjusts gains' and manual tuning. Misleads users. UI text only.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Wizard text no longer claims a learning or gains effect for satautogains (browser test + screenshot)
<!-- AC:END -->
