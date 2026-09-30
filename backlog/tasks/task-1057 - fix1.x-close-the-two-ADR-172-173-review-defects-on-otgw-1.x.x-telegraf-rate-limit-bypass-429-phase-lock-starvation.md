---
id: TASK-1057
title: >-
  fix(1.x): close the two ADR-172/173 review defects on otgw-1.x.x (telegraf
  rate-limit bypass, 429 phase-lock starvation)
status: To Do
assignee: []
created_date: '2026-08-03 16:58'
updated_date: '2026-09-30 12:59'
labels: []
dependencies: []
ordinal: 252000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sibling of TASK-1037 (2.0.0/dev, merged via PR #673, e39f737e). That port fixed two defects the adversarial review of 1.7.2-beta.4 found; both still stand on the otgw-1.x.x line.

Defect A - telegraf bypasses the rate limit. restAPI.ino:442 routes /api/v2/otgw/telegraf and /api/v2/otgw/otmonitor to the same handler with the same payload, but kRateLimitedRoutes[] (restAPI.ino:856) lists only { otgw, otmonitor } and { device, time }. There is no kSubTelegraf. A client that switches to the telegraf path polls unlimited, defeating the cap that ADR-086 put in place.

Defect B - 429 phase-lock starvation. checkApiRateLimit() is a single lastServedMs window per route, burst 1. index.js handles 429 by skipping the cycle (index.js:3298, 4202) but never re-phases: there is no random offset anywhere in the poll paths. Two dashboards that land in the same window refuse each other every cycle and freeze silently.

Reference implementation on dev (do NOT cherry-pick, the lines diverge): GCRA limiter with a route->budget INDEX table so aliases share one counter, burst 2, retry_after repeated in the RFC 9457 body because Retry-After is not CORS-safelisted. See restAPI.ino:2474-2510 on dev and ADR-172/ADR-173.

Note the .ino prototype-generator constraint that bit the dev port: pass the budget index (uint8_t), not a reference to a type defined in restAPI.ino, or the hoisted prototype names an undeclared type and the definition collides with it.

Needs its own worktree: wt-otgw-1.x.x already exists at D:/Users/Robert/Documents/GitHub/RvdB/wt-otgw-1.x.x.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 telegraf and otmonitor share ONE rate-limit budget on otgw-1.x.x; exhausting one returns 429 on the other
- [ ] #2 Limiter allows burst >= 2 so a Telegraf scrape alongside one open dashboard is not starved; sustained rate stays 1 per window
- [ ] #3 429 body carries retry_after in the RFC 9457 payload as well as the Retry-After header
- [ ] #4 index.js re-phases at a random offset inside its period on 429, so two dashboards cannot phase-lock; verified with two tabs for 10 minutes
- [ ] #5 ADR on the otgw-1.x.x line records the alias-budget and re-phase decisions (own numbering, cross-references dev ADR-172/173)
- [ ] #6 python build.py --target esp8266 green, python evaluate.py exit 0, tests/test_evaluate.py green in the wt-otgw-1.x.x worktree
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-09-30 (dev session): the 1.x line already fixed both defects, under its own task numbering, and shipped them in v1.7.5:
- Defect A: 5580e223b (2026-08-25) 'Share one rate-limit budget between the otmonitor and telegraf paths (TASK-1090)'. origin/otgw-1.x.x restAPI.ino: one kRateLimitedRoutes row { otgw, otmonitor, alias telegraf, 1500 ms } with a single lastServedMs.
- Defect B: 053ac9f58 (2026-09-04) 'fix(webui): re-phase polling after a 429 so a second dashboard is not starved'; index.js rephaseDelayMs()/rephaseOTmonitorPolling().
AC status against origin/otgw-1.x.x 8ba6f7ef9 (code read, not run): #1 met (shared budget); #2 NOT met (the 1.x limiter is single-window, 'at most 1 request per window', no burst 2); #3 NOT met (Retry-After header only, no retry_after in the problem+json body); #4 code present, the two-tab 10-minute check is not recorded; #5 NOT met (no 1.x ADR mentions the alias budget or the re-phase; git grep telegraf docs/adr finds only ADR-042); #6 not re-run here.
The 1.x session chose the simpler design (shared single-window budget plus client re-phase) instead of porting dev's GCRA burst 2 and the retry_after body. Maintainer decision: close this task as done differently on 1.x (accept burst 1 plus re-phase, optionally with a short 1.x ADR), or port #2, #3 and #5 in a 1.x worktree. Not changed from this session: the 1.x line may have its own active session.
<!-- SECTION:NOTES:END -->
