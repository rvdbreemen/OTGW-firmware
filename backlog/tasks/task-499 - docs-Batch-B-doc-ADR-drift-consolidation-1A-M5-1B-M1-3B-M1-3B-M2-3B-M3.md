---
id: TASK-499
title: >-
  docs: Batch B doc + ADR drift consolidation (1A-M5, 1B-M1, 3B-M1, 3B-M2,
  3B-M3)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-01 05:22'
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
- [ ] #1 1A-M5: BLE state-topic format reference in code comments points to docs/api/MQTT.md as the single source of truth
- [ ] #2 1B-M1: ADR-077 amended with the bounded-buffer single-publish exception, gated by heap checks
- [ ] #3 3B-M1: ADR-092 references ADR-090 amendment (cross-task race covered there)
- [ ] #4 3B-M2: c4-code-otdirect.md and c4-code-sat.md refreshed against current code (NimBLE 2.x, 8-entry override table, TT/TC vs MsgID 1 mapping)
- [ ] #5 3B-M3: MQTTstuff.ino block-comment correction — 'two-pass shape ADR-077 requires' rephrased
- [ ] #6 evaluate.py --quick zero new violations
<!-- AC:END -->
