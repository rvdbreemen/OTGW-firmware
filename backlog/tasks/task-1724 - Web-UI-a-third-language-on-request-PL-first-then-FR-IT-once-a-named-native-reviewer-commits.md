---
id: TASK-1724
title: >-
  Web UI: a third language on request (PL first, then FR/IT) once a named native
  reviewer commits
status: To Do
assignee: []
created_date: '2026-11-08T09:45:00Z'
labels:
  - i18n
  - webui
  - needs-maintainer-ok
dependencies: []
priority: low
ordinal: 833000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-08 adopted the TASK-1689 language advice: no third language now. A language is added only when a named native-speaker reviewer commits to keep it current. Order of candidates: Polish (OTGW community on arturhome.pl, 13/158 located issue authors), then French and Italian; Spanish and Greek not unless a named reviewer appears. Evidence and refuted claims in TASK-1689 notes (workflow wf_604b1a26-22f).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A named native reviewer for the language has committed (name in the notes)
- [ ] #2 Font coverage decided for the language (Polish: Inter covers 2 of 18 diacritics; Greek: Inter has no Greek glyphs)
- [ ] #3 Pack passes the coverage and placeholder test; a language that stays below the agreed coverage for two releases is removed
<!-- AC:END -->
