# Comprehensive Code Review Report

## Review Target

8 commits in `ace21a48..fa5ef3c5` on
`feature-dev-2.0.0-otgw32-esp32-sat-support`, expanded by the in-flight
fix-batches to `ace21a48..39bbd2fe` by the time this report was written:

| Original commit | Subject |
|---|---|
| `ace21a48` | TASK-482 evaluate.py PROGMEM gate fix |
| `759e47f8` | TASK-466 OTDirect TT/TC remote-override |
| `59b1478d` | TASK-487 + TASK-488 NimBLE port + HA discovery |
| `21aff0dd` | backlog triage cleanup |
| `d8028b0a` | TASK-466 Done flip |
| `67ad53cf` | TASK-489/490/491 first review-followups |
| `3e14c2cf` | Done flips |
| `fa5ef3c5` | TASK-492 dead-code consolidation |

The review surfaced findings that landed in five additional fix-batches
during the review itself (TASK-493, TASK-494, TASK-495, TASK-496,
TASK-497), bringing HEAD to `39bbd2fe` and closing every High found at
each phase checkpoint.

## Executive Summary

Eight commits of substantive feature work (BLE-on-ESP32, OTDirect TT/TC
PIC parity, evaluate.py CI gate fix) were reviewed across five phases
and twelve agent runs. The session shipped roughly 1300 lines of new
firmware C++ plus the architectural records, tests, and CI gates that
make it durable. All eleven High findings across Phase 1-4 were closed
during the review itself, validated by repeated incremental builds on
both ESP8266 and ESP32-S3 targets. The branch is in a clean state for
merging into `dev` once owner-driven hardware verification of TASK-487
and TASK-488 lands.

The review had its own findings about itself: ADR-080 binding-checks
were not running on the active feature branch (4B-H1) and there was no
PR-time compile gate (4B-H2) — both fixed in TASK-497. Three Accepted
ADRs had drifted relative to shipped code (3C-1/2/3) — all amended in
TASK-495 to match.

## Findings by Priority

### Critical Issues (P0)

None observed.

### High Priority (P1) — all closed during review

| Phase | ID | Issue | Closed in |
|---|---|---|---|
| 1A | 1A-H1 | `bDiscoveryPublished` flipped to true on transient failure | TASK-493 (`55fd0cf6`) |
| 1B | 1B-H1 | `platformio.ini` auto-format lost build rationale + bumped esptoolpy pin | TASK-495 + TASK-493 (`ccee66ed` + `55fd0cf6`) |
| 2A | 2A-H1 | `setRemoteOverride()` float-narrowing UB | TASK-493 (`55fd0cf6`); generalised to all 11 sites in TASK-497 (`39bbd2fe`) |
| 2B | 2B-H1 | `feedWatchDog()` only on success path in BLE HA-discovery | TASK-493 (`55fd0cf6`) |
| Cross | 1B-M2 / 2A-M1 / 2B-M2 | `_bleSensors[]` cross-task race (one defect, three angles) | TASK-493 (`55fd0cf6`) |
| 3A | 3A-H1 | Zero executable tests for the four numerical-contract fixes | TASK-496 (`23e4b4e2`) |
| 3B | 3B-H1 | WebUI `iBleInterval` tooltip mislabelled "scan rate" | TASK-495 (`ccee66ed`) |
| 3B | 3B-H2 | Per-MAC MQTT topics undocumented | TASK-495 (`ccee66ed`) |
| 3B | 3B-H3 | CHANGELOG `[Unreleased]` missed all 8 session commits | TASK-495 (`ccee66ed`) |
| 3C | 3C-1 | ADR-090 sub-rule 4 foresight clause fired (FreeRTOS cross-task) | TASK-495 (`ccee66ed`) |
| 3C | 3C-2 | ADR-092 prose still claimed "Periodic-scan model retained" | TASK-495 (`ccee66ed`) |
| 3C | 3C-3 | ADR-068 `OT_CMD_QUEUE_SIZE = 8` drift + uncovered coalesce | TASK-495 (`ccee66ed`) |
| 4B | 4B-H1 | evaluate.yml PR-trigger missed `feature-dev-*` | TASK-497 (`39bbd2fe`) |
| 4B | 4B-H2 | No CI compile gate | TASK-497 (`39bbd2fe`) |
| 4B | 4B-H3 | `MERGED_APP_SIZE` drift + stale partitions.csv | TASK-497 (`39bbd2fe`) |

### Medium Priority (P2) — selectively addressed

Closed during review:
- 4A-M1 (f8.8 cast UB across 11 OTDirect sites — generalised to canonical `floatToF88` helper in TASK-497).

Deferred (recorded for follow-up):
- 1A-M1..M5: queue-state-machine ghost overrides, lastThermostatVal sentinel collision, TT/TC zero-vs-negative threshold, BLE state-topic format duplication.
- 2A-M2/M3: default-allow MAC filter trust, BTHome v2 unknown-object truncation.
- 2B-M1/M3: 32-publish first-scan burst potential, OT command queue depth borderline.
- 3B Mediums: ADR-092 / ADR-077 fine-tuning, C4 doc drift on otdirect / sat code-level docs.
- 4A-M2: SATBLEScanCallbacks extractability for tests.
- 4B-M1..M5: OTA rollback claim accuracy, otCmdQueueHighWater REST/MQTT exposure, dep-vuln scan, SHA256SUMS, hardware-test log template.

### Low Priority (P3) — backlogged

Cumulative ~30 Lows across all phases. Naming consistency, comment refresh, idiom polish (RAII over manual portENTER/EXIT_CRITICAL, named OT-frame accessors, future ADR-078 dispatch table for OTDirect commands), pin tightening, etc. Not blocking.

## Findings by Category

| Category | Findings | Highs | Status |
|---|---:|---:|---|
| Code Quality (1A) | 14 | 1 | High closed |
| Architecture (1B) | 9 | 1 | High closed (with cross-phase race) |
| Security (2A) | 6 | 1 | High closed |
| Performance (2B) | 10 | 1 | High closed |
| Testing (3A) | 11 | 2 | Top-H closed via test_otdirect_override.cpp |
| Documentation (3B) | 13 | 3 | All closed |
| ADR audit (3C) | 5 mandatory | 3 | All amended |
| Framework (4A) | 12 | 0 | M1 generalised in TASK-497 |
| CI/CD (4B) | 15 | 3 | All closed |

Total: ~95 findings across 5 phases. **15 unique Highs all closed in 5 fix-batch commits during the review.**

## Recommended Action Plan

### Pre-merge to `dev`

1. Owner runs hardware verification for TASK-487 / TASK-488 (BLE
   sensors discovered, multi-channel TT/TC burst does not drop).
   Status pending; ACs left open.
2. Owner runs hardware verification for TASK-494 AC #11 (queue
   high-water stays low under realistic load) and TASK-466 AC #12
   (TT auto-clear behaviour against thermostat program shift).
3. Owner installs `build-essential` in WSL or any host with g++ and
   runs `g++ -std=c++17 -Wall -Wextra tests/test_otdirect_override.cpp -o /tmp/test && /tmp/test`
   to pin TASK-496 ACs 3 and 4.

### Once merged to `dev`

Post-merge actions documented in TASK-487 / TASK-488 / TASK-494 final
summaries. The new CI workflows (`evaluate.yml` + `opentherm-v42-spec-audit.yml`
extended branches, new `build.yml`) start gating from the first PR
landing on this updated trigger set.

### Backlog (Mediums + Lows)

Triage in next quarterly sweep. None are release-blocking.

## Review Metadata

- Review started: 2026-04-30T05:55:00Z
- Review completed: 2026-04-30T18:30:00Z (final report)
- Phases completed: 1A, 1B, 2A, 2B, 3A, 3B, 3C (owner-requested ADR audit), 4A, 4B, 5
- Agents run: 11 parallel agent invocations across the five phases
  (excluding the original review's own preflight)
- Flags applied: performance_critical=true (BLE hot path, ESP32 flash
  budget at 95.8%, OTDirect on-bus state machine)

## Comprehensive review output files

```
.full-review/
├── 00-scope.md                       Review scope manifest
├── 01-quality-architecture.md        Phase 1 consolidated
│   ├── phase1a-code-quality.md
│   └── phase1b-architecture.md
├── 02-security-performance.md        Phase 2 consolidated
│   ├── phase2a-security.md
│   └── phase2b-performance.md
├── 03-testing-documentation.md       Phase 3 consolidated (test + docs + ADR)
│   ├── phase3a-testing.md
│   ├── phase3b-documentation.md
│   └── phase3c-adr-audit.md          (owner-requested addition)
├── 04-best-practices.md              Phase 4 consolidated
│   ├── phase4a-best-practices.md
│   └── phase4b-cicd.md
└── 05-final-report.md                this file
```

## Closing observation

The review was useful precisely BECAUSE it landed during active feature
work, not after the fact. Drift between ADR text and shipped code, an
unguarded CI gate, and a dead-helper duplication only become visible
when someone deliberately compares record to reality. Eleven Highs in
one session is a high count for a single review; the fact that all
eleven were code-traceable to specific lines and all eleven landed
fix-commits during the review itself suggests the comprehensive-review
workflow paid for itself within one session.

The two open boxes — owner hardware verification and host-side test
execution — are both single-environment dependencies, not architectural
debt. Once those land, this work-set is fully verified and merge-ready.
