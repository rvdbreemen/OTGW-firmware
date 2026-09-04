---
id: TASK-1090
title: >-
  Close the two ADR-172/173 review defects on the 1.x line (telegraf bypass, 429
  phase-lock)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-08-25 19:46'
updated_date: '2026-09-04 06:03'
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
- [ ] #2 A 429 response causes the client to re-phase with a random offset, so two dashboards cannot stay locked in mutual refusal
- [x] #3 Build green and evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-09-04 board cleanup: half of this task shipped in v1.7.5, half did not. Leaving it In Progress with an accurate split rather than closing it.

Done and released (AC #1, AC #3): the telegraf and otmonitor paths share ONE rate-limit budget. They route to the same handler and return the same payload, but only otmonitor was listed as rate limited, so a client on the telegraf path polled uncapped. A second table row would have carried its own timestamp and let a client alternate between the two paths at twice the intended rate, so they share one entry. Commit 5580e223b, in the v1.7.5 CHANGELOG, ADR-086.

Still open (AC #2): a 429 does not cause the client to re-phase with a random offset, so two dashboards polling in lockstep can stay in mutual refusal indefinitely. This is frontend jitter in the polling code, not firmware, and is the only remaining work on this task.
<!-- SECTION:NOTES:END -->
