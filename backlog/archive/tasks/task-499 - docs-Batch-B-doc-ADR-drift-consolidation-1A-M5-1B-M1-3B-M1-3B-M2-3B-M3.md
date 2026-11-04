---
id: TASK-499
title: >-
  docs: Batch B doc + ADR drift consolidation (1A-M5, 1B-M1, 3B-M1, 3B-M2,
  3B-M3)
status: Done
assignee:
  - '@claude'
created_date: '2026-05-01 05:22'
updated_date: '2026-05-01 05:30'
labels:
  - docs
  - adr
  - follow-up
dependencies:
  - TASK-498
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Five Phase 1-3 doc/ADR drift Mediums per `.full-review/06-followup-plan.md` Batch B.

- **1A-M5**: BLE state-topic format duplicated across SATble.ino caller-wiring comment + MQTTstuff.ino helper docstring. Consolidate into a single canonical reference in `docs/api/MQTT.md` (already documented per TASK-495); add cross-reference comments in code.
- **1B-M1**: ADR-077 bounded-buffer single-publish exception (TASK-490) not codified. Add a one-paragraph addendum noting that for statically-bounded payloads (e.g. <= ~768 B), single-buffer publish via the streaming primitives is permitted, gated by canPublishMQTT() + MQTT_DISCOVERY_HEAP_MIN.
- **3B-M1**: ADR-092 doesn't yet reflect TASK-497 portMUX (continuous-scan was added in TASK-495 amendment; cross-task race-fix reference is missing). Add a brief note + cross-link to ADR-090's amendment.
- **3B-M2**: `docs/c4/c4-code-otdirect.md` and `docs/c4/c4-code-sat.md` have measurable drift — stale `BLEDevice.h` reference, 7-vs-8 entry override-table count, pre-TASK-466 TC=/MsgID 1 mapping. Refresh both against current code.
- **3B-M3**: MQTTstuff.ino block-comment 75 lines above `satBLEPublishOneDiscovery` still claims "two-pass shape ADR-077 requires" — TASK-490 corrected the function-level comment but missed the block-comment. One-line correction.

Doc-only commit; no firmware build needed beyond fixture validation.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 1A-M5: BLE state-topic format reference in code comments points to docs/api/MQTT.md as the single source of truth
- [x] #2 1B-M1: ADR-077 amended with the bounded-buffer single-publish exception, gated by heap checks
- [x] #3 3B-M1: ADR-092 references ADR-090 amendment (cross-task race covered there)
- [x] #4 3B-M2: c4-code-otdirect.md and c4-code-sat.md refreshed against current code (NimBLE 2.x, 8-entry override table, TT/TC vs MsgID 1 mapping)
- [x] #5 3B-M3: MQTTstuff.ino block-comment correction — 'two-pass shape ADR-077 requires' rephrased
- [x] #6 evaluate.py --quick zero new violations
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Closed Batch B doc/ADR drift consolidation in commit 4d7bfaa0.

Closed deferred review findings:
- 1A-M5 (MQTTstuff.ino comment block claiming "two-pass shape ADR-077 requires"): replaced with accurate bounded-payload single-publish description.
- 1B-M1 (ADR-077 missing TASK-490 bounded-buffer exception): added "Amendment 2026-05-01 — Bounded-payload exception" section codifying the satBLEPublishOneDiscovery shape (payload ≤ 1KB, single stack buffer, single writeMqttChunk; still gated by canPublishMQTT/heap-tier; feedWatchDog on every return path post-network-attempt).
- 3B-M1 (ADR-092 missing TASK-497 cross-task race link): added "Cross-task race coverage (TASK-499 / 3B-M1)" section that points at ADR-090's amendment for the canonical portMUX_TYPE snapshot pattern; rationale "NimBLE is the producer of this race, so the cross-reference belongs in this ADR too".
- 3B-M2 (C4 docs drift):
  - c4-code-otdirect.md:181 — split CS=/TC= into separate lines; TC= and TT= now correctly map to MsgID 16 + MsgID 100 with auto-clear vs persistent semantics noted.
  - c4-code-otdirect.md:496 — otOverrides[] entry count 7 → 8 with explicit MsgID 100 RemoteOverrideFunction note.
  - c4-code-sat.md:160 — BLEDevice.h → NimBLEDevice.h with ADR-092 / TASK-487 reference.
- 3B-M3 — closed as part of 1A-M5 in same edit.

Verification:
- ESP32 incremental build SUCCESS (exit 0, firmware.bin 1884480 bytes).
- ESP8266 incremental build SUCCESS (exit 0).
- Pushed: 4d7bfaa0 → origin/feature-dev-2.0.0-otgw32-esp32-sat-support.

Followup plan .full-review/06-followup-plan.md committed and serves as the canonical roadmap for Batches C/D/E. Batch C (TASK-500) starts next.
<!-- SECTION:FINAL_SUMMARY:END -->
