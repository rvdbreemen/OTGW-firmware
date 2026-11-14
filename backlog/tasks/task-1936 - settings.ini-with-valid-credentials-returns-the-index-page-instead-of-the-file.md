---
id: TASK-1936
title: >-
  /settings.ini with valid credentials returns the index page instead of the
  file
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-14T13:45:00Z'
updated_date: '2026-11-14T14:08:00Z'
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
- [x] #1 Find which route answers //settings.ini and why
- [ ] #2 Either serve the file for that spelling or answer 404; no index page
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Root cause: server.on("/", HTTP_GET, sendIndex) in FSexplorer.ino turns "/" into a BackwardCompatible matcher in ESPAsyncWebServer 3.11.2 (WebServer.cpp AsyncURIMatcher::matches: path == uri || path.startsWith(uri + "/")), so every "//<file>" request matches the root route; sendIndex() asks for the password (401 without) and then serves v2.html.gz. Fix: register the root with AsyncURIMatcher::exact("/"); "//<file>" then reaches handleFile(), which canonicalises the path (TASK-1934) and applies the settings.ini check. The other index routes (/index, /index.html, /v2.html) stay as they are (minimal change; their prefix match only catches unusual paths).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-14 (09): tests/test_root_route_exact.py (source check of the route registration; the matcher itself is library code the host harness does not compile): FAIL on origin/dev ('root route is "/"'), PASS on the fix. test_web_file_scheduling PASS 0; evaluate.py --quick 92 checks, 0 failed. Behaviour on the device follows on the bench: old 245 gave the index page (204373 B) for //settings.ini with credentials.
<!-- SECTION:NOTES:END -->
