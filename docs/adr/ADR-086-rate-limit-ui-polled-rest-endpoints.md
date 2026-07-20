# ADR-086: Rate-Limit the UI-Polled REST Endpoints with RFC 9457 429 Responses

## Status

Proposed, 2026-07-20.

**Decision Maker:** User: Robert van den Breemen (maintainer) requested the rate limit and specified the response shape (429 with `Retry-After`, `application/problem+json` per RFC 9457, optional draft `RateLimit` headers). The scope choice inside that request, a per-endpoint global budget rather than a per-client one, was made by the agent and is flagged for review in Consequences.

## Status History

status_history:
  - date: 2026-07-20
    status: Proposed
    changed_by: Agent
    reason: Records the 429 rate limit shipped under TASK-1043 on the otgw-1.x.x line; awaiting maintainer acceptance
    changed_via: adr-kit

## Context

The OTGW (OpenTherm Gateway) web UI refreshes itself from two REST endpoints on independent one-second timers. In `src/OTGW-firmware/data/index.js` the OT monitor timer calls `refreshOTmonitor()` against `/api/v2/otgw/otmonitor` (`index.js:273`) and the status timer calls `refreshDevTime()` against `/api/v2/device/time` (`index.js:290`). Both timers run for as long as the page is loaded, whether or not anyone is looking at it. A browser tab left open on a second screen therefore costs the gateway two requests per second indefinitely, and nothing in the firmware limited that.

The cost is not theoretical. A 60-minute diagnostic capture from a field device (TASK-1037, reporter martreides, 2026-07-19, device `otgw-48E72958B013`) recorded 14500 REST requests in the hour: 7192 on `/api/v2/otgw/otmonitor`, 7190 on `/api/v2/device/time`, 118 on `/api/v2/device/info`. That is a sustained 242 requests per minute. The capture tooling had its own browser disabled, so the load came from the user's own pages; all three endpoints came in at exactly twice the per-page rate, which identifies two open pages. The device ran out of heap and died 60 minutes into the capture, its last log line being `HEAP-FRAG: skip MQTT (maxBlock=760, heap=1608)`.

The ESP8266 serves this load from roughly 40 KB of usable RAM with a cooperative single-threaded loop. It has an existing heap-fragmentation gate that answers `503 Service Unavailable` when the largest contiguous block falls below a threshold (`HTTP_SERVE_MIN_MAXBLOCK`, `OTGW-firmware.h:145`), but that gate speaks about the device as a whole and engages only once damage is already underway. There was no mechanism to tell a specific caller that it was asking too often.

Lowering the client-side poll rates (done separately under TASK-1044) reduces the pressure but cannot be the whole answer: a browser holding a cached older `index.js`, a user's own script, or a third-party poller is not bound by whatever intervals the current UI ships with.

## Decision

Enforce a per-endpoint request budget in the firmware for `/api/v2/otgw/otmonitor` and `/api/v2/device/time`, and answer requests that exceed it with `429 Too Many Requests` carrying `Retry-After`, `Cache-Control: no-store` and an RFC 9457 `application/problem+json` body.

The budget is held in a two-entry table with one `uint32_t` of state per endpoint (`restAPI.ino:856`), giving windows of 1500 ms for `otmonitor` and 4000 ms for `device/time`. Each window sits at roughly 75% of the interval the shipping UI polls at (2000 ms and 5000 ms respectively). The gap is deliberate: `setInterval` is not exact, and background throttling or garbage collection routinely shifts a tick by tens of milliseconds, so a window equal to the client interval would hand 429 responses to a client that is behaving correctly.

The check runs in the v2 route dispatcher before the handler is invoked (`checkApiRateLimit`, `restAPI.ino:889`) and applies to `GET` only, so a mutation never shares a budget with a poll. Elapsed time is computed with unsigned subtraction so the 49-day `millis()` rollover cannot open a hole. `Retry-After` rounds up, so it never reports `0` and invites an immediate retry.

The response is emitted by `sendApiRateLimited` (`restAPI.ino:864`):

```http
HTTP/1.1 429 Too Many Requests
Content-Type: application/problem+json
Retry-After: 2
Cache-Control: no-store
RateLimit: "default";r=0;t=2
RateLimit-Policy: "default";q=1;w=2

{"type":"https://github.com/rvdbreemen/OTGW-firmware/problems/rate-limit-exceeded",
 "title":"Rate limit exceeded","status":429,
 "detail":"This endpoint serves at most 1 request per 2 second(s). Retry after 2 second(s)."}
```

`429` is chosen over the `503` the heap gate already uses because the two conditions are different and a reader has to be able to tell them apart: `503` means the device as a whole is in trouble, `429` means one caller exceeded a quota while the service is otherwise healthy.

The web UI is updated in the same change, because the firmware side alone would have been a regression. An unhandled 429 raised a generic error, which produced a `console.error` and a visible error banner once per second on any page beyond the first. Both fetch handlers now tag the HTTP status onto the thrown `Error` (`index.js:3267`, `index.js:4016`) and their catch blocks return quietly on 429 (`index.js:3298`, `index.js:4202`). The next timer tick retries and is served.

## Alternatives Considered

### Alternative A: per-client rate limit keyed on remote IP

Closer to the reading of RFC 6585 §4, where the limit belongs to a specific caller. Rejected on two grounds. First, it does not achieve the goal: the problem is aggregate load on a device with a single cooperative loop, and a per-client budget lets N clients each poll at the full rate, so ten open tabs would still produce ten requests per second. Second, it costs a client table in roughly 40 KB of usable RAM, with eviction policy and rollover handling, against 8 bytes for the global variant.

### Alternative B: answer with 503 Service Unavailable

Reuses the status code the heap gate already emits, so no new code path in clients. Rejected because it destroys information: an operator reading a log could no longer distinguish "the gateway is running out of memory" from "a browser is polling too fast", and those call for opposite responses. The heap gate's `503` keeps its current meaning precisely because this change does not overload it.

### Alternative C: fix only the client, leave the API unguarded

Lower the poll intervals in `index.js` and ship it (this is TASK-1044, which was done as well). Rejected as a complete answer because the firmware would still have no defence. A browser serving a cached older `index.js` after a filesystem upgrade, a user's own polling script, or a Home Assistant automation hitting the REST API are all outside the reach of the shipped UI's timers. The client fix reduces the load; the server limit bounds it.

### Alternative D: do nothing

Rejected because the field evidence shows the failure is reachable by ordinary user behaviour. Leaving a dashboard open is not misuse, and a gateway that dies from it is at fault.

## Consequences

**Benefits**

- The gateway bounds the load it accepts on its two hottest endpoints regardless of how many clients exist or which UI version they run.
- Costs 8 bytes of state (one `uint32_t` per limited endpoint), no dynamic allocation, and no `String`, satisfying ADR-049.
- Clients receive an actionable, standard answer rather than a timeout or a hung request: `Retry-After` tells them exactly how long to wait.
- The distinction between per-caller throttling (`429`) and device-wide distress (`503`) is preserved and now visible in logs.

**Trade-offs**

- The budget is global per endpoint, so a well-behaved client polling at the shipped interval can receive `429` because a second client is polling at the same time. This is a looser reading of `429` than RFC 6585 implies, and it is accepted deliberately: capping aggregate load is the goal, and with multiple clients the UI degrades instead of the device. This is the single point on which the maintainer's review is specifically invited.
- `RateLimit` and `RateLimit-Policy` are an IETF `httpapi` Internet-Draft, not a standard, as of July 2026. They are emitted as extra signal only; `Retry-After` (RFC 6585 §4, RFC 9110 §10.2.3) is the part clients can be relied upon to honour. If the draft changes shape, these two headers change with it and nothing else breaks.
- Two endpoints now behave differently from the rest of the v2 surface. A future reader adding an endpoint must decide consciously whether it belongs in the table.

**Risks and mitigations**

- *Risk*: a future change to the UI poll intervals leaves the server windows behind, so ordinary polling starts drawing `429`. *Mitigation*: the coupling is written into the comment above the table (`restAPI.ino:840`), which names both intervals and the 75% rule.
- *Risk*: a client that does not honour `Retry-After` retries immediately in a tight loop, turning one blocked request into many. *Mitigation*: a 429 is far cheaper to serve than the JSON payload it replaces, and `Cache-Control: no-store` prevents an intermediary from serving a stale error. The shipped UI waits for its next timer tick.
- *Risk*: rounding `Retry-After` up converts the 1500 ms window into an advertised 2 seconds, so a compliant client waits longer than strictly necessary. *Mitigation*: accepted; the alternative is advertising `1` for a 1500 ms window, which would invite a retry that is still refused.

## Related Decisions

- **ADR-035 (RESTful v2 API error response shape)**: this ADR adds a second error representation. `sendApiError` keeps the `{"error":{"status","message"}}` shape for every other failure; only the rate-limit response uses `application/problem+json`, because RFC 9457 is what the maintainer specified for this case.
- **ADR-050 (Centralized API Route Dispatch Table)**: the rate-limit check hooks into that dispatcher immediately before the handler call, so it inherits the single dispatch point rather than adding a parallel one.
- **ADR-049 (String Class Prohibition in Protocol Paths)**: the implementation uses fixed `char` buffers with `snprintf_P` and holds no `String`.
- **ADR-003 (HTTP-Only Network Architecture)**: unchanged; this decision adds no transport security and assumes the same trusted-LAN model.

## References

- Implementation: `src/OTGW-firmware/restAPI.ino:846-910` (`kSubOtmonitor`, `kRateLimitedRoutes`, `sendApiRateLimited`, `checkApiRateLimit`); `src/OTGW-firmware/data/index.js:3267`, `:3298`, `:4016`, `:4202` (429-aware fetch handlers).
- Tasks: TASK-1043 (this decision), TASK-1044 (client-side poll reduction), TASK-1037 (the field failure that prompted it).
- Field evidence: capture `transcript-20260719-210025-1.7.1+c50cbcc-OTGW-otgw-48E72958B013.txt`, 60 minutes, 14500 REST requests, device died at the 60-minute mark.
- Hardware verification 2026-07-20, bench device 192.168.88.68, build `1.7.2-beta.1+ccb5014`: first request 200, immediate second 429 with the full header set and body; `/api/v2/device/time` returned `Retry-After: 4` matching its own window; the window reopened after 5 s; `/api/v2/device/info` and `/api/v2/health` served three rapid requests each without limiting; browser console showed zero 429 errors.
- RFC 6585 §4 (429 Too Many Requests): <https://www.rfc-editor.org/rfc/rfc6585.html#section-4>
- RFC 9457 (Problem Details for HTTP APIs): <https://www.rfc-editor.org/rfc/rfc9457.html>
- IETF draft, RateLimit header fields for HTTP: <https://datatracker.ietf.org/doc/draft-ietf-httpapi-ratelimit-headers/>

## Enforcement

```json
{
  "forbid_pattern": [
    {
      "pattern": "sendApiError\\(429",
      "path_glob": "src/OTGW-firmware/**/*.ino",
      "message": "Rate-limit responses must use sendApiRateLimited() so they carry Retry-After, Cache-Control: no-store and the RFC 9457 problem+json body (ADR-086)."
    }
  ],
  "forbid_import": [],
  "require_pattern": [
    {
      "pattern": "checkApiRateLimit",
      "path_glob": "src/OTGW-firmware/restAPI.ino",
      "message": "The v2 dispatcher must keep calling checkApiRateLimit() before invoking a handler (ADR-086)."
    }
  ],
  "llm_judge": false
}
```
