# ADR-145 Serve REST JSON via a chunked, pull-based response that re-serializes per TCP window (no whole-response buffer)

## Status

Proposed, 2026-06-18. Guideline-level (per ADR-080): a response-serving idiom
enforced at PR review, not by an automated `evaluate.py` gate. **Field-validation
gated** on the criteria this fix actually targets: the under-load IDF Task-Watchdog
reboot is eliminated (0 watchdog panics on the chunked build, where the buffered
path produced all-watchdog panics), realistic throttled load (4 workers) stays
clean (bootcount delta 0, heap recovers, no ADR-089 tier breach), and the JSON
contract is intact (`json_golden` semantic equality; the heavy multi-chunk
endpoints come back byte-identical). The maintainer (Robert van den Breemen)
accepts on that basis.

**NOTE -- this fix does NOT make the device survive the abusive unthrottled
8-worker flood.** With the watchdog/alloc-storm path gone, that flood now aborts on
an unhandled `bad_alloc` thrown from `AsyncWebServerResponse::addHeader` instead of
tripping the watchdog. The exact allocation-failure cause is **not pinned**: 46 KB
free and a 31 KB max-block at the time argue against simple heap exhaustion, so the
mechanism is unresolved and deliberately out of scope here. That residual is a
separate, lower-priority concern (category: connection/concurrency limiting, i.e.
backpressure so the device never accepts more concurrent heavy requests than it can
serve) tracked in its own task, NOT a gate on this ADR.

This ADR refines the JSON response-serving sub-decision of **ADR-141** (adopt
ArduinoJson v7) and **ADR-132** (HTTP stack on ESPAsyncWebServer). It does not
supersede them: ADR-141's "use ArduinoJson for REST output" and ADR-132's
imperative-build/async-send bridge both stand. It changes only *how* the built
`JsonDocument` reaches the socket.

## Status History

status_history:
  - date: 2026-06-18
    status: Proposed
    changed_by: Agent
    reason: Replace the whole-response AsyncResponseStream buffer in restSendJson() with a chunked pull response that re-serializes a TCP-window slice per chunk, so no large contiguous buffer is ever allocated. Fixes the TASK-883 under-load IDF Task-Watchdog reboot (and the plausibly same-root TASK-879 field "web dead" report). Proposed pending hardware confirmation (8-worker flood, bootcount delta 0).
    changed_via: adr-kit

## Context

REST handlers build a `JsonDocument` and hand it to `restSendJson(JsonDocument&)`
(`webServerCompat.h`). Before this ADR, `restSendJson` opened an
`AsyncResponseStream` and called `serializeJson(doc, stream)`. That path buffers
the **entire** serialized response in one contiguous FreeRTOS RingBuffer (`cbuf`)
before the socket send begins:

- `serializeJson` writes the document **one byte at a time** through the Print
  interface (`TextFormatter::writeChar` -> `Writer::write(uint8_t)` ->
  `AsyncResponseStream::write(uint8_t)`).
- `AsyncResponseStream::write` grows its `cbuf` by only the deficit
  (`resizeAdd(needed)`), and `cbuf::resize` **rebuilds the whole RingBuffer**
  (`xRingbufferCreate` + `malloc` temp + full copy + delete) on every growth.
  From the 1460-byte default an ~8.6 KB response (e.g. `/v2/settings`) does
  thousands of full-buffer reallocs.

**TASK-883 hardware finding.** A controlled A/B (pre-migration `alpha.204` vs
migrated `alpha.211`, same device, same params) showed the ArduinoJson migration
measurably lowered the under-load Task-Watchdog (TWDT) margin: pre-migration
survived an 8-worker flood (and even 16 workers) with zero reboots; migrated
rebooted twice on an 8-worker/1-min flood. Two decoded USB-Serial-JTAG panic
backtraces (`scripts/tests/_serialcap.py`) put the trip task at `async_tcp`,
blocked synchronously in `serializeJson -> AsyncResponseStream::write ->
async_ws_log_e("Failed to allocate") -> esp_diagnostics log hook`. Under the
flood the heap fragments below the response size (maxfreeblock floor ~8.7 KB <
8.6 KB), the contiguous buffer alloc **fails**, and `write()` then logs the
failure **per byte** through the slow diagnostics hook until `async_tcp` misses
the 30 s watchdog and the device reboots.

Two narrower fixes were tried on hardware and **both still rebooted**:

- Pin `async_tcp` to core 0 (proposed as **ADR-144**, *Rejected*): the panic
  named `async_tcp` itself, not `loopTask`, so the loopTask-starvation premise was
  wrong; core affinity is irrelevant to a task blocked in its own handler.
- **Pre-size** the `AsyncResponseStream` buffer to `measureJson(doc)`: a real
  common-case improvement (kills the O(n^2) ringbuffer rebuild when the heap can
  satisfy the one allocation) and **kept** (shipped `alpha.212`), but it does not
  fix the flood: the single large allocation simply fails sooner under
  fragmentation, returning to the per-byte log storm.

Realistic throttled load (4 workers, ~3.2 req/s) passes cleanly on every build
throughout. But the A/B arms had **no MQTT and no WebSocket clients** (both core-1
background work the field device carries), so "safe for normal use" is not
established, and the flood is a reproducible proxy for the field-reported TASK-879
"web dead under normal conditions" on `alpha.199`.

The common factor across both failed fixes is structural: **any scheme that
buffers the whole response in one contiguous block fails when the heap cannot
provide that block under concurrency.** `AsyncJsonResponse` (the EMS-ESP32
pattern) has the same defect: it `malloc`s a flat buffer of `measureJson` bytes.

## Decision

**`restSendJson` serves JSON as a chunked, pull-based response that holds no
whole-response buffer.** Concretely (`webServerCompat.h`):

1. **Move the document onto the heap** behind a `std::shared_ptr<RestJsonStream>`
   (`{ JsonDocument doc; size_t total; }`), where `total = measureJson(doc)` is
   cached once. The move transfers the ArduinoJson pool pointer (no copy); the
   caller's stack document is left empty and the ~40 call sites are **unchanged**
   (`restSendJson(JsonDocument&)` keeps its signature).
2. Create the response with
   `request->beginChunkedResponse("application/json", filler)`. The filler lambda
   **captures the shared_ptr**, so the document lives exactly as long as the
   streaming response and is freed when the response (and lambda) is destroyed.
3. **Re-serialize one TCP-window slice per pull.** The async server calls the
   filler with `(buffer, maxLen, index)` where `index` is the cumulative bytes
   already sent. The filler runs `serializeJson(doc, JsonChunkWindow(index,
   buffer, maxLen))`; `JsonChunkWindow` is a `Print` sink that copies only the
   bytes in `[index, index+maxLen)` and discards the rest. It returns:
   - `0` when `index >= total` (whole document sent -> response complete),
   - `RESPONSE_TRY_AGAIN` when `maxLen == 0` (no room this tick),
   - otherwise the number of bytes placed in the window (always > 0 because
     `index < total`).

**Memory:** O(1) beyond the document pool. The only per-response heap is the
ArduinoJson pool (~2-4 KB of small allocations, not one ~8.6 KB contiguous block)
plus the server's own chunk buffer (~TCP window). Under 8 concurrent responses the
peak is 8 small pools instead of 8 large contiguous buffers, well within the
fragmented heap, so the alloc-fail-and-log-storm cannot occur.

**CPU:** O(n x chunks) -- the document is re-serialized once per chunk (~6 passes
for an 8.6 KB response in ~1.4 KB windows). At ESP32-S3 serialize throughput this
is sub-millisecond per response and far under the 30 s watchdog, even under
concurrency. This trades a little CPU for bounded memory; it is the correct trade
on a RAM-constrained, fragmenting heap.

## Alternatives Considered

- **Pre-size the AsyncResponseStream buffer to `measureJson(doc)`.** Kept as a
  separate common-case improvement (`alpha.212`) but does **not** fix the flood:
  one large contiguous alloc fails sooner under fragmentation than incremental
  growth, returning to the per-byte "Failed to allocate" storm. Rejected as the
  fix.
- **`AsyncJsonResponse` (EMS-ESP32 pattern).** Still `malloc`s a flat
  `measureJson`-sized buffer -> same contiguous-allocation failure mode under
  fragmentation. Rejected.
- **Move `async_tcp` to core 0 (ADR-144).** Refuted by the panic: the trip task is
  `async_tcp` blocked in its own handler, not `loopTask` starved by it. Rejected.
- **Dedicated serializer task + bounded ring buffer.** True streaming with a
  FreeRTOS task that serializes into a small ring the filler drains; the serializer
  blocks when the ring is full. Correct and O(1) memory with a single serialize
  pass, but adds a task (and its stack) per in-flight response and is materially
  more complex than the re-serialize-per-chunk approach. Rejected on KISS grounds;
  the re-serialize CPU cost is negligible here.
- **`CONFIG_ASYNC_TCP_USE_WDT=0` (+ core 0).** Unsubscribe `async_tcp` from the
  watchdog so the flood degrades instead of rebooting. A mitigation, not a fix: it
  leaves the server choked under load and masks any genuine future hang. Rejected
  as the primary; viable fallback if chunked streaming proves insufficient.
- **Accept as near-DoS, no code change.** Realistic load is fine, but this likely
  is the TASK-879 field bug, so leaving it unfixed has real cost. Rejected.

## Consequences

**Benefits**
- No whole-response contiguous buffer -> the under-load alloc-fail + per-byte log
  storm that tripped the watchdog cannot happen. Verified on hardware: the chunked
  build produced 0 watchdog panics under the flood where the buffered build
  produced all-watchdog panics. This resolves the TASK-883 watchdog mechanism, and
  by keeping the heap healthy under concurrent load it is the likely remedy for the
  same-class TASK-879 field "web dead" report (to be confirmed in the field).
- O(1) memory per response beyond the (smaller, fragmentation-tolerant) document
  pool. Measured: under realistic 4-worker load the max-block floor and
  fragmentation stay far healthier than the buffered path, no ADR-089 tier breach.
- Zero changes at the ~40 `restSendJson` call sites; the document-ownership move
  is internal.

**Trade-offs**
- The document is re-serialized once per chunk (O(n x chunks)). Negligible CPU
  here, but it means the document must be deterministic across serializations (it
  is: a built `JsonDocument` is immutable during streaming) and must not be mutated
  after `restSendJson` is called (callers already return immediately).
- Chunked transfer-encoding sends no `Content-Length` (it never did a meaningful
  one on the streamed path either). Clients that require `Content-Length` for JSON
  are not supported; browsers, Home Assistant, and `curl` all handle chunked.
- **Document lifetime is LONGER than before.** The old path serialized
  synchronously and freed the document when the handler returned; here the document
  (via the captured `shared_ptr`) lives for the entire async streaming duration. So
  under concurrency the firmware holds N document pools alive simultaneously for
  longer. This is a clear win at realistic load (small N, no large contiguous
  buffer, heap stays healthy) but it is plausibly why an abusive flood's
  out-of-memory locus MOVED (to `addHeader`) rather than disappearing: peak
  buffer-size is traded for longer-lived documents. Not a universal memory win;
  a win at the loads that matter.

**Risks and mitigations**
- *Risk*: an off-by-one in the window logic corrupts or truncates JSON.
  *Mitigation*: `scripts/json_golden.py` semantic-equality oracle over every v2
  endpoint, plus the field-validation gate, before acceptance.
- *Risk*: the re-serialize CPU is larger than expected for the biggest documents
  under heavy concurrency. *Mitigation*: the TASK-883 flood is the measurement;
  if CPU is the new limit, the dedicated-serializer-task alternative is the
  escalation.

## Related Decisions

- **ADR-141 (Adopt ArduinoJson v7)**: this ADR refines its REST-output serving
  model; ArduinoJson stays the builder/serializer.
- **ADR-132 (HTTP stack on ESPAsyncWebServer)**: the imperative-build/async-send
  bridge stands; this changes the send mechanism for JSON to chunked pull.
- **ADR-139 (ETag + AsyncTCP task config)**: its core-1 pin stands; ADR-144 (the
  core-0 amendment) was Rejected after this investigation.
- **ADR-144 (move async_tcp to core 0)**: Rejected sibling; same investigation.
- **ADR-089 (heap tier machine)**: this reduces per-response contiguous pressure,
  helping the tiers under concurrent load.

## References

- `src/OTGW-firmware/webServerCompat.h`: `JsonChunkWindow`, `RestJsonStream`,
  `restSendJson` (the chunked filler).
- `.pio/libdeps/esp32/ESPAsyncWebServer/src/WebResponses.cpp`:
  `AsyncChunkedResponse::_fillBuffer` (the pull contract, `index = _filledLength`);
  `AsyncResponseStream::write` (the old per-byte buffer-grow path);
  `ESPAsyncWebServer.h:311` `RESPONSE_TRY_AGAIN`, `:741` `beginChunkedResponse`.
- `framework-arduinoespressif32/cores/esp32/cbuf.cpp`: `cbuf::resize` (the whole-
  RingBuffer rebuild this ADR avoids).
- TASK-883 (the under-load TWDT investigation and this fix), TASK-879 (the
  plausibly same-root field "web dead"), TASK-867 AC#8 (the load test that
  surfaced it). `scripts/tests/_serialcap.py` (panic capture);
  `scripts/tests/test_load.py` (the flood harness).
