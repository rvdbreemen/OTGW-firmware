---
id: TASK-1245
title: 'settings.ini: SATblenameprefix is escaped twice, in place in cMsg'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-17 21:53'
updated_date: '2026-10-17 21:57'
labels:
  - bug
  - settings
dependencies: []
ordinal: 383000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Code reading during TASK-1244, not reproduced. writeSettings() escapes settings.sat.sBleNamePrefix into cMsg and then passes cMsg to writeJsonStringKV, which escapes cMsg into cMsg again (src == dest). escapeJsonStringTo (jsonStuff.ino:14) is not safe in place: an expanding escape writes ahead of the read pointer. A prefix with a quote or backslash would be written double-escaped or corrupted. Plain prefixes are unaffected.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Bench: a prefix with a quote shows the defect in settings.ini on the old firmware
- [ ] #2 Fix passes the raw prefix to writeJsonStringKV; same prefix round-trips through settings.ini and REST unchanged
<!-- AC:END -->
