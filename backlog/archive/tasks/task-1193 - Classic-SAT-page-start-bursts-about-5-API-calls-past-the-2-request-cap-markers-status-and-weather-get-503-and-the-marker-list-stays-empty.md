---
id: TASK-1193
title: >-
  Classic SAT page start bursts about 5 API calls past the 2-request cap:
  markers, status and weather get 503 and the marker list stays empty
status: Done
assignee:
  - '@claude'
created_date: '2026-10-02 05:52'
updated_date: '2026-10-02 18:34'
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
- [x] #1 Opening the classic SAT page 5 times through the top navigation, on the bench with a Playwright/CDP capture, returns 200 for every /api/v2 request (no 503 and no 429), and the marker list shows the stored markers on every open
- [x] #2 Old-vs-fix: the same 5-open capture on the current build shows the 503s and the empty marker list, and on the fix does not
- [x] #3 No request path in the change sends more than 2 /api/ requests in flight at once, verified from the capture's request timeline
- [x] #4 build.bat for esp32-combo prints its SUCCESS line for firmware and filesystem; python evaluate.py --quick shows no new failures; the change lands in one commit with its own prerelease bump
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Maintainer decision 2026-10-02: one central queue (option 1 of the three in the description).

1. ADR via adr-kit, accepted only after the maintainer's explicit yes: the classic bundle sends every same-origin /api/ fetch through one FIFO queue with at most 2 in flight. It is the first client-side counterpart of ADR-165's cap, which ADR-165 left undone. The ADR names the trade-offs:
   - a slow request holds a slot;
   - the v2 bundle is not covered;
   - static assets are browser subresources (ADR-147 file gate) and not covered.
2. index.js, first lines before any other code runs: wrap window.fetch for URLs containing /api/; leave other URLs untouched. On settle (resolve or reject), the slot is freed and the next queued request starts.
   - Safety valve: a request unsettled after 20 s frees its slot without being aborted, so one stalled request cannot block the UI. Only a wedged device can then push the count past 2.
   - Check the script order in index.html: the wrapper must be installed before sat.js, graph.js and the other bundles fetch.
3. device/time is rate limited (ADR-172/173: burst 2 per 4 s window) and is called on every page switch plus by its poller, so Home -> SAT within 4 s gets a 429. refreshDevTime() skips the call when the previous request started less than 4 s ago and keeps the last values.
4. Verify on the bench with a Playwright/CDP capture, old (alpha.399) vs fix:
   - 5 SAT opens through the top navigation with no 503 or 429 and the marker list filled each time;
   - the request timeline shows at most 2 /api/ requests in flight on the main page load and on SAT opens.
5. Bump, build.bat esp32-combo, evaluate.py --quick, one commit.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The classic UI sent up to 8 /api/ requests at once against a device gate that admits 2 (REST_MAX_INFLIGHT, ADR-165). On the thermostat page the refused markers GET left the marker list empty, and page switches pushed device/time past its rate limit.

Maintainer decision 2026-10-02: one central queue. ADR-184 records it and was accepted by the maintainer in session, through adr-kit after the acceptance packet, with all strict gates passing. It is related to ADR-165 and ADR-172.

Implementation (b3862df2e, alpha.400):
- The first lines of index.js wrap window.fetch. Every same-origin /api/ request waits in one first-in-first-out queue with at most 2 in flight.
- A slot frees when the response body has arrived (response.clone().arrayBuffer()), because the device holds its slot until the last byte.
- A request still open after 20 s frees its slot without being aborted.
- refreshDevTime() skips a call within 4 s of the previous request.
- CLAUDE.md, c4-code-web-assets.md and the ADR README record the queue. The README's stale ADR-165 entry is corrected to Accepted.

Evidence: Playwright on headless Edge (CDP) against the OTGW32. One main page load plus 5 thermostat page opens through the top navigation. Captures in %LOCALAPPDATA%/OTGW-capture/task1193-bench-20261002/.

AC#1, on the flashed alpha.400 build (t1193-DEV400):
- All 78 /api/v2 responses were 200: 13 on the load and 65 across the opens. No 503, no 429, no failed request.
- The markers GET was 200 on 5 of 5 opens, and the marker list was filled each time.

AC#2, old vs fix:
- OLD alpha.399 (t1193-OLD): 27 responses were 503, 2 on the load and 25 on the opens, and the markers GET was refused on 4 of 5 opens.
- FIX alpha.400: none were refused.
- Before the flash, the same result was measured with the queued index.js served to the device's page through route interception (t1193-FIX).

AC#3: in the request timeline (request start to requestfinished, body received), the peak of /api/ requests in flight was 2 in every phase on alpha.400, against 8 before.

AC#4:
- build.bat --target esp32-combo for alpha.400+b3862df: firmware SUCCESS 175.7 s, filesystem SUCCESS 19.6 s, 'Build completed successfully!'.
- The images were flashed with --update --app --fs, and the fs version.hash b3862df matched.
- evaluate.py --quick: 0 failed. tests/test_adr_governance.py: OK.
- The change is one commit with its prerelease bump.
<!-- SECTION:FINAL_SUMMARY:END -->
