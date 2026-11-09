---
id: TASK-1747
title: >-
  Web UI: the health poll fires simulate and overrides together, the last
  two-tab 503 source after TASK-1741
status: To Do
assignee: []
created_date: '2026-11-09T08:41:00Z'
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
- [ ] #2 The health chain keeps at most one request in flight per tab; host test red on the old chain, green on the fix
- [ ] #3 First paint of the Connection page is at most one extra round trip slower
<!-- AC:END -->
