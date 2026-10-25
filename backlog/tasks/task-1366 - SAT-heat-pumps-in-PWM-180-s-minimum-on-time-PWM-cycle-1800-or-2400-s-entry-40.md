---
id: TASK-1366
title: >-
  SAT heat pumps in PWM: 180 s minimum on time, PWM cycle 1800 or 2400 s (entry
  40)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-24T21:42:00Z'
updated_date: '2026-10-25T07:08:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 501000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-24 (Q8, entry 40): heat pump minimum ON time 180 s; PWM cycle length 30 min (1800 s) or 40 min (2400 s). Fixes the duty-mapper collapse (audit #9) where 1800/2400 were used as min-on.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Heat pump PWM uses min ON 180 s and a 1800/2400 s cycle; duty mapper no longer collapses (host test red/green)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
TASK-1366 plan, corrected by a second reader (Sergeant D entry 40). Plan only: no file was edited.

Every location below was opened in this session. thermo-nova paths are relative to other-projects/SAT-releases-thermo-nova/custom_components/sat at the pin 7beaaf5. The local snapshot is the other-projects submodule, whose HEAD is c5211af ("Record the upstream commit the thermo-nova snapshot mirrors (7beaaf5)"). c5211af is not the firmware repo HEAD, which is 19fa9e64e; the original plan mixed the two up.

## Verdict of the second read

The core reading of entry 40 holds and matches the code: 180 s minimum ON time, and 1800/2400 s as the PWM period.

The following were wrong or missing, and are corrected below:
- (R1) Test case A's HEAD expectation is wrong (float rounding).
- (R2) The existing harness will stop compiling.
- (R3) The ON-phase timeline does not match the firmware.
- (R4) The cycle classifier and saturation exit consume the mapped times; the plan did not name them.
- (R5) The heat-pump gate at :5190 also skips the PWM exits.
- (R6) settingsHelp.h:78 and most doc lines were missing.
- (R7) The hourly-cap interaction with Q12 / audit #18 was missing.
- (R8) The mapper lives in satPwmSchedule(), not in satApplyPWM().

## 1. Ground truth: thermo-nova (re-verified)

**Construction** (once, when PWM is created):
- pwm.py:53: lower = 180 for every system.
- pwm.py:54: period = 3600 / max(1, cycles_per_hour).
- pwm.py:55: maximum = 2 × period.
- pwm.py:58-61: duty lower = 180/period, upper = 1 − lower, min = lower/2, max = 1 − min.

**On each update()** (pwm.py:117), in this order:
1. Without a flow temperature, the status goes IDLE (pwm.py:122-128).
2. The effective temperature is seeded (pwm.py:130-131).
3. `_calculate_duty_cycle` runs (pwm.py:134, mapper at 229-301). Duty is clamped to 0..1 (pwm.py:239).
   - Below min: (180, 2P − 180) with flame on and no hot water, else (0, 2P) (pwm.py:247-260).
   - Up to lower: (180, 180/d − 180) (pwm.py:263-272).
   - Up to upper: (P·d, P·(1 − d)) (pwm.py:274-283).
   - Up to max: (180/(1 − d) − 180, 180) (pwm.py:285-294).
   - Above max: (2P, 0) (pwm.py:296-301).
4. The rolling-hour counter resets after 3600 s (pwm.py:138-141).
5. OFF/IDLE → ON only when on ≥ HEATER_STARTUP_TIMEFRAME (180, const.py:18) and the off time has passed (pwm.py:155).
   - The cap compares the start count with duty_cycle_seconds (pwm.py:156; default 780, entry_data.py:220, const.py:167), so in practice it never holds.
6. ON phase:
   - Wait for the flame for up to 180 s, measured from the phase start (pwm.py:186-199). A flame that lights re-anchors the phase start (`_last_update`).
   - 180 s minimum flame-on hold (pwm.py:201-203).
   - ON ends when elapsed since the phase start ≥ on (pwm.py:205-215).

**Heat-pump options:**
- 2 ("Normal") or 3 ("High") per hour (config_flow.py:686-693).
- The migration sets heat pumps to 2 (__init__.py:172-173). The general default is 4 (const.py:119).
- cycles_per_hour is an int (entry_data.py:55, :219). A 1.5 per hour option: not found in the code.

**Gas oracle:** opened in this session at tests/test_pwm.py (parametrize block around lines 108-133, flow 50, base 27.2, 4 per hour):
- 0.05 → (0, 1800) without flame, (180, 1620) with flame
- 0.15 → (180, 1020)
- 0.50 → (450, 450) ±1
- 0.85 → (1020, 180) ±1
- 0.95 → (1800, 0)

## 2. Firmware today (src/OTGW-firmware)

**Getters:**
- `satGetMaxCyclesPerHour()` (SATcontrol.ino:346-356): 2 for a heat-pump source. Otherwise iCyclesPerHour when it is 2-6, else 3 (underfloor) or 4.
- `satGetMinOnTimeSec()` (SATcontrol.ino:359-365): settings.sat.iHpCycleSeconds for a heat pump, 180 otherwise. Hybrids get 180.

**The mapper and state machine are in `satPwmSchedule()` (SATcontrol.ino:835-997)**, called from `satApplyPWM()` (:1017-1020). satApplyPWM is called from the control loop at :5170 on the 30 s tick (iControlInterval default 30, SATtypes.h:467). Per call:
1. Duty (:846-864).
2. `minOnMs` from the getter (:867); `upperMs` = 3600000/cycles (:869); `maxMs` = 2·upper (:870).
3. Thresholds (:897-900).
4. Mapper (:905-937). Range 5 (duty ≥ dutyMax) returns at :910 before the state machine runs (audit #56).
5. The on/off times are recorded at :938-939.
6. ON phase: ends when `sinceFlameStart >= onTimeMs && >= minOnMs` (:945).
   - `sinceFlameStart` is measured from `satCycleGetFlameOnStartMs()` (:942). That is the cycle tracker's last real flame-on edge (SATcycles.ino:722, :1045), not the start of the ON phase.
   - The 180 s ignition-timeout block (:954-963) runs only when :945 did not fire.
7. OFF phase: start ON when `sinceFlameOff >= offTimeMs` (:967-968) and the hourly cap is not reached (:971). There is no `on >= 180` start gate, unlike pwm.py:155.

**Effect of the current values for a heat pump:**
- At 1800 s: dutyLower = 1.0, dutyMin = dutyMax = 0.5. Ranges 2-4 cannot be reached.
  - Duty clearly above 0.5: continuous (on = maxMs = 3600 s).
  - Below 0.5: ultra-low, 1800 on / 1800 off with flame, 0 / 3600 without.
- At 2400 s: dutyMax = 0.333.
  - Above that: continuous.
  - Below that: ultra-low, 2400 on / 1200 off with flame (maxMs 3600 − 2400), 0 / 3600 without.

**Consumers of the mapped times (missed by the original plan):**
- SATcycles.ino:799 feeds `satPwmLastOnTimeMs()` into `_cycleClassify`. The PWM SHORT threshold is max(floor, 0.8 × onTime) (SATcycles.ino:666-676).
  - Today a heat-pump PWM burn is judged against 1440 s or 2880 s, so nearly every burn is classified SHORT.
  - After the fix the threshold follows the real on time, for example 432 s at on = 540 s.
- SATcycles.ino:1016-1026: the saturation exit (off == 0 sustained → continuous). It now fires only above duty 0.95 / 0.9625 instead of about 0.5 / 0.33.

**Setting:**
- `iHpCycleSeconds`, default 1800, comment "Heat-pump min-on / cycle rate" (SATtypes.h:463).
- Setter constrains to 1800-2400 (settingStuff.ino:999); persisted at settingStuff.ino:321.
- REST GET field `hp_cycle_seconds` (restAPI.ino:2272); settings descriptor range 1800-2400 (restAPI.ino:3879); known-key list (restAPI.ino:4216).
- Help text "Minimum heat pump on-time in seconds, 1800 to 2400" (settingsHelp.h:78). This was missed.
- UI label "Heat-pump min cycle" (data/v2.js:2016, listed at :2164).
- Debug print (handleDebug.ino:91).

**Hourly cap:**
- `satCycleIsHourLimitReached()` (SATcycles.ino:293-299) uses satGetMaxCyclesPerHour(), which is 2 for a heat pump.
- It counts every real flame-on in the last 3600 s, hot-water ignitions and relights included (audit #18, Q12 still open).

**How a heat pump enters and leaves PWM:**
- The control loop calls `satCycleCheckAutoSwitch` only when `!satAlwaysMaxModulation()` (SATcontrol.ino:5190; true for a heat pump, :368-372). This skips both:
  - the entry, sustained overshoot → PWM (SATcycles.ino:986);
  - the exits, underheat and saturation → continuous (SATcycles.ino:999-1026).
- In committed code the other way into PWM is the manual control_mode "pwm" command (SATcontrol.ino:2464-2466).
- A heat pump in PWM then leaves it only through `satPwmDisableOnHeaterOff` (SATcontrol.ino:1101-1110), on heater off or cold cutoff.
- TASK-1331 (on_cycle_end port, In Progress, not in this tree) adds an end-of-cycle path into PWM. Audit #9 (docs/sat/sat-port-audit-2026-10-24.md:139, :409-425) notes that path has no heat-pump gate.

**Boards:** lines 345-997 contain no `#if` or `HAS_*` (grep count 0). The getters, mapper and state machine run identically on PIC (Classic) and OT-Direct (OTGW32). The only board difference in this area already exists: on PIC boards satPicCSKeepalive re-sends SAT's CS every 30 s during the long OFF phase (CLAUDE.md architecture rules).

## 3. Change (minimal; identifier `iHpCycleSeconds` kept, docs fixed)

**a. SATcontrol.ino:**
- Add `static uint32_t satGetPwmPeriodSec()` next to the getters:
  - heat pump: `(settings.sat.iHpCycleSeconds == 2400) ? 2400 : 1800`
  - otherwise: `3600 / satGetMaxCyclesPerHour()`
- `satGetMinOnTimeSec()` returns 180 for every source (pwm.py:53).
- In `satPwmSchedule()`: `upperMs = satGetPwmPeriodSec() * 1000UL`, `maxMs = 2 * upperMs` (pwm.py:54-55).
- Fix the comments at :348-349, :361-362 and :867-869.
- `satGetMaxCyclesPerHour()` stays 2 for a heat pump. It now only means the hourly start cap.
- Result:
  - 1800 s: lower 0.10, upper 0.90, min 0.05, max 0.95.
  - 2400 s: lower 0.075, upper 0.925, min 0.0375, max 0.9625.
  - All five ranges are reachable.

**b. settingStuff.ino:999:**
- Snap the value: `atoi(newValue) == 2400 ? 2400 : 1800` (entry 30: just two options).
- Derived, not traced end to end: REST POST `sathpcycle` reaches the same setter through updateSetting, where the comparison is case-insensitive (strcasecmp_P).
- Keep restAPI.ino:3879 at 1800-2400 and rewrite its comment as "PWM cycle length".

**c. Wording (firmware data, so the prerelease bump applies):**
- SATtypes.h:463 comment: "Heat-pump PWM cycle length (period): 1800 s = 2/h (default) or 2400 s = 1.5/h (entry 40)".
- settingsHelp.h:78: "Heat-pump PWM cycle length in seconds: 1800 (2 per hour, default) or 2400 (1.5 per hour)."
- v2.js:2016:
  - label: "Heat-pump PWM cycle"
  - hint: "1800 s = 2 per hour (default), 2400 s = 1.5 per hour"
  - This is a LittleFS asset, so run build.bat (firmware + fs).

**d. Docs:**
- docs/sat/firmware-sat-spec.md:1605, :1723 and :1799; :1502 only lists the input and can stay.
- docs/sat/sat-comparison.md:479. Its "Keep and document as a firmware addition" flips: the minimum ON time now matches pwm.py:53, and the period options differ (C4).
- docs/c4/c4-code-sat.md:94 and :264; docs/c4/c4-code-settings.md:410.
- Audit rows docs/sat/sat-port-audit-2026-10-24.md:139 and :409-425: mark #9 fixed for the mapper.
- No ADR: a grep of docs/adr for iHpCycleSeconds / hp_cycle / heat-pump min-on found nothing.

**e. Existing harness:**
- tests/test_sat_pwm_override.py cuts from `static float    _pwm_effectiveBoilerTemp` to `// Last PWM off-phase duration in ms` (:304-305) and stubs the getters (:80-81).
- The new `satGetPwmPeriodSec()` sits outside that cut, so the harness will not compile until a stub is added: `static uint32_t satGetPwmPeriodSec() { return 900; }`, which keeps its 4 per hour behaviour.

**f. Commit:**
- `bin/bump-prerelease.sh`, then one commit referencing TASK-1366.

## 4. Timeline after the fix (heat pump, 1800 s, in PWM, 30 s control tick)

1. Each tick: satApplyPWM → satPwmSchedule computes the duty (setpoint − base)/(BT − base) (:846-864).
2. The mapper turns that into on/off times with the thresholds above, for example duty 0.30 → 540 s ON / 1260 s OFF.
3. OFF phase:
   - CS 10 / CH 0 per entries 5/9/44.
   - Once `sinceFlameOff >= offTime` and fewer than 2 flame-ons in the last 3600 s, it sets ON (:968-994).
   - Flame already on: the start-up window anchors now. Flame off: the ignition wait starts (:991-993).
4. ON phase, pre-existing defect (R3), unchanged by this task:
   - On the next tick, :945 compares now with the previous cycle's flame-on edge. That edge is at least on + off old, so the ON phase ends at once (about 30 s) unless the unit lit in between.
   - The 180 s ignition wait (:954) never holds.
   - thermo-nova measures from the phase start and waits for the flame first (pwm.py:186-199).
   - A heat pump's compressor start takes minutes, so this cuts most heat-pump ON phases short both before and after this fix. Out of scope here; see open points.
5. ON phase when the flame lit within the tick: it ends at `sinceFlameStart >= max(onTime, 180 s)`.
6. Duty above 0.95: range 5 returns before the state machine (audit #56). The firmware keeps the flame requested with no expiry.

Unchanged and consistent:
- Entry 43: the first phase is ON.
- Entry 44: CS/MM/CH every update.
- MM rule (satComputeMM, SATcontrol.ino:379-388): PWM heat pump = user max, never 0 (entry 21).

## 5. Verification (red on HEAD, green on the fix)

**Setup:** new tests/test_sat_hp_pwm.py, in the style of test_sat_pwm_override.py.
- Cut the real getter block, SATcontrol.ino:345-366 (`satGetMaxCyclesPerHour` up to `satAlwaysMaxModulation`, which will include `satGetPwmPeriodSec`), plus the real PWM block.
- Stub only:
  - satGetEffectiveHeatingSource/System
  - satGetBaseOffset = 27.2
  - flame, flow, millis, the flame-on/off anchors, hour limit and bDhwActive
  - settings iHpCycleSeconds and iCyclesPerHour (set 4 for gas; the default is 3)
- Read satPwmLastOnTimeMs/OffTimeMs and bPwmFlameRequested.
- Old vs fix: `git show HEAD:src/OTGW-firmware/SATcontrol.ino > old.ino`, run with `OTGW_SAT_INO=old.ino`, and keep both outputs.

Duty vectors are checked in float32. pid 38.6 gives duty 0.4999999, which is below 0.5, so the original case A expectation was wrong. Use a ±1 s tolerance everywhere (millisecond float truncation).

Flow 50, base 27.2, duty = (pid − 27.2)/22.8:

| Case | HEAD (red) | Fix (green) |
|---|---|---|
| A. HP 1800, duty 0.60 (pid 40.88) | continuous, on 3600 / off 0 | 1080 / 720 |
| A2. HP 1800, pid 38.6 (duty 0.4999999), flame on | ultra-low, 1800 / 1800 | 899-900 / 900 |
| B. HP 1800, duty 0.30 (pid 34.04), flame on | 1800 / 1800 | 540 / 1260 |
| C. HP 2400, duty 0.50 | continuous, on 3600 / off 0 | 1200 / 1200 |
| D. HP 2400, duty 0.07, flame on | ultra-low, 2400 / 1200 | range 2, 180 / 2391 |
| E. HP 1800, duty 0.30, ON phase, flame lit 600 s ago | stays ON (min-on 1800) | switches OFF |
| F. Setter given 2100 | stored 2100 | stored 1800 (source-level check if the setter cannot be compiled on the host) |

**Gas regression at 4 per hour:** the thermo-nova oracle in §1 must be green on both HEAD and the fix.

**Also run:**
- `python tests/test_sat_pwm_override.py`, with the new stub; green on both.
- `python evaluate.py --quick`
- `build.bat` (combo, firmware + fs).

**Bench (optional):**
- Heat-pump source, control_mode=pwm, scripts/sat_boiler_emulator.py.
- Read the telnet PWM on/off seconds.
- Expect step 4's early ON end unless the emulator lights within one tick.

## 6. Sergeant D entries

**Relies on:**
- Entry 40 (primary; our reading still needs his yes).
- Entry 30 (two options, default 2 per hour = 1800 s, the user decides).
- Entry 31 (180 s minimum flame time; ranges come from cycles per hour).
- Entry 8 (the duty cycle lives in pwm.py).

**Consistent and untouched:**
- Entry 21: heat pump MM never 0.
- Entries 5/9/17: CH/CS in OFF.
- Entry 43: ON first.
- Entry 44: every update.
- Entries 15/18: TASK-1331, not in this tree.

**Conflicts reported, not resolved here:**
- C4: his 2 / 1.5 per hour against thermo-nova's 2 / 3. The plan follows him; the question is in georgeSummary.
- C3: heat-pump gate at SATcontrol.ino:5190 (entry and exits) and the 40 °C cap at :308. His entries 21/30/40 do not cover either.
- C6: wording only.
- Entry 30's "Interpretation (ours)" ("long minimum on time") is superseded by entry 40's reading, as entry 40 already records.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-25: plan (second-read) stored; waiting for Sergeant D's OK on the open question (open-questions Q29/Q30/Q31) before implementation, per CLAUDE.md SAT rules.

2026-10-25 06:36 UTC Sergeant D answered Q31 (entry 50): option A - min ON 180 s, cycle 1800 s default / 2400 s. Plan confirmed; ready to implement.
<!-- SECTION:NOTES:END -->
