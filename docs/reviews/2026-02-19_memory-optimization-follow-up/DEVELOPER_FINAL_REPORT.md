# Developer Final Report: v1.2.0 Memory Optimization Pass

Date: 2026-02-19  
Target: ESP8266 (`d1_mini`, `xtal=160`, `eesz=4M2M`)  
Build profile: `-DNO_GLOBAL_HTTPUPDATE`

## Executive Summary

The selected memory optimization set is implemented and verified.  
Current baseline is stable with improved RAM headroom and no feature removals:

- Sketch: `669916` bytes
- Global RAM: `53948` bytes
- Free dynamic memory budget: `27972` bytes
- IRAM: `29300 / 32768` (remaining `3468`)

OT log WebSocket and MQTT source-separation features remain enabled.

## What Was Implemented

See `DECISIONS_IMPLEMENTED.md` for full details. Key decisions:

1. WebSocket memory controls (client cap 3, payload cap, central capped sender, send gating).
2. Shared MQTT autodiscovery scratch buffer with guard.
3. Source dedupe switched to 2048-bit Bloom filter per source.
4. MQTT flash-literal scratch reduced to 256 bytes.
5. AceTime cache reduced to 2 entries.
6. `CMSG_SIZE` and OT log buffer reduced to 256.
7. MQTT setting storage limits tightened (top topic, HA prefix, unique ID).
8. `MQTT_MSG_MAX_LEN` reduced to 1152 after measured worst-case payload analysis.

## Exact Gain Accounting

Mixed measured + derived accounting across the selected set:

- `8467 B` static RAM reduction.
- Measured probes were used for deltas where possible.
- Derived deltas are exact byte arithmetic from old/new static layouts.

## Validation Notes

- Repeated full compile probes were performed for:
  - WebSocket client cap (`3 <-> 5`): `408 B`
  - AceTime cache (`2 <-> 1`): `592 B`
  - `msglastupdated` compression probe (`time_t -> uint16_t`): `512 B` (remaining option)
  - MQTT flash-literal scratch (`256 <-> 1200`): `944 B`
  - MQTT setting limits (`12/16/25 <-> 40/40/40`): `68 B`
  - `MQTT_MSG_MAX_LEN` (`1152 <-> 1200`): `48 B`
- Autodiscovery payload sizing validated against `mqttha.cfg`; measured worst expanded payload is `1123 B`.

## Remaining Opportunities (>= 512 B)

See `MEMORY_OPTIMIZATION_OPTIONS.md`.

Top practical remaining candidates:

1. `CACHE_SIZE 2 -> 1` (`592 B`, low risk).
2. `msglastupdated` robust compaction design (`512 B`, medium risk).
3. Remove persistent `mqttAutoCfgScratch` via streaming parser (`1353 B`, high complexity/risk).

## Recommendation

If further RAM is needed without major refactor risk:

1. Do AceTime cache `2 -> 1`.
2. Implement robust timestamp compaction (not raw 16-bit epoch).

If >1 KB further reduction is required, schedule a dedicated parser refactor cycle for MQTT autodiscovery scratch removal with targeted tests.
