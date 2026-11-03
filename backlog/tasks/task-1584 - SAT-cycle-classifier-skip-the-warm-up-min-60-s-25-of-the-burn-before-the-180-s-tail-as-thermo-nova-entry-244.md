---
id: TASK-1584
title: >-
  SAT cycle classifier: skip the warm-up (min 60 s, 25 % of the burn) before the
  180 s tail, as thermo-nova (entry 244)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-03T06:37:00Z'
updated_date: '2026-11-03T06:50:00Z'
labels:
  - sat
dependencies: []
priority: medium
ordinal: 729000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D entry 244 (Q-PARITY-3 A, 2026-11-03): the tail the classifier judges starts after an effective warm-up of min(60 s, 25 % of the burn), and is the last 180 s after that (thermo-nova cycles/tracker.py:113-116). The port takes the last 180 samples including the warm-up (SATcycles.ino:1009-1025; the DHW-in-tail check already skips it, :925-927). Found by TASK-1412.14 (Q-PARITY-3).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test on the real SATcycles.ino: a burn under 240 s no longer counts its first quarter in the tail; the warmup_high_flow_short_cycle parity scenario of tests/test_sat_classifier_parity.py agrees with thermo-nova (removed from the DIFFERENCES catalogue); red on the old code
- [x] #2 RAM delta stated (no new buffer expected)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-03 (agent, verified by lead in wt-next): _tailTrimWarmup(durationSec) lowers _tail_sampleCount at flame-off so samples with t < min(60, 0.25*duration) drop out (tracker.py:113-116, :237-241); sample time scaled by the real mean cadence; RAM 0 B (no timestamp buffer). test_sat_cycle_classifier 29 OK (new 8-case test, old 1 FAIL at W120); test_sat_classifier_parity old 3 FAIL, fix OK with the gate still xfail (other class-b differences remain); warmup_high_flow_short_cycle now GOOD/GOOD.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The cycle classifier skips the warm-up of a burn, min(60 s, 25 % of the burn), before the 180 s tail, as thermo-nova (entry 244); 0 B RAM. Evidence: new 8-case host test (old 1 FAIL) and the real-thermo-nova parity test (old 3 FAIL, fix OK). Not verified on a device or with real sampling jitter.
<!-- SECTION:FINAL_SUMMARY:END -->
