---
id: "ADR-184"
title: "Queue the classic UI's API requests at two in flight"
status: "Accepted"
date: "2026-10-02"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-165"
  - "ADR-172"
topics:
  - "webui"
  - "rest-gate"
  - "concurrency"
aliases:
  - "classic UI fetch queue"
  - "client-side in-flight cap"
components:
  - "classic web UI request queue (index.js window.fetch wrapper)"
symbols:
  - "window.fetch"
  - "refreshDevTime"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-184 Queue the classic UI's API requests at two in flight

## Status

Accepted, 2026-10-02.

Guideline-level per ADR-080: no automated gate. A bench capture verifies it (see Verification).

## Status History

```yaml
status_history:
  - date: 2026-10-02
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-10-02
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: Accepted by the maintainer in session after the acceptance packet (TASK-1193)
    changed_via: adr-kit lifecycle
  - date: 2026-10-02
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-165
    changed_via: adr-kit lifecycle
  - date: 2026-10-02
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-172
    changed_via: adr-kit lifecycle
```

## Context

The device admits at most 2 concurrent requests to its REST (Representational State Transfer) interface. The limit is `REST_MAX_INFLIGHT 2` at `src/OTGW-firmware/restAPI.ino:44`, and the excess gets a cheap 503. Under heap pressure the gate tightens toward 1. The limit comes from ADR-165, which left a client-side limiter unbuilt. Its stated premise was that the frontend's single-flight discipline already stays within the ceiling, so no client change is required (`docs/adr/ADR-165-optimal-request-parallelism-esp32s3-webui-rest.md:44-48`).

A bench capture on 2026-10-02 shows that premise does not hold for the classic UI. Setup: an OTGW32 (OpenTherm Gateway board with an ESP32-S3) running 2.0.0-alpha.397, headless Edge driven through the Chrome DevTools Protocol, every response recorded.

- **Main page load.** The browser sends 6 requests to the device's `/api/` routes at once, with a peak of 7 in flight. Over 11 loads the gate refused `device/info` 19 times, `device/time` 19 times and `filesystem/hash-check` 8 times. `fetchWithRetry()` (`src/OTGW-firmware/data/index.js:89`, from TASK-1033) recovers these, so nothing stays broken. No request was refused after the first 5 seconds of a load.
- **Smart Autotune Thermostat page.** Each open fires about 5 requests at once (status, weather, markers, slave config, hot-water bounds) next to the running polls. Over 5 opens, `GET /api/v2/sat/markers` was refused on 4, `sat/status` on 4 and `sat/weather` on 5. `loadMarkers()` has no retry, so the calibration marker list stayed empty on 4 of 5 opens. The browser console logged `[SAT] marker load error: Service Unavailable` 4 times.
- **Rate limit on device/time.** That route has its own rate limit (window 4000 ms, burst 2, `src/OTGW-firmware/restAPI.ino:2627`, ADR-172). It answered 429 six times: every page switch calls `refreshDevTime()` next to its 5-second poller (`DEVTIME_POLL_MS`, `src/OTGW-firmware/data/index.js:293`).

Fixing call sites one by one would leave independent timers free to overlap. It would also let every new call site bring the burst back.

## Decision

The classic bundle routes every same-origin request to `/api/` through one first-in-first-out queue with at most 2 requests in flight.

- The queue is a wrapper around `window.fetch`, installed at the top of `index.js`. The page loader runs `index.js` before every other bundle (`src/OTGW-firmware/data/index.html:46`).
- `refreshDevTime()` skips a call when the previous `device/time` request started less than 4 seconds ago. Until then the last values stay in place.

## Decision Contract

### Must

- Wrap `window.fetch` once, at the top of `index.js`, before any other code sends a request.
- Route only URLs (web addresses) whose path contains `/api/` through the queue. Every other URL goes to the original `fetch` unchanged.
- Keep at most 2 queued requests in flight. When one settles, resolved or rejected, start the oldest waiting request.
- Free the slot of a request that has not settled after 20 seconds, without aborting it, so one stalled request cannot block the page.
- Keep `fetchWithRetry()`: the device gate tightens toward 1 under heap pressure, so a 503 stays possible.
- Limit `refreshDevTime()` to one `device/time` request per 4 seconds.

### Must Not

- Abort, prioritise or batch queued requests.
- Route static assets, WebSocket traffic or non-`/api/` URLs through the queue.
- Allow more than the 2 requests in flight that ADR-165 set.

### Exceptions

- The v2 bundle (`v2.html`, `v2.js`) is out of scope. It gets its own decision if a capture shows the same burst there.

### Verification

- Bench capture with Playwright on headless Edge, recorded in TASK-1193 (AC#1 to AC#3):
  - The request timeline of a main page load and of 5 page opens through the top navigation shows at most 2 `/api/` requests in flight.
  - No response is 503 or 429.
  - The marker list is filled on every open.

## Alternatives Considered

- **Serialize the start of the thermostat page only.** This fixes that page. It leaves the 6-request burst on the main page load and the overlap between independent poll timers.
- **Retry the thermostat page requests**, the TASK-1033 pattern. The panels recover after a few hundred milliseconds, but the burst and its 503 responses stay. That is against the cap of 2.
- **Raise or remove the device gate.** ADR-165 measured 2 as the highest concurrency with zero refusals under nominal load. Removing the gate gives up that protection of the heap.

## Consequences

**Positive:**

- The classic UI stays within the device gate by construction, also for call sites added later.
- Measured on the bench on 2026-10-02. The queued `index.js` was served to the device's own page through browser route interception, firmware unchanged. One main page load and 5 thermostat page opens were captured per variant:
  - Peak in flight: 2 in every phase, against 8 on the page load and on the first open before.
  - Refused requests: none, against 27 before (all 503 in that run).
  - Markers loaded on 5 of 5 opens, against 1 of 5 before.
  - The same flow sent 65 requests instead of 75, because fewer retries and fewer repeated `device/time` calls were needed.

**Negative:**

- A request can wait for a free slot. A slow request, such as a file listing, delays the requests behind it. The 6 requests of a main page load complete in at least 3 rounds instead of 1.
- A wedged device can hold both slots for 20 seconds each before the valve frees them. The page then waits, as it would on the wedged device anyway.
- The v2 bundle keeps its current behaviour until a capture measures it.

## Open Questions

- [x] Which approach keeps the classic UI within the cap: a central queue, serializing the thermostat page start, or retries? — **Answered 2026-10-02 by User: Robert van den Breemen:** a central queue with at most 2 in flight.

## Related Decisions

- **ADR-165**: sets the device cap of 2. This ADR adds the client-side counterpart that ADR-165 left unbuilt, and corrects its premise that the frontend already stays within the cap.
- **ADR-172**: rate limit on UI-polled routes; the `device/time` budget the throttle respects.
- **ADR-173**: client poll pacing after a 429.
- **ADR-147**: the static file gate. Static assets load as browser subresources and are not covered here.

## References

- TASK-1193 (this decision and its bench verification) and TASK-1172 AC#6 (the capture that found the burst).
- `src/OTGW-firmware/restAPI.ino:44` (`REST_MAX_INFLIGHT`), `src/OTGW-firmware/restAPI.ino:2627` (`device/time` budget).
- `src/OTGW-firmware/data/index.js:89` (`fetchWithRetry`), `src/OTGW-firmware/data/index.js:293` (`DEVTIME_POLL_MS`).
- `src/OTGW-firmware/data/index.html:46` (script load order).
- `docs/adr/ADR-165-optimal-request-parallelism-esp32s3-webui-rest.md:44-48`.
