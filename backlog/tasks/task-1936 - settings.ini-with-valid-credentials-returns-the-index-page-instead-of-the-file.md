---
id: TASK-1936
title: >-
  /settings.ini with valid credentials returns the index page instead of the
  file
status: Done
assignee:
  - '@claude'
created_date: '2026-11-14T13:45:00Z'
updated_date: '2026-11-14T14:31:00Z'
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
- [x] #2 Either serve the file for that spelling or answer 404; no index page
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Root cause: server.on("/", HTTP_GET, sendIndex) in FSexplorer.ino turns "/" into a BackwardCompatible matcher in ESPAsyncWebServer 3.11.2 (WebServer.cpp AsyncURIMatcher::matches: path == uri || path.startsWith(uri + "/")), so every "//<file>" request matches the root route; sendIndex() asks for the password (401 without) and then serves v2.html.gz. Fix: register the root with AsyncURIMatcher::exact("/"); "//<file>" then reaches handleFile(), which canonicalises the path (TASK-1934) and applies the settings.ini check. The other index routes (/index, /index.html, /v2.html) stay as they are (minimal change; their prefix match only catches unusual paths).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-14 (09): tests/test_root_route_exact.py (source check of the route registration; the matcher itself is library code the host harness does not compile): FAIL on origin/dev ('root route is "/"'), PASS on the fix. test_web_file_scheduling PASS 0; evaluate.py --quick 92 checks, 0 failed. Behaviour on the device follows on the bench: old 245 gave the index page (204373 B) for //settings.ini with credentials.

2026-11-14 (09) bench .63, beta.246+fbcf1e6 (pushed fbcf1e6db): on the old 245 //settings.ini and //reboot_log.txt returned the index page (204373 B). On 246 without a password //settings.ini returns the settings file and //reboot_log.txt the log; / and /index still serve the web UI; //api/v2/settings and //api/v2/device/info answer 404 (the API is not reached through '//'). With a temporary admin password: //settings.ini 401 without and the file with credentials; / 401 without; /api/v2/settings 401 without, 200 with; /ReBoot/x and /ResetWireless/x 401 without credentials (not called with credentials). Settings diff [] after removing the password; crashlog unchanged. Evidence: 09 scratchpad b246/.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Cause: ESPAsyncWebServer 3.11 made the plain '/' route a BackwardCompatible matcher (also every path starting with '//'), so //<file> got the index page. Fix: AsyncURIMatcher::exact('/'). AC1: cause found (WebServer.cpp AsyncURIMatcher::matches). AC2: //settings.ini now serves the file (behind the password when one is set); bench .63 beta.246 red on 245, green on 246. Not covered by a behaviour host test: tests/test_root_route_exact.py checks the registration only; the behaviour is the bench evidence.
<!-- SECTION:FINAL_SUMMARY:END -->
