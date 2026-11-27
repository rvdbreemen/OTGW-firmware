---
id: "ADR-301"
title: "Patch ESPAsyncWebServer 3.11.2 so its mutex is a static FreeRTOS recursive mutex that cannot fail to be created, through patch_pio_libs like ADR-232"
status: "Accepted"
date: "2026-11-27"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-232"
  - "ADR-295"
topics:
  - "websocket"
  - "heap"
  - "dependency-patch"
  - "mutex"
  - "low-memory"
aliases:
  - "static recursive mutex"
  - "ESPAsyncWebServer mutex patch"
  - "uninitialised _queue_lock"
components:
  - "asyncsrv::mutex_type in the patched ESPAsyncWebServer.h (patch_pio_libs)"
symbols:
  - "asyncsrv::mutex_type"
  - "AsyncWebSocketClient::_queue_lock"
  - "AsyncWebSocket::_newClient"
  - "xSemaphoreCreateRecursiveMutexStatic"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-301 Patch ESPAsyncWebServer 3.11.2 so its mutex is a static FreeRTOS recursive mutex that cannot fail to be created, through patch_pio_libs like ADR-232

## Status

Accepted, 2026-11-27.

## Status History

```yaml
status_history:
  - date: 2026-11-27
    status: Proposed
    changed_by: "Agent: Claude (otgw-architect)"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-11-27
    status: Accepted
    changed_by: "Robert van den Breemen (decision, DM answer to the coordinator's question 31, relayed; DM text read by otgw-architect), recorded by Agent: Claude (otgw-architect)"
    reason: "Accepted by Robert: verbatim 'A. Toestemming om upstream PR te doen.' (DM 1575795976269074434), option A of question 31 = patch the library; the red/green bench stays a Verification item"
    changed_via: adr-kit lifecycle
```

## Context

Terms: WS (WebSocket, the `/ws` live log stream), SSE (Server-Sent Events, the library's event source), IDF (ESP-IDF, Espressif's
IoT Development Framework under Arduino-ESP32), RTOS (FreeRTOS, the real-time operating system the firmware runs on), TCP (Transmission Control Protocol), ACK (a TCP acknowledgement), RTT (round-trip time), OOM (out of memory),
RAM (random-access memory), PSRAM (pseudo-static RAM, the extra memory some modules have), DWARF (the debug information format
inside the ELF build file).

**What crashed.** On the K bench of TASK-3025 (ADR-295: telnet key `K` holds the free heap under 3 KB), the unit crashed when a browser
opened `/ws` during the hold. otgw-dev's analysis, from the crash in `OTGW-field-data/bench-63/2026-11-27_K/control/crash_run2/`:
the new WS client's mutex was never created, and its first lock faulted. Read in the pinned sources by otgw-architect (2026-11-27):

- ESPAsyncWebServer 3.11.2 (pinned in `platformio.ini`) uses `typedef std::recursive_mutex mutex_type` while `ASYNCWEBSERVER_USE_MUTEX`
  is 1, the ESP32 default (`src/ESPAsyncWebServer.h:53-57`, `:256-264`). Every `AsyncWebSocketClient` holds one, `_queue_lock`
  (`src/AsyncWebSocket.h:225`); the server holds `_ws_clients_lock` (`:383`); the event source holds two (`src/AsyncEventSource.h:139`, `:246`).
- A new WS client is built in a `std::list` node by `AsyncWebSocket::_newClient()` (`src/AsyncWebSocket.cpp:998-1005`), called from
  `_switchClient()` on the first ACK after the upgrade reply (`:1500-1526`), on the async_tcp task.
- The toolchain (xtensa-esp-elf, libstdc++ 14.2.0) has no `PTHREAD_RECURSIVE_MUTEX_INITIALIZER` (newlib only has
  `PTHREAD_MUTEX_INITIALIZER` = `0xFFFFFFFF`, `sys/_pthreadtypes.h:172`), so `bits/gthr-default.h:68-74` selects an init function. The
  `std::recursive_mutex` constructor calls it and drops the result (`<mutex>:89-93`, with libstdc++'s comment `XXX EAGAIN, ENOMEM, EPERM`).
- IDF 5.5.1 `pthread_mutex_init()` mallocs an `esp_pthread_mutex_t` and an RTOS recursive semaphore; on failure it returns `ENOMEM` or
  `EAGAIN` **without writing the mutex word** (`components/pthread/pthread.c:641-657`).
- `pthread_mutex_lock()` creates the mutex late only when the word equals `0xFFFFFFFF` (`:724-735`); any other value is used as a pointer
  (`:737-746`). The word holds whatever the new list node's memory held, so the lock follows a wild pointer.

Caveat: the Arduino build links the prebuilt IDF libraries; the IDF lines above are read from the matching 5.5.1 source
(`framework-espidf` 3.50501), not byte-checked against `libpthread.a`.

**Mitigation in place.** otgw-dev's handshake gate (TASK-3119, lane `lane/ws-reject-lowheap`, local commit 5906e3257) refuses a WS upgrade
at low heap. It narrows the window to the one RTT between the gate and `_newClient()`; it cannot close it, because the allocation happens
later, on another task.

**Upstream.** Issue and PR search of ESP32Async/ESPAsyncWebServer for "mutex", "out of memory" and "low heap websocket crash"
(2026-11-27): no report of this failure. The latest release v3.12.1 (2026-09-12) and `main` still declare `std::recursive_mutex`
(`ESPAsyncWebServer.h:265` at v3.12.1, `:385` on main), so a version bump does not fix it. Upstream does accept low-memory fixes:
issue #500 (a null dereference when a header copy fails to allocate) was closed on 2026-11-09.

**Rule.** Robert's standing rule: no library change without his explicit approval, and only for a confirmed bug (the
`patch_pio_libs.py` mechanism is named in that rule). ADR-232 is the one patch of that kind so far (AsyncTCP 3.4.10 plus upstream fix
7befe25).

## Decision

At build time, `scripts/patch_pio_libs.py` replaces the `std::recursive_mutex` typedef in the libdeps copy of ESPAsyncWebServer 3.11.2 with
a small recursive mutex class whose RTOS semaphore lives in a static buffer inside the object, so creating it needs no heap and cannot fail.

```cpp
// ESPAsyncWebServer.h, inside namespace asyncsrv, replacing "typedef std::recursive_mutex mutex_type;"
// OTGW-firmware patch (ADR-301): the semaphore lives in this object, so creation never allocates and never fails.
class static_recursive_mutex {
public:
  static_recursive_mutex() { _h = xSemaphoreCreateRecursiveMutexStatic(&_buf); }
  ~static_recursive_mutex() { vSemaphoreDelete(_h); }
  static_recursive_mutex(const static_recursive_mutex &) = delete;
  static_recursive_mutex &operator=(const static_recursive_mutex &) = delete;
  void lock() { xSemaphoreTakeRecursive(_h, portMAX_DELAY); }
  bool try_lock() { return xSemaphoreTakeRecursive(_h, 0) == pdTRUE; }
  void unlock() { xSemaphoreGiveRecursive(_h); }
private:
  StaticSemaphore_t _buf;
  SemaphoreHandle_t _h;
};
typedef static_recursive_mutex mutex_type;
```

- `lock_guard_type` and `unique_lock_type` stay `std::lock_guard<mutex_type>` and `std::unique_lock<mutex_type>`; both need only
  `lock()`, `try_lock()` and `unlock()`. The library uses no other `std::recursive_mutex` member (grep over `src/` for `native_handle`:
  none).
- Locking behaves as today: IDF's pthread recursive mutex is itself `xSemaphoreCreateRecursiveMutex()` (`pthread.c:646`); only where its
  memory comes from changes. Static creation is allowed before the scheduler starts (the global `AsyncWebSocket` object is built at
  static initialisation) and is enabled in this framework (`FreeRTOSConfig.h:136` `configSUPPORT_STATIC_ALLOCATION 1`).
- The patch follows ADR-232's mechanism: a stored patch, applied by the `patch_pio_libs` pre-hook to the pinned version only, with a
  fingerprint check that fails the build when the library file is not the expected one.
- otgw-dev's handshake gate (TASK-3119) stays as defence in depth: it keeps the unit from starting WS work at CRITICAL heap at all.

## Decision Contract

### Must

- The patch applies only to ESPAsyncWebServer 3.11.2 and fails the build loudly on any other content of `ESPAsyncWebServer.h`
  (fingerprint, as ADR-232).
- After the patch, `asyncsrv::mutex_type` allocates nothing from the heap when it is constructed.
- The patch is documented in `patch_pio_libs.py` with this ADR, the upstream status and the source lines above.

### Must Not

- Must not set `ASYNCWEBSERVER_USE_MUTEX` to 0: that turns every lock into a no-op while `textAll()` runs on the loop task and the clients
  on async_tcp, a data race instead of a crash.
- Must not change any other line of the library in this patch.
- Must not remove the TASK-3119 handshake gate because of this patch.

### Exceptions

- None.

### Verification

- Host test (to be written with the implementation): the patch applies to the pinned file and the fingerprint check fails on a changed
  file; the patched header has no `std::recursive_mutex` typedef.
- Host test on the real patched class, cut from the patched header, against an RTOS stub whose `malloc` always fails: the mutex
  constructs and locks recursively (lock, lock, unlock, unlock, try_lock). Red on a model of the unpatched path (a stub
  `pthread_mutex_init` that returns `ENOMEM` and leaves the word at a poison value). The model is labelled as a model.
- Bench (otgw-bench-tester), the real proof: otgw-dev's crash_run2 scenario (K deep hold, repeated `/ws` connects during the hold),
  n >= 20 connects per arm, old build and patched build, on the bench Classic with PSRAM and on the OTGW32 without PSRAM. Pass: 0 crashes
  on the patched build and at least one crash on the old build in the same run (a green arm alone proves nothing). Record
  `sizeof(AsyncWebSocketClient)` of both builds and `hd_min_free_heap`.

## Alternatives Considered

- **(B) A free-heap check in `_newClient()` before `emplace_back`.** Kept as an optional addition, not the fix: a time-of-check versus
  time-of-use check narrows the window and does not close it. It also needs care with ownership (`_newClient()` deletes the request;
  its only caller ignores the return value). It would narrow the separate abort when the list node itself cannot be allocated (`operator
  new` with exceptions off), which this ADR does not cover.
- **(C) Only the handshake gate (TASK-3119).** Rejected as the closing fix: one RTT separates the gate from the allocation, on another
  task, so the crash stays possible at CRITICAL heap. Kept as defence.
- **(D) `-DASYNCWEBSERVER_USE_MUTEX=0`.** Rejected: no locks, a data race (Must Not).
- **(E) Wait for upstream or bump the version.** Rejected for 2.0.0: no upstream report exists, and v3.12.1 and `main` keep
  `std::recursive_mutex`. A report upstream is wanted (an outward post, Robert's yes), so this patch can go away later.
- **Do nothing.** Rejected: a browser tab that connects while the unit is at CRITICAL heap can crash the unit, and ADR-295's emergency
  recovery makes exactly that reconnect happen.

## Consequences

**Positive:**

- The "mutex not created" failure is gone for every mutex of the library (WS client, WS server, event source), not only for the case the
  bench hit.
- Two small heap allocations fewer per WS client (the `esp_pthread_mutex_t` and the heap semaphore).

**Negative:**

- Each object with a mutex grows by `StaticSemaphore_t` (about 80 B, an *estimate*; the bench records `sizeof`). Baseline: `sizeof(AsyncWebSocketClient)` = 168 B today (`DW_AT_byte_size` in the DWARF of the control elf 453+e0dde12, read by otgw-dev 2026-11-27); the `std::recursive_mutex` in it is the 4 B `pthread_mutex_t` handle (inferred from `sys/_pthreadtypes.h`). The heap semaphore it
  replaces is about the same size, so the net heap per client is about even. Mitigation: measured on the bench before acceptance.
- A second local patch on a dependency to carry. Mitigation: the fingerprint check fails the build on a library update, and the upstream
  report aims to make the patch unnecessary.
- The abort when the list node itself cannot be allocated remains. Mitigation: the handshake gate, and alternative (B) if the bench shows
  it.

## Open Questions

- [x] Robert: approve this library patch under his rule "no library changes without explicit approval" (coordinator's DM question 31:
  A patch, B gate only, C nothing).
  — **Answered 2026-11-27 by Robert van den Breemen (decision, DM answer to the coordinator's question 31, relayed; DM text read by otgw-architect), recorded by Agent: Claude (otgw-architect):** Approved, option A (patch the library, ADR-232 mechanism). Question 31 (BlueFlame Bot DM 1575786437851480157, 2026-11-27 08:12 UTC) defined A as this patch, B as only the TASK-3119 gate, C as nothing on the library. Robert answered verbatim 'A. Toestemming om upstream PR te doen.' (DM 1575795976269074434, 08:50:37 UTC), read by otgw-architect from the DM history.
- [x] Robert: may the failure be reported upstream (ESP32Async/ESPAsyncWebServer issue, an outward post)? — **Answered 2026-11-27 by Robert van den Breemen (decision, DM answer to the coordinator's question 31, relayed; DM text read by otgw-architect), recorded by Agent: Claude (otgw-architect):** Yes: same DM (1575795976269074434), verbatim 'A. Toestemming om upstream PR te doen.' (permission to make the upstream PR). The coordinator posts it only after the patched build has passed the red/green bench (n >= 20 per arm), without private data.
- [x] Bench: does the crash_run2 scenario show crashes on the old build and none on the patched build, and what is the `sizeof` change? — **Answered 2026-11-27 by Agent: Claude (otgw-architect):** Deferred to Verification, not answered: the red/green K bench (crashes on the old build, none on the patched one, n >= 20 per arm, .63 and .76) and the measured sizeof run after the implementation, as for ADR-295. If the bench fails or is ambiguous, the patch does not ship and a new ADR withdraws this one.

## Related Decisions

- **ADR-232 (Patch AsyncTCP 3.4.10 with upstream fix 7befe25 through patch_pio_libs)**: the mechanism and the precedent for a local
  dependency patch.
- **ADR-295 (emergency heap recovery aborts the /ws TCP connections)**: its recovery makes browsers reconnect at low heap, which is when
  this crash happens.
- **ADR-147 (adopt the maintained AsyncTCP/ESPAsyncWebServer configuration)**: where the library pin and its build flags are decided.

## References

- TASK-3025 (heaprec, the K bench), TASK-3119 (the handshake gate, otgw-dev, local 5906e3257).
- `OTGW-field-data/bench-63/2026-11-27_K/control/crash_run2/` (crash evidence, read by otgw-dev).
- `OTGW-field-data/_comms/design_wsclient_mutex_lowheap_2026-11-27.md` (the design memo behind this ADR).
- ESPAsyncWebServer 3.11.2 `src/ESPAsyncWebServer.h:53-57,256-264`, `src/AsyncWebSocket.h:225,383`, `src/AsyncWebSocket.cpp:998-1005,1500-1526`,
  `src/AsyncEventSource.h:139,246`.
- libstdc++ 14.2.0 `include/c++/14.2.0/mutex:83-98`, `bits/gthr-default.h:68-74`; newlib `sys/_pthreadtypes.h:172`.
- ESP-IDF 5.5.1 `components/pthread/pthread.c:622-660,724-746`; arduino-libs esp32s3 `freertos/config/include/freertos/FreeRTOSConfig.h:136`.
- Upstream: https://github.com/ESP32Async/ESPAsyncWebServer (releases v3.12.1; issue #500).
