---
id: TASK-500
title: 'fix: Batch C real-issue Mediums (1A-M1, 1A-M3, 2A-M2, 2B-M1, 3A-M2/M3/M4/M5)'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-01 06:37'
labels:
  - follow-up
  - code-review
  - ble
  - otdirect
dependencies:
  - TASK-499
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Real-issue Mediums from .full-review/06-followup-plan.md Batch C: ghost-override fix in setRemoteOverride, TT/TC negative-value not-a-bug doc, default-allow MAC trust model in MANUAL.md, intra-burst yield in satBLEPublishMQTT, BLE filter+parser tests, coalesce-by-MsgID test extension. Defers 2B-M3 (queue depth review until field data) and 4A-M2 follow (TT/TC dispatch table refactor).
<!-- SECTION:DESCRIPTION:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Closed in commit 83e0a72e fix(otdirect,ble,tests): Batch C real-issue Mediums (TASK-500).

Closed:
- 1A-M1 ghost-override: setRemoteOverride enqueues MsgID 16 first, only commits otRemoteOverride.* and state.otd.* on success. Honours user's "queue is the channel, no side-channels" principle. enqueueWriteCommand return type changed void → bool.
- 1A-M3: TT/TC negative-value documented as not-a-bug with gateway.asm parity reference. Clear is exact-zero only.
- 2A-M2: Default-allow MAC trust model documented in docs/MANUAL.md "BLE Temperature Sensors" section: empty filter accepts any parseable BTHome v2 / ATC sensor; configured filter strict; slot stickiness; no-write-no-query stance; dense-environment guidance.
- 2B-M1: delay(0) added to end of satBLEPublishMQTT per-slot iteration to release FreeRTOS loop task during the worst-case 32-publish first-scan burst.
- 3A-M5: Coalesce table in test_otdirect_override.cpp extended to MsgIDs 1, 14, 100 in addition to 16.
- 3A-M2 + 3A-M3 + 3A-M4: New tests/test_ble_parsers.cpp covers ATC/pvvx + BTHome v2 byte-layout parsers, encrypted-flag rejection, packet-id prefix skip, MAC-filter strict-vs-empty paths. 12 assertion blocks, ~40 individual checks. Pure-host C++17.

Deferred (per Batch C rules):
- 2B-M3 queue-depth review: defer until otCmdQueueHighWater field data is in.
- 4A-M2 follow: TT/TC dispatch table is a larger architectural sweep, no-op this round.

Verification:
- pio run -e esp32 -j 1 SUCCESS (exit 0)
- pio run -e nodemcuv2 -j 1 SUCCESS (exit 0)
- Pushed: 83e0a72e → origin/feature-dev-2.0.0-otgw32-esp32-sat-support
<!-- SECTION:FINAL_SUMMARY:END -->
