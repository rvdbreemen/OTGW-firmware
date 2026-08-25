---
id: "ADR-172"
title: "Rate-Limit the UI-Polled REST Endpoints with RFC 9457 429 Responses"
status: "Proposed"
date: "2026-07-26"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
---
# ADR-172 Rate-Limit the UI-Polled REST Endpoints with RFC 9457 429 Responses

## Status

Proposed. Date: 2026-07-26.

Port of `otgw-1.x.x` ADR-086, with four deliberate divergences (below).

## Status History

status_history:
  - date: 2026-07-26
    status: Proposed
    changed_by: Agent
    reason: Ports 1.x ADR-086 (TASK-1043) to the 2.0.0 async line as part of TASK-1037, incorporating two defects found by adversarial review of 1.7.2-beta.4.
    changed_via: manual

## Context

The classic web UI refreshes from two REST endpoints on independent timers:
`refreshOTmonitor()` against `/api/v2/otgw/otmonitor` and `refreshDevTime()`
against `/api/v2/device/time`. Both run for as long as the page is loaded.

A 60-minute field capture on the 1.x line (device `otgw-48E72958B013`, reporter
martreides, 2026-07-19) recorded 14 500 REST requests in the hour: 7192 on
otmonitor, 7190 on device/time, 118 on device/info — a sustained 242 req/min from
two open pages. The device ran out of heap and died at the 60-minute mark, last
log line `HEAP-FRAG: skip MQTT (maxBlock=760, heap=1608)`.

This branch has an in-flight backpressure gate (`REST_MAX_INFLIGHT 2`, ADR-165)
that answers 503 when too many requests are concurrent, and a heap floor that
answers 500. Neither bounds *rate*: two clients politely taking turns never
exceed 2 concurrent and never trip either gate, while still costing the device
4 req/s indefinitely.

Lowering client poll rates (ADR-173) reduces pressure but cannot be the whole
answer: a browser holding a cached older `index.js`, a user script, or a
third-party poller is not bound by whatever intervals the shipped UI ships with.

### Two defects in the 1.x implementation

An adversarial review of 1.7.2-beta.4 found two problems this port must not
reproduce.

**(a) The `telegraf` alias bypassed the limit entirely.** `handleOtgw()` serves
`sendOTmonitorV2()` for *both* `otmonitor` and `telegraf` — identical bytes from
one branch — but 1.x's table listed only `otmonitor`. A caller that hit a 429
could switch aliases and continue at full rate, and the endpoint literally named
after a third-party polling agent was the unprotected one.

**(b) Two dashboards phase-lock and one starves permanently.** With two clients at
period P against a window W, requests alternate at gaps Δ and P−Δ. Both are served
only if Δ ≥ W *and* P−Δ ≥ W, which for 1.x's P=2000 ms, W=1500 ms is
unsatisfiable. So at least one request per cycle is always refused — and because
`setInterval` phase is fixed, it is the *same* client every cycle. That
dashboard's data freezes indefinitely, and 1.x's client swallowed the 429 with a
bare `return`, so the user saw no explanation.

## Decision

A per-endpoint, globally-shared poll budget checked in the v2 dispatcher
immediately before the handler call (`restAPI.ino`, mirroring the 1.x insertion
point). Excess GETs get `429` with RFC 9457 `application/problem+json`,
`Retry-After`, `Cache-Control: no-store`, and the draft `RateLimit` /
`RateLimit-Policy` headers.

The budget is global per endpoint, not per client: capping aggregate load is the
goal, and a per-client budget would let N clients each poll at full rate.

### Divergence 1 — aliases share one budget (fixes defect a)

The table is split into a const PROGMEM route list carrying a **budget index**,
and a separate RAM array of budget state:

```cpp
static const ApiRateLimitRoute kApiRateLimitRoutes[] PROGMEM = {
  { kRouteOtgw,   kSubOtmonitor, RL_BUDGET_OTMONITOR   },
  { kRouteOtgw,   kSubTelegraf,  RL_BUDGET_OTMONITOR   },   // same bytes, same budget
  { kRouteDevice, kSubTime,      RL_BUDGET_DEVICE_TIME },
};
```

The indirection is the point: a mutable row array (1.x's shape) forces a 1:1
route→state mapping, so `telegraf` could only ever be omitted (bypass) or given
its own window (half the protection). Note the split is **not** required by
`check_dispatch_tables_progmem` — that gate matches only `static const` tables and
PROGMEM is a no-op on ESP32; it is required by the shared-budget semantics.

### Divergence 2 — GCRA with burst 2, not one-per-window

Sustained rate is unchanged from 1.x (1 per window: 40 req/min on otmonitor,
15 on device/time, against the 242 req/min that killed the field device). The
slack matters for three reasons:

- **Including telegraf at burst 1 would starve it.** An agent arriving at uniformly
  random phase against one dashboard is refused with probability ≈ W/P = 1500/2000
  = 75 %, and Telegraf's `inputs.http` does not retry. A limiter that includes the
  alias only by making it unusable has not fixed defect (a), it has moved it.
- **It is the server half of the fix for defect (b).** With two tokens, two
  dashboards at the shipped interval are both served, so the phase-lock never arms
  in the common case. Client re-phasing (ADR-173) then covers N ≥ 3 and stale
  cached clients.
- **It matches this branch's own concurrency story.** ADR-165 already says the
  device is sized for two concurrent consumers; burst 2 says the same thing in the
  time domain.

Implemented as GCRA (leaky bucket as a virtual clock) using signed differences
against a theoretical arrival time, so the 49-day `millis()` rollover can neither
open a hole nor wedge a budget shut.

### Divergence 3 — `retry_after` in the body as well as the header

`Retry-After` is not a CORS-safelisted response header, so
`response.headers.get('Retry-After')` returns `null` for any cross-origin
client — precisely the population `sendCorsOriginHeader()` exists to serve. The
value is repeated as an RFC 9457 extension member (§3.2) so the client can always
read it.

### Divergence 4 — explicit `primed` flag

1.x used `lastServedMs != 0` as "never served". A stamp landing exactly on
`millis() == 0` (once per 49.7 days) re-opened the window for one free request.
A `bool primed` says what is meant.

### Ordering against the existing 503

**503 stays first.** The in-flight gate runs before URI parsing and cannot know
the route; moving the limiter above it would require parsing before admission
control, defeating a cheap pre-parse rejection. Under concurrent load a
rate-limited request therefore receives 503, not 429 — which is correct, because
503 genuinely dominates. The 429 early-return sits after `restInFlight++` and the
`onDisconnect` registration, so the counter stays balanced.

The two codes stay semantically distinct, and this is the contract:

- **503** — device-wide: too many concurrent requests, heap floor. Retry shortly.
- **429** — this endpoint's quota; the device is otherwise healthy. Carries
  `Retry-After`; a well-behaved client re-phases.

The two existing hand-rolled 429s (discovery and MQTT republish cooldowns) keep
ADR-035's `{"error":{...}}` envelope and `application/json`: they are POST
cooldowns, not poll budgets, and their shapes are documented in `openapi.yaml`
with published examples. `application/problem+json` is scoped to the poll limiter
only. 1.x's `forbid_pattern: sendApiError\(429` is deliberately **not** ported — it
does not match those call sites and a broad ban would invite someone to "fix" them.

## Alternatives Considered

### A: Per-client budget
Closer to RFC 6585's reading. Rejected: it does not achieve the goal (N clients
each poll at full rate) and costs a client table with eviction policy against
8 bytes per budget for the global variant.

### B: Reuse 503
Rejected: it destroys information. An operator could no longer distinguish "the
gateway is running out of memory" from "a browser is polling too fast", and those
call for opposite responses.

### C: Fix only the client (ADR-173 alone)
Rejected as a complete answer: a cached older `index.js`, a user script, or an HA
REST sensor are all outside the shipped UI's reach. The client fix reduces load;
the server limit bounds it.

## Consequences

**Benefits**
- Aggregate load on the two hottest endpoints is bounded regardless of client
  count or UI version, and the alias cannot be used to route around it.
- Costs 12 bytes per budget, no allocation, no `String`.
- 429 vs 503 is now a meaningful distinction in logs.

**Trade-offs**
- The budget is global, so a well-behaved client can still receive 429 because
  another client is polling. Burst 2 plus client re-phasing makes this rare rather
  than permanent, but it is not eliminated.
- Two endpoints now behave differently from the rest of the v2 surface. A future
  reader adding an endpoint must decide consciously whether it belongs in the table.
- `RateLimit` / `RateLimit-Policy` are an IETF Internet-Draft, not a standard, as
  of July 2026. Emitted as extra signal only.
- The 429 path stages 5 of `WEB_MAX_PENDING_HEADERS` (6). `webPushHeader()` drops
  silently past the cap, so a `static_assert` guards a cap *reduction*; it cannot
  catch a sixth push, hence the "5 of 6, do not add" comment at the call site.

**Risks and mitigations**
- *Risk*: a client ignoring `Retry-After` retries in a tight loop. *Mitigation*: a
  429 is far cheaper to serve than the JSON it replaces, and `no-store` stops an
  intermediary caching it.
- *Risk*: UI poll intervals drift away from the server windows. *Mitigation*:
  `check_poll_window_coupling` reads both sides.

## Related Decisions

- **ADR-165** — N*=2 request parallelism; the source of the burst-2 symmetry.
- **ADR-035** — v2 error shape; this adds a second representation, scoped.
- **ADR-050** — centralized dispatch; the check hooks into that single point.
- **ADR-173** — the client half (poll reduction and 429 re-phasing).
- **1.x ADR-086** — the sibling decision, whose two defects this corrects. A
  matching 1.x task is required to fix them on that line.

## Enforcement

- `evaluate.py::check_api_rate_limit_alias_coverage` — the dispatcher calls
  `checkApiRateLimit()` before the handler; every member of an alias disjunction in
  a handler is in the table with the *same* budget id; `sendApiRateLimited` emits
  problem+json with all four headers.
- `evaluate.py::check_poll_window_coupling` — each `windowMs` sits between 50 % and
  90 % of the corresponding `data/index.js` poll constant.
