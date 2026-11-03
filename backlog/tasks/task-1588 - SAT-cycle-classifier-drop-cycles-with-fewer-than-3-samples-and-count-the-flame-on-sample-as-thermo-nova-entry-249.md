---
id: TASK-1588
title: >-
  SAT cycle classifier: drop cycles with fewer than 3 samples and count the
  flame-on sample, as thermo-nova (entry 249)
status: To Do
assignee: []
created_date: '2026-11-03T08:41:00Z'
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
- [ ] #1 Host test on the real SATcycles.ino: a 1-2 s flicker leaves the cycle count, fractions and windows unchanged; the three_second_cycle, two_second_cycle and kind_ch_exactly_80pct parity scenarios agree (removed from DIFFERENCES); red on the old code
- [ ] #2 MQTT/REST: insufficient-cycle counters documented as no longer fed by flickers (docs/api/MQTT.md if it lists them)
<!-- AC:END -->
