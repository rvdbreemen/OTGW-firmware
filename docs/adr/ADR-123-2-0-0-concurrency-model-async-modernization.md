# ADR-123: 2.0.0 Concurrency Model — FreeRTOS PIC Task + Event-Driven Async Networking (ESP32-S3 only, ESP8266 dropped)

## Status

Accepted, 2026-06-12. Drafted 2026-06-04; amended 2026-06-12. The maintainer (Robert)
confirmed the **hybrid model** (2026-06-04) and then decided (2026-06-12) to **stop
ESP8266 support to simplify the codebase**, making 2.0.0 **ESP32-S3 only**. The
previously deferred ESP8266 sub-decision is therefore resolved: ESP8266 is dropped.
This decision depends on **ADR-128** (drop ESP8266 support; supersedes ADR-082),
accepted the same day.

## Context

### Problem

The 2.0.0 firmware still runs the ESP8266-era **cooperative single-loop** model:
`loop()` → `doBackgroundTasks()` calls every subsystem in turn, each obliged to
return within a few milliseconds, policed by an external hardware watchdog
(ADR-011). Every subsystem that has to *wait* on I/O — WiFi, the PIC serial line,
MQTT connect, an outbound webhook — has had to be hand-rewritten as a non-blocking
state machine or queue interaction. That tax is visible across a string of ADRs:
ADR-007 (timer scheduler), ADR-010 (concurrent services in one loop), ADR-047
(non-blocking WiFi reconnect), ADR-048 (non-blocking webhook), ADR-058
(non-blocking PIC `PR=`), ADR-108 (MQTT `connect()` accepted as a 15 s synchronous
loop-blocker). The recurring cost is the design and bug surface: each new
I/O-bound feature must be chopped into non-blocking fragments, and shared scratch
buffers are fragile because the loop can re-enter itself via `feedWatchDog()` →
`yield()`.

The 2.0.0 / OTGW32 hardware moves to the **ESP32**, which brings two capabilities
the ESP8266 lacked: a dual-core **FreeRTOS** preemptive scheduler, and ~320 KB+ RAM
(vs ~40 KB). This is the natural moment to decide whether — and how — 2.0.0 sheds
the cooperative model. The supporting research is in
`docs/research/2.0.0-async-modernization.md`.

### The decisive constraint: 2.0.0 is ESP32-S3 only

When first drafted this ADR treated 2.0.0 as a dual-target line (ESP8266 +
ESP32/OTGW32). The maintainer's decision of 2026-06-12 — *"stop ESP8266 support to
simplify the codebase"* — removes that constraint: 2.0.0 targets the **ESP32-S3**
(LOLIN/Wemos S3 mini / mini pro, drop-in board swap; build targets `esp32` and
`esp32-classic`, ADR-126) and ESP8266 is dropped. The event-driven async stack
(`AsyncTCP`/`ESPAsyncWebServer`) is ESP32-centric and the ESP8266 equivalent
(`ESPAsyncTCP`) is poorly maintained, so single-targeting ESP32-S3 is exactly what
makes "fully async" clean. Dropping ESP8266 supersedes the Accepted ADR-082 and
reshapes ADR-061/072/083/126; that is handled by **ADR-128** (Proposed), on which
this ADR depends.

### Constraints

- **Single target.** 2.0.0 is ESP32-S3 only (ADR-128); there is no ESP8266 path to
  keep compatible. This removes the platform-divergence an earlier draft had to
  design around.
- **The PIC UART is safety-critical and bursty.** OT frames arrive ~1 Hz but in
  bursts; the RX FIFO must never overrun. Today `handleOTGW()` is bounded to four
  lines per call (TASK-671) precisely so a noisy PIC cannot starve the loop.
- **Dependency maturity.** The maintained `ESPAsyncWebServer`/`AsyncTCP` live under
  the ESP32Async (mathieucarbou) fork on a "best effort" basis; the original
  me-no-dev repos are effectively dead. `espMqttClient` (bertmelis) is the
  actively maintained non-blocking MQTT client and supersedes `AsyncMqttClient`.
- **HA contract surface.** Discovery, retained state, LWT, and availability are
  governed by a large ADR set (ADR-041/052/077/088/096/097/102/104/108…). Any MQTT
  client swap must re-validate all of it.
- **No automated firmware tests.** Validation is build-green + `evaluate.py` +
  field testing in Discord `#beta-testing`. Concurrency bugs are the hardest class
  to catch this way, which raises the bar on the thread-safety design.

## Decision

Adopt a **hybrid concurrency model** for 2.0.0 (ESP32-S3 only): a dedicated FreeRTOS
task for the PIC UART plus event-driven async networking.

### Concurrency model (ESP32-S3)

1. **Dedicated FreeRTOS task for the PIC UART.** A single high-priority task,
   pinned to the application core (core 1 / `APP_CPU`), is the *only* code that
   touches `OTGWSerial`: it drains RX continuously and transmits from the command
   queue. This removes the four-lines-per-call bound (TASK-671) and guarantees the
   FIFO is serviced regardless of network load. This is the primary reliability
   win and the first phase to land.
2. **Event-driven async networking.** HTTP, WebSocket, and MQTT move to the
   AsyncTCP stack: `ESPAsyncWebServer` (ESP32Async fork) with its built-in
   `AsyncWebSocket` (consolidated onto port 80, retiring the separate
   `WebSocketsServer` on 81), and `espMqttClient` for MQTT. These run on AsyncTCP's
   own task (core 0 / `PRO_CPU`). This removes `httpServer.handleClient()` polling
   from the loop and eliminates ADR-108's 15 s synchronous connect blocker.
3. **Residual periodic work stays in `loop()`** on the Arduino task: timers
   (ADR-007), sensor polling, NTP, JIT discovery drip. These are not I/O-blocked
   and need no task of their own.
4. **Thread-safety discipline.** Parsed OT frames cross from the PIC task to
   consumers through a FreeRTOS queue. `OTGWState` follows a **single-writer-per-
   field** rule; the small set of genuinely shared mutable fields is guarded by a
   FreeRTOS mutex. No subsystem reads `OTGWSerial` or writes another subsystem's
   state fields directly. This discipline is the load-bearing part of the decision
   and the main source of risk.

### ESP8266 support dropped

There is no ESP8266 path to retain. The maintainer's 2026-06-12 decision drops
ESP8266 from 2.0.0 (formalised by ADR-128), so this ADR carries no cooperative-loop
fallback and no platform-conditional compilation — the model above is simply *the*
2.0.0 concurrency model. Removing the ESP8266 build env and code paths is a separate,
phased implementation effort tracked under ADR-128, not part of this decision.

### Rollout

Phased, each phase its own backlog task, build/evaluator gates, field-validation,
and ADR supersession where it lands:

0. This ADR (the model decision).
1. ESP32 PIC-UART FreeRTOS task + queue + state-locking discipline.
2. MQTT → `espMqttClient` (ESP32 path); re-validate the HA pipeline; supersede
   ADR-108 on the ESP32 side.
3. Web + WebSocket → `ESPAsyncWebServer`/`AsyncWebSocket` (ESP32 path).
4. Retire the now-redundant ESP32-side manual state machines (ADR-047/048/058
   ESP32 variants) and re-scope the watchdog role (ADR-011) for the ESP32 TWDT.

### Why this choice

The hybrid puts preemption exactly where the cooperative model is weakest — the
safety-critical PIC UART — while using event-driven async for the network-fan-out
problem the AsyncTCP stack was built for. With ESP8266 dropped there is a single
code path and no platform-divergence to maintain. It matches the proven shape of
EMS-ESP32, an architecturally near-identical project (ESP32, HA discovery, MQTT,
async web UI) that runs `ESPAsyncWebServer` + `espMqttClient` + FreeRTOS tasks.

### Enforcement

Manual review only at this stage. This is a forward-looking architectural decision
with no single code surface yet; each rollout phase will add its own enforceable
boundaries (e.g. "only the PIC task may reference `OTGWSerial`") in the ADR that
lands that phase. No declarative `Enforcement` block until Phase 1.

## Alternatives Considered

### Alternative 1 — Port the cooperative loop as-is to ESP32
Keep the single-loop model on both targets; treat the ESP32 as a faster ESP8266.

**Pros:** zero architectural risk; one code path; no thread-safety work.
**Cons:** wastes the ESP32's dual core; keeps the manual-state-machine tax and
ADR-108's 15 s connect blocker; new I/O features keep costing an ADR each.
**Rejected:** it spends the hardware upgrade on nothing and leaves the actual pain
(blocking-avoidance) in place.

### Alternative 2 — Full event-driven async, including an async PIC path, no dedicated task
Drive everything (incl. the serial line) from AsyncTCP callbacks; no FreeRTOS task
for the PIC.

**Pros:** one concurrency mechanism; smallest task count.
**Cons:** AsyncTCP callbacks run with a limited (~8 KB) stack and must not block —
a poor fit for a bursty safety-critical UART; you would still hand-roll a serial
state machine; LittleFS access from callbacks is fragile.
**Rejected:** the PIC UART is exactly the surface that benefits from a dedicated
preemptive task; forcing it into the async callback model recreates the problem we
are trying to remove.

### Alternative 3 — Task-per-subsystem keeping the existing synchronous libraries
Run `ESP8266WebServer`/`PubSubClient` unchanged but each inside its own FreeRTOS
task, blocking freely.

**Pros:** familiar synchronous code; removes blocking from the loop without
learning the async APIs; espMqttClient's author notes a dual-core ESP32 can simply
publish synchronously from a task.
**Cons:** keeps the polling web server (unmaintained on ESP32) and PubSubClient's
synchronous connect; one stack per task inflates RAM despite the ESP32's headroom;
forgoes the maintained ESPAsyncWebServer/espMqttClient ecosystem and its built-in
WebSocket/SSE/auth.
**Rejected as the primary model**, but it is the natural *fallback* if the
AsyncTCP fork's maintenance risk (see Consequences) materialises — the task-based
shape is reusable.

### Alternative 4 — ESP32-only async; drop ESP8266 from 2.0.0 (ADOPTED)
Make 2.0.0 async everywhere by dropping the ESP8266 target.

**Pros:** one (async) code path; no platform divergence; simplest async story.
**Cons:** breaks ADR-082's LTS-pin commitment to keep ESP8266 building on 2.0.0
(handled by superseding it via ADR-128); existing ESP8266 hardware must migrate to an
ESP32-S3 board.
**Adopted (2026-06-12):** the maintainer chose to stop ESP8266 support to simplify the
codebase. This is now the line's direction; ADR-128 supersedes ADR-082 and reshapes
ADR-061/072/083/126. The earlier "deferred" framing is resolved.

### Alternative 5 — ESP-IDF-native rewrite (drop Arduino)
Rebuild on ESP-IDF to get first-class FreeRTOS/event-loop primitives.

**Pros:** native concurrency and networking primitives; no third-party async fork.
**Cons:** contradicts ADR-013 (Arduino framework over ESP-IDF); rewrites the entire
codebase and every vendored library; enormous surface for a hobby-scale
maintenance team.
**Rejected:** scope is disproportionate; ADR-013 stands.

## Consequences

### Positive
- **PIC reliability:** a dedicated preemptive task guarantees UART drain
  independent of network load; the four-lines-per-call bound (TASK-671) can be
  removed.
- **Loop simplification:** `httpServer.handleClient()` polling and the ESP32-side
  manual state machines (ADR-047/048/058) disappear; new I/O-bound features no
  longer each need a bespoke non-blocking rewrite.
- **Responsiveness:** the web UI and MQTT are serviced from their own tasks, not
  between every other handler; ADR-108's 15 s connect stall is gone on ESP32.
- **Maintained dependencies:** ESP32Async + espMqttClient are actively maintained
  and battle-tested in EMS-ESP32.
- **Headroom:** ESP32 RAM relaxes the strict static-buffer/`String`-prohibition
  posture on the ESP32 path (ADR-004/042 remain in force on ESP8266).

### Negative
- **ESP8266 hardware must be replaced.** Dropping ESP8266 means existing
  ESP8266-based OTGW users cannot run 2.0.0 without swapping to an ESP32-S3 board
  (S3 mini drop-in; header-less boards need headers soldered). The 1.x line remains
  their migrate-from baseline. This migration burden is owned by ADR-128.
- **Thread-safety is now a first-class concern.** `OTGWState` is touched from the
  PIC task, the AsyncTCP task, and the loop. Getting the single-writer/mutex
  discipline wrong yields exactly the hard-to-field-debug races this project's
  test model is weakest against.
- **Async callback constraints** (limited stack, no blocking, careful LittleFS
  use) are new footguns for contributors.
- **Large re-validation surface:** the entire HA discovery/retained/LWT/
  availability pipeline must be re-tested against `espMqttClient`.
- **ADR churn:** several 2.0.0 ADRs (007/010/011/047/048/058/108) need ESP32-side
  superseding as phases land.

### Risks & Mitigation
- **Risk:** the ESP32Async fork is "best effort" and could stall.
  **Mitigation:** Alternative 3 (task-based, synchronous libs) is a pre-identified
  fallback that reuses the same FreeRTOS task structure.
- **Risk:** state races under concurrency.
  **Mitigation:** single-writer-per-field rule + mutex on shared fields, codified
  and enforced per-phase; queue-based hand-off for OT frames; land Phase 1 (PIC
  task) first in isolation to prove the pattern before networking changes.
- **Risk:** dropping ESP8266 strands users who cannot or will not swap hardware.
  **Mitigation:** the 1.x ESP8266 line stays available as the migrate-from baseline;
  ADR-128 documents the board-swap migration path (S3 mini drop-in).

## Related Decisions

- **Depends on:** ADR-128 (drop ESP8266 support; supersedes ADR-082, reshapes
  ADR-061/072/083/126). This ADR assumes the ESP32-S3-only target ADR-128 establishes.
- **Builds on:** ADR-120 (platform abstraction headers), ADR-013 (Arduino over ESP-IDF).
- **Expected to supersede (per phase):** ADR-047, ADR-048, ADR-058, ADR-108;
  re-scopes ADR-007, ADR-010, ADR-011.
- **Re-validation touch-points:** ADR-005/025 (WebSocket), ADR-006/041/052/077/088/
  096/097/102/104 (MQTT + HA discovery), ADR-121 (per-consumer heap gating).
- **Research input:** `docs/research/2.0.0-async-modernization.md`.

## References

- ESP32Async / mathieucarbou — ESPAsyncWebServer: <https://github.com/mathieucarbou/ESPAsyncWebServer>
- mathieucarbou — AsyncTCP: <https://github.com/mathieucarbou/AsyncTCP>
- espMqttClient (bertmelis): <https://www.emelis.net/espMqttClient/>
- EMS-ESP32 — AsyncMqttClient → espMqttClient migration (#1178): <https://github.com/emsesp/EMS-ESP32/issues/1178>
- ESP-IDF FreeRTOS SMP / `xTaskCreatePinnedToCore`: <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos.html>
- ADR-128 — Drop ESP8266 support from the 2.0.0 line (supersedes ADR-082).
- Maintainer announcement (Discord, June 2026) + decision 2026-06-12 "stop ESP8266 support".
