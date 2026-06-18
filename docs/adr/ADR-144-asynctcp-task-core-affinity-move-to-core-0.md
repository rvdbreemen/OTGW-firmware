# ADR-144 Move the AsyncTCP service task to core 0 (amend ADR-139's core-1 pin); fix the under-load IDF Task-Watchdog reboot

## Status

**Rejected, 2026-06-18.** This ADR proposed moving the `async_tcp` service task
from core 1 to core 0 (`-DCONFIG_ASYNC_TCP_RUNNING_CORE` 1 -> 0) on the
hypothesis that the under-load IDF Task-Watchdog reboot (TASK-883) was `loopTask`
being starved by `async_tcp` on the shared app core. **A controlled hardware
experiment refuted that hypothesis.** With `async_tcp` pinned to core 0 the
device still rebooted under the same 8-worker flood (worse: 3 reboots in a
1.5-min run vs 2 on core 1), and the decoded panic backtrace named the trip task
as **`async_tcp` itself** (CPU 0), with `loopTask` running fine on CPU 1. The
watchdog tripped because `async_tcp` was blocked synchronously in an O(n^2)
`cbuf::resize` realloc loop inside `serializeJson(doc, AsyncResponseStream)` —
the `AsyncResponseStream` buffer grew one byte at a time from the 1460-byte
default, rebuilding the whole FreeRTOS RingBuffer on each growth. **Core affinity
is irrelevant to that bug.** The real fix is a one-line bug fix in `restSendJson`
(`webServerCompat.h`: pre-size the response buffer to `measureJson(doc)` so the
buffer never resizes), a refactor within ADR-141's existing pattern that
therefore needs no ADR. `-DCONFIG_ASYNC_TCP_RUNNING_CORE` is restored to
ADR-139's value of `1`; ADR-139's core-1 pin stands unchanged (no amendment in
force).

This ADR is **kept, not deleted**, to preserve the investigation trail: the
hypothesis, the experiment that refuted it, and why the real cause lay elsewhere.
The Context / Decision / Alternatives below are retained as originally drafted
(under the now-refuted hypothesis); read them as the rejected proposal, not as
current guidance.

## Status History

status_history:
  - date: 2026-06-18
    status: Proposed
    changed_by: Agent
    reason: Amend ADR-139's SECONDARY AsyncTCP core-affinity sub-decision. Move the async_tcp service task off the Arduino app core (core 1) onto core 0 by changing -DCONFIG_ASYNC_TCP_RUNNING_CORE from 1 to 0 in the global [env] build_flags, to fix the under-load IDF Task-Watchdog reboot observed in the TASK-883 hardware A/B. ADR-139's primary cache-busting decision is untouched. Proposed pending hardware confirmation that the core-0 build survives the 8-worker flood that rebooted the core-1 build (field/hardware validation, so this ADR stays Proposed until the maintainer accepts).
    changed_via: adr-kit
  - date: 2026-06-18
    status: Rejected
    changed_by: Agent
    reason: Hardware experiment refuted the loopTask-starvation premise. Core-0 build still rebooted (3x in a 1.5-min 8-worker flood); decoded panic named async_tcp itself as the trip task (CPU 0), loopTask healthy on CPU 1. Root cause is an O(n^2) cbuf::resize realloc loop in serializeJson(doc, AsyncResponseStream) from the 1460-byte default buffer growing one byte at a time, not core affinity. Real fix is a bug fix in restSendJson (pre-size to measureJson(doc)); RUNNING_CORE restored to ADR-139's 1. Kept for the investigation trail.
    changed_via: manual

## Context

The 2.0.0 line is single-target ESP32-S3 (ADR-128 dropped ESP8266). The S3 is
dual-core: **core 0 (PRO_CPU)** runs the WiFi / system stack, and **core 1
(APP_CPU)** runs the Arduino `loopTask` (the cooperative `loop()` that drives
the firmware). HTTP is served by ESPAsyncWebServer over AsyncTCP @ 3.4.10; the
async work runs on a dedicated FreeRTOS service task named `async_tcp`. That
task is created by `xTaskCreatePinnedToCore(_async_service_task, "async_tcp",
CONFIG_ASYNC_TCP_STACK_SIZE, NULL, CONFIG_ASYNC_TCP_PRIORITY,
&_async_service_task_handle, CONFIG_ASYNC_TCP_RUNNING_CORE)` at
`AsyncTCP.cpp:382`. Its priority is the **library default**
`CONFIG_ASYNC_TCP_PRIORITY = 10` (`AsyncTCP.h:50`, `#ifndef`-guarded): we do
**not** override it. The library's default core is
`CONFIG_ASYNC_TCP_RUNNING_CORE = -1` ("any available core",
`AsyncTCP.h:37`, also `#ifndef`-guarded).

ADR-139's SECONDARY sub-decision pinned `CONFIG_ASYNC_TCP_RUNNING_CORE = 1` in
the global `[env]` build_flags, putting `async_tcp` on the **same core as
`loopTask`**. ADR-139 was explicit that this was "deterministic-placement
hygiene ... **not** claimed as the cause of the `/` hang", chosen to match the
EMS-ESP32 blueprint. Important caveat about that blueprint match: EMS-ESP32 pins
**five** `CONFIG_ASYNC_TCP_*` values together (`RUNNING_CORE=1`,
`STACK_SIZE=6144`, `QUEUE_SIZE=64`, `PRIORITY=10`, `MAX_ACK_TIME=5000`); ADR-139
adopted only `RUNNING_CORE`, leaving the other four at library defaults. So we
took the blueprint's *core* without its *priority/stack/queue/ack* tuning.

The IDF Task Watchdog (TWDT) in this build runs with `idle_core_mask = 0` (the
idle tasks are **not** subscribed). The subscribed tasks are `loopTask` and
`async_tcp` (the latter self-subscribes via `CONFIG_ASYNC_TCP_USE_WDT = 1`, see
`AsyncTCP.cpp:320-323`). `loopTask` feeds the watchdog from the cooperative loop
via `feedWatchDog()`. The failure mechanism follows directly from the task
model: if `async_tcp` (priority 10) and `loopTask` (priority 1) are on the
**same** core, then under sustained HTTP load the higher-priority `async_tcp`
starves `loopTask`. `loopTask` then never reaches `feedWatchDog()`, its TWDT
entry expires (~30 s), and the IDF panics and reboots.

**Empirical A/B (TASK-883).** Same device (`192.168.88.39`), same parameters,
both arms fresh-flashed, NVS/WiFi preserved and **neither arm provisioned**, so
both share an equal heap baseline. Both arms had `RUNNING_CORE = 1`
(`async_tcp` on core 1, the pre-existing state):

- **Arm A = `alpha.204` (`0a3e5eee`)**: pre-ArduinoJson-migration, hand-rolled
  one-pass JSON, `async_tcp` on core 1.
- **Arm B = `alpha.211` (`e2ba3ee0` / `f27030c`)**: full ArduinoJson v7 migration
  (ADR-141), `async_tcp` on core 1.

Results:

- **Realistic throttled load** (4 workers @ 1 s think-time, ~3.2 req/s, 3 min):
  **both arms pass, 0 reboots.**
- **Unthrottled flood**: Arm A survived an **8-worker / 1 min**, **8-worker /
  2 min**, AND a **16-worker / 2 min** flood with **zero reboots** (it degraded
  gracefully, responses timed out but the device stayed up). Arm B **rebooted
  twice** on the **8-worker / 1 min** flood: once cold (boot 1->3) and once warm
  at 8 min uptime (boot 3->5).

The inter-arm commit range is migration + version banner + scripts only;
`RUNNING_CORE = 1` is identical in BOTH arms; the partition layout differs but
that is not a scheduling confound. The reading: the ArduinoJson migration's
heavier **per-request** work (build a `JsonDocument`, then serialize, versus the
old one-pass streaming) **widened the starvation window** so that the same flood
now trips the watchdog. But the ROOT mechanism is structural and pre-dates the
migration: `async_tcp` (priority 10, core 1) starving `loopTask` (priority 1,
core 1) under concurrent HTTP. The migration did not introduce the contention;
it lowered the load threshold at which the contention becomes fatal.

This same loop-starvation mechanism is the most likely explanation for the
pre-existing **TASK-879** report (`alpha.199`, OTGW32 combo, "AsyncWebServer not
listening / web dead under normal conditions"): there too the loop task was
starved off core 1 under combined MQTT + WebSocket + HTTP load, with the HTTP
response arriving 4-8 s late and TWDT reboot symptoms (not a heap exhaustion,
not an accept-failure, not a crash).

## Decision

**Change `-DCONFIG_ASYNC_TCP_RUNNING_CORE=1` to `=0` in the GLOBAL `[env]`
`build_flags` of `platformio.ini`** (applies to `esp32`, `esp32-classic`, and
`esp32-combo`, because the two `*-classic`/`*-combo` envs rebuild their
`build_flags` from `${env.build_flags}`). This moves the `async_tcp` service
task **off** the Arduino app core (core 1) and onto **core 0** (the WiFi/system
core).

With `async_tcp` on core 0, `loopTask` on core 1 runs **regardless of HTTP
load**, so it keeps reaching `feedWatchDog()` and the TWDT no longer expires.
`async_tcp` now contends with the WiFi/system stack on core 0 instead; under a
heavy HTTP flood it responds **more slowly** (graceful degradation) rather than
starving the loop and rebooting. Slower-under-flood is strictly preferable to a
reboot.

This decision changes **only** the core value that ADR-139's secondary hygiene
tweak chose; it does so on **empirical evidence (TASK-883) that ADR-139 did not
have** at the time. As before, this ADR sets **only** `RUNNING_CORE`; the other
`CONFIG_ASYNC_TCP_*` values stay at their library defaults (priority 10, etc.),
exactly as ADR-139 left them. Placing the flag in the global `[env]` (not
`[env:esp32]`) is unchanged from ADR-139 and is correct for the same reason
ADR-139 recorded: `esp32-classic` / `esp32-combo` rebuild `build_flags` from the
global `[env]` and do NOT inherit additions made to `[env:esp32]`, and
post-ADR-128 the global `[env]` is ESP32-only, so the flag is safe there.

(Note: as of this writing the fix is already present in the working tree:
`platformio.ini:58` reads `-DCONFIG_ASYNC_TCP_RUNNING_CORE=0` with an explanatory
comment block at lines 47-57. This ADR records and justifies that decision; it
remains Proposed pending the hardware confirmation described under Consequences.)

## Alternatives Considered

### Alternative A: Keep `RUNNING_CORE = 1` (do nothing, ADR-139 status quo)

Leave `async_tcp` pinned to core 1 alongside `loopTask`.

Rejected. This is precisely the configuration that **rebooted twice** under the
TASK-883 8-worker flood (Arm B), and structurally the configuration that lets a
priority-10 task starve the priority-1 loop on the same core. "Do nothing" keeps
a known under-load reboot. The migration widened the window, but the config is
the root cause carrier, so leaving it is not an option.

### Alternative B: Revert to the library default `RUNNING_CORE = -1` (float anywhere)

Remove the flag entirely and let the scheduler place `async_tcp` on "any
available core" (the pre-ADR-139 default, `AsyncTCP.h:37`).

Rejected. `-1` is non-deterministic: the scheduler may still land `async_tcp` on
core 1, reproducing the starvation intermittently and unrepeatably. The whole
point of this ADR is to *guarantee* the loop core is free of the high-priority
async task; `-1` gives no such guarantee. Deterministic core 0 is strictly
better than an unpredictable placement that sometimes re-creates the bug.

### Alternative C: Keep `async_tcp` on core 1 but lower its priority below `loopTask`

Keep `RUNNING_CORE = 1` and additionally set `CONFIG_ASYNC_TCP_PRIORITY` below
`loopTask`'s priority so the loop wins the core under contention.

Rejected for this fix. It is a larger, riskier surface: it overrides a library
default this project has deliberately left untouched, it changes the relative
scheduling of the async stack against everything else on core 1 (with its own
throughput and latency consequences that we have not measured), and a too-low
async priority can itself starve the network stack. Moving the task to the other
core solves the starvation with one value change and no priority surgery, keeping
to the project's KISS / minimal-change-surface principle. The other
`CONFIG_ASYNC_TCP_*` knobs are deliberately left at defaults, as in ADR-139.

### Alternative D: Widen or disable the Task Watchdog (raise the timeout, or unsubscribe a task)

Stop the symptom by raising the TWDT timeout, or by not subscribing `loopTask`
(or `async_tcp`) to the watchdog.

Rejected. This masks the problem rather than fixing it: if `loopTask` is starved
off core 1, the firmware loop genuinely is not running (MQTT, OT processing,
timers all stall), watchdog or no watchdog. Silencing the watchdog converts a
visible reboot into an invisible hang, which is worse. The watchdog is correctly
reporting that the loop stopped; the fix is to stop starving the loop.

### Alternative E: Throttle inbound HTTP concurrency

Cap the number of concurrent requests / connections the server accepts so the
flood never builds up enough load to starve the loop.

Rejected as the primary fix: it addresses the symptom (load) not the root cause
(co-located priority inversion on one core), and it degrades legitimate use
(e.g. a browser firing ~8 parallel sub-resource requests, the bug-113 pattern
this device is already known to be sensitive to). Once `async_tcp` is on core 0,
the loop survives the flood without an artificial concurrency cap. Throttling
may remain a useful independent robustness measure, but it is not the fix for
this reboot.

## Consequences

**Benefits**

- Fixes the under-load IDF Task-Watchdog reboot: `loopTask` on core 1 keeps
  feeding the watchdog regardless of HTTP load, so the migration-worsened flood
  case (TASK-883 Arm B) no longer reboots.
- Very likely also fixes the pre-existing **TASK-879** (`alpha.199`) "web dead
  under normal conditions" report, which is the same `loopTask`-starvation
  mechanism under combined MQTT + WebSocket + HTTP core-1 load. (Likely, not
  certain: see the caveat below.)
- One value change, no priority/stack/queue surgery, no watchdog masking, no
  concurrency cap: the smallest change that removes the root cause.

**Trade-offs**

- `async_tcp` now shares core 0 with the WiFi / system stack. Under a heavy HTTP
  flood it may respond **more slowly** than the deterministic core-1 placement
  gave (the async task and the radio stack now compete for the same core).
  This is graceful degradation. **Slower under flood is far preferable to a
  reboot under flood.** Under normal (throttled) load both placements pass, so
  the trade-off only bites at flood levels that the old config could not survive
  at all.

**Risks and mitigations**

- *Risk (honest caveat)*: the starvation mechanism (`loopTask` starved off
  core 1) is **inferred** from the TASK-883 A/B plus the FreeRTOS task model; it
  is **not yet confirmed by a panic backtrace**. *Mitigation*: a
  USB-Serial-JTAG capture during a fresh repro on the core-1 build would print
  the TWDT panic line ("Task watchdog got triggered. CPU 1: <taskname>"),
  naming whether `loopTask` or `async_tcp` is the unfed task. Note the fix is
  correct **regardless of which** of the two subscribed tasks trips: moving
  `async_tcp` off core 1 frees core 1 for the loop either way, so a confirming
  backtrace strengthens the explanation but does not change the decision.
- *Risk*: moving `async_tcp` onto core 0 introduces a new contention with the
  WiFi stack that regresses something we have not measured. *Mitigation*: the
  validation flood (below) exercises exactly the heavy-HTTP-plus-WiFi case; a
  pass there with zero reboots and acceptable response behaviour clears this.

**Validation (field / hardware)**

Rebuild the `esp32` target, flash the OTGW32 hardware, and re-run the TASK-883
**8-worker flood** that rebooted the core-1 build (Arm B). **PASS = boot-count
delta of 0** across the flood where the core-1 build incremented the boot
counter (rebooted) twice. Because this is hardware/field validation, this ADR
stays **Proposed** until the maintainer (Robert van den Breemen) accepts it after
the hardware confirmation.

## Related Decisions

- **ADR-139 (ETag + Bounded max-age Cache-Busting Standard; AsyncTCP Task
  Config)**: **amended by this ADR.** This ADR amends ONLY ADR-139's SECONDARY
  AsyncTCP core-affinity sub-decision (`RUNNING_CORE` 1 -> 0). ADR-139's PRIMARY
  cache-busting decision and all of its serving mechanics are untouched. ADR-139
  stays immutable (Accepted); on maintainer acceptance of this ADR it gains an
  "Amended by ADR-144" status line (the sanctioned immutability exception).
- **ADR-123 (2.0.0 Concurrency Model / Async Modernization)**: the master
  concurrency decision. The core-affinity placement of `async_tcp` relative to
  `loopTask` is a direct parameter of the dual-core model ADR-123 establishes.
  (Disambiguated by title: there are two files claiming ADR-123 in
  `docs/adr/`; this references the concurrency-model one.)
- **ADR-141 (ArduinoJson v7 migration)**: the migration whose heavier
  per-request JSON work **widened** the starvation window enough that the
  TASK-883 flood started rebooting (Arm A -> Arm B). It did not create the root
  cause; it lowered the load threshold at which the core-1 contention becomes
  fatal.
- **ADR-128 (Drop ESP8266 from 2.0.0)**: makes the global `[env]` ESP32-only, so
  this build flag is safe to place there (the same placement rationale ADR-139
  used).
- **ADR-135 (ESP32 TWDT is the Primary Watchdog)**: establishes the IDF Task
  Watchdog as the firmware's primary watchdog. The reboot this ADR fixes is a
  TWDT panic, so ADR-135 is the decision that gives that watchdog its
  authority; the fix keeps the `loopTask` watchdog entry fed by freeing core 1
  for the loop. This ADR does not change ADR-135's watchdog policy.
- **ADR-143 (Telnet + ser2net on AsyncTCP to remove loop-task socket-write
  blocking)**: a sibling decision in the same campaign of protecting `loopTask`
  health on the 2.0.0 async stack. ADR-143 stops the loop task *blocking* on
  socket writes; this ADR stops the loop task being *starved off its core* by
  the async service task. Complementary, non-overlapping mechanisms toward the
  same goal (a loop that always runs and always feeds the watchdog).
- **ADR-080 (Binding ADR rules must have a CI gate)**: this ADR is labeled
  guideline-level per ADR-080 because it is a single build-flag value with no
  clean automated gate planned; it is enforced at PR review.

## References

- `platformio.ini:58`: global `[env].build_flags` now carrying
  `-DCONFIG_ASYNC_TCP_RUNNING_CORE=0` (with the explanatory comment block at
  `platformio.ini:47-57`). ADR-139 had set this to `=1`; this ADR changes it to
  `=0`.
- `.pio/libdeps/esp32-combo/AsyncTCP/src/AsyncTCP.cpp:382`: the
  `xTaskCreatePinnedToCore(_async_service_task, "async_tcp", ...,
  CONFIG_ASYNC_TCP_PRIORITY, ..., CONFIG_ASYNC_TCP_RUNNING_CORE)` call that
  creates the service task with the priority and core this ADR governs.
- `.pio/libdeps/esp32-combo/AsyncTCP/src/AsyncTCP.h:50`:
  `CONFIG_ASYNC_TCP_PRIORITY 10`, the library default (`#ifndef`-guarded) that
  we do NOT override, so `async_tcp` runs at priority 10 above `loopTask`'s
  priority 1.
- `.pio/libdeps/esp32-combo/AsyncTCP/src/AsyncTCP.h:37`:
  `CONFIG_ASYNC_TCP_RUNNING_CORE -1` ("any available core"), the library default
  that ADR-139 overrode to `1` and this ADR sets to `0`.
- `.pio/libdeps/esp32-combo/AsyncTCP/src/AsyncTCP.cpp:320-323`: `async_tcp`
  self-subscribes to the Task Watchdog (`CONFIG_ASYNC_TCP_USE_WDT = 1`),
  confirming both subscribed tasks (`loopTask`, `async_tcp`) are watchdog-fed.
- ADR-139, "SECONDARY (blueprint alignment, not the fix)" section: the amended
  sub-decision (the `RUNNING_CORE = 1` pin).
- TASK-883: this fix and the hardware A/B that surfaced it (Arm A `alpha.204`
  `0a3e5eee` vs Arm B `alpha.211` `e2ba3ee0`/`f27030c`; 4-worker throttled load
  passes both arms; 8-worker flood reboots Arm B twice, survives Arm A).
- TASK-867 AC#8: the load test that ran the flood and surfaced the reboot.
- TASK-879 (`alpha.199`, OTGW32 combo): the pre-existing "web dead under normal
  conditions" / TWDT-reboot report this fix very likely also resolves (same
  `loopTask`-starvation mechanism under combined MQTT + WebSocket + HTTP load).
- `other-projects/EMS-ESP32-dev/platformio.ini`: the blueprint that ADR-139's
  core-1 pin came from. The blueprint pins five `CONFIG_ASYNC_TCP_*` values
  together; this project adopted only `RUNNING_CORE`, which is why the blueprint
  core value alone proved harmful in our single-flag context.

This ADR has a code surface (one `platformio.ini` build flag) but is enforced at
PR review, not by an automated gate: there is no clean forbid/require
`evaluate.py` pattern for "the AsyncTCP service task is pinned to core 0", and a
literal grep for the flag value would be brittle against future tuning. It is
therefore labeled guideline-level per ADR-080, and no `## Enforcement` block is
added.
