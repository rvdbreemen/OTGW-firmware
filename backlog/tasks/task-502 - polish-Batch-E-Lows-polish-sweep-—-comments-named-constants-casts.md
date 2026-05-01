---
id: TASK-502
title: 'polish: Batch E Lows polish sweep — comments, named constants, casts'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-01 06:39'
labels:
  - follow-up
  - code-review
  - polish
dependencies:
  - TASK-501
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Polish 8 of the 46 Lows from the comprehensive review at .full-review/05-final-report.md. Selection rule: comment / log text / static_cast / const-naming changes only — zero runtime behaviour change. Refactors and rule-breaking polishes deliberately skipped (per .full-review/06-followup-plan.md Batch E). Executed by parallel implementer agent E with disjoint file ownership against Batch D's CI/ops sweep.
<!-- SECTION:DESCRIPTION:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Closed in commit e13e34b3 polish(src): Batch E Lows polish sweep (TASK-502).

8 Lows touched across 4 source files:
- SATble.ino: BLE_STALE_MS static_assert tripwire (1A-L5); NimBLE 0.625 ms tick-unit comments (1B-L1); named scan-tuning constants BLE_SCAN_INTERVAL_TICKS / BLE_SCAN_WINDOW_TICKS / BLE_SCAN_MAX_RESULTS plus static_assert(window <= interval) (4A-L3); static_cast<int8_t> on dev->getRSSI() (4A-L7); macCompact[BLE_MAC_COMPACT_SIZE] (4A-L8).
- MQTTstuff.ino: MQTTDebugTf log on every malformed-MAC return path in satBLEMacToCompact (1A-L6); "publish cycle" wording refresh post-TASK-494 continuous-scan model (3B-L1 partial); BLE_MAC_COMPACT_SIZE bound (4A-L8).
- OTDirect.ino: float-truncation foot-gun comment above OT_OVERRIDE_*_DELTA_F88 explaining why (uint16_t)(0.25f * 256.0f) is safe at compile time but not at runtime (1B-L3).
- OTGW-firmware.h: shared BLE_MAC_COMPACT_SIZE constant + BLE forward-decl block "every publish cycle" wording refresh (4A-L8 + 3B-L1 partial).

Skipped Lows by design: refactors, ADR-077/090/091/092, tests/, .github/, build.py, Makefile, evaluate.py, c4-container, c4-code-otdirect, c4-code-sat — all already touched or owned by other batches.

Verification:
- pio run -e esp32 -j 1 SUCCESS (exit 0; flash 95.9 %, RAM 31.7 %)
- pio run -e nodemcuv2 -j 1 SUCCESS (exit 0; flash 77.3 %, RAM 84.7 %)
- Pushed: e13e34b3 → origin/feature-dev-2.0.0-otgw32-esp32-sat-support

Remaining ~38 Lows are low-value or out-of-scope per Batch E rules and will not be swept further unless surfaced again in a future review.
<!-- SECTION:FINAL_SUMMARY:END -->
