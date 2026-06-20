---
id: ADR-149
title: "Accept the LWIP TCP-pcb connection ceiling on the ESP32-S3: keep the app-level connection mitigations, do not raise CONFIG_LWIP_MAX_ACTIVE_TCP (TASK-884)"
status: Proposed
date: 2026-06-20
tags: [esp32s3, lwip, asynctcp, websocket, webserver, twdt, concurrency, connection-flood, platform-limits, task884, task879, task883]
supersedes: []
superseded_by: []
related: [ADR-080, ADR-089, ADR-135, ADR-147]
deciders: [Robert van den Breemen]
---

# ADR-149: Accept the LWIP TCP-pcb connection ceiling on the ESP32-S3, do not raise CONFIG_LWIP_MAX_ACTIVE_TCP (TASK-884)

## Status

Proposed, 2026-06-20.

This ADR resolves TASK-884 and the connection-backpressure aspect of TASK-879 and TASK-883.
It records the maintainer's decision, grounded in an on-device A/B/C experiment on the real
OTGW32 at alpha.227 (2026-06-20), to accept the 16-entry LWIP active-TCP pcb ceiling rather
than raise it. Acceptance is reserved to the maintainer through the adr-kit gates; this ADR
is not self-accepted.

**Guideline-level** (per ADR-080): this is a constraint-documenting decision. It introduces
no new automated CI gate. Its enforcement is the set of application-level connection
mitigations already present in the firmware (cited in Decision and References below); those
are reviewed at PR time, not by a dedicated `evaluate.py` check. No firmware code changes
ship with this ADR.

## Status History

status_history:
  - date: 2026-06-20
    status: Proposed
    changed_by: Agent
    reason: Initial decision record. Records the maintainer's decision (Robert, 2026-06-20) to accept the LWIP TCP-pcb ceiling on the ESP32-S3 after the alpha.227 A/B/C field experiment, rather than raise CONFIG_LWIP_MAX_ACTIVE_TCP.
    changed_via: adr-kit

## Context

The ESP32-S3 async web stack (epic TASK-865) serves the web UI, the REST API, and the
ADR-133 live-log WebSocket from a single AsyncTCP service task, all sharing one LWIP
instance. Under heavy *concurrent connections* the device crashes. The failure was
empirically pinned on a real OTGW32 at alpha.227 on 2026-06-20.

Root cause, in order of events:

1. The LWIP active-TCP pcb pool is bounded at `CONFIG_LWIP_MAX_ACTIVE_TCP = 16` (and
   `CONFIG_LWIP_MAX_SOCKETS = 16`). On this build those constants are baked into the
   pioarduino prebuilt arduino-esp32 libraries, so the firmware does not see them as
   tunable flags at compile time.
2. A load of persistent `ws://host/ws` live-log subscribers combined with an HTTP request
   flood (plus TCP sockets lingering in TIME_WAIT) exhausts the 16-entry pcb pool.
3. With the pool empty, AsyncTCP's accept path receives `pcb == NULL` and logs
   "tcp_accept(): _accept failed: pcb is NULL" (AsyncTCP, `tcp_accept` callback). This
   error-storms on the LwIP thread.
4. The storm starves the loop task. The ESP32 Task Watchdog Timer (TWDT, ADR-135) does its job
   and resets the device: bootcount climbs, `lastreset` reads "Unknown", and no coredump is
   produced for the watchdog reset.

Captured console evidence and a coredump are archived in
`bisect-testset/otgw32-wsrealism-crash-20260620/`
(`console-pcb-null-flood-alpha227.log`, `coredump-alpha226-6b57115.bin`).

This continues the platform-limits work of ADR-147 (the static-file-serving half of
TASK-879). ADR-147 already documented the 16-entry pcb pool as a hard platform ceiling and,
in its Alternative C, judged raising `CONFIG_LWIP_MAX_ACTIVE_TCP` impractical on a prebuilt
arduino-esp32 build. ADR-149 closes the remaining open question by testing that raise on the
device and recording the resulting decision against the connection-flood scenario.

The firmware already carries three application-level connection mitigations, plus one
framework default, that bound the *realistic* load:

- A WebSocket client cap, `MAX_WEBSOCKET_CLIENTS = 3`
  (`src/OTGW-firmware/webSocketStuff.ino:60`), rejecting connections beyond the cap
  (`webSocketStuff.ino:163`).
- A WebSocket-connect heap reject: connections are refused when free heap is below the
  warning threshold (`webSocketStuff.ino:174`).
- A heap-tier-aware REST request-inflight gate, `restEffectiveInflightCap()`
  (`src/OTGW-firmware/restAPI.ino:55`), which serializes large responses toward an inflight
  cap of 1 as the largest contiguous block shrinks.
- REST responses are not kept alive (ESPAsyncWebServer does not hold REST connections open
  between requests), so a served request does not retain an idle socket that holds a pcb.

These cover the realistic scenario: at most three live-log browser tabs plus a browser's
parallel asset fetches. They do not, and cannot, defend an adversarial concurrent-connection
flood on a 16-pcb device.

## Decision

Keep the existing application-level connection mitigations in force and accept that the
device cannot survive an adversarial concurrent-connection flood. Do not raise
`CONFIG_LWIP_MAX_ACTIVE_TCP` via a pioarduino `custom_sdkconfig`.

Scope and honesty of the claim:

- The mitigations bound the *realistic* load. They do not *prevent* the crash under an
  adversarial flood: in the experiment below, the baseline arm that runs with exactly these
  mitigations (Path C) still crashed under the synthetic flood. The honest claim is that the
  mitigations make the device robust for the load a real deployment produces (a handful of
  live-log tabs and a browser's parallel requests), not that they make it flood-proof.
- An adversarial flood cannot be fully defended at the application layer. The vendored
  AsyncTCP and ESPAsyncWebServer expose no connection-count cap to application code and are
  treated as read-only managed dependencies, and the 16-entry limit lives below them in the
  prebuilt LWIP. Raising the pool is the only mechanism that would add headroom, and it is
  not buildable in this environment (see Alternative A).
- Therefore the adversarial-flood reset is a documented known limit, not a defect to chase.
  Recovery is automatic: the TWDT reset (ADR-135) reboots the device back into service.

This decision changes no firmware code. The mitigations it relies on are already shipped.

## Alternatives Considered

All three paths were run on the real OTGW32 under an identical synthetic load
(`test_ws_liveload.py`: 6 WebSocket subscribers plus 14 HTTP flood workers, 75 s, app-only
flash), alpha.227, 2026-06-20.

### Alternative A: Raise CONFIG_LWIP_MAX_ACTIVE_TCP from 16 to 32 (chosen against)

Increasing the LWIP active-TCP pcb pool is the only mechanism that adds real connection
headroom, because the limit lives below the application layer in LWIP.

Rejected. The override requires a pioarduino `custom_sdkconfig`, which rebuilds the full
ESP-IDF (Espressif IoT Development Framework) component set. That rebuild failed structurally on Windows in this environment after
about 9 minutes (doubled `.pio` path, `https_server.crt.S` codegen not found), so the build
never completed and the runtime effect is unverified. This refines, rather than contradicts,
ADR-147 Alternative C: the raise is achievable in principle through `custom_sdkconfig`, but
it is not buildable in this environment, and it would also carry a full-IDF-rebuild cost plus
a permanent RAM cost for the larger pcb and socket tables. Practical conclusion unchanged
from ADR-147: the ceiling cannot be raised here.

### Alternative B: Tighten the WebSocket client cap from 3 to 2 (chosen against)

Lowering `MAX_WEBSOCKET_CLIENTS` from 3 to 2 frees one pcb that the live-log subscribers
would otherwise hold, on the theory that fewer persistent sockets leave more pool headroom
for the HTTP flood.

Rejected. With the cap at 2 the device still crashed (bootcount climbed by 1, with 19
"pcb is NULL" accept failures logged). The HTTP flood plus TCP TIME_WAIT exhausts the
16-entry pool regardless of whether the WebSocket cap is 3 or 2. The one freed slot is
marginal and provides no meaningful defense, while costing a usable live-log tab for normal
users.

### Alternative C: Keep the existing mitigations and accept the ceiling (chosen)

Run the device at its shipped configuration: LWIP pool 16, WebSocket cap 3, plus the
heap-reject and the REST inflight gate.

Under the synthetic adversarial flood this baseline also crashed (bootcount climbed by 1,
"pcb is NULL" flood). That is expected and is the point of the decision: no app-layer arm
survives the synthetic flood, the only arm that could add headroom (A) is not buildable here,
and B does not help. The shipped configuration nevertheless handles the realistic load, keeps
the build system untouched, and preserves RAM. Recovery from the rare adversarial case is
automatic via the TWDT reboot. This is the chosen path.

## Consequences

**Benefits**

- No build-system upheaval. The fragile `custom_sdkconfig` full-IDF-rebuild path is not
  taken, so the standard pioarduino prebuilt-lib build remains the supported build.
- RAM is preserved. The pcb and socket tables stay at 16 entries rather than growing to
  accommodate a larger pool.
- The realistic load (at most three live-log tabs plus a browser's parallel asset fetches)
  is handled by the existing mitigations.
- The limit is now documented and empirically grounded, so a future field report of a
  watchdog reset under heavy concurrent connections can be recognized as this known limit
  rather than re-investigated from scratch.

**Trade-offs**

- An adversarial concurrent-connection flood still exhausts the 16-entry pcb pool, storms
  the accept path with "pcb is NULL", starves the loop task, and triggers a TWDT reset. The
  device does not survive such a flood in place.
- Recovery is by reboot, not by graceful shedding. The reset interrupts any in-flight work
  and increments the bootcount; the device returns to service automatically after the
  watchdog reboot (ADR-135), but the event is a hard reset, not a soft degrade.

**Risks and mitigations**

- *Risk*: a non-adversarial but unusually heavy real deployment (for example several browser
  tabs each holding a live-log WebSocket while also flooding REST polls) drifts close to the
  pool ceiling and resets under normal use.
  *Mitigation*: the shipped mitigations bound this case (WS cap 3, heap-reject on WS connect,
  REST inflight gate that serializes large responses as the heap tightens, ADR-089). If field
  telemetry shows real deployments hitting the ceiling, that is the trigger to reopen via the
  reopening path below, rather than a reason to widen the mitigations speculatively.
- *Risk*: the accepted limit is forgotten and the reset is mis-triaged as a regression.
  *Mitigation*: this ADR plus the archived capture in
  `bisect-testset/otgw32-wsrealism-crash-20260620/` are the reference for recognizing the
  signature (bootcount climb, `lastreset` "Unknown", no coredump, "pcb is NULL" flood).

**Reopening path (future work, not part of this ADR)**

If connection-flood resilience is later required, raising the pool (Alternative A) becomes a
dedicated build-system task: fix the pioarduino `custom_sdkconfig` full-IDF rebuild on
Windows, then build with `CONFIG_LWIP_MAX_ACTIVE_TCP = 32`, measure the RAM cost, and
field-validate the flood scenario on the device. That work is referenced here as future work,
not undertaken by this ADR.

## Related Decisions

- **ADR-147 (ESP32-S3 platform limits for concurrent webui static-file serving)**:
  Complements and resolves the open empirical question from ADR-147's Alternative C. ADR-147
  documented the 16-entry pcb pool as a hard ceiling and judged raising it impractical;
  ADR-149 tests the raise on the device, finds it not buildable in this environment, and
  records the decision to accept the ceiling for the connection-flood scenario. ADR-149 does
  not supersede or amend ADR-147; ADR-147 remains Accepted and immutable.
- **ADR-135 (ESP32 TWDT is the Primary Watchdog)**: The TWDT reset that ends the flood is the
  recovery mechanism this ADR relies on. The accepted outcome of an adversarial flood is a
  TWDT reboot, by design.
- **ADR-089 (Heap tier-machine contract)**: The REST inflight gate `restEffectiveInflightCap()`
  is heap-tier-aware and serializes large responses as the largest contiguous block shrinks;
  it is one of the cited mitigations.
- **ADR-080 (Binding ADR rules must have a CI gate)**: ADR-149 is explicitly guideline-level
  with no new CI gate, as required when a decision is not promoted to a binding pattern-level
  rule.
- **TASK-884**: Resolved by this ADR (accept the LWIP TCP-pcb connection ceiling).
- **TASK-879**: The connection-backpressure aspect is resolved here; the static-file-serving
  aspect was resolved by ADR-147.
- **TASK-883**: The connection-backpressure aspect is resolved here; the remaining
  whole-response-buffer streaming work (true chunked streaming) stays open in TASK-883.

## References

- TASK-884, TASK-879, TASK-883.
- On-device A/B/C experiment, real OTGW32, alpha.227, 2026-06-20:
  `test_ws_liveload.py` (6 WebSocket subscribers + 14 HTTP flood workers, 75 s, app-only
  flash). Path C baseline: crash, bootcount +1. Path B (WS cap 2): crash, bootcount +1, 19x
  "pcb is NULL". Path A (LWIP 32 via custom_sdkconfig): not buildable here (full-IDF rebuild
  failed on Windows after ~9 min), runtime effect unverified.
- Captured console + coredump: `bisect-testset/otgw32-wsrealism-crash-20260620/console-pcb-null-flood-alpha227.log`,
  `bisect-testset/otgw32-wsrealism-crash-20260620/coredump-alpha226-6b57115.bin`.
- Application-level mitigations (the in-code enforcement of this guideline-level decision):
  - `src/OTGW-firmware/webSocketStuff.ino:60` (`MAX_WEBSOCKET_CLIENTS = 3`),
    `webSocketStuff.ino:163` (max-clients reject), `webSocketStuff.ino:174` (heap reject on
    WebSocket connect).
  - `src/OTGW-firmware/restAPI.ino:55` (`restEffectiveInflightCap()`, heap-tier-aware REST
    inflight gate).
  - REST responses not kept alive: ESPAsyncWebServer framework default (no firmware line
    sets `Connection: close`; the framework does not keep REST connections open between
    requests).
- LWIP ceiling: `CONFIG_LWIP_MAX_ACTIVE_TCP` and `CONFIG_LWIP_MAX_SOCKETS` are 16 in the
  pioarduino prebuilt arduino-esp32 libraries on this build; espressif/esp-idf
  `components/lwip/Kconfig` documents `LWIP_MAX_ACTIVE_TCP` default 16, range 1 to 1024:
  https://github.com/espressif/esp-idf/blob/master/components/lwip/Kconfig
- AsyncTCP accept failure signature: "tcp_accept(): _accept failed: pcb is NULL" from the
  AsyncTCP `tcp_accept` callback (managed dependency, not vendored under `src/libraries`).
