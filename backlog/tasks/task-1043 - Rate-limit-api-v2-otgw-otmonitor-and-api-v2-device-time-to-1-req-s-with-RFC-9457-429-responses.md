---
id: TASK-1043
title: >-
  Rate-limit /api/v2/otgw/otmonitor and /api/v2/device/time to 1 req/s with RFC
  9457 429 responses
status: In Progress
assignee:
  - '@claude'
created_date: '2026-07-19 21:31'
updated_date: '2026-07-19 21:33'
labels: []
dependencies: []
priority: high
ordinal: 162000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The OTGW web UI polls two endpoints once per second per open page, from two independent 1s timers in data/index.js (line 269 refreshOTmonitor -> /api/v2/otgw/otmonitor, line 281 refreshDevTime -> /api/v2/device/time). Every extra open tab therefore adds a further 2 requests per second, indefinitely, whether or not anyone is looking at it.

Field evidence (TASK-1037, martreides 2026-07-19): two open pages produced 14500 requests in one hour, a sustained 242/min, and the device died at the 60 minute mark. The firmware has no defence against this: a user who leaves a dashboard open on a second screen can degrade the gateway without any indication that they are doing so.

Decision: enforce the poll rate server-side on these two endpoints, at 1 request per second, and answer excess requests with 429 rather than serving them.

Response shape, per RFC 6585 and RFC 9457:
- Status 429 Too Many Requests
- Retry-After with the seconds until the window reopens (mandatory, the interoperable part)
- Cache-Control: no-store so the error is never cached
- Content-Type: application/problem+json with type, title, status and detail members
- RateLimit and RateLimit-Policy headers per the IETF httpapi draft. Note these are draft, not standard, as of July 2026; Retry-After is what clients can be relied on to honour.

429 is the correct code here rather than 503: the limit belongs to a specific caller exceeding a quota, not to the service being unavailable as a whole. The existing 503 responses from the heap gate stay as they are, because those genuinely signal a device-wide condition.

Scope note requiring a decision: this task implements a per-endpoint global budget, not a per-client one. A global budget is the only variant that actually caps aggregate load, which is the point, and it costs 8 bytes of state. The trade-off is that a single well-behaved client can receive 429 because another client is polling, which deviates from the per-client reading of 429 semantics. Per-client would need an IP table and would let N clients each poll at the full rate, defeating the purpose.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Both endpoints serve at most 1 request per second; excess requests receive 429
- [ ] #2 429 response carries Retry-After, Cache-Control no-store, and an application/problem+json body with type, title, status and detail
- [ ] #3 No other endpoint is rate-limited, in particular flash upload and crashlog polling are unaffected
- [ ] #4 Rate-limit state costs no dynamic allocation and no String usage
- [ ] #5 Build passes and evaluator shows no new failures
- [ ] #6 Web UI behaviour under 429 verified: no console error storm, no stuck display
<!-- AC:END -->
