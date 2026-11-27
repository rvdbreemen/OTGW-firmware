---
id: TASK-3127
title: >-
  Web UI: remove em dashes from UI strings (SAT title v2.js:1055 and :4320, fs
  usage line key) in en/nl/de/el
status: To Do
assignee: []
created_date: '2026-11-27T13:49:00Z'
labels:
  - webui
milestone: 2.0.0
dependencies: []
ordinal: 1035000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Rule from Robert: no em dashes in anything we write. UI strings still carry them: the SAT title (data/v2.js:1055 i18n title, :4320 category title) and the filesystem usage line (793.3 KB of 2.00 MB, 1.23 MB free). Plain text change only, in en and the nl/de/el packs; find the rest with a grep over data/v2.js and i18n/*.json. Lane after the B1 tag.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 No em dash in the SAT titles and the fs usage line in en/nl/de/el
- [ ] #2 grep for the em dash in the UI string sources shows only strings the task lists as kept, or none
- [ ] #3 i18n-lang, i18n-no-hardcoded and lang-menu one-file tests green
<!-- AC:END -->
