---
id: TASK-1489
title: >-
  webui test sat-page-order is flaky: openSat() clicks before v2.js binds its
  handlers
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-29T22:13:00Z'
updated_date: '2026-10-29T22:39:00Z'
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
- [x] #1 The failure is reproduced deterministically (e.g. by delaying v2.js init) and the hypothesis is confirmed or refuted with evidence
- [ ] #2 After the fix, sat-page-order passes 20/20 consecutive runs under load (for example with a parallel full-suite run)
- [ ] #3 The DevTools-endpoint failures in the parallel full-suite run are explained, and fixed or filed separately
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC#1 (d5, 2026-10-29): hypothesis CONFIRMED. v2.html's loader injects v2.js only after v2-bundle.css (TASK-978/989), while the .seg buttons are static HTML; init() binds their click handlers and sets window.OTGWv2 last. Deterministic repro: copy of data/ with init() delayed 3 s (setTimeout(init,3000)), OTGW_DATA_DIR=<copy>: old test 3/3 FAIL, first 'dial=false, hist=false', same 8 FAIL lines as the flaky run spo_fix2 (+ legend). Fix: openSat() waits for window.OTGWv2 (30 s cap, throws). With the slow-init copy the fixed test passes (runs 1 and 3 passed; run 2 died on the DevTools endpoint because of a port collision with 75's verifier, see AC#3). AC#3 explained: (a) fixed ports, v2-navigation and v2-settings-dirty-survives-refetch both used 8148/9340; (b) other sessions running the same test (75's workflow worktree wf_3931dcef-391-2 held 8152/9352 at 23:21) collide. Fix: all 11 browser tests listen(0) + --remote-debugging-port=0 + DevToolsActivePort. Evidence: old pair parallel -> EADDRINUSE 127.0.0.1:8148; new pair parallel both pass; sat-page-order twice in parallel both pass; each of 11 tests solo exit 0. Local commit c00030a69 in ../wt-1489 (tests only, no bump). AC#2 (20/20 under load) held until 75 says 'workflow done'.

AC#2 partial (2026-10-29 ~23:40): soak = 20x sat-page-order sequential + full tests/webui suite looping in parallel + 75's workflow. Claude Code reaped it at run 12 for low memory (commit 41.4/53.3 GB). Runs 1-12: all exit 0, 27 PASS/0 FAIL; time 12.5-34.6 s; CDP startup 1.1-10.1 s (budget 30 s here, 20 s in other tests) -> no evidence for a CDP-budget cause, budget unchanged. Follow-up commit c0aa19b55 logs CDP startup time. Load suite: 6 iterations, iters 4 and 6 had 1 failure each in pic-flash-status.test.mjs (picAfterFlash sequence missing 'banner 6.8'; picUpdatePoll banner stuck), timing-sensitive under load, not browser/port related, reported to 75 for a separate task. Not restarted: needs Robert's OK (memory).
<!-- SECTION:NOTES:END -->
