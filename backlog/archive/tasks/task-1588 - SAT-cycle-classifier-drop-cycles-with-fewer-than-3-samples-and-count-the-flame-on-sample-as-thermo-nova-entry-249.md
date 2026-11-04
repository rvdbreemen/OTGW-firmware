---
id: TASK-1588
title: >-
  SAT cycle classifier: drop cycles with fewer than 3 samples and count the
  flame-on sample, as thermo-nova (entry 249)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-03T08:41:00Z'
updated_date: '2026-11-03T09:14:00Z'
labels:
  - sat
dependencies: []
priority: medium
ordinal: 733000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D entry 249 (Q-PARITY-8 A, 2026-11-03): a cycle with fewer than 3 samples is not recorded at all (thermo-nova cycles/tracker.py:98-100), instead of an INSUFFICIENT cycle that enters the counts, fractions and 4 h/daily windows (SATcycles.ino:846); and the flame-on sample is counted (tracker.py:71-72; the port skips it, SATcycles.ino ~869/1009). Merges Q-PARITY-7.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test on the real SATcycles.ino: a 1-2 s flicker leaves the cycle count, fractions and windows unchanged; the three_second_cycle, two_second_cycle and kind_ch_exactly_80pct parity scenarios agree (removed from DIFFERENCES); red on the old code
- [x] #2 MQTT/REST: insufficient-cycle counters documented as no longer fed by flickers (docs/api/MQTT.md if it lists them)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-03 (agent, verified by lead): SAT_CYCLE_MIN_SAMPLES=3; a flame with <3 samples sets only _cycle_lastFlameOffMs (tracker.py:106) and returns, so no record, count, fractions, EMAs, duty, 4 h/daily window or last-cycle update; the per-second tail/kind block moved into _cycleTailSample(), called from the flame-on edge (k=0, tracker.py:71-72) and from satCycleSample; _tailTrimWarmup drop adjusted for k=0. INSUFFICIENT enum kept (MQTT/REST/v2.js still map it; now only duration<=0). API note in docs/api/MQTT.md:680 + CHANGELOG: insufficient no longer appears for 1-2 s flames, cycle_count no longer counts flickers; kind fractions shift by one sample. Tests: cycle_classifier 34 OK (old 3 FAIL), parity old 7 FAIL, fix OK; last_cycle_expiry, mqtt_contract_1485, rest_contract_1496, lowload_contract green. Parity gate still xfail: Q-PARITY-2/4/6/11 remain catalogued (withdrawn, not decided).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Flame flickers under 3 samples are no longer recorded as cycles, and the flame-on sample counts, as thermo-nova (entry 249). Evidence: host tests on the real SATcycles.ino red on the old code, green on the fix, plus the parity and API contract tests. API: 'insufficient' no longer appears for flickers, cycle_count stops counting them (documented). Not bench-tested.
<!-- SECTION:FINAL_SUMMARY:END -->
