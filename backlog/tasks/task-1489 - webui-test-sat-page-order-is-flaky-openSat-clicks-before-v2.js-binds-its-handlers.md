---
id: TASK-1489
title: >-
  webui test sat-page-order is flaky: openSat() clicks before v2.js binds its
  handlers
status: To Do
assignee: []
created_date: '2026-10-29T22:13:00Z'
labels:
  - webui
  - tests
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 648000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by otgw-firmware-d5 during TASK-1476 (2026-10-29). tests/webui/sat-page-order.test.mjs fails intermittently on base origin/dev c6caf2096 (fail, pass, pass) and on 07ed281a4 (fail, fail, pass). First assertion: :168 'Thermostat view: dial and temperature history are visible (dial=false, hist=false)', followed by other visibility checks; the DOM-order checks pass in the same run, so the SAT page never became active. Hypothesis (not verified): openSat() (:129-140) waits only for the static '.seg[data-page="sat"]' button, so under machine load .click() lands before v2.js has bound its handler, and the statusHits wait times out. Related: sat-status-pill and v2-navigation fail only in the parallel full-suite run ('Chrome DevTools endpoint never came up') and pass alone. Logs: d5 scratchpad spo_base1.txt / spo_fix2.txt.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The failure is reproduced deterministically (e.g. by delaying v2.js init) and the hypothesis is confirmed or refuted with evidence
- [ ] #2 After the fix, sat-page-order passes 20/20 consecutive runs under load (for example with a parallel full-suite run)
- [ ] #3 The DevTools-endpoint failures in the parallel full-suite run are explained, and fixed or filed separately
<!-- AC:END -->
