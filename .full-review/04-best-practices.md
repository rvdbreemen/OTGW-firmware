# Phase 4: Best Practices & Standards

Consolidated from `phase4a-best-practices.md` (framework / language)
and `phase4b-cicd.md` (CI/CD / DevOps).

## Framework & Language Findings (Phase 4A)

**Counts**: 0 Critical, 0 High, 2 Medium, 10 Low.

### Medium

- **4A-M1**: f8.8 float-to-uint16 cast pattern duplicated across four
  sites in `OTDirect.ino` (1851, 1956, 2418, plus heating-curve).
  TASK-495 only clamped the TT/TC entry path; the BS= and
  heating-curve sites still have latent narrow-cast UB. Fix: extract
  one `static inline uint16_t floatToF88(float c)` helper with the
  clamp + cast inline; replace the four sites.
- **4A-M2**: `SATBLEScanCallbacks` is a 90-line class hosting one
  method with no member state. Could be `final` and the parse-and-store
  body could be extracted to a free static — would let TASK-496's
  host-fixture test reach in without a NimBLE harness mock.

### Low

- 4A-L1..L10: SemVer pin tightening for NimBLE, enum-class consistency,
  static_cast over C-style, named BLE scan tuning constants, RAII over
  manual portENTER/EXIT_CRITICAL, named OT-frame accessors, future
  ADR-078-style dispatch table for OTDirect commands.

### Strengths
- PROGMEM discipline consistent.
- NimBLE 2.x API usage current (no deprecated callback class).
- FreeRTOS `portMUX_TYPE` is canonical IDF idiom.
- Snapshot-under-lock pattern is correct.
- All new enums explicitly typed `: uint8_t`.

## CI/CD & DevOps Findings (Phase 4B)

**Counts**: 0 Critical, 3 High, 5 Medium, 7 Low.

### High

- **4B-H1**: `evaluate.yml` does not gate PRs into `feature-dev-*`
  branches. The PR-trigger filter only matches `dev`, `1.4.*`,
  `release/**`. Every PR into the 2.0.0 active work branch this
  session executed without the ADR-080 machine-enforced gate. Fix:
  add `feature-dev-*` (and ideally `feature-*`) to the branches
  filter on both `evaluate.yml` and `opentherm-v42-spec-audit.yml`,
  or drop the branches filter entirely.
- **4B-H2**: No CI compile step. No workflow runs `pio run` for
  either target. Build breakage only surfaces locally; PR artefacts
  are not published.
- **4B-H3**: `MERGED_APP_SIZE = 0x2E0000` in
  `OTGW-ModUpdateServer-esp32.h:137` drifted from the active
  partition table value `0x1E0000`. Merged-binary OTA uploads will
  trip `Update.begin()` range-check. The stale
  `src/OTGW-firmware/partitions.csv` also describes the old layout.

### Medium

- 4B-M1: "Safe OTA rollback" claim contradicts the single-`ota_0`-slot
  reality (no `esp_ota_mark_app_valid_*` calls anywhere).
- 4B-M2: `otCmdQueueHighWater` invisible outside telnet — CHANGELOG
  promises diagnostic visibility, REST/MQTT can't see it.
- 4B-M3..M5: no dependency-vulnerability scan on `lib_deps`, no
  SHA256SUMS on releases, no formal hardware-verification log
  template for AC #11 gates.

### Strengths
- Working `evaluate.py` gate with named ADR-080 checks.
- OT v4.2 spec audit workflow.
- Hand-curated `docs/guides/pr-checklist.md`.
- `build.py` as proper build-IaC.
- OTA browser-side `/api/v2/health` poll.
- Meta-testing via `tests/test_evaluate.py`.

## Cross-phase signal

Phase 4B-H1 + 4B-H2 together explain why the comprehensive review
itself surfaced so many ADR-drift items in Phase 3C: the CI gate
(`evaluate.py`) wasn't running on PRs into the active branch, so the
ADR-080 named checks couldn't catch drift in time. With 4B-H1 fixed,
future feature-branch work gets the gate immediately.

## Critical issues for Phase 5 context

The three 4B Highs and the 4A-M1 latent UB are concrete fixes that
should land before Phase 5's final consolidated report so the report
reflects an actually-clean baseline. Plan: bundle them into a single
TASK-497 commit + push, then write the Phase 5 final report.
