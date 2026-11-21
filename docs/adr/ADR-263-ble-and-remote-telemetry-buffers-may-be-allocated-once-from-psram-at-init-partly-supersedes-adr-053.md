---
id: "ADR-263"
title: "BLE and remote telemetry buffers may be allocated once from PSRAM at init (partly supersedes ADR-053)"
status: "Accepted"
date: "2026-11-20"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
topics:
  - "psram"
  - "memory"
  - "static-buffers"
  - "remote-telemetry"
  - "ble"
aliases:
  - "PSRAM buffers"
  - "allocate once from PSRAM"
components:
  - "PSRAM buffer allocation rule for BLE and remote telemetry"
symbols: []
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-263 BLE and remote telemetry buffers may be allocated once from PSRAM at init (partly supersedes ADR-053)

## Status

Accepted, 2026-11-20.

## Status History

```yaml
status_history:
  - date: 2026-11-20
    status: Proposed
    changed_by: "Agent: Claude (5e, TASK-2937)"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-11-20
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: "Robert 2026-11-20: 'ja' on the acceptance packet in session 5e"
    changed_via: adr-kit lifecycle
```

## Context

ADR-053 (Accepted) says every feature-specific working buffer is a named global array: never heap, never local static, never stack. That rule was made for internal RAM (DRAM), where a heap allocation fragments a small heap and a static array shows its cost in the link map. On the ESP32-S3 line some boards have 2 MB of PSRAM (pseudo-static RAM, an external RAM chip), and two features need buffers that do not fit internal RAM at all: the remote telemetry recorder (TASK-2901/2937) and BLE (NimBLE). Robert, 2026-11-19: "Anything related to BLE and Remote Telemetry can be run from PSRAM to save DRAM."

Measured in the remote telemetry PoC on .63 (Classic + ESP32-S3, 2 MB PSRAM):

- poc.18 hung with the recorder buffers as internal statics; poc.20 starved the internal heap with mbedTLS blocks of up to 4 KB internal (TASK-2901 notes, 2026-11-19).
- With the buffers allocated once from PSRAM (record buffers, LZ window, reply buffer, the 128-slot send queue of ~525 KB, the 10 KB task stack, the slot table) the lowest free internal heap since boot went from 3,944 B (poc.22) to 15,212 B (poc.31) under the worst case (BLE on, two web UI tabs), and static internal DRAM is 136,684 B.
- PSRAM stayed constant at about 1.35 MB free during a full 24-minute run (no growth, no leak).

A global array cannot live in PSRAM on this platform without the linker attribute for external RAM, which the prebuilt Arduino libraries do not enable; the allocation has to be at run time.

## Decision

On a board with PSRAM, buffers of BLE and remote telemetry may be allocated from PSRAM once, at init, and are never freed; for BLE this includes the NimBLE host's own memory pools. On a board without PSRAM, BLE stays off (ADR-257) and the remote telemetry upload stays off (ADR-218); only the local recording may fall back to a bounded internal allocation. This partly supersedes ADR-053 for these two features only; every other feature keeps ADR-053.

## Decision Contract

### Must

- Allocate once, at init or at the first enable, through one helper (`platformPsramAllocOnce()` or `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` behind the Platform library), and keep the pointer for the life of the boot.
- Name each buffer and its size in one place (a struct or named constants) so the cost stays readable, as ADR-053 asks of globals.
- Decide PSRAM at run time (`platformHasPsram()`); without PSRAM either fall back to a bounded internal allocation that leaves the ADR-030 heap reserve free, or keep the feature off with a reason in the log and the UI.
- Let NimBLE allocate its host memory from PSRAM (`CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL`), set where the project already sets NimBLE options (the `sdkconfig.h` wrapper of ADR-185); BLE runs only on boards with PSRAM (ADR-257).

### Must Not

- Free and re-allocate these buffers during a boot (no allocation churn).
- Put buffers of other features in PSRAM under this ADR.
- Keep a large static array for these two features in internal RAM.

### Exceptions

- None.

### Verification

- `tests/test_remote_telemetry_no_big_statics.py` (no large statics in the remote telemetry sources) and `tests/test_remote_telemetry_queue.py` (task stack and slot table from PSRAM); on the bench, `/api/v2/health` heap and PSRAM numbers stay flat over a run.

## Alternatives Considered

- **Keep ADR-053 as it is: internal static arrays.** Rejected: the recorder buffers alone (two record buffers, LZ window, reply buffer, send queue) are far larger than the free internal RAM; poc.18 hung that way.
- **Ordinary heap allocation, freed when idle.** Rejected: churn of large blocks fragments the heap, the failure mode ADR-053 exists for, and a later re-allocation can fail when the heap is busy.
- **Linker placement of globals in external RAM (`EXT_RAM_BSS_ATTR`).** Rejected for now: needs `CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY`, which the prebuilt Arduino-ESP32 libraries do not set; a self-built sdkconfig is a heavier build path (ADR-218 alternatives).

## Consequences

**Positive:**

- Internal RAM stays free for the rest of the firmware: lowest free heap 15,212 B instead of 3,944 B on the same worst case.
- The cost stays visible: one struct or one named constant per buffer.

**Negative:**

- The memory is not in the link map, so a static size check cannot show it. Mitigation: the named sizes and the health endpoint's PSRAM numbers; the tests above.
- PSRAM is slower than internal RAM. Mitigation: these are bulk buffers (records, queue, ring), not hot per-byte state; measured upload cadence and replay timing were not affected (poc.31: 0.0 ms gap deviation).
- Two behaviours per feature (with and without PSRAM). Mitigation: the run-time check and a stated fallback per feature.
- Not verified yet: whether `CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL` takes effect through the ADR-185 wrapper, or whether the prebuilt Arduino-ESP32 libraries (which set `CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_INTERNAL`) need a self-built sdkconfig for it. Mitigation: a build that shows the NimBLE pools in PSRAM (heap-caps numbers before and after BLE start) before the change is merged; until then NimBLE stays internal.

## Open Questions

- [x] BLE today allocates through NimBLE's own allocator (internal, `CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_INTERNAL` in the prebuilt libraries). Does this ADR also cover changing that, or only BLE buffers the firmware itself allocates? — **Answered 2026-11-20 by User: Robert van den Breemen:** B (Robert 2026-11-20: 'Open vraag BLE, moet ook in PSRAM, de BLE staat uit bij board zonder PSRAM'): the NimBLE host memory also goes to PSRAM; without PSRAM BLE stays off (ADR-257).

## Related Decisions

- **ADR-053 (Large Feature Buffer Static Allocation)**: partly superseded, for BLE and remote telemetry buffers only.
- **ADR-218 (Outbound TLS client on boards with PSRAM)**: the first user; its mbedTLS blocks go to PSRAM under the same idea.
- **ADR-030 (Heap Memory Monitoring and Emergency Recovery)**: the internal-heap reserve a no-PSRAM fallback must leave free.
- **ADR-257 (On boards without PSRAM BLE cannot be switched on)**: BLE needs PSRAM; this ADR adds where its memory comes from.
- **ADR-185 (NimBLE options through a project sdkconfig.h wrapper)**: the place to set the NimBLE allocation mode.

## References

- Robert, 2026-11-19 (session 5e): "Anything related to BLE and Remote Telemetry can be run from PSRAM to save DRAM."
- TASK-2901 notes (poc.18-poc.31), report `OTGW-field-data/bench-63/2026-11-20_poc.31_remote-telemetry/report.html`.
- Branch research/cloud-recording: `src/OTGW-firmware/RemoteTelemetryDevice.h` (`RemoteTelemetryBufs`, `remoteTelemetryInitCore()`), `src/libraries/Platform/src/platform_esp32.h` (`platformTaskCreatePinnedPsramStack`).
