---
id: "ADR-173"
title: "Client Poll Pacing, Locally-Ticked Device Clock, and 429 Pause in the v2 Web UI"
status: "Accepted"
date: "2026-10-15"
binding: false
gate: "check_poll_window_coupling"
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-192"
---
# ADR-173 Client Poll Pacing, Locally-Ticked Device Clock, and 429 Pause in the v2 Web UI

## Status

Accepted, 2026-10-15.

Port of `otgw-1.x.x` TASK-1044, plus the client half of the fix for the
starvation defect found in 1.7.2-beta.4.

## Status History

```yaml
status_history:
  - date: 2026-07-26
    status: Proposed
    changed_by: Agent
    reason: Ports 1.x TASK-1044 to the 2.0.0 classic UI as part of TASK-1037, adding the anti-starvation re-phasing that 1.x lacks.
    changed_via: manual
  - date: 2026-10-15
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-192
    changed_via: adr-kit lifecycle
  - date: 2026-10-15
    status: Accepted
    changed_by: Robert van den Breemen
    reason: Maintainer accepted 2026-10-15 after the code-vs-ADR verification and refresh (TASK-1237)
    changed_via: adr-kit lifecycle
```

## Context

ADR-172 puts a server-side poll budget on `/api/v2/otgw/otmonitor` (and its alias
`telegraf`): 1 request per 1500 ms, burst 2. `/api/v2/device/time` gets 1 per
4000 ms, burst 2. A client that polls faster than the budget is refused with 429,
so client pacing is a correctness concern, not only an efficiency one: how the
client reacts decides whether the UI degrades quietly or looks broken.

Measurement on the 1.x line (TASK-1044, 60-minute capture against a real boiler)
showed the fastest-moving OT values (`RelModLevel`, `TrOverride`, `TSet`,
`Tboiler`) change once every 5 to 6 seconds. Polling faster than about 2 s returns
mostly identical payloads, and a clock readout does not need the network at all.

Since alpha.425 the classic UI is gone (ADR-192) and `data/v2.js` is the only Web
UI on this line. This ADR was first written for the classic UI; it now records the
pacing design `v2.js` actually ships. No code change comes with this revision.

The v2 UI gets live OT data over the WebSocket. REST polling of the two budgeted
endpoints is a fallback and a side channel, not the main data path.

## Decision

The v2 client paces itself with the following mechanisms. Each is in `v2.js`
today.

### 1. Named poll periods the CI gate can read

- `OTMON_POLL_MS = 2000` (`v2.js:2585`) is the otmonitor tick.
- `DEVTIME_POLL_MS = 10000` (`v2.js:2640`) is the minimum gap between PS=1
  checks on device/time.

`evaluate.py::check_poll_window_coupling` reads both constants and pairs them
with the firmware windows (see Enforcement).

### 2. otmonitor is polled only when it is needed

`otmonTick()` (`v2.js:2626-2631`) runs every `OTMON_POLL_MS` from a fixed
`setInterval` (`v2.js:5176`). It sends a request only while Monitor > Values is
open or while the PIC is in PS=1 mode (the socket stays open but carries no
frames, so Home would freeze). The tick itself costs no request, and it does
nothing while the tab is hidden. Outside those two cases otmonitor is asked once
as a startup seed (`v2.js:5172`), when the socket closes (`v2.js:944`), and every
20 s while the socket is down (`v2.js:5173`).

All callers go through one fetcher, `fetchOtmonitor()` (`v2.js:2594-2607`):

- **In-flight guard** (`v2.js:2595`): at most one otmonitor request from this page
  at a time.
- **429 pauses all callers** (`v2.js:2598`): the client reads `retry_after` from
  the RFC 9457 problem+json body (`retryAfterMs()`, `v2.js:2587-2593`; the
  `Retry-After` header is not CORS-readable), falls back to 3 s, and sets
  `otmonPauseUntil`. Every caller (tick, seed, PS=1 entry) respects that pause.

### 3. device/time: PS=1 check and a locally ticked clock

`psCheckTick()` (`v2.js:2642-2657`, every 5 s from `v2.js:5177`) asks device/time
only after the open socket has been silent for 15 s, or while PS=1 is on, and never
more often than `DEVTIME_POLL_MS`. On 429 it reads `retry_after` the same way
(default 5 s) and moves its next allowed ask past the pause (`v2.js:2650`).

The header clock shows the **device** clock, so a wrong NTP server or timezone is
visible. `applyDevTime()` (`v2.js:208-214`) parses `devtime.dateTime`
("YYYY-MM-DD HH:MM:SS" in the device timezone). `tick()` (`v2.js:215-223`) renders
it every second from the local `Date.now()` offset (`v2.js:5098`), with no
network. `devClockTick()` (`v2.js:226-235`, first run at 4.4 s, then every 30 s
from `v2.js:5178`) re-syncs every 10 minutes (`DEVCLOCK_SYNC_MS`, `v2.js:207`). It
shares `devTimeInflight` / `devTimeLastMs` with `psCheckTick()`, so the two never
overlap and keep at least 5 s apart. A failed sync, a 429 included, clears the
sync stamp so the next 30 s tick tries again. Until the first answer the browser
clock shows, prefixed with `~`.

### 4. What the user sees when data goes stale

Monitor > Values marks each row by the age of its value, not by refusal count:
a row whose value has not changed for `VAL_STALE_MS` (120 s, `v2.js:2687`), or
that has no data, gets the `vstale` class (`v2.js:2753-2760`), which dims it
(`v2.css:623`). The age column keeps moving between answers. A refused or failed
poll therefore shows up as ageing rows rather than as an error banner.

### 5. Concurrency cap

Every same-origin `/api/` request from the page passes the ADR-184 queue at the
top of `v2.js` (`MAX_IN_FLIGHT = 2`, `v2.js:27`), so these pollers can never push
the page above the ADR-165 cap.

### Accepted gaps

The following parts of the older classic design are **not** in `v2.js`, and this
ADR accepts their absence:

- **No random jitter on 429.** After a pause the poller resumes on its next fixed
  `setInterval` tick.
- **No period back-off** (the classic poller backed off up to 4x after repeated
  refusals).
- **No stale marker after 3 consecutive refusals.** The age-based `vstale` marker
  is the only staleness signal.

Consequence: the otmonitor tick has a fixed phase (`v2.js:5176`). With three or
more tabs open on Monitor > Values (or in PS=1 mode) against one gateway, the
ADR-172 phase lock can arm and one tab can be refused cycle after cycle; its rows
then age into `vstale`. The likelihood is low: polling happens only on that page or
in PS=1 mode, and burst 2 covers two tabs.

### Historical note: the classic design

For the classic UI (`data/index.js`, removed in alpha.425) TASK-1037 shipped
`OTMONITOR_POLL_MS = 2000` and `DEVTIME_POLL_MS = 5000`, a self-rescheduling
`setTimeout` poller that on 429 rescheduled at `retryAfterMs + random(0, period)`
to break phase locks, a period back-off up to 4x after 4 refusals, a `data-stale`
marker after 3 refusals, and a device clock learned from the `epoch` /
`dateTime` difference. Defect fix D2 (commit 97ad7217c) capped a trusted
`Retry-After` at 4 poll periods plus jitter. None of this code exists on the line
any more.

## Alternatives Considered

### A: Port the classic jittered poller into v2.js
Would close the 3+ tab gap. Rejected for now: v2 polls otmonitor only on one page
or in PS=1 mode, so the scenario is rare, and the jitter plus back-off machinery
adds code and makes poll timing harder to reproduce. Revisit if a field report
shows a frozen Values page with several tabs open.

### B: Exponential back-off on 429, no jitter
Rejected on the arithmetic: back-off changes *rate*, and a phase lock is a *phase*
problem. A client that backs off still arrives after the winner every time it does
arrive.

### C: Client-side coordination between tabs (BroadcastChannel / localStorage lease)
Would let tabs elect one poller and share results. Rejected as disproportionate: it
only helps same-origin tabs in one browser, does nothing for two devices, and adds
a distributed-lease failure mode to a dashboard.

### D: Poll otmonitor continuously, like the classic UI
Rejected: the WebSocket already carries live data, so a continuous poll would spend
the shared budget that Telegraf and scripts also draw from.

## Consequences

**Benefits**
- With the socket open on a live bus and Monitor > Values closed, one v2 page sends
  no periodic otmonitor requests, and device/time about once per 10 minutes (on a
  silent bus the PS=1 check adds one device/time request per 10 s).
- The clock reads at 1 Hz with zero network cost and shows the device's own time.
- A 429 never produces a request storm: every caller waits out `retry_after`.

**Trade-offs**
- With three or more tabs polling, one tab can be starved (see Accepted gaps). The
  user sees its rows age and dim, not an explanation.
- A refusal is not shown as such; staleness is only inferred from value age, and a
  value that legitimately does not change for 120 s is also dimmed.

**Risks and mitigations**
- *Risk*: a future change raises a poll rate above the budget. *Mitigation*:
  `check_poll_window_coupling` fails the build when the windows and the v2
  constants drift apart.

## Related Decisions

- **ADR-172**: the server-side budget this paces against.
- **ADR-165**: N≤2 request parallelism.
- **ADR-184**: the 2-in-flight fetch queue, carried into `v2.js` (TASK-1210).
- **ADR-192**: v2 is the only UI; the reason this ADR describes `v2.js`.
- **1.x TASK-1044**: the sibling change this ported.

## Enforcement

`evaluate.py::check_poll_window_coupling` (`evaluate.py:2366`, helper
`poll_window_coupling()` at `evaluate.py:870`, run from `evaluate.py:3757`) reads
`OTMON_POLL_MS` and `DEVTIME_POLL_MS` from `data/v2.js` and the windows from
`restAPI.ino`. The otmonitor window must sit at 50-90 % of `OTMON_POLL_MS`; the
device/time window must be at most 90 % of `DEVTIME_POLL_MS`. The gate does not
check the in-flight guard, the 429 pause or the stale marker; those are covered
by review only.

## References

- Tasks: TASK-1037 (original port and defects D1-D4); 1.x TASK-1044; TASK-1223.12
  (classic UI removed); TASK-1223.14 (v2 otmonitor fetcher and PS=1 check);
  TASK-1229 (device clock in the v2 header); TASK-1210 (ADR-184 queue in v2).
- `src/OTGW-firmware/data/v2.js:27` (`MAX_IN_FLIGHT`), `:207-235` (device clock),
  `:2585-2631` (`OTMON_POLL_MS`, `retryAfterMs()`, `fetchOtmonitor()`,
  `otmonTick()`), `:2640-2657` (`DEVTIME_POLL_MS`, `psCheckTick()`),
  `:2687` and `:2753-2760` (`vstale`), `:5172-5178` (timers).
- `src/OTGW-firmware/restAPI.ino:2651-2655`: the server windows.
- Commits: 24be052f2 (classic pacing port, TASK-1037), 97ad7217c (D1-D4),
  92cfcc2b7 (two and three tabs verified against the real limiter),
  a74a23a1f (classic UI removed, coupling gate moved to `v2.js`).
