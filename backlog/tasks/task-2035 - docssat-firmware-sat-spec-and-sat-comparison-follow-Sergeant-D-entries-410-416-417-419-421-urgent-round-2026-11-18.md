---
id: TASK-2035
title: >-
  docs(sat): firmware-sat-spec and sat-comparison follow Sergeant D entries 410,
  416, 417, 419, 421 (urgent round 2026-11-18)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-18T19:54:00Z'
updated_date: '2026-11-19T22:22:00Z'
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
- [x] #3 Manual chapters for 410/417 updated once otgw-dev's lane lands
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-18 local commit on branch sat-docs-410 (from origin/dev 3a75f6c9d). Opened this session: entries 410, 416, 417, 419, 421 (sergeant-d-corrections.md:8136, :8238, :8261, and the 419/421 sections); firmware SATcontrol.ino:8265-8289 (iHeatingMode in zone aggregation :8273, band :8277, solar :8288) and :8380 (switch call), SATcycles.ino:1376 (satCycleCheckOvershootSwitch); thermo-nova local copy heating_control.py:276-282, cycles/tracker.py:314-316, cycles/classifier.py:25-26, cycles/history.py:102 (record_cycle), climate.py:377-379. Spec: band bullet names entry 421; 410/417 gate added (marked from the build that adds it) to the two switch table rows and both switch bullets, the stale 'margin 1.5, fail-safe never first' sentence corrected for the TASK-2005 defaults; 416 paragraph after the classification list (decided, not built, TASK-1998); 419 note on the flame-off hold row. Six stale 'ECO/COMFORT has no effect' statements (and the open-Q58 remark) corrected: ECO skips zones, limits the band to the main room and takes no solar offset (entry 118). Comparison: fail-safe bullet (410/417), ECO rows 44, 'Requested setpoint: ECO / COMFORT' and 19 now match. Checks: tests/test_sat_ch_signals.py OK, tests/test_sat_rest_contract_1496.py OK (the two tests that read these files); no em dash added. AC3 waits for otgw-dev's lane-410.

Correction (coordinator, otgw-dev): entry 416 is current behaviour, not a pending change. Verified this session: kind DHW SATcycles.ino:792 (_cycleDetectKind, tracker.py:315-316), UNCERTAIN on a hot-water tail SATcycles.ino:899 (classifier.py:25-26), satCycleApplyEndOfCycle acts only on OVERSHOOT / UNDERHEAT_PWM (SATcycles.ino:1006-1021), flames under SAT_CYCLE_MIN_SAMPLES 3 dropped (SATcycles.ino:30, :1110-1114; tracker.py:98-100). The 416 paragraph now says current behaviour, matches thermo-nova; TASK-1998 works-as-designed.

2026-11-20 AC3: lane-410 landed as c1a232370 (beta.377, in v2.0.0-beta.382 and .392). Opened this session: SATcontrol.ino:8397-8405 at origin f5bce220a (switch only when pidOutput > satColdThreshold() && !_sat_tolBandOff, else satCycleSkipAutoSwitch); thermo-nova heating_control.py:276-282 (heater not ON / requested <= COLD_SETPOINT clear the start). ch05:192 / h05:217 gain the 410/417 sentence (from beta.377, both rules, band pause, leftover hot-water heat). Stale 'from the build that adds it (... not on origin/dev at 3a75f6c9d)' replaced by 'from beta.377 (c1a232370 ...)' in firmware-sat-spec.md (4x) and sat-comparison.md:148. Superseding entries checked: no entry after 430; 428/429/430 (beta.391) change the fail-safe to 3.0 C and add the per-burn rule, already in the same paragraphs; nothing supersedes 410/416/417/419/421. The new text states agreement with thermo-nova, no difference, so no second reader needed (sat-ground-truth). Gates: build_manual --check exit 0, test_manual_* 10 passed, test_sat_corrections_index 5 passed, no em dashes.

Correction: the AC3 note above is dated 2026-11-20 by mistake; it was written 2026-11-19 23:21.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: firmware-sat-spec.md and sat-comparison.md describe entries 410/417, 416, 419, 421 with entry numbers and citations (2026-11-18 session), the 410/417 markers now 'from beta.377 (c1a232370)'. AC2: ECO/COMFORT statements corrected (entry 421). AC3: ch05/h05 (and MANUAL.md) state that from beta.377 neither continuous->PWM rule runs while heating is not allowed (band pause), citing entries 410/417, SATcontrol.ino:8397-8405 and thermo-nova heating_control.py:276-282. Checked against later entries 422-430: none supersedes 410-421; 428/429 (3.0 C, per burn) already in the text. Gates: build_manual --check, test_manual_* 10 passed, test_sat_corrections_index 5 passed. Not verified: behaviour on a boiler (docs only).
<!-- SECTION:FINAL_SUMMARY:END -->
