---
id: TASK-1193
title: >-
  Classic SAT page start bursts about 5 API calls past the 2-request cap:
  markers, status and weather get 503 and the marker list stays empty
status: To Do
assignee: []
created_date: '2026-10-02 05:52'
labels:
  - bug
  - webui
  - sat
dependencies: []
priority: high
ordinal: 315000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found during the TASK-1172 AC#6 bench run (2026-10-02, OTGW32, alpha.397 app + LittleFS, Playwright capture on headless Edge). Opening the classic SAT page through the top navigation runs satPage() -> SAT.start(), which fires fetchStatus(), fetchWeather(), loadMarkers(), fetchSlaveConfig() and fetchDHWBounds() at once, next to refreshDevTime() and the still-running polls. That is more than the 2 concurrent API calls the REST gate admits (ADR-165, REST_MAX_INFLIGHT 2), and CLAUDE.md forbids bursting past it. Measured over 5 opens: GET /api/v2/sat/markers 503 on 4 of 5 opens, and loadMarkers() has no retry, so the marker list stayed empty (console '[SAT] marker load error: Service Unavailable' x4); sat/status 503 x4, sat/weather 503 x5, otdirect/status 503 x5, otgw/otmonitor 503 x5, otgw/messages/3 and /48 503 x2, device/info 503 x2, device/time 429 x6 (ADR-172/173 rate limit). The main page load shows the same pattern at a smaller scale (6 API requests at once on load, browser peak 7 in flight; device/info, device/time and filesystem/hash-check 503 within the first seconds), but there fetchWithRetry (TASK-1033) recovers and nothing stays broken. No REST slot leaks: after the session, telnet 'z' plus a sequential probe shows hwm 1/1. Design choice for the maintainer before implementing: (1) serialize SAT.start()'s initial fetches as one sequential chain (smallest change, SAT page only); (2) give the SAT fetches fetchWithRetry like TASK-1033 (recovers but keeps the burst); (3) one 2-slot queue for every /api/ fetch in the classic bundle (covers the main page and future call sites, a new frontend mechanism). Evidence: %LOCALAPPDATA%/OTGW-capture/task1172-bench-20261002/t1172-FIX-ui-p2-capture.json and t1172-FIX-ui-part1-capture.json.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Opening the classic SAT page 5 times through the top navigation, on the bench with a Playwright/CDP capture, returns 200 for every /api/v2 request (no 503 and no 429), and the marker list shows the stored markers on every open
- [ ] #2 Old-vs-fix: the same 5-open capture on the current build shows the 503s and the empty marker list, and on the fix does not
- [ ] #3 No request path in the change sends more than 2 /api/ requests in flight at once, verified from the capture's request timeline
- [ ] #4 build.bat for esp32-combo prints its SUCCESS line for firmware and filesystem; python evaluate.py --quick shows no new failures; the change lands in one commit with its own prerelease bump
<!-- AC:END -->
