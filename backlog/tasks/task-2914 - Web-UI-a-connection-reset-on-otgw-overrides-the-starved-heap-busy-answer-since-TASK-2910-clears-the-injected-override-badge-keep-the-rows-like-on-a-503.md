---
id: TASK-2914
title: >-
  Web UI: a connection reset on otgw/overrides (the starved-heap busy answer
  since TASK-2910) clears the injected-override badge; keep the rows like on a
  503
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-19T12:44:00Z'
updated_date: '2026-11-19T13:20:00Z'
labels:
  - webui
  - rest
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1054000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Since beta.389 (TASK-2910) the device answers busy on a starved heap (largest block < 4096 B) by closing the connection without a response, instead of 503 + Retry-After. In the browser that is a rejected fetch. fetchOverrides (data/v2.js) keeps the rows on a 503 (TASK-1747) but its catch set ovrRows = [], so the injected-override badge on the connection map and the Home badge vanish under a starved heap. Other paths already treat a reset like a 503: fetchWithRetry (TASK-1033), fetchConn (health poll, SIM badge), the SAT page, the SAT wizard health check. Writes reject on both alike (no change). Follow-up of TASK-2910, coordinator 2026-11-19.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test drives the real v2.html/v2.js against a fake device that resets the connection: override badge red on the old v2.js (badge hidden), green on the fix; SAT page and SIMULATION badge cases green on both
- [x] #2 fetchOverrides tries a failed request once more after 400 ms (fetchWithRetry, sequential, inside the ADR-184 queue) and keeps the rows when it fails again; the test bounds the request count (no retry storm)
- [ ] #3 Static web UI tests and evaluate.py --quick: no new FAIL; esp32-combo build; bench: override badge on a unit after the build
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Investigation (data/v2.js at c141e1a4d): the ADR-184 queue releases the slot on a rejected fetch (startJob error branch). fetchWithRetry treats a network error like a 503. fetchConn (health, SIM badge), the SAT page and the SAT wizard health check keep their state on a reset as on a 503. Writes reject on both alike (unchanged). Only fetchOverrides differed: its catch emptied ovrRows. Fix: fetchWithRetry(url, undefined, 2) (one retry after 400 ms, sequential inside the queue) and a final error keeps the rows. Harness: { __reset: true } closes the socket with resetAndDestroy. tests/webui/rest-reset-keeps-state.test.mjs: OLD v2.js (OTGW_DATA_DIR=c141e1a4d copy) 2 pass 1 fail ('a reset must not hide the override badge: {display:none, text:⛓ 0 injected}'); FIX 3/3. Related green on the fix: rest-503-keeps-state 3/3, health-chain-one-in-flight 2/2, otd-override-clear 5/5, otd-remote-override-panel 5/5, poll-spread-hidden 5/5, settings-fetch-retry 2/2 (the first runs of the two before last stalled in CDP while a peer build ran; reruns clean). evaluate --quick 88/0.

Rebased onto origin 0f788017e as beta.391 (local). The request bound was racy on the harness: Chrome silently resends a reset GET over its next idle keep-alive socket (5 overrides GETs per health poll in the log). The device's async server closes every connection, so the harness got apiConnClose (Connection: close on /api/), used by this test; the bound is now exact: overrides <= 2 x (health polls + 1). Fix 3/3 twice; old v2.js 2 pass 1 fail (badge display none, 0 injected); mutant fetchWithRetry(url, -, 6, 100) red on the bound (19 requests for 3 polls). rest-503-keeps-state 3/3, health-chain-one-in-flight 2/2, evaluate 88/0, progmem OK.
<!-- SECTION:NOTES:END -->
