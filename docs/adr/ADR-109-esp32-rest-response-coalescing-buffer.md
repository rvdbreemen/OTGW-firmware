# ADR-109 — ESP32 REST response coalescing buffer

## Status
Proposed

## Context

On ESP32-S3 (OTGW32, OT-Direct mode), three REST endpoints had unacceptable latency measured in the field on build 2.0.0-alpha.3+ca845dd (SergeantD, 2026-05-07):

| Endpoint | T_total | T_send | T_render | chunks |
|---|---:|---:|---:|---:|
| `/api/v2/sat/status` | 3843 ms | 3816 ms | 27 ms | 406 |
| `/api/v2/device/info` | 3686 ms | 3382 ms | 304 ms | 350 |
| `/api/v2/settings` | 2840 ms | 2804 ms | 36 ms | 292 |

The same endpoints return in < 200 ms on ESP8266 (dev, 1.5.x).

Per-`sendContent()` cost is ~1.2 ms on ESP8266 (direct Arduino TCP stack) and ~13 ms on ESP32-S3 (FreeRTOS scheduler yields control to lwIP on every write). The existing streaming helpers in `jsonStuff.ino` called `sendContent()` for every JSON entry (a few dozen bytes each), producing 300–400 round-trips through the FreeRTOS/lwIP boundary per response.

JSON payload size correlates *negatively* with latency (settings is 2.3× larger than sat/status yet faster), ruling out payload size as the cause. Render time is 27–304 ms; the bottleneck is TCP flush count, not JSON generation.

Evidence files: TASK-529, Discord #dev-sat-mqtt thread 2026-05-03–07 (sergeantd reports), `state.restperf` instrumentation (`restAPI.ino:2419-2432`, `jsonStuff.ino:184-294`).

## Decision

Introduce a static 4 KB coalescing transmit buffer (`sTxBuf`) in `jsonStuff.ino` under an `#ifdef ESP32` guard. All `restSendContent()` and `restSendContentP()` calls on ESP32 append to this buffer via `restTxAppend()`. The buffer flushes automatically when full (every 4095 bytes) and is drained by `restFlushContent()`, which is called at `sendEndJsonMap()` and `sendEndJsonObj()` — the natural termination points for all JSON responses.

On ESP8266 the code path is unchanged (`sendContent` / `sendContent_P` inline); the guard ensures zero ESP8266 impact.

Do NOT migrate the sync `WebServer` to `AsyncWebServer`.

## Alternatives Considered

**A. AsyncWebServer migration** — Non-blocking async handler would eliminate TCP slot exhaustion entirely and allow concurrent requests. Rejected: requires replacing every `httpServer.send*` call site, introduces `ESPAsyncWebServer` as a new dependency, needs validation on both ESP8266 and ESP32-S3, and does not address the root cause (per-flush FreeRTOS overhead) — it hides it. Migration scope is too large relative to benefit when the coalescing buffer achieves the same result.

**B. Larger per-call chunk in `sendEscapedJsonStringContent`** — Increase the 24-byte internal chunk to 256 or 512 bytes. Reduces flush count per escaped string call, but each outer `sendJsonMapEntry` would still call `sendContent` once per entry. Would require changing every call site and still results in 100+ flushes per response; does not fully address the problem.

## Consequences

**Positive:**
- Estimated T_send reduction: 400 flushes × 13 ms → ~4 flushes × 13 ms ≈ 52 ms for a 16 KB response; total response time expected to drop from ~4 s to < 100 ms.
- No new dependencies; no changes to the ESP8266 code path.
- `restFlushContent()` is already called at every JSON response termination point; no handler changes required.

**Negative:**
- `sTxBuf.data[4096]` is a file-static buffer alive for the entire firmware lifetime. On ESP32-S3 with 320 KB DRAM this is negligible; on ESP8266 it would be significant, but the guard keeps it ESP32-only.
- The buffer is not re-entrant. The sync `WebServer` handles one request at a time, so this is safe. If `AsyncWebServer` were introduced later, this static buffer would need to become per-request.
- Hardware validation (< 500 ms on-device with active MQTT/BLE) still pending (TASK-529 ACs #2, #3, #6).

## Related Decisions

- ADR-108 — sync `WebServer` MQTT connect socket timeout (same single-threaded WebServer context)
- TASK-529 — latency profiling task; ACs #1 (instrumentation) and #4 (bottleneck identified) complete; ACs #2/#3/#6/#7/#8 pending hardware

## References

- `src/OTGW-firmware/jsonStuff.ino` — `sTxBuf` struct (line 197), `restTxAppend` (line 211), `restFlushTxBuf` (line 202), `restFlushContent` (line 262)
- `src/OTGW-firmware/restAPI.ino` — perf metrics exposed via `/api/v2/device/info` (line 2419)
- Discord #dev-sat-mqtt, 2026-05-07: SergeantD perf payload on alpha.3 (T_send=3816ms, chunks=406)
