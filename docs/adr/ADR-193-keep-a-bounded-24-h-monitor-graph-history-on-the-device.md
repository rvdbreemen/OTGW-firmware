---
id: "ADR-193"
title: "Keep a bounded 24 h Monitor graph history on the device"
status: "Proposed"
date: "2026-10-22"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
format: "canonical"
topics:
  - "web-ui"
  - "v2"
  - "graph"
  - "history"
  - "rest-api"
  - "static-ram"
aliases:
  - "graph history"
  - "monitor graph history"
  - "24h graph"
  - "graph history endpoint"
  - "device-side graph buffer"
components:
  - "graph history ring buffer"
  - "GET /api/v2/graph/history"
symbols:
  - "doTaskEvery1s"
  - "restSendChunked"
  - "pushGraphSlot"
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-193 Keep a bounded 24 h Monitor graph history on the device

## Status

Proposed, 2026-10-22; revised 2026-10-23 (two ring sizes). Guideline-level per ADR-080: this ADR claims no CI (continuous
integration) gate in `evaluate.py` or `tests/`. The ring sizes, slot layout and REST
contract are checked at PR review and by the TASK-1296 acceptance criteria.

## Status History

```yaml
status_history:
  - date: 2026-10-22
    status: Proposed
    changed_by: "Agent: Claude"
    reason: Initial proposal
    changed_via: adr-kit
```

## Context

On 2026-10-22 gruijter reported that the v2 web UI (Web User Interface)
Monitor graph, and the CSV (comma-separated values) export made from it, only
hold data since the page was opened (TASK-1296). The cause is structural: the
graph store `gHist` is filled in the browser from the WebSocket stream. One
record is written per 5 s slot (the `GSLOT` constant, 5000 ms), up to 24 h
(`src/OTGW-firmware/data/v2.js:1262`, `pushGraphSlot()` at
`src/OTGW-firmware/data/v2.js:1271`). Closing or reloading the tab loses
everything. The 1.x classic graph worked the same way (ADR-039), so the
device has never kept any graph history.

The UI offers 10 min, 1 h, 4 h and 24 h windows
(`src/OTGW-firmware/data/v2.html:627-628`). A history that covers less than
24 h leaves the longer windows partly empty after a page load.

Constraints on a device-side store:

- **No PSRAM (pseudo-static RAM) guarantee.** The combo image also runs on
  ESP32-S3 boards without PSRAM (ADR-169), so a store that fits every board
  must live in static internal DRAM (data RAM), allocated at link time
  (ADR-053), and must stay small. Boards with PSRAM have room for a larger one.
- **Heap headroom is tight with BLE (Bluetooth Low Energy) on.** Bench
  measurement 2026-10-22, no-PSRAM unit, firmware alpha.461: free heap
  81.4 KB with max block 38.9 KB when BLE is off, and 35.2 KB with max block
  21.5 KB when BLE is on. BLE costs about 46 KB.
- **The REST (Representational State Transfer) path runs on the async_tcp
  task** (ADR-132, ADR-137). A handler must not block or allocate a large
  response buffer. Chunked responses re-run the emit closure once per
  ~1460 B window and must produce byte-identical output on every pass
  (`src/OTGW-firmware/jsonChunked.h:35`).
- **No ArduinoJson** (ADR-146). JSON is written with JsonEmit.

## Decision

The firmware keeps a ring of 14-byte slots and serves it through an HTTP
(Hypertext Transfer Protocol) GET request on `/api/v2/graph/history`. The ring
has two sizes, chosen at boot (maintainer decision, 2026-10-23):

| Board | Slots | Slot interval | Served | Storage | Size |
|---|---|---|---|---|---|
| without PSRAM | 126 | 30 s | newest 120 = 1 h | static array in internal DRAM | 1764 B |
| with PSRAM | 726 | 120 s | newest 720 = 24 h | one PSRAM block, allocated once | 10,164 B |

One binary serves both. `graphHistoryInit()` asks the platform shim
`platformPsramAllocOnce()` for the large block. If PSRAM is missing or the
allocation fails, the 1764-byte static array is used. On a PSRAM board that
array stays unused (1764 B of `.bss`, the price of one binary). The slot
layout, sampler, JSON shape and v2 merge are the same for both sizes. The JSON
carries `interval_s`, so the browser needs no board knowledge.

**Storage.** Each slot is 14 bytes:

- six temperatures (boiler flow, return, control setpoint, thermostat
  setpoint, room, outside) as `int16_t` in tenths of a degree, with
  `INT16_MIN` meaning "no value";
- modulation as `uint8_t` percent, with 255 meaning "no value";
- one flags byte for flame, DHW (domestic hot water) mode and CH (central
  heating) mode.

The response emits at most `cap - 6` slots. The 6 spare slots are a writer
margin (3 minutes on the 30 s ring, 12 minutes on the 120 s ring): while a
response streams, the sampler may push slots without overwriting any slot
that the response still has to emit. The margin counts pushed slots, and one
late tick can push many gap slots at once, so a stream is only at risk when
the 1 Hz tick stalls for longer than the margin while it runs.

**ADR-053 exception.** The PSRAM block is a deliberate, documented exception
to ADR-053. That rule exists to keep internal DRAM from fragmenting. A single
block that is allocated once at boot from PSRAM and never freed does not touch
internal DRAM, so the reason for the rule does not apply. The exception is
limited to this block; the allocation goes through the Platform library shim
and never falls back to internal DRAM.

**Sampling.** The sampler runs from `doTaskEvery1s()` on the loop task. That is
the task that runs `processOT`, so it reads `OTcurrentSystemState` without a
lock. Analog values keep the last value seen in the slot. The three flags are
OR-accumulated every second over the slot, so a burner pulse of a few seconds
still shows. A platform mutex (`platformMutexCreate()`) guards each slot write
against the async_tcp reader. When the sampler misses one or more slot
boundaries (for example during a long blocking flash write), it writes empty
"no value" slots for the gap, so each slot keeps its true position on the
timeline.

**Serving.** The handler uses `restSendChunked()` with a per-response
snapshot of head, count and age, frozen when the request arrives. The body is
integer columnar JSON: one array per series, no floats and no per-slot keys.
Every chunk pass therefore produces the same bytes, and each pass is cheap on
async_tcp. The contract is documented in `docs/api/openapi.yaml`.

**Browser.** v2 loads the history when Monitor > Graph opens (again after 10
minutes) and merges it by slot time into its own live 5 s records; the live
records win where both cover the same time. A graph line breaks where more
than 150 s of data are missing. The CSV export reads the merged store, so it
covers the history without extra code. The request follows the two-in-flight
queue of ADR-165 and ADR-184. With the 30 s ring the 4 h and 24 h windows hold
at most 1 h of device history.

**Out of scope.** Dallas (1-Wire) sensor temperatures are not in the device
history; they stay live-only. The change is not ported to `otgw-1.x.x`.

## Decision Contract

### Must

- Use the static 126-slot array on boards without PSRAM, and keep the
  PSRAM block to one allocation at boot that is never freed.
- Allocate PSRAM only through `platformPsramAllocOnce()`; never fall back to
  internal DRAM for the large ring.
- Emit at most `cap - 6` slots per response.
- Write slots only from the loop task, under the platform mutex.
- Freeze head, count and age per response so every `restSendChunked()` pass
  is byte-identical.
- Write "no value" slots for missed slot boundaries.
- Document `GET /api/v2/graph/history` in `docs/api/openapi.yaml`.

### Must Not

- Allocate the ring from the internal-DRAM heap, or grow or free it at run
  time.
- Read live OpenTherm state inside the emit closure.
- Use ArduinoJson or floats in the response body.

### Exceptions

- The single never-freed PSRAM block described above (ADR-053).

### Verification

- Build map or `size` output shows the small ring in `.bss` at 1764 B
  (TASK-1296).
- Host test `tests/test_graph_history.cpp` runs both ring sizes: wrap, gap
  fill, JSON shape and chunk-window reassembly with the margin pinned.
- Bench: on a no-PSRAM unit, free heap and max block after a reboot at
  5 min uptime, BLE off and on, against the build without the history.
  Measured 2026-10-23 (alpha.461 working tree): combo static RAM +1792 B;
  BLE off free 82116 -> 80232 B, max block 38900 -> 36852 B; BLE on free
  35572 -> 33680 B, max block 23540 -> 23540 B (REST in-flight cap stays 2).
  After 1 h the response held 120 slots, 3937 B, served in 0.067 s.
  On a PSRAM unit: 24 h of data and the PSRAM free size (not yet measured;
  no PSRAM board was on the bench).
- Browser: after a reload, the 1 h window (and on PSRAM boards the 4 h and
  24 h windows) show data from before the reload, and the CSV contains it.
  Measured 2026-10-23 on the no-PSRAM unit: the 1 h window and CSV held
  123 rows reaching back 59.9 min, against 3 rows (0.3 min) on the old build.

## Alternatives Considered

- **One static ring of 726 slots of 120 s in internal DRAM (10,164 B) on every
  board. Built and measured, rejected.** Bench 2026-10-22, no-PSRAM unit:
  free heap fell by 10.3 KB, and the largest free block fell from 38,900 B to
  31,732 B with BLE off and from 23,540 B to 15,348 B with BLE on. Below a
  16,000 B largest block the REST in-flight cap tightens from 2 to 1
  (`restEffectiveInflightCap()`, ADR-165), so with BLE on the device lost
  half its REST parallelism for a feature most users open rarely.
- **1 h at 30 s on every board (1764 B), no PSRAM tier.** Cheap and simple, but
  boards with PSRAM would never offer the 4 h and 24 h windows. Rejected as a
  waste of the memory those boards have.
- **1 h at 10 s (about 5 KB).** More than the chosen small ring for no
  benefit the 5 s live records do not already give. Rejected.
- **24 h at 60 s (about 20 KB).** Finer history, but too costly in internal
  DRAM. Rejected.
- **Two tiers, fine recent plus coarse old.** Two writers, two snapshot paths
  and a merge in the browser for a gain the live 5 s records already give once
  the page is open. Rejected as more code than it earns.
- **Keep the history client-side only.** No firmware change, but the reported
  bug stays. Rejected.

## Consequences

**Positive:**

- The 1 h window and the CSV show data immediately after a page load or a
  reconnect on every board; boards with PSRAM show up to 24 h.
- Internal DRAM cost is 1764 B static on every board, with no heap allocation
  and no fragmentation. PSRAM boards pay 10,164 B of PSRAM once.
- Integer columnar JSON keeps the response small (about 4 KB for 1 h, about
  25 KB for 24 h) and the chunk passes cheap on async_tcp.

**Negative:**

- **Boards without PSRAM keep only 1 h.** The 4 h and 24 h windows fill from
  page load, as before. Mitigation: boards with PSRAM keep 24 h.
- **History resolution is 30 s or 2 minutes**, against 5 s for live data.
  Mitigation: live records override history for the time the page is open.
- **1764 B of `.bss` stays unused on PSRAM boards** (one binary for all).
- **History is lost on reboot**, because it lives in RAM only.
  Mitigation: none in this ADR; persisting to LittleFS would add flash wear.
- **Flags can miss pulses shorter than about 1 s**, because they are sampled
  once per second.
  Mitigation: the OR-accumulation keeps every pulse that spans a sample.
- **A new REST contract to keep stable.** Mitigation: documented in
  `docs/api/openapi.yaml`.
- **Dallas sensors have no history.** They stay live-only.

## Open Questions

- None. The maintainer chose 30 s / 1 h for boards without PSRAM and
  120 s / 24 h for boards with PSRAM on 2026-10-23.

## Related Decisions

- **ADR-039 (Real-Time OTGraph Charting Architecture)**: the 1.x graph was
  client-side only; this ADR adds a device-side history for 2.0.0.
- **ADR-053 (Large Feature Buffer Static Allocation)**: the small ring is a
  static buffer under this rule; the PSRAM block is a documented exception.
- **ADR-080 (Binding ADR rules must have a CI gate)**: this ADR is
  guideline-level and claims no gate.
- **ADR-132 (HTTP stack on ESPAsyncWebServer)** and **ADR-137 (Defer
  outbound HTTP from async handlers)**: the handler runs on async_tcp and
  must stay non-blocking.
- **ADR-146 (JsonEmit, no ArduinoJson)**: the response is written with
  JsonEmit.
- **ADR-165 (N*=2 request parallelism)** and **ADR-184 (two-in-flight API
  queue)**: the history request goes through the same queue.
- **ADR-169 (PSRAM-aware BLE default)**: the reason a store for every board
  cannot rely on PSRAM, and why PSRAM boards get the larger ring.

## References

- TASK-1296 (Monitor graph keeps only data since page load).
- `src/OTGW-firmware/data/v2.js:1262` (`GSLOT`, `gHist`),
  `src/OTGW-firmware/data/v2.js:1271` (`pushGraphSlot()`).
- `src/OTGW-firmware/data/v2.html:627-628` (window chips).
- `src/OTGW-firmware/jsonChunked.h:35` (determinism contract),
  `src/OTGW-firmware/jsonChunked.h:115` (`restSendChunked()`).
- `src/OTGW-firmware/OTGW-firmware.ino:757` (`doTaskEvery1s()`).
- `src/libraries/Platform/src/platform_esp32.h` (`platformMutexCreate()`, `platformPsramAllocOnce()`).
- ESP-IDF memory types (internal DRAM versus PSRAM on ESP32-S3):
  https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/memory-types.html
- Bench measurement 2026-10-22, no-PSRAM unit, alpha.461 (free heap and max
  block, BLE off and on).

The decision has no machine-checkable rule beyond review, so it has no
Enforcement block: it is guideline-level per ADR-080.
