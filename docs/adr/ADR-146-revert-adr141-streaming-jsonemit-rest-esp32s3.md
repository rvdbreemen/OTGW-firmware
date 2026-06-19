---
id: ADR-146
title: "Revert ADR-141: remove ArduinoJson, return to a hand-rolled streaming JSON writer (JsonEmit) on the ESP32-S3 REST path"
status: Accepted
date: 2026-06-19
tags: [json, arduinojson, rest, esp32s3, memory, streaming, jsonemit, heap, evaluate]
supersedes: [ADR-141, ADR-145]
superseded_by: []
related: [ADR-042, ADR-080, ADR-088, ADR-089, ADR-128, ADR-139, ADR-140, ADR-141, ADR-145]
deciders: [Robert van den Breemen]
---

# ADR-146: Revert ADR-141 -- remove ArduinoJson, return to a hand-rolled streaming JSON writer (JsonEmit) on the ESP32-S3 REST path

## Status

Accepted, 2026-06-19 (Proposed 2026-06-19; accepted by the maintainer 2026-06-19 after
field validation on OTGW32 across alpha.216-220).

**Binding-level** (per ADR-080): the enforcing gate is `evaluate.py::check_no_arduinojson`
(restored/renamed under TASK-886). It FAILs on any ArduinoJson symbol
(`#include <ArduinoJson.h>`, `JsonDocument`, `serializeJson`, `deserializeJson`,
`JsonObject`, `JsonArray`, `JsonVariant`) outside `src/libraries`. This returns the
library ban to the spirit of ADR-042 and makes ADR-146 pattern-level from acceptance.

**Supersedes ADR-141** (Adopt ArduinoJson v7 for JSON on the ESP32-S3 2.0.0 line) and
**supersedes/retires ADR-145** (Serve REST JSON via a chunked, pull-based response that
re-serializes per TCP window). ADR-145 is Proposed and moot: it exists only to stream a
`JsonDocument` across TCP windows; without a `JsonDocument` there is nothing to stream.
ADR-141 and ADR-145 remain immutable; their `superseded_by` fields are updated on
acceptance of this ADR.

**Does NOT supersede ADR-042.** ADR-042 (Streaming JSON I/O -- No ArduinoJson) is
already superseded by ADR-141. ADR-146 re-adopts ADR-042's stance for the ESP32-S3
2.0.0 line rather than inheriting its original ESP8266 rationale, so no further
supersession action is needed on ADR-042.

## Status History

```yaml
status_history:
  - date: 2026-06-19
    status: Proposed
    changed_by: Agent
    reason: A/B experiment (TASK-885) produced hard numbers that satisfy the full-revert criteria from PLAN-streaming-json-jsonEmit.md 5.8 (JsonEmit 0 reboots across 8/12/16 workers; ArduinoJson craters to <12 KB maxblock floor and aborts on bad_alloc; settings output byte-identical; binary ~8 KB smaller; json_golden 27 PASS, 4 environmental). The maintainer chose full revert on those numbers (2026-06-18). Proposing ADR superseding ADR-141/ADR-145 and restoring the evaluate.py gate under TASK-886.
    changed_via: adr-kit
  - date: 2026-06-19
    status: Accepted
    changed_by: maintainer (Robert van den Breemen)
    reason: Accepted after field validation on OTGW32 across alpha.216-220 (JsonEmit streaming + heap-tier backpressure gate; evaluate.py check_no_arduinojson restored; OpenAPI 18/18 live; json_golden semantic-equal). The residual flood-TWDT is mitigated by the gate, not eliminated; the real fix (true chunked JsonEmit streaming, no ArduinoJson) is tracked in TASK-883.
    changed_via: manual
```

## Context

### What ADR-141 adopted and what actually moved

ADR-141 (Accepted 2026-06-15) adopted ArduinoJson v7 for the 2.0.0 ESP32-S3 line,
intending coverage of MQTT HA-discovery AND the REST API. In practice only the REST
path migrated to ArduinoJson. The MQTT HA-discovery path (`MQTTHaDiscovery.cpp`) still
uses the hand-rolled `MqttJsonWriter` (writeJsonComma/writeJsonOpen) today, and was
never touched by the ArduinoJson migration.

Verified by grep: the entire ArduinoJson footprint is the REST layer (restAPI, FSexplorer,
OTDirect, SATble, SATcontrol, SATweather, jsonStuff, webServerCompat plumbing) plus
two `#include` sites (`OTGW-firmware.h:17`, `webServerCompat.h:43`). MQTT HA-discovery
never adopted it. Consequently, reverting the REST path removes the LAST ArduinoJson
use and the library can be dropped from the build entirely.

### F1/F5 non-regression (critical clarification)

ADR-141's headline benefit was "removes F1/F5 by construction." F1 (MEASURE/WRITE
desync that drops the device block/first entity in HA discovery) and F5 (unescaped
hostname/manufacturer/model in the device block) are bugs in the MQTT HA-discovery
MqttJsonWriter two-pass pattern. BOTH were fixed by ADR-140 (single-device HA topology)
in the manual `MqttJsonWriter` itself, INDEPENDENTLY of ArduinoJson. Because the REST
path never contained F1 or F5, and because the MQTT path never moved to ArduinoJson,
reverting the REST path to JsonEmit does NOT resurrect F1 or F5. The MQTT path stays
on the fixed MqttJsonWriter; ArduinoJson is simply absent from the build.

### The heap fragmentation problem on the REST path

REST handlers build a `JsonDocument` and pass it to `restSendJson(JsonDocument&)` in
`webServerCompat.h`. The ArduinoJson `JsonDocument` heap pool fragments under concurrent
load: the pool allocates many small nodes elastically, and under an 8-16 worker flood
the contiguous max-block craters below 12 KB. `serializeJson` writing into a growing
`AsyncResponseStream` cbuf then triggers an O(n^2) "Failed to allocate" log storm (one
log per byte on a per-byte write path in `WebResponses.cpp:945-947`) that starves
`async_tcp` past the 30-second IDF Task-Watchdog, rebooting the device (TASK-883).

ADR-145 (chunked pull, Proposed 2026-06-18) addressed the watchdog by re-serializing
the `JsonDocument` one TCP-window slice at a time, removing the whole-response contiguous
buffer. A backpressure gate (`REST_MAX_INFLIGHT=4`) and `bad_alloc` backstop (alpha.214)
then kept the device survivable under an abusive flood. But the heap weakness from the
`JsonDocument` pool was gated, not eliminated.

### The A/B experiment (TASK-885)

A controlled A/B on hardware compared ArduinoJson (B-build, alpha.214 HEAD with ADR-145
chunked path) against a streaming JsonEmit rewrite (A-build, same partition table, same
methodCompat, same everything except the JSON layer). The JsonEmit writer (`jsonEmit.h`,
added under TASK-885) emits live firmware state directly into the `AsyncResponseStream`
sink field-by-field, with no `JsonDocument` and no intermediate pool.

Hard numbers (TASK-885 results, decision spine):

- **Heap under 8-16 worker flood, gate OFF:** JsonEmit maxblock floor ~31 KB, 0 ADR-089
  tier entries, no fragmentation collapse. ArduinoJson maxblock floor craters below 12 KB,
  aborts on `bad_alloc`, triggers the watchdog reboot path.
- **Correctness:** settings output byte-identical between A-build and B-build.
  `json_golden` oracle 27 PASS (4 environmental fails, approach-independent).
- **Binary size:** A-build ~8 KB smaller (library removed).
- **Verdict (maintainer, 2026-06-18):** full revert.

### The inbound JSON scanner gap

ADR-141 intended to replace `extractJsonField` in `jsonStuff.ino` (a flat `strstr`-based
scanner used by the single inbound REST site) with `deserializeJson`. That migration also
never shipped. The pre-ArduinoJson scanner had a known bug: a key-looking substring inside
a value could produce a false match. The revert replaces it with a bounded, non-throwing,
flat top-level scanner that skips values wholesale, host-validated by
`scripts/tests/test_extract_json_field.py` (32 cases: key-in-value, escapes, \u UTF-8,
strlcpy truncation, adversarial no-throw).

## Decision

**Remove ArduinoJson entirely from the firmware** and convert all REST JSON output to the
`JsonEmit` streaming writer (`src/OTGW-firmware/jsonEmit.h`). The ADR-145 chunked
plumbing (`restSendJson(JsonDocument&)`, `RestJsonStream`, `JsonChunkWindow`,
`measureJson` in `webServerCompat.h`) is deleted because it exists only to stream a
`JsonDocument`; with no document, it is moot.

### 1. Dependency removal

Drop `bblanchon/ArduinoJson` from `lib_deps` in `platformio.ini` (all ESP32 envs).
Remove both `#include <ArduinoJson.h>` sites (`OTGW-firmware.h:17`,
`webServerCompat.h:43`).

### 2. REST output: JsonEmit

The canonical call shape for every REST handler is:

```cpp
AsyncResponseStream* s = restBeginStream("application/json");
if (s) {
    JsonEmit je(*s);
    je.beginObject();
    // ... field calls ...
    je.endObject();
}
restFinalize();
```

`JsonEmit` is header-only, non-throwing, zero heap allocation, and type-correct: real
JSON booleans (not string `"true"`), native-decimal floats with NaN/Inf mapping to
`null`, RFC-8259 string escaping with an unsigned-char fix for UTF-8 bytes 0x80-0xFF.
Single-pass direct emit means each volatile field (uptime, freeheap, millis) is read
exactly once per response; there is no chunk-pass re-read that could shift decimal width
and desync a byte window.

### 3. Inbound JSON: bounded flat scanner

Replace the `strstr`-based `extractJsonField` in `jsonStuff.ino` with a bounded,
non-throwing, flat top-level scanner that skips values wholesale so a key-looking
substring inside a value string is never mis-matched. Algorithm host-validated by
`scripts/tests/test_extract_json_field.py` (32 test cases including adversarial inputs).

### 4. ADR-145 plumbing deletion

Delete from `webServerCompat.h`: `restSendJson(JsonDocument&)`, `RestJsonStream`,
`JsonChunkWindow`, and any `measureJson` usage. These ~40 call sites are replaced by the
`restBeginStream` / `JsonEmit` / `restFinalize` pattern.

### 5. CI gate (ADR-080)

`evaluate.py::check_no_arduinojson` is restored/renamed under TASK-886 to FAIL on any
ArduinoJson symbol (`#include <ArduinoJson.h>`, `JsonDocument`, `serializeJson`,
`deserializeJson`, `JsonObject`, `JsonArray`, `JsonVariant`) outside `src/libraries`.
This is the binding enforcement gate for ADR-146, per ADR-080.

**Scope.** This ADR covers the REST JSON path only. The MQTT HA-discovery path
(`MQTTHaDiscovery.cpp`, `MqttJsonWriter`) is unaffected; it never adopted ArduinoJson
and is not changed here.

## Alternatives Considered

### Alternative A: Keep ArduinoJson + ADR-145 chunked pull + `REST_MAX_INFLIGHT=4` gate (status quo)

The alpha.214 state: ArduinoJson builds the `JsonDocument`, ADR-145 streams it in
TCP-window slices, the gate caps concurrent heavy responses at 4, and the `bad_alloc`
backstop catches allocation failures. Rejected on the A/B numbers: even with the chunked
path, the `JsonDocument` pool fragments the heap to a <12 KB maxblock floor under an
unthrottled flood, and the `bad_alloc` backstop catches failures the architecture should
not be producing. The gate mitigates the symptom; it does not remove the OOM failure
class. JsonEmit's 0-ADR-089-tier / ~31 KB maxblock floor with gate OFF is the more
robust property.

### Alternative B: Hybrid -- JsonEmit for the heavy endpoints, keep ArduinoJson for light

Convert the 8 heavy REST endpoints (>2 KB each: settings, debug, sat/status, device/info,
boiler-support, ot-support, otmonitor, sat/weather) to JsonEmit and leave the ~20 light
endpoints (<2 KB) on ArduinoJson. The A/B result that drives the revert (bad_alloc under
8-16 workers) is attributable to the heavy endpoints, which supports a hybrid in
principle. Rejected for two reasons: (a) once JsonEmit covers the full type surface
required by the heavy set (bool, int32/uint32, float-NaN, string-escaped, raw-numeric,
nested object/array, dynamic runtime keys), the light set introduces no new type or
structural surface that JsonEmit does not already handle, so the hybrid retains the
entire ArduinoJson dependency (flash cost, pool fragmentation risk) for zero measurable
benefit; (b) dual-path complexity on a single target (ESP32-S3 only) violates KISS
without a concrete remaining benefit.

### Alternative C: Keep ArduinoJson, raise the evaluate.py fragmentation floor (gate ADR-089 tighter)

Tighten the ADR-089 heap tier thresholds so the REST path is throttled earlier and
the `JsonDocument` pool never pushes maxblock below the dangerous threshold. Rejected:
this tightens one mitigation to compensate for a structural fragmentation source. The
A/B experiment shows the source itself (the doc pool) is removable; tightening a gate
to mask a removable source is the wrong trade.

### Alternative D: Do nothing

The residual `bad_alloc` in `addHeader` (documented in ADR-145) and the TWDT core-1
reboots (TASK-879) are both present regardless of which *serializer* runs (ArduinoJson's
default whole-response `serializeJson` OR JsonEmit) — only ADR-145's chunked path, removed
here, avoided them. If both are tolerable in field use, the alpha.214 baseline could be
left in place. Rejected: the TASK-885 A/B
shows a materially better heap posture is achievable with a straightforward path rewrite
and no new architectural complexity. There is no reason to carry the OOM failure class
when the tested alternative removes it.

## Consequences

**Benefits**

- ArduinoJson `JsonDocument` pool fragmentation eliminated from the REST path. Under an
  8-16 worker unthrottled flood, maxblock floor holds at ~31 KB (JsonEmit) vs craters
  below 12 KB (ArduinoJson), with 0 ADR-089 tier entries. The `bad_alloc`/OOM failure
  class on REST is removed.
- Binary approximately 8 KB smaller (ArduinoJson library removed).
- No `JsonDocument` lifetime overlap under concurrent requests (the ADR-145 trade-off
  where longer-lived documents held pools alive simultaneously is gone).
- F1 and F5 are NOT resurrected (they were fixed in the MQTT `MqttJsonWriter` by
  ADR-140, independently of ArduinoJson; the MQTT path is unchanged by this ADR).
- Single REST JSON code path; no dual ArduinoJson/JsonEmit surface to maintain.
- `evaluate.py` gate restored; ADR-146 is binding from acceptance.

**Trade-offs**

- `JsonEmit` emits single-pass into the `AsyncResponseStream` cbuf, which IS a
  whole-response buffer. This is NOT the zero-whole-buffer property that ADR-145's
  chunked path provided. ADR-145 could re-serialize an immutable `JsonDocument` snapshot
  per TCP window because the document was stable between passes. A no-document live-state
  producer cannot: re-reading volatile fields (uptime, freeheap, millis) on a second pass
  can shift decimal widths and desync the byte window, producing corrupt JSON. Single-pass
  is the correct choice for a live-state writer; the heap win is structural (no doc-pool
  fragmentation, medium-chunk writes) rather than architectural (no whole-response buffer).
- The inbound JSON scanner (`extractJsonField`) is hand-rolled rather than backed by a
  library. Correctness is established by the 32-case host test suite but rests on the
  project's own implementation rather than a third-party corpus.
- Bounds check on the whole-response buffer: the largest real REST response is
  `/api/v2/settings` at ~7.3 KB. With `REST_MAX_INFLIGHT=4`, peak simultaneous cbuf
  allocation is approximately 4 x 7.3 KB = ~29 KB against a ~90 KB free-heap floor.
  Comfortable, but the cbuf is still present per response.

**Risks and mitigations**

- *Risk*: hand-rolled string escaping or type formatting introduces a correctness bug
  that ArduinoJson would have caught. *Mitigation*: `scripts/tests/test_load.py` with
  `json_golden` semantic-equality oracle across all 31 endpoints (27 PASS, 4
  environmental) plus mandatory byte-level checks for float format (no integer-coercion)
  and boolean format (literal `true`/`false`, not `1`/`0` or quoted strings) before
  acceptance. `scripts/tests/test_extract_json_field.py` 32-case coverage for the
  inbound scanner.
- *Risk*: the revert does not fix the residual TWDT reboots under extreme unthrottled
  flood. *Mitigation*: documented honestly, and NOT mis-attributed. These reboots are
  core-1 (`async_tcp` pinned on core-1, ADR-139) starvation, but they are NOT independent
  of the JSON approach in the way an earlier draft claimed. The hardware A/B in the
  45e26b8d / ADR-145 trail establishes they are *serializer*-independent but *whole-
  response-buffering*-DEPENDENT: ArduinoJson's default `serializeJson(doc, stream)` and
  JsonEmit both buffer the whole response in one contiguous `AsyncResponseStream` cbuf, and
  both starve `async_tcp` once an 8-worker flood fragments the heap below the response size
  and `cbuf::resizeAdd` fails per write through the slow `esp_diagnostics` log hook. Only
  ADR-145's true chunked/pull-based streaming avoided the whole-response buffer and so was
  the only verified fix. This revert removes that path (it depended on ArduinoJson's
  immutable-document-snapshot property) and therefore reintroduces the flood reboot. Two
  mitigations carry the gap: (1) the `REST_MAX_INFLIGHT` backpressure gate is now
  HEAP-TIER-AWARE (alpha.216) — it tightens the concurrency ceiling toward 1 as `maxblock`
  shrinks, so at most one large response buffers at a time under pressure (cheap 503s
  instead of the resize storm); (2) the real architectural fix, true chunked/pull-based
  JSON streaming on top of JsonEmit (no ArduinoJson), is scoped to **TASK-883**. TASK-879
  remains the tracker for the core-1 field "web dead" report of the same class.
- *Risk*: future endpoints or response shapes exceed `JsonEmit`'s current type surface.
  *Mitigation*: `jsonEmit.h` is header-only with full coverage of bool, int32/uint32,
  float-NaN-null, string-escaped, raw-numeric passthrough, nested object/array, and
  dynamic runtime keys. Any genuinely new type surface extends the header, not the
  library dependency.

## Related Decisions

- **ADR-141 (Adopt ArduinoJson v7)**: superseded by this ADR. The REST adoption is fully
  reverted; the MQTT intent never shipped and the MQTT path remains on the manual writer.
- **ADR-145 (Chunked pull JSON response streaming)**: superseded/retired by this ADR.
  ADR-145 is Proposed, not Accepted, and its sole mechanism (stream a `JsonDocument` per
  TCP window) is moot without a `JsonDocument`. The problem it solved (O(n^2) cbuf
  rebuild storm) is eliminated upstream by removing the per-byte write path entirely.
- **ADR-042 (Streaming JSON I/O -- No ArduinoJson)**: NOT superseded. ADR-042 is already
  superseded by ADR-141; ADR-146 re-adopts its ban for the ESP32-S3 2.0.0 line. The
  original ESP8266 RAM rationale is a historical footnote; the current rationale is heap
  fragmentation under concurrent async load on the ESP32-S3.
- **ADR-128 (Drop ESP8266 from 2.0.0)**: the platform premise that made ADR-141 possible
  (300 KB+ heap, no ESP8266 RAM constraint). Unchanged; `JsonEmit` is ESP32-S3-only code,
  consistent with ADR-128.
- **ADR-089 (Heap tier machine)**: the measurement axis of the A/B experiment. JsonEmit's
  0 ADR-089 tier entries at 8-16 workers vs ArduinoJson's tier-breaches under the same
  load is the quantitative decision anchor.
- **ADR-139 (ETag + AsyncTCP task config)**: its core-1 pin stands. The residual TWDT
  reboots under extreme flood are a core-1 starvation issue tracked in TASK-879, not
  addressed by this ADR.
- **ADR-088 (MQTT burst windowing)**: the MQTT background work (core-1) that thins the
  real heap margin under load. Unchanged by this ADR; noted because it is a confound in
  the load-test environment.
- **ADR-140 (Single-device HA topology)**: the ADR that fixed F1/F5 in the MQTT
  MqttJsonWriter. Unchanged and unaffected by this ADR; the F1/F5 fix is independent of
  the ArduinoJson revert.
- **ADR-080 (Binding ADR rules must have a CI gate)**: the meta-rule. ADR-146 is
  pattern-level and cites `evaluate.py::check_no_arduinojson` as its gate.

## References

- Implementation tasks: TASK-885 (JsonEmit writer + A/B experiment, the decision spine),
  TASK-886 (this revert: endpoint rewrites, inbound scanner, library removal,
  evaluate.py gate), TASK-883/884 (watchdog + backpressure path that proved the
  ArduinoJson weakness), TASK-879 (field "web dead", same core-1 class, residual).
- New writer: `src/OTGW-firmware/jsonEmit.h` (header-only, added TASK-885).
- Inbound scanner: `src/OTGW-firmware/jsonStuff.ino` `extractJsonField` (rewrite under
  TASK-886).
- Deleted plumbing: `src/OTGW-firmware/webServerCompat.h` -- `restSendJson(JsonDocument&)`,
  `RestJsonStream`, `JsonChunkWindow`, `measureJson` usage.
- Removed dependency: `bblanchon/ArduinoJson` from `platformio.ini` lib_deps.
- Removed includes: `src/OTGW-firmware/OTGW-firmware.h:17`,
  `src/OTGW-firmware/webServerCompat.h:43`.
- Inbound scanner test suite: `scripts/tests/test_extract_json_field.py` (32 cases).
- Quality oracle: `scripts/tests/json_golden.py` (27 PASS, 4 environmental; run across
  all 31 v2 endpoints before acceptance).
- PLAN: `docs/plans/PLAN-streaming-json-jsonEmit.md` (A/B design, decision criteria).
- `evaluate.py::check_no_arduinojson` (restored/renamed, FAIL gate on ArduinoJson
  symbols outside `src/libraries`, added under TASK-886).
