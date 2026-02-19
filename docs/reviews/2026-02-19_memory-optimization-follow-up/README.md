# Memory Optimization Follow-up (2026-02-19)

This review captures the verified post-optimization baseline for v1.2.0, documents what was implemented, and re-evaluates remaining memory options.

## Scope

- Target: `src/OTGW-firmware` on ESP8266 (`d1_mini`, `xtal=160`, `eesz=4M2M`)
- Build command used for all reported numbers:
  - `arduino-cli compile --fqbn esp8266:esp8266:d1_mini:eesz=4M2M,xtal=160 --libraries src/libraries --build-property compiler.cpp.extra_flags="-DNO_GLOBAL_HTTPUPDATE" --build-path .tmp/build --config-file arduino/arduino-cli.yaml src/OTGW-firmware`
- Date of verification: `2026-02-19`
- Code version measured: `1.2.0-beta+27e9acb`

## Verified Current Baseline

- Sketch: `669916` bytes (`64%`)
- Global RAM: `53948` bytes (`65%`)
- Remaining global headroom: `27972` bytes
- IRAM: `29300 / 32768` (remaining `3468` bytes)
- RAM segments:
  - `DATA=1820`
  - `RODATA=14072`
  - `BSS=38056`

## Implemented Optimization Snapshot

Detailed per-decision write-up is in `DECISIONS_IMPLEMENTED.md`.

Measured and derived results (see methodology there):

- WebSocket library client cap (`5 -> 3`): `408 B` RAM saved (measured)
- AceTime zone cache (`3 -> 2`): `592 B` RAM saved (measured)
- `CMSG_SIZE 512 -> 256` + `OT_LOG_BUFFER_SIZE 512 -> 256`: `512 B` RAM saved (measured)
- MQTT flash-literal scratch (`1200 -> 256`): `944 B` RAM saved (measured)
- MQTT setting field limits (`41/41/41 -> 13/17/26 arrays`): `68 B` RAM saved (measured)
- MQTT autodiscovery buffer (`1200 -> 1152`): `48 B` RAM saved (measured)
- Source discovery dedupe (`hash table -> 2048-bit Bloom per source`): `1536 B` RAM saved (derived exact from type sizes)
- Shared MQTT autodiscovery scratch refactor: `3847 B` RAM saved (derived exact from old/new static layouts)

Approximate total static RAM reduction across the selected set: `8467 B`.

## Remaining Work

See `MEMORY_OPTIMIZATION_OPTIONS.md` for current remaining options (critical review, expected gains, and risk notes), focusing on candidates with `>= 512 B` potential impact.

## Documents

- `MEMORY_OPTIMIZATION_OPTIONS.md`
  - Remaining opportunities after the implemented v1.2.0 memory work
  - Critical evaluation and measured/derived gains per option
- `DECISIONS_IMPLEMENTED.md`
  - Selected issues and final decisions
  - Exact evidence markers (`measured` vs `derived`)
- `DEVELOPER_FINAL_REPORT.md`
  - Final handoff report for the developer
