---
id: ADR-147
title: "ESP32-S3 platform limits for concurrent webui static-file serving: keep LittleFS, guard the file-serve path, adopt the AsyncTCP config block (TASK-879)"
status: Accepted
date: 2026-06-19
tags: [esp32s3, littlefs, asynctcp, lwip, webserver, static-files, heap, concurrency, task879, platform-limits]
supersedes: []
superseded_by: []
related: [ADR-080, ADR-088, ADR-089, ADR-128, ADR-139, ADR-144, ADR-145, ADR-146]
deciders: [Robert van den Breemen]
---

# ADR-147: ESP32-S3 platform limits for concurrent webui static-file serving -- keep LittleFS, guard the file-serve path, adopt the AsyncTCP config block (TASK-879)

## Status

Accepted, 2026-06-19 (Proposed 2026-06-19; accepted by the maintainer the same day on the
strength of the on-device A/B + control isolation and the two adversarially-verified research
workflows referenced below).

**Guideline-level** (per ADR-080): this is a structural / constraint-documenting decision.
It has no automated CI gate planned; it is enforced at PR review, not by `evaluate.py`.
The one new build-time change it mandates (the AsyncTCP config block in `platformio.ini`)
is self-checking: the firmware does not compile if a flag is malformed.

**Post-acceptance note (2026-07-04):** two details in the Decision below have since changed in shipped code and are recorded here without editing the immutable body. (1) `CONFIG_ASYNC_TCP_STACK_SIZE` was later raised 4096 to 16384 to stop a non-recovering port-80 wedge (the AsyncTCP task died in its low-heap cleanup path under a concurrent burst); the rationale is inlined in `platformio.ini`. (2) The D4.1 static-file heap-backpressure serve gate described below as future work has since landed (`webFileGateTryAdmit()` in `webServerCompat.h`, returning 503 when the largest free block is too low).

## Status History

```yaml
status_history:
  - date: 2026-06-19
    status: Proposed
    changed_by: Agent
    reason: On-device A/B + control (test_ws_liveload.py, TASK-888) isolated the TASK-879 silent hang to the LittleFS static-file serving path, WebSocket-independent. Two adversarially-verified research workflows (platform limits with official Espressif/lwIP citations; static-file-hang known-issue + FLASHMEM evaluation) produced the mechanism + the keep-LittleFS recommendation. Drafted superseding nothing; documents constraints + the fix direction for TASK-879.
    changed_via: adr-kit
  - date: 2026-06-19
    status: Accepted
    changed_by: maintainer (Robert van den Breemen)
    reason: Accepted by explicit maintainer instruction in-session, with the directive to adopt the AsyncTCP config block (QUEUE_SIZE=64, MAX_ACK_TIME=5000, PRIORITY=10, RUNNING_CORE=1, STACK_SIZE=4096) as part of the decision. Root cause + recommendation match the maintainer's KISS / minimal-change-surface stance and avoid the ADR-139 reversal that embedding would cost.
    changed_via: manual
```

## Context

### The symptom

On OTGW32 (ESP32-S3, Arduino-ESP32, ESPAsyncWebServer 3.x over AsyncTCP 3.4.10, ~9 web
UI assets served from a LittleFS image) the webserver goes into a **silent hang** under a
concurrent browser-like static-file burst: no panic, no Task-Watchdog reboot, no console
output, crashlog empty; only an `esptool` hard-reset recovers it. This is TASK-879's
"webserver dead" report, and it is **distinct** from the TASK-883 flood-TWDT reboot (heap/CPU
axis) and from George's `Exception(2) StoreProhibited` panic (which carries a backtrace).

### On-device isolation (the anchor evidence, TASK-888)

`scripts/tests/test_ws_liveload.py` ran a 2x2 A/B + control on the live device
(alpha.222+73ff982, MQTT-ON):

| persistent WS subscribers | HTTP flood | result |
|---|---|---|
| 3 | static+API (8w) | **HANG** (silent, <10s, reset needed) |
| 0 | static+API (8w) | **HANG** (silent, reset needed) |
| 3 | API-only (8w)   | survived (heap recovered; WS churned 149 reconnects, device fine) |
| 0 | API-only (test_load 16w) | survived (0 reboot, ~98% handled) |

The only variable that determines the hang is **static-file serving** (LittleFS), independent
of WebSocket count. Persistent WebSocket subscribers are **exonerated**. A serial capture on
COM4 at the hang returned **0 bytes** (no panic / no TWDT / no storm on the console),
confirming the silent-hang classification.

### Is this a known ESP issue? Yes.

Multiply reproduced upstream: ESPAsyncWebServer issues
[#1450](https://github.com/me-no-dev/ESPAsyncWebServer/issues/1450) (deadlock serving static
files), [#772](https://github.com/me-no-dev/ESPAsyncWebServer/issues/772) (`task_wdt` on
`async_tcp` + `pcb is NULL` under multi-browser SPIFFS reload),
[#1162](https://github.com/me-no-dev/ESPAsyncWebServer/issues/1162) ("as little as 3 required
assets will crash the system"), [#984](https://github.com/me-no-dev/ESPAsyncWebServer/issues/984)
(`serveStatic` WDT for files >150 K). **Lineage caveat:** the `me-no-dev` repo was archived
2025-01-20; maintenance moved to the **ESP32Async** org, whose maintained AsyncTCP reframes
most crashes as *"improper use or configuration"* and ships a recommended config block (below).
So the archived-era reports are best read as "documented symptom of the old library +
misconfiguration", not "open bug in the version we run". The mechanism (single `async_tcp`
task; the `_async_queue` deadlock) is community-analyzed and plausible, **not**
maintainer-confirmed; we do not assert it as canonical.

### The actual mechanism (corrected framing)

The intuitive "LittleFS ran out of file descriptors" framing is **wrong in an important way**,
and the correction is load-bearing for the decision:

- **`maxOpenFiles` is a no-op on this build.** Arduino `LittleFSFS::begin(formatOnFail,
  basePath, maxOpenFiles, partitionLabel)` literally discards the count (`(void)maxOpenFiles;`,
  `LittleFS.cpp`), `esp_vfs_littlefs_conf_t` has **no `max_files` field**, and esp_littlefs
  ships only as a prebuilt static lib (`libjoltwallet__littlefs.a`). So TASK-879 fix-direction
  "raise `LittleFS.begin(false,"/littlefs",N)`" is a **no-op** and must not be pursued.
- **There is no fixed FD-count cap to exhaust.** The esp_littlefs FD cache grows from the heap.
  The `esp_littlefs: Unable to allocate FD` on the serial bus is therefore a **heap allocation
  failing under fragmentation/pressure**, not a fixed pool filling up.
- **One disease: heap pressure / fragmentation under concurrency on the single `async_tcp`
  task.** Static-file serving is a **heavier heap consumer per concurrent stream** than the
  in-memory API JSON path (each stream wants an FD-struct allocation + VFS buffering from the
  heap). That is exactly why static+flood hangs while API-only survives in the A/B above: same
  disease, heavier load. It is also the same disease as the REST path's logged
  `AsyncWebServerResponse::addHeader` `bad_alloc` (ADR-145 residual): heap-OOM under concurrency.

### The causal chain (TASK-879 webui hang)

1. **Heap pressure under a parallel static-file burst** -> an esp_littlefs FD-struct allocation
   fails: `esp_littlefs: Unable to allocate FD` -> `fopen(/littlefs/index.html) failed`.
2. `webSendFile` (`webServerCompat.h:305`) hands the resulting invalid-source response to
   `send()` because it guards only the **missing-file** case (`LittleFS.exists()` -> 404 at
   `:310`), **not** an alloc failure, so the connection hangs instead of erroring cleanly.
3. Hung connections never release their accept slot -> they drain the **16-slot active-TCP PCB
   pool** (`CONFIG_LWIP_MAX_ACTIVE_TCP = 16`, a hard prebuilt-lwIP ceiling).
4. With the pool drained, lwIP hands AsyncTCP a NULL pcb on every new connection: the
   `tcp_accept ... _accept failed: pcb is NULL` storm.
5. Webserver dead to new connections; HTTP + serial both silent; needs a hard reset.

### What is and is not tunable (official sources)

- `CONFIG_LWIP_MAX_ACTIVE_TCP` = **16** (default 16, range 1-1024 per espressif/esp-idf
  `components/lwip/Kconfig`). **Un-raisable on this build**: it is baked into the precompiled
  `liblwip.a`; `build_flags` cannot relink it; an IDF/core source rebuild is out of scope.
  Sibling ceilings `CONFIG_LWIP_MAX_LISTENING_TCP` / `CONFIG_LWIP_MAX_SOCKETS` /
  `CONFIG_LWIP_MAX_UDP_PCBS` are likewise 16 and likewise locked.
- **AsyncTCP IS tunable** (it is compiled from source under `.pio/libdeps`, not prebuilt), via
  the config block the maintained library recommends.

## Decision

**D1 -- Root cause.** The TASK-879 webui silent hang is **heap pressure / fragmentation under
concurrent static-file serving on the single `async_tcp` task**, surfacing as an esp_littlefs
FD-struct allocation failure that leaves an invalid-source response to hang, draining the
16-slot lwIP active-TCP PCB pool into the accept-NULL storm. It is **not** a fixed FD-pool
exhaustion and **not** a WebSocket fault. It is the same heap-under-concurrency disease as the
TASK-883/ADR-145 REST-path residual, reached via a heavier (static-file) heap load.

**D2 -- Keep LittleFS; do NOT embed the web assets in flash.** Embedding assets as PROGMEM
arrays (the WLED / Tasmota / EMS-ESP32 approach) would *structurally eliminate* the FD-struct +
VFS-buffer heap consumers on the static path, but it is **misdirected** for OTGW: it does not
fix the underlying disease (single-task starvation, the 16-PCB ceiling, the REST-path
`bad_alloc`), it would partially **reverse ADR-139** and the maintainer directive that assets
ship plain, inspectable, and on-device-updatable via FSexplorer, and the flash budget is **not**
the blocker (firmware.bin ~1.86 MB of the 2.375 MB app partition; the gzipped UI is ~142 KB, so
it would fit -- the reason is mechanism + ADR cost, not space). Peer projects embed for
RAM/speed/atomic-OTA, not to fix this hang, and keep a filesystem mounted for mutable state
anyway -- exactly OTGW's situation.

**D3 -- Adopt the AsyncTCP config block** (maintainer directive) in `platformio.ini` `[env]`
`build_flags`, as a coherent set, matching the maintained ESP32Async / EMS-ESP32 blueprint:

```
-DCONFIG_ASYNC_TCP_QUEUE_SIZE=64     ; maintained-fork default (was 32); the queue whose
                                     ; overflow the community deadlock analysis blames
-DCONFIG_ASYNC_TCP_MAX_ACK_TIME=5000 ; bound the per-connection ACK wait
-DCONFIG_ASYNC_TCP_PRIORITY=10       ; async_tcp task priority
-DCONFIG_ASYNC_TCP_RUNNING_CORE=1    ; pin async_tcp to the app core (already set, ADR-139)
-DCONFIG_ASYNC_TCP_STACK_SIZE=4096   ; async_tcp task stack
```

These are a cheap, low-risk, library-recommended hardening block. They are **secondary** to D4
(they do not by themselves fix the heap-under-concurrency disease) but are adopted because they
are the canonical-upstream guidance and the cost is build flags only.

**D4 -- The load-bearing fixes (priority order), tracked in TASK-879:**
1. **Extend the heap/max-block-aware backpressure gate to the static-file path.**
   `restEffectiveInflightCap()` (`restAPI.ino:55`) tightens REST inflight toward 1 / returns 503
   as the largest free block shrinks; it is **REST-only** today. The static-file path is
   currently ungated -- bring it under the same gate. This maps directly to the demonstrated
   failure mode and is the highest-value fix.
2. **Keep per-request work on `async_tcp` cheap and bounded** (pre-size response buffers; no
   blocking work in callbacks). Field-proven via the ADR-144/146 line of work.
3. **Reduce request count** -- immutable / long `max-age` + ETag/304 conditional GETs (largely
   already in place).
4. **Reduce request bytes** -- gzip assets (`.gz` + `Content-Encoding: gzip`).
5. **Keep** the existing existence-check before `beginResponse(LittleFS, ...)` in `webSendFile`
   (`webServerCompat.h:310`) and `serveVersionedAsset` (`FSexplorer.ino:80`). On some forks
   `beginResponse(LittleFS, <missing>)` yields a 500 or a hung connection rather than a clean
   404, so this guard is genuinely must-have. (The desirable extension is to also fail cleanly
   when the *open succeeds but the source is otherwise invalid* under alloc pressure.)

**D5 -- Debunked / closed.** Raising LittleFS `maxOpenFiles` is a no-op (D1) -- do not attempt
it. Raising `CONFIG_LWIP_MAX_ACTIVE_TCP` is not possible on this prebuilt-lib build.

## Alternatives Considered

### Alternative A: Embed the web assets in flash (PROGMEM arrays)

Embed the ~9 UI assets as PROGMEM byte arrays compiled into the firmware image (the
WLED / Tasmota / EMS-ESP32 approach), structurally eliminating the esp_littlefs FD-struct +
VFS-buffer heap consumers on the static-file path.

**Rejected.** It is misdirected for OTGW: it does not fix the underlying disease (single
`async_tcp`-task starvation, the 16-PCB lwIP ceiling, the REST-path `bad_alloc`), it would
partially **reverse ADR-139** and the maintainer directive that assets ship plain,
inspectable, and on-device-updatable via FSexplorer, and flash space is **not** the blocker
(firmware.bin ~1.86 MB of the 2.375 MB app partition; the gzipped UI is ~142 KB and would
fit). Peer projects embed for RAM/speed/atomic-OTA, not to fix this hang, and keep a
filesystem mounted for mutable state anyway -- exactly OTGW's situation. The reason is
mechanism + ADR cost, not space.

### Alternative B: Raise LittleFS `maxOpenFiles`

Raise the file-descriptor count via `LittleFS.begin(false, "/littlefs", N)` to grow the
pool that the `esp_littlefs: Unable to allocate FD` failure appears to exhaust.

**Rejected.** It is a **no-op** on this build. Arduino `LittleFSFS::begin(...)` literally
discards the count (`(void)maxOpenFiles;` in `LittleFS.cpp`), `esp_vfs_littlefs_conf_t` has
**no `max_files` field**, and esp_littlefs ships only as a prebuilt static lib
(`libjoltwallet__littlefs.a`). There is no fixed FD-count cap to enlarge: the FD cache grows
from the heap, so the allocation failure is heap pressure, not pool exhaustion.

### Alternative C: Raise `CONFIG_LWIP_MAX_ACTIVE_TCP`

Raise the 16-slot lwIP active-TCP PCB ceiling that the hung connections drain, so the
accept-NULL storm cannot occur.

**Rejected.** It is **un-raisable on this build**. The value is baked into the precompiled
`liblwip.a`; `build_flags` cannot relink it, and an IDF/core source rebuild is out of scope.
The sibling ceilings (`CONFIG_LWIP_MAX_LISTENING_TCP`, `CONFIG_LWIP_MAX_SOCKETS`,
`CONFIG_LWIP_MAX_UDP_PCBS`) are likewise 16 and likewise locked.

### Alternative D: Move the `async_tcp` task to core 0

Pin the single `async_tcp` task to core 0 to relieve the contention on the app core (core 1).

**Rejected.** Already evaluated and rejected as **ADR-144**. This ADR identifies the
static-file path as the same heap-under-concurrency disease as the REST path covered there;
re-litigating the core placement does not address the FD-struct alloc failure or the
backpressure gap.

## Consequences

- `platformio.ini` gains four AsyncTCP flags (RUNNING_CORE was already present). All three
  esp32 targets that rebuild `build_flags` from `${env.build_flags}` inherit them; verify the
  per-env blocks that override `build_flags` still include `${env.build_flags}`.
- The static-file backpressure gate (D4.1) becomes the next TASK-879 implementation step; until
  it lands, an abusive parallel static-file flood can still hang the device. A normal browser
  (~6 connections) is well within budget and is not affected.
- The web assets stay on LittleFS: plain, inspectable, FSexplorer-updatable. ADR-139 stands.
- The "raise maxOpenFiles" and "embed in flash" directions are explicitly closed for OTGW, so a
  future reader does not re-derive them.
- Field-test traceability: the AsyncTCP flag change alters runtime behaviour, so it ships under
  its own prerelease tag.

## Related Decisions

- **ADR-139** (async web stack: AsyncTCP on core 1) -- this ADR adds the rest of the AsyncTCP
  config block alongside the existing `RUNNING_CORE=1`.
- **ADR-144** (rejected async_tcp->core-0) / **ADR-145** (chunked re-serialization, retired by
  ADR-146) -- the REST-path heap-under-concurrency line; this ADR identifies the static-file
  path as the same disease.
- **ADR-146** (JsonEmit streaming + heap-tier gate) -- the gate whose extension to the static
  path is D4.1.
- **ADR-088 / ADR-089** -- the MQTT burst-window + heap-tier contracts the gate builds on.
- **TASK-879** (the webui hang), **TASK-883/884** (the REST flood residual), **TASK-888** (the
  WS-realism tool that produced the A/B anchor).

## References

Official / canonical-upstream sources:

- espressif/esp-idf `components/lwip/Kconfig`: `LWIP_MAX_ACTIVE_TCP` default 16, range 1-1024.
- ESP32Async/AsyncTCP README: the recommended `CONFIG_ASYNC_TCP_*` block (QUEUE_SIZE=64 default,
  MAX_ACK_TIME=5000, PRIORITY=10, RUNNING_CORE=1, STACK_SIZE=4096) and the "most crashes are
  improper use or configuration" framing.
- Espressif Component Registry, esp32async/espasyncwebserver v3.9.2 readme: same config guidance;
  warns against blindly increasing the queue, optimise callbacks instead.
- ESPAsyncWebServer issues #1450, #772, #1162, #984 (archived me-no-dev) -- the documented
  static-serve hang/WDT symptom class.
- Arduino-ESP32 `LittleFS.cpp` (`(void)maxOpenFiles;`) and esp_littlefs `esp_littlefs.h`
  (no `max_files` field) -- the maxOpenFiles no-op.
