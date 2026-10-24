---
id: TASK-1347
title: 'SAT wizard: do not overwrite the heating curve coefficient with 1.4/0.6/0.8'
status: To Do
assignee: []
created_date: '2026-10-24T10:11:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 470000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the SAT port audit 2026-10-24 (docs/sat/sat-port-audit-2026-10-24.md), finding #7: data/v2.js:5368,5426-5434 writes 1.4 (radiators) / 0.6 (underfloor) / 0.8 (heat pump), also over a stored value on migrate. Entry 24 (Sergeant D): default 2.0, stored values stay. Also covers finding #28 (default still 1.5, range 0.1-5 vs thermo-nova 0.1-12, const.py:182).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Wizard and migrate keep a stored coefficient; a fresh install gets 2.0 (entry 24); range matches thermo-nova or the deviation is put to Sergeant D (webui test + settings test)
<!-- AC:END -->
