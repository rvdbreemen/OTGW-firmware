---
id: TASK-1817
title: >-
  SAT self-tune: PWM hours count for the warm-up phase (Q-1796b, Robert B; waits
  for Sergeant D)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-11T20:20:00Z'
updated_date: '2026-11-12T14:40:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 910000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert decided Q-1796b = B (DM msg 1570171160111620147, relayed by the coordinator 2026-11-11): PWM hours count for the warm-up phase of the self-tune; the both-phases-agree rule stays, the floating minimum stays 60 min (entry 315, TASK-1816). Today PWM-controlled ticks/hours are excluded from the tuner's clean hours (see the clean-hour rule, ADR-205 decision 2, and the satCurveTune block in SATcontrol.ino / SATcurveTune.h). Sergeant D's answer to the same question is still open: implement only after his answer or explicit silence is recorded in docs/sat/sergeant-d-corrections.md (sat-ground-truth rule; changes SAT control behaviour). Check whether ADR-205 decision 2 / ADR-228 need a superseding ADR.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Sergeant D's answer (or recorded silence) on Q-1796b is in docs/sat/sergeant-d-corrections.md before the code commit
- [x] #2 Host test: an hour with PWM ticks counts for the warm-up phase on the fix and is excluded on the old code; floating unchanged
- [x] #3 ADR impact settled (ADR-205 d2 / ADR-228) via adr-kit, recorded in the notes
- [x] #4 Build, evaluate --quick, OTA bench on .61 with PWM active (satboilercapacity low) shows the PWM hour counted in SATTUNEH
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Sergeant D answered Q-1796b B as well, entry 316 (20:04:33 UTC, msg 1570167372474617888); with Robert's DM B this revises entry 174. Needs an ADR-228 update (ADR-228 is Accepted: supersede or amend via adr-kit). Unblocked once entry 316 is on origin/dev.

ADR-230 (Proposed) carries this rule as decision 2. Built only after Sergeant D answers Q-1817a (A: every PWM tick counts for warm-up, advice; B: PWM hours stay in the normal phase split) and Q-1817b (hybrid: gas-boiler rule, advice, or heat-pump rule), asked via comms 92.

2026-11-12 Robert chose to split (session d3): ADR-230 now holds only the floating minimum (Accepted); the PWM-hours rule gets its own new ADR once Sergeant D answers Q-1817a/b (posted by 92, msgs 1570214599238877266 and 1570214605882662923).

2026-11-12 otgw-firmware-d3: AC1 met: Sergeant D entries 316 (Q-1796b B: PWM hours count for warm-up on gas boilers), 318 (Q-1817a A: every PWM tick goes to warm-up, whatever the PID error) and 319 (Q-1817b B: a hybrid in PWM follows entry 205, PWM hours logged and not used) are on origin (e6f9be70c) before any code. Gap found before implementing: satTuneTickReason() (SATcontrol.ino:6018-6031, order of ADR-205 decision 2) checks pwm before flame_off (chOn && !flameOn) and min_mod (relMod <= minMod); lifting only the pwm gate leaves those two, which a PWM hour hits by construction (the flame pulses, the boiler runs low). Measured on gruijter's capture 10-11 Nov (beta.171, capture-gruijter-1111, 20 tuner hours; SATTICK mode=2 = SAT_MODE_PWM, relMod from boiler MsgID 17 Read-Ack in otframes.log, minMod 5 = SAT_CT_DEFAULT_MIN_MOD because the boiler answers MsgID 15 Unknown-DataID): B (only the pwm gate goes): 0 hours gain; PWM hours 18/19 (120/120 PWM ticks) are then excluded by min_mod (61 and 56 of 120 PWM ticks at or below 5 %, plus 3-4 flame-off ticks). A (on a gas boiler a PWM tick also skips flame_off and min_mod; sun, DHW, no_room, coefficient change and tick count stay): 1 hour gains (hour 19 counts; hour 18 still excluded by 2 DHW ticks; sun hours 13-17 by sun; mixed hours 8, 9, 11 by min_mod on their non-PWM ticks). Scripts: scratchpad pwmhours.py / pwmhours_a.py, outputs pwmhours-gruijter1111*.txt. Not covered by entries 316/318/319: asked Sergeant D via comms 92 (Q-1817c). Signal source checked: ti.signal = clamp(pidRaw) - curve (SATcontrol.ino:7593-7596), the PID request before the PWM duty and ignition override, as entry 316 says. AUTO satsource resolves to gas (satGetEffectiveHeatingSource, SATcontrol.ino:309-314), so an unconfigured hybrid gets the gas rule: to be named in the ADR.

ADR-231 Proposed (docs/adr, local commit on lane-1817): the PWM tick rule for the self-tune; amends ADR-205 decision 2 and ADR-228 decision 1 (relate written on both sides via adr-kit); Q-1817c listed as the open question, decision 2's flame_off/min_mod skip written as our interpretation. tests/test_adr_governance.py passes (README category entry added). Acceptance by Robert after Sergeant D answers Q-1817c; no code before.

ADR-231 Accepted 2026-11-12 (adr accept --confirm): Robert DM msg 1570402646186139730 (11:39:27 UTC) "A", author.id 384411356616720384, message_reference 1570337513208414273 (the accept question), checked first-hand via Discord REST in d3. Clarity gate fixed first (SAT, PWM, PID, AUTO expanded; Status line split so accept does not cut the sentence).

Implemented (lane-1817, uncommitted until the build number is given): SATcurveTune.h SatTuneTickIn.pwmGas; satTuneTickReason(): 'if (in.pwm && !in.pwmGas) return SATT_PWM', flame_off and min_mod skipped for a PWM tick on a gas boiler; satTuneAddTick(): such a tick goes to SATP_WARMUP whatever its error; glue satCurveTuneNoteTick() sets pwmGas = (satGetEffectiveHeatingSource() == SAT_SRC_GAS_BOILER), so AUTO = gas (entry 323). Heat pump and hybrid unchanged (reason=pwm, pwmTicks kept).

Host evidence 2026-11-12: tests/test_sat_curve_selftune.py 82/82 OK on the fix (new class PwmGasRule, 5 tests; SourceChecks pins satGetEffectiveHeatingSource AUTO->gas and the glue line). Old-vs-fix: OTGW_SAT_INO/OTGW_SAT_HDR = HEAD (ec5b70d31) copies, PwmGasRule 5 run, 5 FAIL: gas and AUTO PWM hour ('0','pwm','0','3','na','2.00') != ('1','none','3','0','2.00','na'); no_room and coeff_changed hidden behind pwm; 3 PWM days (2.0,'hold','0','0','0') != (2.1,'raise','216','0','0'). Heat-pump/hybrid subtests pass on both (unchanged behaviour). Existing gas-PWM tests moved to src:3 (hybrid), generic pwm exclusion runs on a hybrid (EXCL_PRE); ExclusionMutation updated and green. evaluate --quick: 0 failed. docs/sat/sat-log-format.md SATTUNEH row names the rule.

AC3: ADR-231 (adr-kit) amends ADR-205 d2 and ADR-228 d1, relate written both sides; Accepted 2026-11-12 on Robert's DM 'A' (msg 1570402646186139730, reply to 1570337513208414273).

Full scripts/run_tests.py on lane-1817 (2026-11-12, -j 4, 1037 s): 221 files, 217 passed, 4 failed. Claude Code stopped the shell for low system memory, and the 4 failed files do not touch the tuner code. Rerun one at a time in the foreground, with the change in place: test_ota_flash rc=0 OK; test_refresh_storm rc=0 OK (skipped=2); test_stream_real_cbuf rc=0 'PASS: green with the presize, red without it'; test_dhw_water_meter rc=0 RESULT: PASS. So the 4 failures were artifacts of the memory reaper.

Bench by otgw-firmware-09 on .61, beta.216+0a10492 (2026-11-12): OTA app + fs, settings 164 keys changed none, githash 0a10492, bootcount 479 -> 481 then stable. otdmode 3 + sim never reached PWM ('PWM refused (path 1): no valid MsgID 3 reply'), so the run used otdmode 4 (loopback, MsgID 3 Read-Ack), satsource 1, target 19.8, interval 10, coefficient 0.8, lat 70, tune_hour 60, plus a sim solar_gain 0.15. PWM 14:54:26-14:57:26. SATTUNEH hour=17 mean=-5.42 ticks=6 counted=1 reason=none wticks=6 fticks=0, and hour=18 mean=-6.85 counted=1 reason=none wticks=6 fticks=0; both hours 6 of 6 SATTICK mode=2. Mixed hours 16 and 19 drop on min_mod (ADR-231 Negative). Flame, modulation and minMod come from the sim model (SATcontrol.ino:2721, :2727, :7605). Not benched: flame=0 ticks inside a counted PWM hour (the sim stayed lit), and the satsource=2 control (PWM did not return after the switch); both are host-proven (PwmGasRule). Health: bootcount 481 stable over 12 polls, heap low 60068 B, no new crashlog. Settings restored, diff 0. Side finding: a sim run wrote satthermalcoeff 0.0966 -> 0.0998, filed as TASK-1869. Pushed origin/dev f0dd92d5e..0a1049215; zip posted to #alpha-testing, msg 1570447126356430879 (covers beta.215 and beta.216).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
ADR-231 (Accepted by Robert, DM 1570402646186139730): on a gas boiler or AUTO a SAT PWM tick is a warm-up tick, and the pwm, flame_off and min_mod exclusions do not drop its hour; heat pumps and hybrids keep reason=pwm. Shipped in 2.0.0-beta.216 (0a1049215). AC1: entries 316, 318, 319, 322, 323 recorded. AC2: tests/test_sat_curve_selftune.py 82/82 OK on the fix; PwmGasRule 5 FAIL on the old code (a gas/AUTO PWM hour gives reason=pwm, 3 PWM days give no raise). The heat-pump and hybrid subtests pass on both, by design: they check the unchanged rule. Full run_tests 217/221; the 4 failures were artifacts of the memory reaper and passed when rerun alone. AC3: ADR-231 via adr-kit, amends ADR-205 d2 and ADR-228 d1. AC4: 09 bench .61: two full PWM hours (6 of 6 mode=2) counted=1 reason=none fticks=0; build exit 0, evaluate --quick 0 failed. Not benched: flame=0 inside a counted PWM hour and the heat-pump control (host-proven only). The PWM came from the sim model with loopback for MsgID 3, not from a real boiler.
<!-- SECTION:FINAL_SUMMARY:END -->
