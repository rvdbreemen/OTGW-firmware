---
id: TASK-1936
title: >-
  /settings.ini with valid credentials returns the index page instead of the
  file
status: To Do
assignee: []
created_date: '2026-11-14T13:45:00Z'
labels:
  - bug
  - webui
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 991000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen in the TASK-1934 bench on .63 (beta.242 and beta.245): GET //settings.ini (doubled leading slash, path sent as-is) with valid admin credentials answers 200 with the index page (v2.html.gz, 204373 B) instead of settings.ini; without credentials it answers 401 as expected. Probably the web server or a route matches '//' as '/' before handleFile() runs. Low impact: an unusual spelling only.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Find which route answers //settings.ini and why
- [ ] #2 Either serve the file for that spelling or answer 404; no index page
<!-- AC:END -->
