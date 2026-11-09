---
id: TASK-1747
title: >-
  Web UI: the health poll fires simulate and overrides together, the last
  two-tab 503 source after TASK-1741
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-09T08:41:00Z'
updated_date: '2026-11-09T09:02:00Z'
labels:
  - bug
  - webui
  - rest
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 850000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
After TASK-1741 (beta.160) two visible tabs on Monitor > Values still get 1.9 REST 503s per minute on bench .61 (38/2340 in 20 min, 2026-11-09). 26 of the 38 fall on /api/v2/otdirect/overrides, 8 on otmonitor. fetchConn (v2.js:1892-1896) fires fetchSim() and fetchOverrides() at once after /v2/health, so one tab puts 2 requests in flight (the REST cap, ADR-165), and the other tab's otmonitor tick or its own chain then gets 503. Option: chain simulate behind overrides so one tab has at most 1 request from this chain in flight; measure with the TASK-1741 harness. Baseline evidence: %LOCALAPPDATA%/OTGW-capture/bench-1741/runs (GREEN-2vis-b160).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Two visible tabs on Monitor > Values: 503s on otdirect/overrides and simulate measured before and after on a no-PSRAM bench unit (same harness, 20 min each)
- [x] #2 The health chain keeps at most one request in flight per tab; host test red on the old chain, green on the fix
- [ ] #3 First paint of the Connection page is at most one extra round trip slower
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
v2.js only. (1) fetchConn: chain the health follow-ups one at a time: overrides, then otdirect/status (OT-Direct), then simulate; fetchSim returns its promise. (2) fetchOverrides: a busy 503 keeps ovrRows and the badge (it cleared them). Verify: tests/webui/health-chain-one-in-flight.test.mjs red on the old code, green on the fix; bench .61 two visible tabs with the TASK-1741 harness, 20 min, against GREEN-2vis-b160 (1.9/min, 26 on overrides).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-09 host red/green (session 4c, wt-1741 on 5ef19cdbe): tests/webui/health-chain-one-in-flight.test.mjs, real v2.html/v2.js, each chain response held 300 ms by a beforeScript. Old code 0/2: '2 chain requests in flight at once (v2/simulate v2/otdirect/overrides v2/otdirect/status)' and 'after a busy 503 the badge still shows 1 injected override' failed (the 503 cleared ovrRows). Fix 2/2. Related webui tests (rest-503-keeps-state, otd-override-clear, otd-hidden-in-pic-mode, poll-spread-hidden, connection-map-text-fits, otd-mode-running-note) 19/19. AC3 by reading: the Connection page's override list and OT-Direct status come first as before (overrides was already first in the chain, status already waited for it); only the SIMULATION badge now waits for them, one or two round trips later. AC1 waits for the build slot and the .61 bench (after tonight's release).

2026-11-09 full webui suite on the 1747 diff: 295 tests, 295 pass, 0 fail, 0 skipped.
<!-- SECTION:NOTES:END -->
