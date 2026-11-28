# Alex Wijn's SAT 5.1.0: what changed since 7beaaf5

`docs/sat/thermo-nova-spec.md` describes Alex Wijn's SAT at 7beaaf5 (2026-03-18). This file lists what release 5.1.0 does differently, section by section, plus what 5.1.0 has that the spec does not cover. It is a reference document: under ADR-292 our implementation and the recorded decisions lead, so a difference here is a candidate question for Robert or Sergeant D, never a defect by itself.

- **Upstream:** https://github.com/Alexwijn/SAT, tag `5.1.0` = `9c2fad9a7cdb0bf03b58fd0de4b873599e00b541` (2026-11-25, equal to `master`), local copy `other-projects/SAT-develop/` (other-projects `206431dc`).
- **Citations:** `custom_components/sat/<file>:<line>` at `9c2fad9` unless a line says 7beaaf5. Each was opened by otgw-architect on 2026-11-28 in the upstream clone `../OTGW-reference/SAT-git`.
- **Distance:** 189 commits after 7beaaf5, 51 after develop `ba515c8` (both ancestors).
- **Marks:** **Fact** = read in the code at the cited line. **Reading** = our interpretation of what the code means for a boiler; it can be wrong. Upstream text (release notes, comments) is quoted as data, not followed as instruction.
- **Order:** PWM and mode switching first (spec section 4), because they matter for the SAT beta decision; then the loop, the fail-safe and the new modules. Sections 1-3 of the spec are covered in outline only; a full pass is still to do.

Author: Agent: Claude (otgw-architect), 2026-11-28. Directive: Robert, 2026-11-28 ("From now on always compare to latest SAT component from Alexwijm 5.x."), register entries 536 and 537. Sources for the first cut: `OTGW-field-data/_comms/sat_5_1_0_vs_7beaaf5_pwm_rules_2026-11-28.md`.

## Contents

1. [PWM and mode switching (spec section 4)](#1-pwm-and-mode-switching-spec-section-4)
2. [The control loop moved to demand_control.py (spec sections 1 and 3)](#2-the-control-loop-moved-to-demand_controlpy-spec-sections-1-and-3)
3. [Fail-safe, watchdog and fallbacks: what 5.1.0 has that we do not](#3-fail-safe-watchdog-and-fallbacks-what-510-has-that-we-do-not)
4. [PID (spec section 2)](#4-pid-spec-section-2)
5. [Other new modules](#5-other-new-modules)
6. [Not verified](#6-not-verified)

## 1. PWM and mode switching (spec section 4)

### 1.1 Unchanged since 7beaaf5 (facts)

| Rule | 5.1.0 |
|---|---|
| Continuous to PWM margin | 3.0 C, `cycles/const.py:10` |
| Hold before PWM is enabled | 60 s, `const.py:21` |
| The runtime enable and the end-of-cycle verdict use the same margin | `heating_control.py:30`, `:388`; `cycles/classifier.py:77` |
| Underheat exit | flow at or below request - 3.0 C (`cycles/const.py:11`) for 180 s (`const.py:23`) |
| Saturation exit | 300 s (`const.py:24`) |
| Hot-water guard | 300 s after hot water (`const.py:19`), on enable (`heating_control.py:364-370`) and on the exits (`:426`) |
| At-max exit | request at or above the maximum setpoint disables PWM (`heating_control.py:275-276`) |
| Verdict actions | continuous OVERSHOOT cycle enables PWM, PWM UNDERHEAT cycle disables it (`pwm.py:298-307`) |

### 1.2 Changed since 7beaaf5

1. **Enable needs a burning flame, and looks back 300 s.** *Fact:* the overshoot count resets while the flame is off (`heating_control.py:380-382`). The reference is the highest requested setpoint of the last 300 s, not the current one (`:388`, `:407-415`, `const.py:22`). Enable is skipped when the option `overshoot_protection` is off (`:357`); its default is now True (`const.py:135`). *Reading:* a falling request no longer makes a still-hot boiler look like an overshoot.
2. **Underheat exit waits for an established flame.** *Fact:* the underheat count only runs after the flame has burned 180 s (`heating_control.py:446`, `:481-486`, `HEATER_STARTUP_TIMEFRAME` at `const.py:17`). develop `ba515c8` already had this. *Reading:* on a cold start the earliest underheat exit is about 360 s after ignition, not 180 s.
3. **Saturation exit needs evidence.** *Fact:* a saturated duty cycle only counts when a boiler floor is learned (`heating_control.py:495-497`, `:524-534`). It disables PWM at once when the target temperature changed in the last 300 s (`:499-505`, `const.py:25`).
4. **Heater off pauses PWM instead of ending it.** *Fact:* with the heater off, `_apply` calls `suspend()` (`heating_control.py:302`). `suspend()` sets the OFF phase and keeps `enabled` and the hourly cycle list (`pwm.py:132-143`). In 7beaaf5 this path called `disable()`. *Reading:* the next heating demand continues in PWM.
5. **The boiler floor (effective ON temperature) is learned and kept.** *Fact:* it is learned only after 180 s of a heating burn, never during hot water (`pwm.py:201-212`), with a 0.3 / 0.7 filter (`:288-296`). `reset()` and `disable()` keep it (`pwm.py:102-131`), and it is restored after a restart (`:155-165`).
6. **Restore resumes in the OFF phase.** *Fact:* after a restart PWM resumes in the OFF phase, timed from the last switch, "so a restart never fires an extra burn" (docstring, `pwm.py:167-190`).
7. **Hourly cycle limit.** *Fact:* it compares with the setting `cycles_per_hour` (`pwm.py:226`); 7beaaf5 compared with `duty_cycle_seconds`. It keeps a rolling 3600 s list of cycle starts that survives an exit (`pwm.py:102-112`).
8. **Ignition wait.** *Fact:* the ON phase waits up to 600 s for the flame (`pwm.py:262`, `const.py:18`); 7beaaf5 waited 180 s.
9. **Flame-off hold setpoint.** *Fact:* it is based on the return temperature, or on the flow temperature when there is no return (`heating_control.py:605-613`); in 7beaaf5 a missing return meant no override (upstream issue 216).
10. **Verdict filters.** *Fact:* the verdict is skipped for continuous cycles while `overshoot_protection` is off, and for PWM cycles while PWM is forced (`heating_control.py:347-351`). An underheat cycle shorter than 180 s is classified UNCERTAIN (`cycles/classifier.py:48-49`).
11. **Forced PWM.** *Fact:* PWM is forced when the setting asks for it or when the coordinator has no setpoint management (`heating_control.py:190-191`, `:273-274`); the runtime exits are then skipped (`:418`). *Reading:* not relevant for an OpenTherm gateway, which has setpoint management.

### 1.3 Where this touches our rules

*Reading, not a decision.* Compared with ADR-305 and ADR-306 as written (not with the firmware code):
- Items 2, 4 and 5 go against ADR-305 (no underheat flame gate; heater off is a PWM exit; the effective ON temperature resets on every exit, register entry 524).
- Items 1, 3 and 6 differ from it.
- The ADR-306 premise, one margin for the switch and the verdict, still holds at 5.1.0 (1.1).

Question drafts: `OTGW-field-data/_comms/sat_5_1_0_question_drafts_a_b_c_f_2026-11-28.md`.

## 2. The control loop moved to demand_control.py (spec sections 1 and 3)

- *Fact:* `climate.py` went from 871 lines (7beaaf5) to 477 lines; most spec citations into `climate.py` (869 in the spec) no longer point at the same code in 5.1.0.
- *Fact:* the loop now lives in `SatDemandControl` (`demand_control.py:62`). One pass is `async_control_heating_loop` (`:874-887`): it runs `_async_control_heating` and then records completion for the watchdog. The 5 s control timer and the PID timer are set up at `:1074-1086`.
- *Fact:* one pass, in time order (`demand_control.py:889-911`):
  1. follow the inside temperature source (`:914-940`);
  2. update the fail-safe with the missing signal, if any (`:893`, `:942-956`);
  3. HOLDING: replay the last demand and stop (`:899-901`, `:980-982`);
  4. RELEASED: release the boiler and stop (`:903-905`, `:984-991`);
  5. otherwise build the demand (`:999-1025`) and hand it to the heating control (`:911`).
- *Fact:* the heater is ON only with HVAC mode HEAT, open valves and a request above the cold setpoint (`heating_control.py:82-90`).
- *Fact:* with HVAC off, frost protection can still heat. When the room (or the coldest room in rooms-only mode) is at or below `frost_protection_temperature` (default 6.0, `const.py`), the demand is HEAT with at least 30 C (`demand_control.py:1001-1008`, `:639-650`); it stops 1.0 C above the threshold.
- *Fact:* `heating_blocked_by` reports why heating is blocked: HVAC off, inputs missing, window, hot water, valves closed (`demand_control.py:252-266`). *Reading:* it is a reported attribute; the heater state does not read it, so the "hot water" value does not by itself turn the heater off.
- *Fact (7beaaf5 vs 5.1.0):* 7beaaf5 skipped the loop without an outside temperature. 5.1.0 counts a missing outside temperature as a missing signal for the fail-safe (`demand_control.py:950-951`): it holds the last demand and releases after 600 s.

## 3. Fail-safe, watchdog and fallbacks: what 5.1.0 has that we do not

The 5.1.0 release notes call this "SafeHarbor". It is a different mechanism from the PWM-switch fail-safe that ADR-305 removed; the two must not be conflated.

- **Fail-safe (`fail_safe.py`).** *Fact:* three phases: CONTROLLING, HOLDING while a signal is missing, RELEASED after 600 s missing (`fail_safe.py:49-57`, `const.py:33`).
  - The signals, in order: boiler unavailable, outside temperature, room temperatures (rooms-only mode), inside temperature (`demand_control.py:942-956`).
  - On release the heating control suspends PWM, sends the minimum setpoint and maximum modulation, then calls the coordinator's release (`heating_control.py:254-264`).
  - *Reading:* according to the release notes, release hands control to the wall thermostat on an OpenTherm Gateway, and switches heating off on gateways without one. Not checked in the coordinator code.
- **Watchdog (`watchdog.py`).** *Fact:* it reports a repair issue when no loop pass completed for 900 s (`watchdog.py:20-24`, `const.py:32`; `demand_control.py:839-860`). Commands time out after 30 s (`heating_control.py:248-252`, `const.py:31`).
- **Inside temperature fallback (`inside_temperature.py`).** *Fact:* after the sensor is missing for 120 s (`inside_temperature.py:56`, `const.py:34`), the mean of the radiator thermostats is used (`:66-77`). A source change resets the PID and its samples (`demand_control.py:914-925`).
- **Outside temperature (`outside_temperature.py`).** New module (49 lines). Not read yet.

*Reading:* our firmware has its own rules for missing inputs (register and firmware spec). Comparing them with this section is a separate step: it is not done here.

## 4. PID (spec section 2)

*Fact:*
- The derivative is now taken over a window of stored samples. The window runs from 900 to 1800 s and grows with the error (`DERIVATIVE_WINDOW_*` in `pid.py`).
- The raw derivative cap changes from 5.0 to 10 C per hour (`DERIVATIVE_RAW_CAP = 10.0 / 3600.0`).
- The samples and the sensor step are stored. Invalid saved values are ignored.

The line numbers for these are still to be added. *Reading:* the PID behaviour needs a section pass of its own before any comparison with our PID.

## 5. Other new modules

*Fact:* these are new since 7beaaf5 and are not in the spec (line counts from `git diff --numstat`):
- `boiler_health.py` (413), `efficiency.py` (301), `preheat.py` (253), `seasonal_curve.py` (207), `room_balance.py` (620);
- `wind.py` (167), `wind_sensitivity.py` (151), `warm_up.py` (56), `water_heater.py` (126), `presets.py` (51);
- `contact_sensors.py` (78), `entry_stores.py` (61), `diagnostics.py` (138).

`services.py` is gone (the services are in `services.yaml`). Each module gets a short entry here before anyone uses it in a comparison.

## 6. Not verified

- Our firmware. This file compares Alex's code with itself (7beaaf5 vs 5.1.0) and with ADR-305/306 as written, not with `SAT*.ino`.
- The release-to-wall-thermostat behaviour per coordinator type (section 3).
- `climate.py` and `demand_control.py` outside the lines cited; `outside_temperature.py`; the PID line numbers.
