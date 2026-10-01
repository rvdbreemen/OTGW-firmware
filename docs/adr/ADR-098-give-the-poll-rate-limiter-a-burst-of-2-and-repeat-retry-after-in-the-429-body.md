---
id: "ADR-098"
title: "Give the poll rate limiter a burst of 2 and repeat retry_after in the 429 body"
status: "Accepted"
date: "2026-10-01"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-086"
topics:
  - "rate-limit"
  - "rest-api"
  - "http-429"
  - "polling"
  - "telegraf"
aliases:
  - "telegraf rate limit"
  - "GCRA"
  - "burst 2"
  - "retry_after"
  - "429 re-phase"
  - "phase lock"
components:
  - "REST poll rate limiter (kRateLimitedRoutes, rateLimitTryAdmit, checkApiRateLimit)"
  - "429 problem+json rate-limit response (sendApiRateLimited)"
symbols:
  - "kRateLimitedRoutes"
  - "ApiRateLimit"
  - "aliasSub"
  - "burstTokens"
  - "rateLimitTryAdmit"
  - "checkApiRateLimit"
  - "sendApiRateLimited"
  - "rephaseDelayMs"
context_scope: "selective"
format: "madr"
---

<!-- markdownlint-disable MD025 -->

# ADR-098 Give the poll rate limiter a burst of 2 and repeat retry_after in the 429 body

## Status

Accepted, 2026-10-01.

**Decision Maker:** User: Robert van den Breemen (maintainer), 2026-10-01, chose to port the 2.0.0 line's burst of 2 after the measurement in the Context. Two details within that choice were made by the agent and are flagged in Decision Outcome: the `device/time` route gets the same burst, and the body carries `retry_after`.

## Status History

```yaml
status_history:
  - date: 2026-10-01
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-10-01
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: "ADR-098 amends ADR-086: the shared per-endpoint budget gains a burst of 2 and the 429 body gains retry_after"
    changed_via: adr-kit lifecycle
  - date: 2026-10-01
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: Maintainer accepted in session 2026-10-01 after the acceptance packet (host harness, simulation, browser runs, build.bat and evaluate.py green)
    changed_via: adr-kit lifecycle
```

## Context and Problem Statement

ADR-086 rate-limits the two REST endpoints the web interface polls. Each endpoint
has one budget, shared by every client rather than kept per client, and a request
over it gets HTTP 429 (RFC 6585) with `Retry-After` and an RFC 9457
`application/problem+json` body. The windows are 1500 ms for
`GET /api/v2/otgw/otmonitor` (the web UI polls it every 2000 ms) and 4000 ms for
`GET /api/v2/device/time` (polled every 5000 ms). The limiter allowed one request
per window: a burst of 1.

An adversarial review of 1.7.2-beta.4 found two defects, and TASK-1090 fixed both
on this line while keeping the burst of 1:

* **Alias bypass.** `/api/v2/otgw/telegraf` is served by the same handler as
  `/api/v2/otgw/otmonitor` (`src/OTGW-firmware/restAPI.ino:446`) but was not
  limited. Since commit 5580e223b the otmonitor row names `telegraf` as an alias
  that draws from the same budget (`aliasSub`).
* **429 phase lock.** The web UI polls from `setInterval`, which keeps a fixed
  phase, so two dashboards opened together refused each other at the same instants
  forever. Since commit 053ac9f58 a poller that gets a 429 restarts its interval
  after a random delay within one poll period (`rephaseDelayMs()`).

The burst of 1 left a third failure. A Telegraf scrape that arrives within 1500 ms
of an open dashboard's last otmonitor poll is refused, which is 75% of the
dashboard's 2000 ms period. Telegraf collects at a fixed phase: its agent defaults
are `Interval` 10 s, `RoundInterval` true (collect on :00, :10, :20) and no
`CollectionJitter` (Telegraf `config/config.go`, `NewConfig()`). The dashboard
moves its phase only when it is refused itself, which happens only after Telegraf
was served. Every Telegraf success therefore gives the dashboard a new random
phase, and once that phase blocks Telegraf, nothing moves it again. Simulated
against the real `checkApiRateLimit()` (1000 starting phases, 10 minutes each,
one dashboard): a 10 s scrape got 0.6% of its scrapes answered, and 750 of 1000
phases got no answer at all; a 60 s scrape got 3.4%. With the dashboard closed,
every scrape is served.

A second gap concerns cross-origin clients. `sendCorsOriginHeader()`
(`restAPI.ino:44`) copies any request `Origin` into `Access-Control-Allow-Origin`,
so browser pages on another origin are supported clients. Under CORS (Cross-Origin
Resource Sharing), `Retry-After` is not a safelisted response header and the
gateway does not expose it, so such a page receives the 429 but cannot read the
wait. The body states it only in its `detail` prose.

The 2.0.0 line (branch `dev`) solved both with a GCRA (generic cell rate
algorithm: a leaky bucket kept as a virtual clock) limiter with a burst of 2 and
a `retry_after` field in the body.
The 2.0.0 line records them as ADR-172 and ADR-173, Proposed there.
The two lines number their ADRs independently.

On 2026-09-30 the maintainer first chose to keep the burst of 1, on an option text
that described the Telegraf collision as occasional. The simulation above showed
near-total starvation instead, and on 2026-10-01 the maintainer chose to port the
burst of 2.

## Decision Drivers

* A Telegraf scrape and one open dashboard must both be served. Home Assistant or
  Telegraf monitoring next to a web interface tab left open is the normal setup.
* ADR-086's protection stays: a sustained rate of one request per window per
  endpoint, one budget shared by all clients.
* Alias paths keep sharing one budget, so alternating paths cannot raise the rate.
* A cross-origin browser client, which the CORS echo admits, must be able to read
  how long to wait.
* The change on this maintenance line stays small, and the observable behaviour
  matches the 2.0.0 line where that costs nothing.

## Considered Options

* **Option A:** a GCRA budget per route with a burst of 2 and a sustained rate of
  one per window, the alias kept as `aliasSub`, `retry_after` in the 429 body, and
  the web UI re-phase kept.
* **Option B:** keep the burst of 1 with the alias budget and the re-phase (the
  2026-09-30 choice).
* **Option C:** give `telegraf` a budget row of its own.
* **Option D:** a budget per client instead of per endpoint.
* **Option E:** the burst of 2 without the web UI re-phase.

## Decision Outcome

Chosen option: **Option A**, because it serves the Telegraf scrape and the
dashboard together (0.6% to 100% of the scrapes in the simulation), keeps the
sustained cap of ADR-086 (a flood gets 41 requests a minute instead of 40, the
difference being the burst), and keeps the alias closed.

Concretely, at the commit that introduces this ADR:

1. `ApiRateLimit` (`src/OTGW-firmware/restAPI.ino:920`) replaces `lastServedMs`
   with a GCRA state: `burstTokens`, `tat` (the theoretical arrival time of the
   next request) and `primed`. Both rows of `kRateLimitedRoutes[]`
   (`restAPI.ino:938`) get a burst of 2: otmonitor with its `telegraf` alias at
   1500 ms, and `device/time` at 4000 ms.
2. `rateLimitTryAdmit()` (`restAPI.ino:982`) admits a request when `tat - now` is
   at most `(burstTokens - 1) * windowMs`, and then moves `tat` one window on. A
   refusal does not move `tat`. The arithmetic is unsigned, so the 49-day
   `millis()` wrap is harmless, and a lead larger than `burstTokens * windowMs`
   means `tat` lies in the past and the budget is full again, however long the
   idle gap. `primed` replaces the `lastServedMs != 0` sentinel, which let every
   request through while a stamp sat at `millis() == 0`. The function takes the
   row index, not a reference: the `.ino` prototype generator hoists a
   declaration above the struct, and a builtin parameter type keeps it valid.
3. `sendApiRateLimited()` (`restAPI.ino:950`) adds `"retry_after": <seconds>` to
   the problem+json body (RFC 9457 section 3.2 permits extension members), equal
   to the `Retry-After` header. The `detail` text names the burst, and
   `RateLimit-Policy` reports `q=2`.
4. The web UI keeps re-phasing after a 429 (`src/OTGW-firmware/data/index.js:320`
   to `:358`); its code is unchanged.

Two details were the agent's call within the maintainer's choice:

* **`device/time` gets the burst too.** Its period is also shorter than two
  windows (5000 < 8000 ms), the same code path serves it, and the 2.0.0 line does
  the same.
* **The body carries `retry_after`.** The option the maintainer chose named the
  burst only. The field is additive, costs one format argument, and is the only
  way a cross-origin page can learn the wait.

### Confirmation

* **Host harness** (in the record of 1.x TASK-1188 and dev TASK-1057). The real
  limiter is sliced from `restAPI.ino` and compiled with MSVC (Microsoft Visual
  C++), then run as 16 cases:
  * The change passes all 16.
  * `origin/otgw-1.x.x` at 8ba6f7ef9, the burst-1 limiter, fails exactly the six
    defect cases:
    * a Telegraf scrape 500 ms after a dashboard poll;
    * a burst of exactly 2;
    * the full burst after a 25.5-day idle gap;
    * the `device/time` burst;
    * 5 of 5 requests let through at `millis() == 0`;
    * `retry_after` in the body.
  * It passes the ten controls:
    * both alias directions;
    * at most 8 requests in 10 s when alternating the two paths;
    * 40 or 41 requests in a 60 s flood;
    * `Retry-After` of at least 1;
    * the `millis()` wrap;
    * the window refill;
    * `device/time` keeping its own budget;
    * POST never limited;
    * a 273-byte body at the largest values, under the 320-byte buffer, parsed with
      a JSON parser.
  * Five mutants of the change each fail exactly their cases: the alias as a second
    row, a signed rollover test, `Retry-After` rounded down, a burst of 1, and a
    wrong `retry_after`.
* **Simulation** against the same sliced limiter, 1000 starting phases of 10
  minutes, with the dashboard re-phasing as `index.js` does:
  * Telegraf every 10 s beside one dashboard: served 0.6% before, 100% after.
  * Every 60 s: 3.4% before, 100% after.
  * Every 5 s: 0.8% before, 75.8% after.
  * The dashboard: 99.7 to 99.9% before, 99.7 to 100% after.
* **Browser runs**, 10 minutes each. The real `index.js` runs in headless Chrome
  against a host process that answers with the sliced limiter, burst 2 against
  burst 1:
  * One tab and a Telegraf poller every 10 s: Telegraf got 60 of 60 scrapes
    (longest gap 10.0 s), against 2 of 60 before (longest gap 588.8 s). The tab
    got 300 of 300 polls, against 297 of 299.
  * Two tabs opened together: 191 and 205 otmonitor grants with longest gaps of
    8.9 s and 8.0 s, against 139 and 167 grants with longest gaps of 75.8 s and
    47.6 s. On `device/time` the longest gaps were 20.6 s and 23.4 s, against
    43.6 s and 84.3 s.

## Decision Contract

### Must

* Count every alias path of a limited endpoint against that endpoint's one budget
  (`aliasSub` in `kRateLimitedRoutes[]`).
* Keep each budget global per endpoint, not per client (ADR-086).
* Keep the sustained rate at one request per window, with a burst of 2.
* Answer an over-budget GET with 429, a `Retry-After` of at least 1 second, and a
  problem+json body whose `retry_after` equals `Retry-After`.
* Compute the GCRA clock with unsigned arithmetic, so the `millis()` wrap and idle
  gaps longer than 2^31 ms leave a full budget.
* Re-arm a web UI poller after a 429 at a random offset within one poll period.

### Must Not

* Add an alias as a row of its own.
* Raise the burst above 2 without a new decision: the cap protects the device.
* Limit a mutation (non-GET) request.

### Exceptions

* None.

### Verification

* The Enforcement block below requires `aliasSub`, `burstTokens` and
  `retry_after` in `src/OTGW-firmware/restAPI.ino` and `rephaseDelayMs` in
  `src/OTGW-firmware/data/index.js`.
* The host harness and simulation recorded in 1.x TASK-1188.

## Consequences

### Positive

* A Telegraf scrape beside one open dashboard is served: 100% of the scrapes at a
  10 s or 60 s interval in the simulation, against 0.6% and 3.4% before.
* Two dashboards opened together are served more evenly and more often. Simulated
  in phase over 300 runs of 120 s: 76% and 73% of their polls, with a mean longest
  gap of 7.8 s (worst 9.9 s). Before: 63% and 58%, mean longest gap 25.8 s
  (worst 69 s).
* The sustained protection is unchanged: a flood every 100 ms gets 41 requests in
  60 s instead of 40.
* A cross-origin page can read the wait from `retry_after`.
* The 429 body has the same shape as on the 2.0.0 line, so one client handles both
  lines.
* A request landing on `millis() == 0` no longer opens the budget for every
  request in that millisecond.

### Negative

* A code change on the maintenance line. It ships in the next beta (the open
  1.7.7-beta.2 when this was written), needs a beta cycle before a release, and
  costs 16 bytes of RAM (two table rows of 28 bytes instead of 20).
* Clients that parse the `detail` prose see new wording ("(burst 2)"). The prose
  was never a contract; `Retry-After` and now `retry_after` are.
* With two dashboards and a Telegraf scrape every 10 s, the demand exceeds the
  budget by design, and Telegraf gets 46% of its scrapes (17.5% before), with a
  mean longest gap of 62 s. ADR-086's cap is doing its job there; the API
  documentation recommends a scrape interval of 10 s or more.
* For two dashboards the re-phase trades latency for evenness under a burst of 2.
  Without it the second dashboard gets a steady one poll in three (a 6 s gap);
  with it both get about 75%, with a mean longest gap of 7.8 s. It stays because
  with three dashboards the burst alone starves the third (zero grants in 300 of
  300 runs) and the re-phase prevents that.
* The two lines implement the alias differently: this line keeps `aliasSub` in the
  route table, the 2.0.0 line indexes routes into a budget table. The observable
  behaviour is the same.

## Pros and Cons of the Options

### Option A: GCRA with a burst of 2, alias kept, retry_after in the body

* Good, because the measured starvation is gone and the sustained cap is kept.
* Good, because cross-origin clients can read the wait.
* Bad, because it is a code change with a beta cycle on the maintenance line.

### Option B: keep the burst of 1

* Good, because there is no code change.
* Bad, because a Telegraf scrape is refused nearly every time while a dashboard
  is open (0.6% served at 10 s).

### Option C: a budget row of its own for telegraf

* Good, because it is the smallest code change that serves Telegraf.
* Bad, because a client that alternates the two paths polls at twice the rate,
  which is the bypass commit 5580e223b closed. The harness mutant for this design
  serves 16 requests in 10 s instead of 8.

### Option D: a budget per client

* Good, because two clients never compete for one window.
* Bad, because N clients could each poll at the full rate, which is the load
  ADR-086 exists to cap, and it needs per-client state on a device with a few tens
  of kilobytes of free heap.

### Option E: the burst of 2 without the re-phase

* Good, because the client code would be simpler.
* Bad, because three dashboards opened together leave the third with zero grants
  (300 of 300 simulated runs).

## Open Questions

* None.

## Related Decisions

* **ADR-086 (Rate-Limit the UI-Polled REST Endpoints with RFC 9457 429
  Responses)**: this ADR amends it. ADR-086's per-endpoint shared budget and 429
  response stay; the budget gains a burst of 2 and the body gains `retry_after`.
* **2.0.0 line ADR-172 and ADR-173 (branch `dev`, own numbering, Proposed on that
  line)**: the GCRA limiter with a burst of 2 and `retry_after`, and the client
  poll pacing with the 429 re-phase. This ADR ports their behaviour; the code is
  written for this line.

## References

* `src/OTGW-firmware/restAPI.ino`, at the commit that introduces this ADR:
  * `:44`: `sendCorsOriginHeader()`;
  * `:446`: one handler for otmonitor and telegraf;
  * `:906`: `kSubTelegraf`;
  * `:920`: `ApiRateLimit`;
  * `:938`: `kRateLimitedRoutes[]`;
  * `:950`: `sendApiRateLimited()`;
  * `:982`: `rateLimitTryAdmit()`;
  * `:997`: `checkApiRateLimit()`.
* `src/OTGW-firmware/data/index.js`, at the same commit:
  * `:266-267`: poll periods of 2000 ms and 5000 ms;
  * `:320-358`: `rephaseDelayMs()`, `rephaseOTmonitorPolling()` and
    `rephaseTimeUpdates()`.
* Commit 5580e223b (shared otmonitor and telegraf budget) and commit 053ac9f58
  (re-phase after a 429), both under 1.x TASK-1090.
* RFC 6585 section 4 (429 Too Many Requests): <https://www.rfc-editor.org/rfc/rfc6585#section-4>
* RFC 9457 (Problem Details for HTTP APIs), section 3.2 on extension members: <https://www.rfc-editor.org/rfc/rfc9457#section-3.2>
* RFC 9110 section 10.2.3 (Retry-After): <https://www.rfc-editor.org/rfc/rfc9110#section-10.2.3>
* Fetch Standard, CORS-safelisted response header names (Retry-After is not one): <https://fetch.spec.whatwg.org/#cors-safelisted-response-header-name>
* Telegraf agent defaults, `NewConfig()` in `config/config.go`: <https://github.com/influxdata/telegraf/blob/master/config/config.go>
* Telegraf configuration documentation, `round_interval` and `collection_jitter`: <https://github.com/influxdata/telegraf/blob/master/docs/CONFIGURATION.md>

## Enforcement

```json
{
  "forbid_pattern": [],
  "forbid_import": [],
  "require_pattern": [
    {
      "pattern": "aliasSub",
      "path_glob": "src/OTGW-firmware/restAPI.ino",
      "message": "The telegraf path must share the otmonitor rate-limit budget through ApiRateLimit.aliasSub (ADR-098). A row of its own lets a client alternate paths at twice the rate."
    },
    {
      "pattern": "burstTokens",
      "path_glob": "src/OTGW-firmware/restAPI.ino",
      "message": "The poll rate limiter keeps a burst of 2 (ADR-098). With a burst of 1 a Telegraf scrape beside an open dashboard is refused nearly every time."
    },
    {
      "pattern": "retry_after",
      "path_glob": "src/OTGW-firmware/restAPI.ino",
      "message": "The 429 problem+json body must repeat Retry-After as retry_after (ADR-098): a cross-origin browser client cannot read the header."
    },
    {
      "pattern": "rephaseDelayMs",
      "path_glob": "src/OTGW-firmware/data/index.js",
      "message": "The web UI must re-phase a poller at a random offset after a 429 (ADR-098). Without it three dashboards opened together leave one with no data."
    }
  ],
  "llm_judge": false,
  "llm_judge_reason": "The four require_pattern rules cover the mechanical surface of this decision; the burst size and the arithmetic are covered by the recorded host harness and reviewed at PR."
}
```
