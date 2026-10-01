---
id: TASK-1188
title: Give the poll rate limiter a burst of 2 and repeat retry_after in the 429 body
status: To Do
assignee: []
created_date: '2026-10-01 05:21'
labels:
  - rest-api
  - rate-limit
dependencies: []
priority: medium
ordinal: 240000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up to TASK-1090 (shared otmonitor/telegraf budget, 429 re-phase in the web UI), tracked on the dev line as its TASK-1057.

The limiter is a single window per route with burst 1. A Telegraf scrape beside one open dashboard is refused whenever it lands inside the 1500 ms window the dashboard's last otmonitor poll opened, which is 75% of the 2000 ms poll period. Telegraf keeps a fixed phase and the dashboard re-phases only when it is refused itself, so the pair drifts into the arrangement where Telegraf is always refused. Modelled against the real checkApiRateLimit() with a 10 s scrape: 0.6% of the scrapes answered, and 750 of 1000 phase offsets got no answer at all in 10 minutes.

Maintainer decision 2026-10-01: port the 2.0.0 line's GCRA limiter with burst 2 and a sustained rate of 1 per window (dev ADR-172), and repeat retry_after in the RFC 9457 body, because Retry-After is not CORS-safelisted and sendCorsOriginHeader() admits cross-origin browser clients.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 otmonitor and telegraf keep sharing one budget: exhausting either path returns 429 on the other (host harness on the real code, both directions)
- [ ] #2 Both limited routes allow a burst of 2 with a sustained rate of 1 per window; a Telegraf scrape beside one open dashboard is served (host harness, simulation, browser run)
- [ ] #3 The 429 problem+json body carries retry_after equal to the Retry-After header and parses as JSON at the largest values (host harness)
- [ ] #4 Two dashboards opened together are both served for 10 minutes against the real limiter (browser run)
- [ ] #5 An ADR records the decision, amends ADR-086 and cross-references the 2.0.0 line's ADR-172 and ADR-173
- [ ] #6 build.bat green with its SUCCESS line and fresh binaries, python evaluate.py exit 0, tests/test_evaluate.py green
<!-- AC:END -->
