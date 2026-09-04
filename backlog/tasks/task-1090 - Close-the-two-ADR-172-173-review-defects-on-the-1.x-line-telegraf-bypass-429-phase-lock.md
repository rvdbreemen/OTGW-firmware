---
id: TASK-1090
title: >-
  Close the two ADR-172/173 review defects on the 1.x line (telegraf bypass, 429
  phase-lock)
status: Done
assignee:
  - '@claude'
created_date: '2026-08-25 19:46'
updated_date: '2026-09-04 06:27'
labels:
  - bug
dependencies: []
priority: medium
ordinal: 189000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
1.x-side counterpart of TASK-1057, whose file lives in the 2.0.0 backlog while the work lands here. The commit-msg hook requires the referenced task record to be tracked in the same worktree, which is what surfaced the mismatch.

Defect A: /api/v2/otgw/telegraf and /api/v2/otgw/otmonitor route to the same handler and return the same payload, but kRateLimitedRoutes listed only otmonitor. A client switching to the telegraf path polled with no cap, defeating ADR-086.

Defect B: checkApiRateLimit keeps a single lastServedMs window per route with burst 1, and index.js handles 429 by skipping the cycle without ever re-phasing. Two dashboards that land in the same window refuse each other every cycle and freeze silently.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The telegraf and otmonitor paths share ONE rate-limit budget, so alternating between them cannot exceed the cap
- [x] #2 A 429 response causes the client to re-phase with a random offset, so two dashboards cannot stay locked in mutual refusal
- [x] #3 Build green and evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-09-04 board cleanup: half of this task shipped in v1.7.5, half did not. Leaving it In Progress with an accurate split rather than closing it.

Done and released (AC #1, AC #3): the telegraf and otmonitor paths share ONE rate-limit budget. They route to the same handler and return the same payload, but only otmonitor was listed as rate limited, so a client on the telegraf path polled uncapped. A second table row would have carried its own timestamp and let a client alternate between the two paths at twice the intended rate, so they share one entry. Commit 5580e223b, in the v1.7.5 CHANGELOG, ADR-086.

Still open (AC #2): a 429 does not cause the client to re-phase with a random offset, so two dashboards polling in lockstep can stay in mutual refusal indefinitely. This is frontend jitter in the polling code, not firmware, and is the only remaining work on this task.

2026-09-04: AC #2 implemented and verified.

On a 429 both UI pollers now clear and re-arm their timer after a delay drawn uniformly from one full poll period, giving the refused client a uniformly random phase. Skipping the cycle, which is what the catch handlers did before, preserved the lock instead of breaking it.

Verified three ways rather than by inspection:
1. Simulation of the ADR-086 one-request-per-second window, two clients starting in phase, 200 seeds, 120 s each. Before: 200/200 runs left one client with ZERO grants at both 2 s and 5 s periods. After: 0/200 starved. The 5 s poller converges in one step (24 vs 24 grants, 0 vs 1 refusals); the 2 s poller keeps churn because two 2 s clients against a 1 s budget sit exactly at saturation, but total useful responses rise from 60 to 73 over two minutes and both clients are served.
2. Shipped to the bench gateway at 192.168.88.68 and confirmed present in the served asset: /index.js is 263962 bytes and contains both re-phase functions and both call sites.
3. Confirmed the server actually produces the condition: three rapid GETs to /api/v2/otgw/otmonitor returned 200, 429, 429.

Gates: full build completed successfully (1.7.6-beta.1+a8436a6), evaluate.py --quick 35/37 pass 0 failures, node --check clean.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Both ADR-172/173 review defects on the 1.x line are closed.

Shared rate-limit budget (shipped v1.7.5): /api/v2/otgw/telegraf and /api/v2/otgw/otmonitor route to the same handler and return the same payload, but only otmonitor was listed as rate limited, so a client on the telegraf path polled uncapped. Both paths now share ONE table entry; a second row would carry its own timestamp and let a client alternate between the two paths at twice the intended rate.

429 phase lock (this change): the gateway grants one request per second per endpoint, and the UI pollers run on setInterval, which keeps a fixed phase. Two dashboards opened together polled at the same instants forever, so one won every window and the other was refused every window, permanently. On a 429 the timer is now re-armed after a delay drawn uniformly from one full poll period. The cost is bounded by one skipped cycle, which is what a 429 already cost.

Measured: before, 200 of 200 simulated runs left one client with zero grants over two minutes, at both the 2 s and the 5 s poll period. After, none. Details and the on-device confirmation are in the implementation notes.
<!-- SECTION:FINAL_SUMMARY:END -->
