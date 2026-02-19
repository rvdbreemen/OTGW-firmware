# Remaining Memory Optimization Options (Post v1.2.0 Profile)

## Method

- Baseline build (current tree):
  - Sketch `669916`
  - Global RAM `53948`
  - IRAM `29300 / 32768`
- Symbol analysis source:
  - `.tmp/build/OTGW-firmware.ino.elf` via `xtensa-lx106-elf-nm --size-sort`
- Differential checks:
  - One change at a time, full compile, restore baseline
  - Baseline revalidated after each probe

## Current RAM Hotspots (largest BSS/DATA symbols)

| Symbol | Size (bytes) | Notes |
|---|---:|---|
| `g_ic` | 1704 | ESP8266 SDK global |
| `mqttAutoCfgScratch` | 1353 | Shared autodiscovery scratch |
| `zoneProcessorCache` | 1184 | AceTime zone cache (`CACHE_SIZE=2`) |
| `msglastupdated` | 1024 | Per-message last-update epoch table |
| `sourceConfigFilter` | 768 | Bloom filter (2048-bit x 3 sources) |
| `webSocket` | 712 | WebSocket server object |
| `cmdqueue` | 560 | OTGW command queue |
| `DallasrealDevice` | 320 | 16 Dallas slots metadata |
| `payloadBuf` | 256 | Flash-literal MQTT scratch |
| `ot_log_buffer` | 256 | OT log aggregation buffer |
| `cMsg` | 256 | General parsing/formatting buffer |

## Remaining Option Matrix (>= 512 bytes)

| ID | Option | Gain | Confidence | Critical Evaluation |
|---|---|---:|---|---|
| R1 | Reduce AceTime cache `2 -> 1` | **592 B RAM** (measured) | High | Clean and deterministic. Tradeoff is lower timezone conversion cache hit rate. Good candidate if timezone operations are infrequent. |
| R2 | Replace `msglastupdated[256]` epoch storage with compact representation | **512 B RAM** (measured with `uint16_t` probe) | High (size), Medium (behavior) | Size gain is real. Naive 16-bit epoch is unsafe. Needs robust base-epoch+delta or periodic rebasing design to avoid timestamp wrap/fidelity issues. |
| R3 | Remove persistent `mqttAutoCfgScratch` via streaming parser state machine | **1353 B RAM** (derived from symbol size) | Medium | Largest remaining deterministic static save in project-owned code. Highest complexity/risk due parser/replacement logic and retry semantics. |
| R4 | Make source dedupe filter conditional/lazy when source-separated MQTT is disabled | **768 B RAM** (feature-off mode) | Medium | Works for low-memory profile users who disable `MQTTSeparateSources`. No gain in default-enabled profile. Adds lifecycle/state complexity. |
| R5 | Make `cmdqueue` lazy-allocated (heap) or profile-gated | **560 B RAM** (derived from symbol size) | Medium-Low | Saves static RAM but shifts pressure to heap and fragmentation risk. Not preferred for default ESP8266 reliability profile. |

## Not Worth Further Tuning (Current Profile)

- `MQTT_MSG_MAX_LEN 1152 -> 1136` would save only `16 B`.
- Additional Bloom filter shrink (`2048 -> 1024`) would save `384 B` but materially raises false-positive probability.
- Further cuts in already-reduced `cMsg` / `ot_log_buffer` are likely to cause truncation regressions.

## Recommendation Order

1. **R1** first if you want the next low-risk memory win.
2. **R2** second, but only with a robust timestamp encoding design (not raw `uint16_t` epoch).
3. **R3** only if more than ~1 KB extra RAM is required and refactor/test budget is available.
4. **R4/R5** for opt-in low-memory profile builds, not default.

## Probe Evidence

- `CACHE_SIZE 2 -> 1`:
  - Global RAM: `53948 -> 53356` (`-592 B`)
  - Sketch unchanged
- `msglastupdated time_t[256] -> uint16_t[256]`:
  - Global RAM: `53948 -> 53436` (`-512 B`)
  - Sketch: `669916 -> 669996` (`+80 B`)

## Constraints to Keep

- Keep OT log WebSocket feature.
- Keep source-separated MQTT behavior and discovery reliability.
- Keep current heap-pressure backpressure behavior.
