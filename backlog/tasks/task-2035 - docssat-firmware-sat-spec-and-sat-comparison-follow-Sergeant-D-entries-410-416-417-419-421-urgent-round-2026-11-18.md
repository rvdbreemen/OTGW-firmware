---
id: TASK-2035
title: >-
  docs(sat): firmware-sat-spec and sat-comparison follow Sergeant D entries 410,
  416, 417, 419, 421 (urgent round 2026-11-18)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-18T19:54:00Z'
updated_date: '2026-11-18T20:00:00Z'
labels:
  - docs
  - sat
milestone: 2.0.0
dependencies: []
ordinal: 1044000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Coordinator request 2026-11-18. Behaviour text only. 410/417 (both continuous->PWM rules only while heating is allowed) marked 'from the build that adds it' (otgw-dev lane-410, not landed); 416 (hot-water burns counted, marked hot water; TASK-1998, not built); 419 (high start setpoint until light-off = ignition, no change); 421 (band eco/comfort = SAT heating mode, current firmware). Manual chapters for 410 wait until that build lands.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 firmware-sat-spec.md and sat-comparison.md describe entries 410/417 (marked from the build that adds it), 416 (decided, not built), 419 and 421 with entry numbers and file:line citations opened this session
- [x] #2 Stale ECO/COMFORT 'no effect' statements corrected against origin/dev 3a75f6c9d
- [ ] #3 Manual chapters for 410/417 updated once otgw-dev's lane lands
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-18 local commit on branch sat-docs-410 (from origin/dev 3a75f6c9d). Opened this session: entries 410, 416, 417, 419, 421 (sergeant-d-corrections.md:8136, :8238, :8261, and the 419/421 sections); firmware SATcontrol.ino:8265-8289 (iHeatingMode in zone aggregation :8273, band :8277, solar :8288) and :8380 (switch call), SATcycles.ino:1376 (satCycleCheckOvershootSwitch); thermo-nova local copy heating_control.py:276-282, cycles/tracker.py:314-316, cycles/classifier.py:25-26, cycles/history.py:102 (record_cycle), climate.py:377-379. Spec: band bullet names entry 421; 410/417 gate added (marked from the build that adds it) to the two switch table rows and both switch bullets, the stale 'margin 1.5, fail-safe never first' sentence corrected for the TASK-2005 defaults; 416 paragraph after the classification list (decided, not built, TASK-1998); 419 note on the flame-off hold row. Six stale 'ECO/COMFORT has no effect' statements (and the open-Q58 remark) corrected: ECO skips zones, limits the band to the main room and takes no solar offset (entry 118). Comparison: fail-safe bullet (410/417), ECO rows 44, 'Requested setpoint: ECO / COMFORT' and 19 now match. Checks: tests/test_sat_ch_signals.py OK, tests/test_sat_rest_contract_1496.py OK (the two tests that read these files); no em dash added. AC3 waits for otgw-dev's lane-410.

Correction (coordinator, otgw-dev): entry 416 is current behaviour, not a pending change. Verified this session: kind DHW SATcycles.ino:792 (_cycleDetectKind, tracker.py:315-316), UNCERTAIN on a hot-water tail SATcycles.ino:899 (classifier.py:25-26), satCycleApplyEndOfCycle acts only on OVERSHOOT / UNDERHEAT_PWM (SATcycles.ino:1006-1021), flames under SAT_CYCLE_MIN_SAMPLES 3 dropped (SATcycles.ino:30, :1110-1114; tracker.py:98-100). The 416 paragraph now says current behaviour, matches thermo-nova; TASK-1998 works-as-designed.
<!-- SECTION:NOTES:END -->
