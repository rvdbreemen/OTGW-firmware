# ADR-173 Client Poll Pacing, Locally-Ticked Device Clock, and 429 Re-Phasing

## Status

Proposed. Date: 2026-07-26.

Port of `otgw-1.x.x` TASK-1044, plus the client half of the fix for the
starvation defect found in 1.7.2-beta.4.

## Status History

status_history:
  - date: 2026-07-26
    status: Proposed
    changed_by: Agent
    reason: Ports 1.x TASK-1044 to the 2.0.0 classic UI as part of TASK-1037, adding the anti-starvation re-phasing that 1.x lacks.
    changed_via: manual

## Context

The classic UI (`data/index.js`) polled two endpoints at 1 Hz each. Measurement on
the 1.x line (TASK-1044, 60-minute capture against a real boiler) showed the
fastest-moving OT values — `RelModLevel`, `TrOverride`, `TSet`, `Tboiler` — change
once every 5 to 6 seconds. A 1 Hz poll therefore returned roughly five identical
payloads per real change. The only thing genuinely needing 1 Hz was the clock
readout, and that does not need the network at all.

ADR-172 adds a server-side poll budget. That makes client behaviour a correctness
concern, not just an efficiency one: a client that polls faster than the budget
now gets refused, and how it reacts determines whether the UI degrades gracefully
or appears broken.

**Scope: `data/index.js` only.** `data/v2.js` is deliberately untouched — it
receives live OT data over a WebSocket, uses `/v2/otgw/otmonitor` only as a 20 s
fallback, never touches `/v2/device/time`, and already ticks its clock locally.
Every v2 cadence sits far above both server windows. (`v2.js` does lack a
`visibilitychange` listener, which is a real gap but out of scope here.)

## Decision

### 1. Poll periods, named

`OTMONITOR_POLL_MS = 2000` and `DEVTIME_POLL_MS = 5000`, as named constants rather
than inline literals, because `check_poll_window_coupling` reads them and pairs
them against the firmware's windows.

`GATEWAY_MODE_REFRESH_INTERVAL` drops 60 → 12. It counts *ticks* of the
device-status timer, not seconds, so with the tick moving 1000 → 5000 ms, 12 × 5 s
preserves the existing ~60 s wall-clock cadence. Getting this wrong is silent —
it would turn a once-a-minute gateway-mode poll into a once-every-five-minutes one
— hence the explicit comment at the constant.

### 2. Device clock: learned once, ticked locally

`/api/v2/device/time` already returns both `epoch` (device UTC) and `dateTime`
(device wall clock). The difference between them is the device's timezone offset;
adding the browser's own skew against the device yields an offset that renders the
device's wall clock locally, at 1 Hz, with no network and no timezone database. A
DST change is picked up at the next 5 s status poll.

**Deviation from 1.x:** `learnDeviceClock()` returns a boolean, and the caller
falls back to writing `devtime.dateTime` directly when the offset cannot be
learned. 1.x bails silently when `epoch <= 0` or `dateTime` is unparseable, which
on a device that has never reached NTP leaves `#theTime` stuck on its
`[00:00:00]` placeholder forever.

### 3. Paced poller with 429 re-phasing

Both `setInterval`s are replaced with a self-rescheduling `setTimeout` poller.
This is the core of the change and the reason a simple rate reduction is not
sufficient.

ADR-172 §Context (b) shows that two clients on identical fixed periods phase-lock
against a shared window: the same client is refused every cycle, indefinitely.
**Slowing the loser down does not help — a slower loser still arrives second.**
Breaking a phase lock requires changing *phase*, and `setInterval` cannot: its
phase is fixed for the life of the timer.

So on 429 the poller reschedules at `retryAfterMs + Math.random() * periodMs` —
jitter across a **full** period, because the winner's phase is unknown.
P(collide again) falls geometrically; expected escape is one to two cycles.
Additionally, after ≥4 consecutive refusals the period itself backs off (up to 4×),
so with N ≥ 3 dashboards a client stops adding load it will not be served.

An in-flight guard prevents requests stacking if a response is slow.

The pollers deliberately do **not** use the existing `fetchWithRetry()` helper:
retrying a poll whose next tick is 2 s away is pointless, its network-error retry
triples request count exactly while the device is rebooting, and the extra
in-flight request fights the N≤2 convention that `fetchWithRetry`'s own comment
exists to protect.

### 4. What the user sees while paced

Split by persistence:

- **Fewer than 3 consecutive refusals: nothing.** With burst 2 and jitter, an
  occasional 429 on a healthy two-tab setup is normal and self-heals within a
  cycle. Showing an error banner per tick was the exact regression 1.x ADR-086
  called out.
- **3 or more (~6 s otmonitor, ~15 s device/time): mark the region stale.**
  `data-stale` is set on `.otmontable` (OT monitor) or `#theTime` + `#heap-info`
  (device status), with a `title` explaining that the gateway is pacing updates.
  Cleared on the first success.

This also covers 503 and network stalls, which were previously invisible in the
classic UI — `refreshDevTime()`'s catch built a `<p>` element and never appended
it, silently swallowing every error since it was written. That dead code is removed.

Styled in `components.css` with existing tokens only, no new custom properties
(ADR-091 / `check_design_system_drift` is a FAIL gate).

## Alternatives Considered

### A: Reduce poll rates only, keep `setInterval` (this is what 1.x shipped)
Rejected: it leaves the phase-lock starvation in place. 1.x's client returns
quietly on 429, so the starved dashboard freezes with no indication at all.

### B: Exponential backoff on 429, no jitter
Rejected on the arithmetic: backoff changes *rate*, and a phase lock is a *phase*
problem. A client that backs off from 2 s to 8 s still arrives after the winner
every time it does arrive.

### C: Route the pollers through `fetchWithRetry()`
Rejected — see §3. Its retry semantics are wrong for a periodic poll and its extra
in-flight request works against ADR-165.

### D: Client-side coordination between tabs (BroadcastChannel / localStorage lease)
Would let tabs elect one poller and share results. Rejected as disproportionate:
it only helps same-origin tabs in one browser, does nothing for two devices, and
adds a distributed-lease failure mode to a dashboard.

## Consequences

**Benefits**
- Steady-state request rate from one dashboard drops from 2 req/s to
  0.5 + 0.2 req/s.
- The clock reads at 1 Hz with zero network cost.
- Two dashboards both stay live; neither freezes silently.
- 503s and network stalls become visible for the first time in this UI.

**Trade-offs**
- OT values can now be up to 2 s stale rather than 1 s. Measured change cadence is
  5-6 s, so this is below the noise floor.
- A user running a stale cached `index.js` at 1 Hz will draw sustained 429s. That
  is intended, and the stale marker means they at least see *something* — but
  expect a support question after the upgrade. Worth a CHANGELOG note.
- `tid` and `timeupdate` become "active" flags rather than timer handles. One call
  site (`webhookPage()`) did `clearInterval(tid)` directly and is updated to call
  `stopOTmonitorPolling()`.

**Risks and mitigations**
- *Risk*: `GATEWAY_MODE_REFRESH_INTERVAL` silently wrong. *Mitigation*: called out
  at the constant and in review; observable by toggling gateway mode.
- *Risk*: the jitter makes reproduction of poll-timing bugs harder.
  *Mitigation*: accepted; determinism here was the bug.

## Related Decisions

- **ADR-172** — the server-side budget this paces against.
- **ADR-165** — N≤2 request parallelism.
- **ADR-091** — design-system class drift; governs `data-stale`.
- **1.x TASK-1044** — the sibling change this ports.

## Enforcement

`evaluate.py::check_poll_window_coupling` pairs `OTMONITOR_POLL_MS` /
`DEVTIME_POLL_MS` against the firmware's rate-limit windows and fails if either
drifts outside 50-90 % of the client period.
