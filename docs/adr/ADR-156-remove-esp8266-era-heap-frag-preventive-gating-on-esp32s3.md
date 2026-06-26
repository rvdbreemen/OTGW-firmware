# ADR-156 — Remove the ESP8266-era heap-fragmentation preventive gating on the ESP32-S3 (2.0.0) line

- **Status**: Accepted
- **Date**: 2026-06-27
- **Tags**: heap, esp32, memory, mqtt, websocket, performance
- **Supersedes**: ADR-121 (per-consumer heap gating), ADR-107 (emergency heap recovery)
- **Amends**: ADR-030 (heap memory monitoring), ADR-089 (heap tier machine contract)
- **Relates to**: TASK-934 (heap-soak instrumentation), TASK-935 (the soak), TASK-937 (this removal)

## Status

Accepted, 2026-06-27, on maintainer go-ahead. The decision rests on the TASK-935 ESP32-S3 soak evidence (below). A longer multi-hour/day confirmatory soak under heavier sustained load is recommended and is AC#1 of TASK-937; the kept defense-in-depth floors and the retained observability bound the risk of accepting on the first 30-minute window.

## Context

The 2.0.0 (ESP32-S3) line inherited a layered heap-fragmentation PREVENTIVE gating system from the ESP8266 (1.x) line, where `umm_malloc` has no compaction and the largest contiguous block can collapse to a few hundred bytes while total free still looks adequate — the failure mode ADR-030/ADR-088/ADR-089 attack. That machinery is:

- A 4-state free-heap tier machine (`getHeapHealth`, ADR-030/089) with **maxBlock-promotion** (`HEAP_FRAG_PROMOTE_MAXBLOCK`): a small contiguous block promotes the tier so consumers back off before the next alloc fails.
- **Per-consumer ladders** (ADR-121 Option B): independent `WS_HEAP_*`/`MQTT_HEAP_*` thresholds + `getHeapHealthForWebSocket()`/`getHeapHealthForMQTT()`, feeding `canSendWebSocket()` / `canPublishMQTT()` throttle gates that drop/throttle WebSocket and MQTT messages under pressure.
- The discovery **drip slow-mode** throttle (pressure-gated 2 s↔10 s cadence).
- **`emergencyHeapRecovery()`** (ADR-107): drop WS clients, restart OTGWstream, clear MQTT discovery pending, triggered at `HEAP_CRITICAL`.

The ESP32-S3 uses a different multi-heap allocator (and far more RAM). TASK-934 added direct observability (a since-boot minimum-`maxBlock` watermark + a maxBlock histogram + the loop-gap watermark) to measure whether the ESP8266 failure mode actually occurs here.

## Evidence (TASK-935 soak)

A 30-minute sustained-load soak on real ESP32-S3 hardware (instrumented `2.0.0-alpha.275`), from a healthy heap:

- `maxblock_hist <2k/<4k/<8k/<16k/>=16k = 0/0/0/0/1967` — **every** 1 Hz sample had the largest contiguous block ≥ 16 KB.
- `min_max_block = 31732` (31.7 KB) — the smallest contiguous block ever, vs the gate thresholds 8192 (emergency) / 1536 (promote).
- `drip_slow_mode = ws_drops = mqtt_drops = entered_low = entered_warning = entered_critical = 0` — the gating never fired; the heap never left HEALTHY.
- `max_loop_gap_ms = 76` (< 200 ms) — no loop stalls.

The ESP8266 fragmentation collapse does not reproduce on the ESP32-S3 allocator. The preventive gating is dead weight here: it adds branches, state, per-consumer duplication, and 5 binding `evaluate.py` gates that constrain `getHeapHealth()`'s shape, for a code path that never executes.

## Decision

Remove the COMPLEX preventive gating on the 2.0.0 line; keep cheap insurance and observability.

**Removed**: per-consumer ladders (`WS_HEAP_*`/`MQTT_HEAP_*`, `getHeapHealthForWebSocket/MQTT`, `heapTierWithThresholds`), the throttle state/intervals, the maxBlock tier-promotion (`HEAP_FRAG_PROMOTE_MAXBLOCK`), and `emergencyHeapRecovery()` + its loop gate. `canSendWebSocket()`/`canPublishMQTT()` are neutered to always-allow.

**Kept**:
- All TASK-934 observability (the watermark, histogram, `sampleHeapWatermark`/`resetHeapWatermark`, the telnet `z` reset, and the `min_max_block`/`maxblock_*`/`free_heap`/`max_block`/`frag_pct`/`max_loop_gap` MQTT stats).
- A SIMPLIFIED `getHeapHealth()` that only classifies the free-heap tier and increments the diagnostic `entered_*` counters (still published for field telemetry). It gates nothing; it is called once per loop solely to keep those counters live. ADR-089 sub-rule 1 (ordered thresholds) and sub-rule 3 (tier-entry counters) remain enforced; sub-rule 2 (maxBlock-promotion) is retired.
- The cheap last-resort `maxBlock < 8192` 503 floors (restAPI / SATcontrol) and the `MQTT_DISCOVERY_HEAP_MIN` discovery floor, as defense-in-depth — 1-line, zero-cost when healthy, and a genuine-OOM backstop the 30-minute soak cannot rule out for every future load/feature.
- The independent TASK-342/347 Status-burst quiesce + cooldown (rate-limiting, NOT heap gating).

`evaluate.py`: retire `check_heap_fragmentation_promotion` (ADR-089 sub-rule 2) and `check_per_consumer_heap_gate` (ADR-121); keep `check_heap_tier_thresholds_ordered`, `check_heap_tier_entry_counters`, and `check_json_buffer_arithmetic`.

## Consequences

- Simpler heap code; MQTT/WebSocket sends are unconditional; `getHeapHealth()` is diagnostic-only.
- A future heap regression (new feature, different load) remains VISIBLE via the retained `min_max_block`/histogram observability, and a genuine OOM is still caught by the kept floors — so removal is reversible and monitored, not blind.
- ADR-121 and ADR-107 are superseded; ADR-030/089 are amended (the tier machine survives as a diagnostic, not a gate).
- Risk accepted on a 30-minute window; the longer confirmatory soak (TASK-937 AC#1) should run before this ships to the field.
