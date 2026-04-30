---
id: TASK-497
title: >-
  fix(ci,partitions,otdirect): close Phase 4 highs (CI gate scope, partition
  drift, f8.8 helper)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-30 18:19'
labels:
  - ci
  - ota
  - otdirect
  - code-review
  - follow-up
dependencies:
  - TASK-487
  - TASK-488
  - TASK-494
  - TASK-495
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Origin

Four findings from Phase 4 of the comprehensive review of session
`ace21a48..722589fd`. Three CI/CD Highs from 4B and one framework
Medium from 4A bundled because they all close measurable gaps and
touch unrelated files (no merge conflicts).

## Findings being addressed

- **4B-H1**: `.github/workflows/evaluate.yml` PR-trigger filter does
  not include `feature-dev-*` branches. Every PR into the 2.0.0 active
  work branch this session ran WITHOUT the only machine-enforced
  ADR-080 gate. Fix: add `feature-dev-*` (and ideally `feature-*`) to
  the branches filter on both `evaluate.yml` and
  `opentherm-v42-spec-audit.yml`. Cleaner: drop the branches filter
  entirely.
- **4B-H2**: No CI compile step. No workflow runs `pio run` for either
  ESP8266 or ESP32. Build breakage only surfaces locally and PR
  artefacts are not published. Fix: add a minimal `pio run -e esp32`
  + `pio run -e esp8266` job to a workflow (existing one or a new
  `build.yml`) so the compile is gated at PR time.
- **4B-H3**: `MERGED_APP_SIZE = 0x2E0000` in
  `OTGW-ModUpdateServer-esp32.h:137` drifted from the active
  partition table value `0x1E0000`. Merged-binary OTA uploads will
  trip `Update.begin()` range-check. Plus the stale
  `src/OTGW-firmware/partitions.csv` describes the old layout. Fix:
  align `MERGED_APP_SIZE` with the partition-table app-slot size and
  retire the stale CSV (the active table is `partitions_otgw_esp32.csv`
  per platformio.ini).
- **4A-M1**: f8.8 float-to-uint16 cast pattern duplicated across four
  sites in `OTDirect.ino` (1851, 1956, 2418, plus heating-curve).
  TASK-495 only clamped the TT/TC entry path; BS= and heating-curve
  sites still have latent narrow-cast UB. Fix: extract one
  `static inline uint16_t floatToF88(float c)` helper with the clamp
  + cast inline; replace the four sites.

## Files modified

| File | Change |
|---|---|
| `.github/workflows/evaluate.yml` | Add `feature-dev-*` to branches filter |
| `.github/workflows/opentherm-v42-spec-audit.yml` | Same |
| `.github/workflows/build.yml` (new) | Minimal `pio run -e esp32` + `pio run -e esp8266` PR gate |
| `src/libraries/OTGW-ModUpdateServer/OTGW-ModUpdateServer-esp32.h` | Update `MERGED_APP_SIZE` to match partition slot size |
| `src/OTGW-firmware/partitions.csv` | Delete (retired) OR update to reflect active layout (`partitions_otgw_esp32.csv` is what's wired in platformio.ini) |
| `src/OTGW-firmware/OTDirect.ino` | Extract `floatToF88(float)` helper; replace the 4 sites |

(Audit actual line numbers and file paths via Glob/Grep before editing.)

## Out of scope

- Phase 4 Mediums and Lows (OTA rollback claim, otCmdQueueHighWater
  exposure, dependency vulnerability scan, SHA256SUMS, hardware-log
  template, framework idiom polish) — defer.
- Wiring `tests/test_otdirect_override.cpp` (TASK-496) into CI — done
  by 4B-H2's compile job by convention if `tests/` is included in the
  build.yml steps.

## Validation

- ESP32 build SUCCESS.
- ESP8266 build SUCCESS.
- `python tests/check_otdirect_fixture.py` PASS.
- `python evaluate.py --quick` no new violations.
- The four `OTDirect.ino` cast sites all route through `floatToF88`;
  grep for `(uint16_t)((int16_t)(.*\\* 256.0f))` shows the helper as
  the only writer.
- The CI workflow YAMLs are valid (lint via `yamllint` if available,
  otherwise via inspection).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 4B-H1: `.github/workflows/evaluate.yml` and `opentherm-v42-spec-audit.yml` PR-trigger filter includes `feature-dev-*` branches
- [ ] #2 4B-H2: A workflow runs `pio run -e esp32` and `pio run -e esp8266` on PRs into the active branches; build failure blocks merge
- [ ] #3 4B-H3: `MERGED_APP_SIZE` in `OTGW-ModUpdateServer-esp32.h` matches the active partition table app-slot size; the stale `src/OTGW-firmware/partitions.csv` is retired or aligned
- [ ] #4 4A-M1: `OTDirect.ino` exports a `static inline uint16_t floatToF88(float c)` helper with the -40..127 clamp; the four call sites use the helper
- [ ] #5 ESP32 build SUCCESS
- [ ] #6 ESP8266 build SUCCESS
- [ ] #7 python evaluate.py --quick zero new violations
- [ ] #8 python tests/check_otdirect_fixture.py PASS
- [ ] #9 Workflow YAMLs are valid (parseable and PR-trigger conditions correct)
<!-- AC:END -->
