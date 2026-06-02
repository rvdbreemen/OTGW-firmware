---
id: TASK-808
title: >-
  fix(webui): active tab not restored on page refresh — 2.0.0 WebUI lands on
  Home
status: To Do
assignee: []
created_date: '2026-06-02 05:25'
updated_date: '2026-06-02 05:26'
labels:
  - webui
  - field-report
  - needs-repro
  - 2.0.0
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Field report @sergeantd (alpha.99, OTGW32, 2026-05-30): "when I hit refresh and I am NOT in the Home page, it reloads and lands in the home page." The 2.0.0 WebUI does not restore the active tab across a browser refresh; it always returns to Home. Code: data/index.js logs window.location.hash (index.js:139) but has no evident restore-active-tab-from-hash logic; localStorage is used for PIC settings + log/data persistence, not for the active tab. Verify still present on current alpha.139 (finding is alpha.99-era, ~40 alphas old), then persist + restore the active tab via URL hash or localStorage. Frontend-only (data/index.js); rebuild filesystem with python build.py. 2.0.0-only.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Active tab is restored after a browser refresh (you land back on the tab you were viewing, not Home)
- [ ] #2 Home remains the default on first load / when no tab state is present; no console errors
- [ ] #3 python build.py green (firmware + filesystem); evaluate.py --quick no new failures
- [ ] #4 Field-confirmed by @sergeantd on OTGW32
<!-- AC:END -->
