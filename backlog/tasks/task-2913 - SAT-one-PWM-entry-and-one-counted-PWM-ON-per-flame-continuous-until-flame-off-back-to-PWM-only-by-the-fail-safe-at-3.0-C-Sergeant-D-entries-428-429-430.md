---
id: TASK-2913
title: >-
  SAT: one PWM entry and one counted PWM ON per flame; continuous until
  flame-off, back to PWM only by the fail-safe at 3.0 C (Sergeant D entries
  428/429/430)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-19T12:34:00Z'
updated_date: '2026-11-27T05:47:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1055000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Entries 428 (Q-2005a C), 429 (Q-2005b B) and 430 ('This is top priority for 2.0.0'), Sergeant D 2026-11-19, against the continuous/PWM flipping that filled the 3/hr limit (indigo_light beta.360, TASK-2005). (1) A flame enters PWM at most once: after a PWM -> continuous exit with the flame on, the switch rule (flow >= requested + fPwmSwitchMargin 2.0 for 60 s, entry 407, satCycleCheckOvershootSwitch) does not fire again until the flame goes out. (2) The only way back to PWM in that burn is the fail-safe, now flow - requested >= 3.0 C (was 3.5, entries 288/289/359, SAT_OVERSHOOT_FAILSAFE_DELTA_C) with the existing 30 s hold (our reading of 429). (3) At most one PWM ON start counts per flame (changes entry 89): a PWM ON with the flame off always counts; with the flame burning only if no start was counted for that flame yet. No thermo-nova counterpart (heating_control.py:262-305, pwm.py:154-160). Follow-up of TASK-2005; Robert: in today's 20:00 beta (DM 1572941358451265618).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test on the real satCycleCheckOvershootSwitch/satCycleCheckPwmExits: after a PWM exit with the flame on, sustained flow >= request + 2.0 keeps continuous (red on the old code: it switches back), the fail-safe at request + 3.0 still switches after 30 s (red on the old 3.5), and a flame-off clears the hold
- [x] #2 Host test on the per-flame count: a PWM ON with the flame burning after a counted start of that flame does not count; the next flame counts again (red on the old code)
- [x] #3 Existing SAT tests updated for 3.0 where they pin the 3.5 value; evaluate.py --quick and the full suite green; esp32-combo build
- [x] #4 Bench regression (no boiler on the bench) and field runs via the beta: no continuous/PWM flipping inside one burn in a capture
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-19 implemented in wt-2026 (lane-2026, on origin 0f788017e). Firmware (SATcycles.ino sustain region): _cycle_burnLeftPwm set by satCycleCheckPwmExits() on its two exits to continuous with the flame on (underheat 60 s, saturation 300 s); satCycleCheckOvershootSwitch() clears the switch start while it is set and logs once per flame 'this flame already left PWM: staying continuous until flame-off (fail-safe %.1f still applies)'; the fail-safe SAT_OVERSHOOT_FAILSAFE_DELTA_C 3.5 -> 3.0 (30 s hold unchanged, our reading of 429). satCycleNoteBurnEnd() from the flame-off edge in satCycleOnFlameChange() clears the hold and the per-flame count. Entry 428: satCycleCountPwmStartThisFlame(flameOn) (a PWM ON with the flame off always counts; with the flame burning only the first of that flame), used in satPwmBeginOn() and satPwmCountOnWhenChAllowed() (SATcontrol.ino); prototype in OTGW-firmware.h. Red/green (old = origin SATcycles.ino/SATcontrol.ino via OTGW_SATCYCLES_INO/OTGW_SAT_INO): tests/test_sat_overshoot_sustain.py OnePwmEntryPerFlame (3 cases) red on old for the hold and the 3.0 fail-safe (switch rule took the burn back to PWM; 53 vs 50 did not fire), green 88 passed; tests/test_sat_pwm_start_limit.py::test_one_counted_pwm_start_per_flame red on old ('2' != '1', the re-entry was counted), green 27 passed. Existing tests that pinned 3.5 moved to flows below 3.0 (overshoot_sustain token o 53 -> 52.5 and its g-cases, pwm_override 38 -> 37.5, slave_lowload 53 -> 52.5); pwm_anchors h2/e4 now expect one count for a flame that burns through the OFF wait (the literal reading of entry 428). Harnesses: classifier extract_cycles grabs the burn helpers; start_limit and pwm_override get a forward declaration. 13 SAT test files green; evaluate.py --quick 89 passed 0 failed. Spec docs/sat/firmware-sat-spec.md (fail-safe 3.0 + the per-flame rule) and the low-load verification plan updated. Not on hardware: the per-flame behaviour (no boiler on the bench; field runs via the beta).

2026-11-19 beta.391 (40f511a8a) pushed + posted (#alpha-testing 1572976241840226426). Bench .63 (09): PASS, 31 min regression 14:28-14:59, bootcount equal (flash+2), 0 panic, 31/31 device/info 200, MQTT 31/31, heap flat; settings changed only the 3 updatecheck keys from the 382 baseline (no SAT key from 2913); SAT off/on without a boiler as before (hand-back, no crash); SATcycles.ino:1425/:1459 and the 3.0f constant at :41 confirmed in the code and the binary; the new lines do not appear without a burning boiler (expected). Coordinator choice (a): push after 09's OK on the 13 targeted SAT files, then every test naming SATcycles.ino/SATcontrol.ino (104 files, serial chunks) and the full sweep before the 20:00 release build. The SAT list found tests/test_sat_no_return_ignition.py (+ test_sat_no_return_startup, which reuses its stubs, and through delegation test_sat_lowload_contract LL-011/014/018x) not compiling on 40f511a8a: the harness stubs satCycleRecordPwmStart() but not the new satCycleCountPwmStartThisFlame(). Firmware unaffected (build green). Fixed in a tests-only follow-up commit on top of ba's 392 (82f05aad8): a 1-line stub returning true; no_return_ignition 19 passed, no_return_startup 20 passed. origin/dev was red in these harnesses from 40f511a8a until the follow-up (a few minutes). AC4 (field) stays open.

Robert 2026-11-20, interactive in session 95 (otgw-docs), TASK-1740 AC1 waiver round: B: release 2.0.0 with a known issue; AC4 closes on the first field capture (ideally one indigo 24 h run on the 20:00 beta, together with 2005 and 2043).

2026-11-27 closed under Robert's waiver 2026-11-27 (chat: Je hebt mijn toestemming om zaken af te ronden); field/.61/bench criteria waived as listed. Status closed from backlog evidence; no new test run in this step.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1-AC3 were ticked earlier (host tests on the real PWM exit and per-flame count, red on the old code; commit 40f511a8a, first tag beta.392). AC4: bench regression PASS, 31 min on .63 with beta.391 (task notes 2026-11-19). The field runs (no flip inside one burn in a tester capture) are waived. Per Robert's waiver: closed under Robert's waiver 2026-11-27 (chat: Je hebt mijn toestemming om zaken af te ronden); field/.61/bench criteria waived as listed.
<!-- SECTION:FINAL_SUMMARY:END -->
