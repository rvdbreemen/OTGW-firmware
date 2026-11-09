---
id: TASK-1762
title: >-
  SAT continuous -> PWM fail-safe: flow - CS >= 3.5 for 30 s (extra beside the
  60 s rule)
status: In Progress
assignee:
  - '@system32-030509'
created_date: '2026-11-09T19:17:00Z'
updated_date: '2026-11-09T21:46:00Z'
labels:
  - sat
  - pwm
dependencies: []
priority: high
ordinal: 861000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D, 2026-11-09, #dev-sat-mqtt: entry 288 (18:20:47 UTC, msg 1569416482025119805: "as a fail safe condition when boiler temperature - cs >= 3.5 in continuous mode fall back to PWM") and entry 289 (18:58:46 UTC, msg 1569426039703216169, answer A to Q-288a: "we need to stay 30 seconds at flow - CS >= 3.5 just not to trigger unnecessary PWM cycles. This will be an extra fail safe mechanism for flame out"). Unblocked by entry 289.

Rule: in continuous mode, when flow - CS >= 3.5 holds for 30 s, SAT switches to PWM. CS = the requested (PID) setpoint. Extra beside the existing 60 s sustained-overshoot rule (thermo-nova heating_control.py:262-305, const.py:21, cycles/const.py:10; firmware SATcycles.ino satCycleCheckOvershootSwitch(), margin settings.sat.fOvershootMargin default 2.0), which stays unchanged. Entry 213 (MsgID 3 PWM-not-allowed bit blocks every PWM entry path) applies. Our reading in entry 289, not asked: the 30 s timer runs like the 60 s one (reset on a non-qualifying tick, missing flow, outside continuous mode or within the hot-water guard; entry 109: ticks never catch up); equality counts. Deliberate addition on top of thermo-nova (record as such). Related TASK-1412; entries 88, 109, 213, 288, 289.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Firmware: in continuous mode, flow - CS >= 3.5 for 30 s switches to PWM, beside the unchanged 60 s rule; CS is the requested (PID) setpoint; entry 213's MsgID 3 block applies
- [ ] #2 Timer behaviour as in entry 289 (start, reset conditions, no catch-up, >=) covered by native tests, including the boundary 3.5 and 29/30 s
- [ ] #3 Recorded as a deliberate deviation from thermo-nova in the SAT ground-truth docs, citing entries 288/289
- [ ] #4 Shown on a capture or simulation replay of tjfs's 2026-11-09 case: the switch comes 30 s after flow - CS reaches 3.5 instead of via the 60 s rule
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Ground truth: Sergeant D entries 288 (fail-safe) and 289 (Q-288a answered A with a 30 s hold); entry 213 (MsgID 3 bit 4 blocks every PWM entry path) still applies; entries 88/109 (switch before apply, ticks never catch up). thermo-nova at the pin (7beaaf5) has only the 60 s rule, heating_control.py:262-305 (_maybe_enable_pwm_on_sustained_overshoot: reset on PWM enabled :264-266, hot water :268-270, DHW guard :272-274, heater off :276-278, request <= COLD_SETPOINT :280-282, flow None :284-286, flow < requested + OVERSHOOT_MARGIN_CELSIUS :288-291, sustained_runtime :293-297, pwm.enable() + _reset_pwm_disable_guards() :304-305), OVERSHOOT_SUSTAIN_SECONDS 60 (const.py:21), OVERSHOOT_MARGIN_CELSIUS 3.0 (cycles/const.py:10). The fail-safe is a deliberate addition on top of thermo-nova.
1. SATcycles.ino: own timer pair _sustain_overshootFailsafeRunning/StartMs and constants SAT_OVERSHOOT_FAILSAFE_DELTA_C = 3.5 (fixed, not the margin setting) and SAT_OVERSHOOT_FAILSAFE_SUSTAIN_MS = 30000, in satCycleCheckOvershootSwitch(): shared gates first (not continuous, DHW guard, missing flow reset both timers), then each rule evaluated independently (the margin setting can be 0.5-5.0, so the 3.5 rule must not sit inside the margin check); a tick with flow - CS < 3.5 resets only the fail-safe timer. CS = requested (PID) setpoint, >= counts, satSustainedFor() (no catch-up). Either rule firing asks satPwmEntryAllowed(1) (same entry path as the overshoot switch); refused -> both timers reset. satCycleResetSustainGuards() clears the new timer too. Own narration line naming the 3.5/30 s rule.
2. Host test tests/test_sat_overshoot_sustain.py against the real code: token 'o' moves to flow 53 (inside [req+2, req+3.5)) so the 60 s cases keep testing the 60 s rule; g-cases with flow >= req+3.5 reviewed one by one; new cases: 3.5 boundary, 29 999 vs 30 000 ms, a dip that resets only the fail-safe, missing flow, PWM tick, DHW guard, margin 5.0, MsgID 3 refusal. Red on the old SATcycles.ino (OTGW_SATCYCLES_INO), green on the new. Run the other SAT tests that cut SATcycles.ino.
3. Docs: firmware-sat-spec.md, the deviation record (as entry 279 KD 0.063), comments.
4. Version bump, build via build.bat in a slot from the coordinator; bench .61 (OTA + SAT simulation, settings saved and restored) for the transition.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementation (lane-1762, wt-1762, on top of origin/dev 00b2f2b6c; not bumped yet: beta.165 after 80's beta.164). SATcycles.ino satCycleCheckOvershootSwitch(): own timer _sustain_overshootFailsafeRunning/StartMs, SAT_OVERSHOOT_FAILSAFE_DELTA_C 3.5 (fixed), SAT_OVERSHOOT_FAILSAFE_SUSTAIN_MS 30000; shared gates (continuous, hot-water guard, missing flow clear both starts); each rule evaluated on its own (margin setting 0.5-5.0 may exceed 3.5); a tick below req + 3.5 clears only the fail-safe start; satPwmEntryAllowed(1) for both, a refusal clears both; satCycleResetSustainGuards() clears it; own narration line. thermo-nova at the pin re-opened: heating_control.py:262-305 (60 s rule only), const.py:21 (60), cycles/const.py:10 (3.0).

Adjusted harness inputs (review point of the coordinator):
(a) What each test proved and why the new input still proves it. The fail-safe fires on flow >= req + 3.5 for 30 s, so any test that wanted the 60 s rule with a flow >= req + 3.5 now saw the fail-safe first. Their inputs moved into [req + margin, req + 3.5), where only the 60 s rule qualifies, so they prove the same thing as before:
- test_sat_overshoot_sustain.py token o (55 -> 53 at req 50) and the LowLoadEntryCells g-cases (58/54 -> 53; start-up ramp 58/64/66 -> 53/53.2/53.4; post-draw 62 -> 44 at req 41): 60 s fire time, dips, PWM/bypass/missing-flow resets, DHW guard windows, late ticks, setpoint fall, valve step. Same expected times.
- test_sat_pwm_override.py swtick (45 -> 38 at pid 35): loop level, the real overshoot gate and apply step in one tick; the switch tick sends the PWM setpoint (Q47 A, entry 88), not the continuous one, after 60 s at 1/10/30/60/120 s ticks. Same assertions.
- test_sat_slave_lowload.py overshootTicks (55 -> 53 at req 50): the MsgID 3 permission (entry 213) blocks the automatic 60 s switch, traces reason/path, and after bit 4 clears the switch needs a fresh 60 s (3rd 30 s tick). Same assertions.
(b) The old inputs are kept as explicit cases of the new behaviour (red on the old SATcycles.ino, green on the new):
- test_sat_overshoot_sustain.py test_old_60s_case_inputs_now_switch_at_30s: (50,55), (50,54), (50,58), (35,45) switch 30 s after the first qualifying tick, also at a 10 s tick; test_heat_left_far_above_the_request_after_a_draw_..._failsafe_takes_30s: the old post-draw input 62/41 switches 330 s after the draw. Old code: fails.
- test_sat_pwm_override.py test_failsafe_switch_tick_sends_the_pwm_setpoint_after_30s: swtick with the old flow 45: switch on tick 31, PWM CS 48.0 on that tick, CH 1. Old code: '61' != '31'.
- test_sat_slave_lowload.py test_failsafe_needs_a_valid_reply_with_bit4_clear: the old flow 55 switches on the 2nd 30 s tick when permitted; bit 4 = 1 and no valid reply keep continuous; reason 2, path 1. Old code: ('2','3') != ('2','2').

Evidence (host, Windows, 2026-11-09): test_sat_overshoot_sustain 77 OK (FlameOutFailsafe 13 cases; red on old: 12 of 14 new cases + the old-input case); test_sat_pwm_override 83 OK; test_sat_slave_lowload 20 OK; the other tests that cut SATcycles.ino, run per file with python: fs_update_keep 37, ch_signals 4, classifier_parity 12, cycle_classifier 43, hp_pwm 35, last_cycle_expiry 10, lowload_contract 35 (1 skip, 2 expected failures as before), mqtt_contract_1485 15, narrate_fits 1, pwm_anchors 64, pwm_start_limit 26, requested_vs_transmitted 10, tail_percentile 9 (1 skip), telemetry_wire 18: all OK. Not yet: full run_tests, bump, ESP32 build, bench .61 (satsimulation).
<!-- SECTION:NOTES:END -->

## Comments

<!-- COMMENTS:BEGIN -->
created: 2026-11-09T21:46:00Z

---

Bench .61, 2026-11-09 (slot from the coordinator): build.bat exit 0, build/OTGW-firmware-esp32-combo-2.0.0-beta.165+cbf48f1.ino.bin + .littlefs.bin (22:33). OTA via scripts/ota_flash.py: app + fs OK, running 2.0.0-beta.165+cbf48f1, 'settings: 164 keys, changed: none'; settings diff after the bench against the pre-OTA dump: none; SATsimulation was already true and stayed true. Sim check (SAT simulation, solar_gain 0.2 C/min from 22:40:41): the PID stepped 34.25 -> 30.41 at 22:42:43 with sim flow 33.8, flow - pid = 3.39 K for 50 s, then the PID fell to 10.6 (cold cutoff, 22:43:43). Fail-safe correctly not triggered at 3.39 K < 3.5; positive switch on hardware not shown, only in host tests. (The 60 s rule, margin 3.0 on .61, had reached 50 s.) Health at 22:46: bootcount 197, lastreset Software reset, freeheap 64660, hd_min_free_heap 18044, maxfreeblock 31732, max loop gap 305 ms, mqtt drops 0; no panic/exception in telnet. Pushed on the coordinator's go (option a: push with this caveat).

---
<!-- COMMENTS:END -->
