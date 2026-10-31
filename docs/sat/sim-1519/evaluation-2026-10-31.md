# TASK-1519 evaluation: simulations, current SAT and the OpenTherm low-load specification

Date: 2026-10-31. Requested by Robert: a critical evaluation of the simulation results and the current SAT, against the
OpenTherm Application Functional Specification v1.1f (AFS, 22 December 2010), section 2.1.1 "Low-Load & Pump Control
Function" (pages 6-10), with improvements weighed against KISS and the AFS.

Sources: AFS v1.1f (local copy `OTGW-other-projects/OT-Spec/OT Application Functional Specification v1-1f.pdf`,
text extracted with pdftotext and read in this session); firmware `src/OTGW-firmware` at origin/dev 433ff886d;
thermo-nova at the pinned commit 7beaaf5 (`other-projects/SAT-releases-thermo-nova/custom_components/sat`);
simulation report `round5b-round7.md` (rounds 5-9) and rounds 10-11 below; Sergeant D entries 160-175.
Code facts were collected by a research agent and the load-bearing ones re-read in this session
(`SATcontrol.ino:453-462`, `:964-972`, `SATcycles.ino:1103-1116`, `OTGW-Core.ino:3168-3186`, thermo-nova
`heating_control.py:439-448`, `:515-519`).

Ranking rule (ADR-196): Sergeant D's recorded decisions, then thermo-nova, then specifications, then the firmware.
The AFS therefore does not override SAT behaviour by itself; where the firmware or thermo-nova departs from it, the
departure is listed and put to Sergeant D.

## 1. What the simulations show, and how sure we are

| finding | strength | evidence |
|---|---|---|
| The curve integral (setpoint - curve) is the signal; the PID integral cannot lift a curve that is too low | strong, reproduced in rounds 1, 7 | integral reset outside 0.1 C (`pid.py:187-189`) |
| Hourly decisions chase the weather; a daily decision on several days does not | strong, rounds 1-2, 17,460 winters | 110-180 vs ~7 changes per winter, same comfort |
| A derivative term adds nothing to a multi-day mean | strong, round 7, and it follows from the arithmetic | D window mean <= 0.024 C vs 0.74-2.25 C |
| A cliff below the right coefficient, a plateau above it | strong in shape, two years and 20 winters | e.g. gas 1992-2012: 267 min/day at 0.8, 21-28 at 1.3-2.4 |
| The decided rule is safe | strong, 20 winters | worst pair +33 min/day, 1-4 % of pairs > 5 min/day worse |
| The tuner's value is the rescue from a curve far too low; near the right curve it does little | strong | from 1.0: gas -33 median (-415 in cold winters); from 1.8: ~0 |
| Excluding PWM and minimum-output hours is required, and hides the low-load case | strong, rounds 4, 5c-5g | worst pair without it +100; with it 0 clean hours in BENG at 1.0 |
| 3 days is steadier than 2 days | moderate, 20 winters | runs with >= 3 flips 88 -> 25 of 3,000 |
| A coefficient step may leave the PID terms alone (ADR-204) | strong, round 10 | +-5 min/day, 443 / 450 same end coefficient |
| Firmware overshoot margin 2.0 beats thermo-nova 3.0 for gas | **indicative only**: the margin acts through PWM entry, the unvalidated part of the model (section 2, point 1) | median -6 to -13 min/day in 75-85 % of gas pairs; hp30 neutral, hp60 +2-3 |
| Absolute coefficient values (1.8 etc.) | **not established** | the radiators were sized so that 1.8 is right (entry 166 anchor) |
| Burner / compressor starts, gas use | **indicative only** | flame fraction matches the field (0.21 vs 0.24), starts per hour do not (1.5 vs 2.5) |

## 2. Critical notes on the simulation itself

1. **The boiler's own behaviour is the weakest part.** The model has a minimum output, a flow loop and an assumed
   anti-cycling. It has no boiler minimum off-time (AFS 2.1.1.5), no modulating pump (AFS 2.1.1.4) and no high
   start modulation (AFS 2.1.2). Start counts are therefore not reliable, and the low-load numbers are a ranking,
   not a prediction.
2. **The absolute scale is an assumption.** The coefficient that fits a house depends on the radiators; the model
   cannot know them. All "1.8" results are relative to a house built to need 1.8.
3. **The sun filter differs from the decision.** The simulation used solar radiation < 50 W/m2; entry 160 decides
   sun elevation < 12 degrees. They overlap in winter but are not the same rule.
4. **Sensor noise and DHW were simplified.** The room sensor is noise-free (a real 0.1 C step sensor makes a slope
   noisy, which only strengthens the "no D" decision); hot water is two fixed 10-minute draws a day.
5. **One location.** 52.1N 6.2E. Coastal or southern locations have other sun and temperature patterns.
6. **One controller error was found and corrected during the study** (the round 5 reference, section 1 of
   `round5b-round7.md`), and one deviation from the firmware was tested afterwards (PID reset, round 10). Both are
   documented. Further deviations of this kind are possible; a field capture is the check.

## 3. Current SAT low-load behaviour against AFS 2.1.1

| AFS requirement | AFS text (section) | firmware | thermo-nova | assessment |
|---|---|---|---|---|
| Detect low load when the flame goes off while heat is requested, modulation <= 5 % (ID17), boiler temp > CS, not at ID57, not after DHW, not after start-up, not when ID3 HB bit 4 is set | 2.1.1.2.1 | PWM entry on a sustained overshoot: flow >= setpoint + 2.0 C for 60 s, continuous mode, not within 300 s of DHW (`SATcycles.ino:1103-1116`, `:36`). No flame-bit, ID17, ID15 or ID57 test | same rule with 3.0 C (`heating_control.py:262-305`) | **Different detection.** SAT reacts to the overshoot that comes before the burner switches off; the AFS reacts to the switch-off itself. SAT's rule is earlier, not wrong. The "after start-up" and "not at ID57" guards are missing in both (requested >= own max goes continuous, `SATcontrol.ino:6131`). |
| Capacity (on/off) control with on- and off-times from the room deviation | 2.1.1.2.2 | duty = (SP - base) / (boiler temp - base), 5 ranges (`SATcontrol.ino:1172-1264`), period 3600 / cycles per hour (3) | same formula (`pwm.py:229-301`), 4 cycles per hour | compliant in principle; the duty follows the PID setpoint, which follows the room deviation |
| OFF period: CH bit off, CS low | 2.1.1.2.3 | CS = 10, CH = 0 (`SATcontrol.ino:1361-1363`, `SATchEnable.h:50`) | same (`heating_control.py:388-390`) | compliant |
| ON period with ID14: MM = 0 %, CS = maximum | 2.1.1.2.6 | gas: MM = 0 (`SATcontrol.ino:453-462`); CS = flow - 1 K, refreshed every 3 s (`:964-972`, `:1383-1405`) | MM = 0 also for heat pumps (`:517-518`); CS = flow - 1 K (`:439-448`) | **Compliant in effect, stricter than needed.** CS = boiler temp - 1 K is the AFS method *without* ID14 (2.1.1.2.5); combined with MM = 0 it also works for boilers that ignore ID14. Keep. |
| Return to modulating: CS first, then MM back to 100 % | 2.1.1.2.7 | CS, MM, CH sent in that order in one tick (`SATcontrol.ino:6221-6225`) | same order (`heating_control.py:236-238`) | compliant in order; nothing waits for the CS to take effect, which the AFS does not require |
| Read ID3 HB bit 4 at start-up; if 1, the master must not run its own low-load control | 2.1.1.2.10 ("It's mandatory for the master to read ID3 at start-up and check bit 4"); polarity confirmed in the protocol spec v4.2 (`docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2.md:1610`: "Master low-off&pump control function [allowed, not allowed]", so 0 = allowed, 1 = not allowed) | bit 4 is read and published to MQTT only (`OTGW-Core.ino:3168-3186`); SAT does not use it. The topic name `supports_pump_control` reads as the opposite of the meaning: ON means the master is **not** allowed to run low-load control | read as a binary sensor only (`coordinator/serial/binary_sensor.py:54`) | **Non-compliant in both.** A boiler that runs its own low-load algorithm and SAT's PWM can fight each other (AFS: "could influence the working of the algorithm in the slave"). |
| Correct the cycle for the boiler's minimum off-time, measured from the flame flag | 2.1.1.5 | not found | not found | not implemented; the AFS describes it as an improvement ("can be solved by"), not a requirement |
| Keep the boiler on for at least one minute at start | 2.1.2 | 180 s minimum burn (`SATcontrol.ino:1286`) | 180 s (`pwm.py:201-203`) | compliant |
| Pump: CH bit off may stop the pump | 2.1.1.3 | CH = 0 in OFF phase | same | compliant; no pump-specific handling, none required |

## 4. Current settings against the simulation results

| setting | firmware | thermo-nova | simulation | assessment |
|---|---|---|---|---|
| Overshoot margin | 2.0 C (`SATtypes.h:487`) | 3.0 C | round 11: 2.0 better for gas (median -6 to -13 min/day), neutral hp30, +2-3 hp60 | keep 2.0 (indicative: boiler cycling not validated) |
| Cycles per hour | 3 (`SATtypes.h:551`) | 4 | not varied in this study | no evidence either way |
| Minimum on time | 180 s | 180 s | used in the model | consistent with AFS 2.1.2 |
| MM in PWM for heat pumps | max (not 0) | 0 | heat pumps were modelled at their minimum during ON | not tested; departs from thermo-nova (known firmware choice) |
| Heat-pump cycle | 1800 s (2400 s option) | 3600 / cph | hp60 makes ~27-33 starts per day in the model | **concern**: far from "low and long" (Robert 08:10 UTC); the curve tuner cannot fix it |
| Coefficient default | 2.0 | 2.0 | the plateau is above the right value, the cliff below | 2.0 is on the safe side for most houses (entry 166: 1.6-2.4) |
| Self-tune | removed (entry 72, TASK-1413) | none | rounds 1-11 | the ADR-205 design is safe, its gain is largest after a bad manual setting |

## 5. Improvements, weighed against KISS and the AFS

Effort estimates are rough and labelled as such. "AFS" says whether the change brings the firmware closer to the
specification. Every change to how SAT controls the boiler needs Sergeant D's OK first (ADR-196, CLAUDE.md SAT rules).

| # | improvement | AFS | KISS | evidence | recommendation |
|---|---|---|---|---|---|
| 1 | **Honour ID3 HB bit 4** (already planned as TASK-1412.4; Robert 2026-10-31, entry 179: build it after Sergeant D's OK): when the boiler reports bit 4 = 1 (master low-load not allowed), SAT does not enter PWM and stays in modulating control | closes the one mandatory gap (2.1.1.2.10) | very small: the bit is already decoded (`OTGW-Core.ino:3186`); one condition at the PWM entry | spec text in two sources; no simulation; **no field data yet** on how many boilers report 1 | **ask Sergeant D, with data**: first collect the bit from testers (MQTT topic `supports_pump_control` / `master_low_off_pump_control_function`). For every boiler that reports 1, this change switches SAT PWM off, a visible behaviour change. thermo-nova does not do it, so it is a firmware addition (ADR-196) |
| 2 | **Map the tested tuner exclusion onto firmware signals**: an hour does not count when SAT is in PWM, or the flame is off while CH is enabled, or ID17 (relative modulation) is at or below the boiler minimum from ID15 (5 % when ID15 is missing) | uses the AFS signals (2.1.1.2.1), but not its rule: the AFS detects low load when *all* its conditions hold; the tuner excludes an hour when *any* of these holds, which is the rule rounds 5b-5g tested ("PWM, or source off / at minimum") | small: data SAT already has | rounds 5b-5g; "flow > CS" is **not** included: it holds in many ordinary modulating hours and was never simulated | **do it** in the ADR-205 implementation, exactly as tested |
| 3 | **Log the low-load state** (why the tuner holds, entry 174) with the AFS reason (flame off with demand, modulation <= 5 %, PWM) | supports field diagnosis | small, part of the hourly log | entry 174 | **do it**, already decided |
| 4 | **Measure and log the boiler's flame-on delay and off-time per PWM cycle** before building the AFS 2.1.1.5 cycle correction | prepares 2.1.1.5 | small for logging; the correction itself is medium | the model cannot test it (no boiler min-off) | **log first, decide later** (YAGNI until field data shows the effect) |
| 5 | Add the AFS guards "not shortly after start-up" and "not at ID57" to the PWM entry | closer to 2.1.1.2.1 | small | none from the model | **ask Sergeant D**; low value, low cost |
| 6 | Change PWM detection to the AFS "flame off with demand" rule | closer to the letter of 2.1.1.2.1 | medium, and it changes core SAT behaviour | none; SAT's overshoot rule acts earlier | **do not** without strong field evidence; thermo-nova parity and Sergeant D's design weigh more (ADR-196) |
| 7 | Heat pumps: longer PWM cycles or a separate low-load study ("low and long") | outside the AFS (gas boilers) | medium | hp60 ~30 starts/day in the model, unvalidated | **separate task**, not part of TASK-1519; needs a heat-pump capture |
| 8 | Warn in the UI when a user sets the coefficient below about 1.2 | none | very small | cliff below the right value in every winter | **optional**; the tuner covers most of this when it is on |
| 9 | Validate the model on a 72 h capture (flame, modulation, flow, return, SAT log) with the TASK-1523 capture tool | n/a | no firmware change | starts per hour 1.5 model vs 2.5 field | **do it** before trusting any start or gas number |

## 6. Overall verdict

- The tuner design (ADR-205, entries 160-175) is well supported where the model is strong (signal, rhythm, safety)
  and honest about where it is weak (absolute values, boiler cycling, low-load houses).
- Its practical gain is limited for a user who keeps the default 2.0: most houses sit on the plateau. The main value
  is the recovery from a bad manual setting and the hourly log that shows what the controller sees.
- The clearest AFS gap in the current SAT is not in the tuner: SAT ignores ID3 HB bit 4, which the AFS makes
  mandatory. The change is small, but it switches PWM off on every boiler that reports bit 4 = 1, so the testers'
  values come first, then Sergeant D's decision.
- The open risk is heat pumps at low load (many starts), which no curve tuner can solve.

## 7. Appendix: rounds 10 and 11 (run for this evaluation)

**Round 10, PID left alone on a coefficient step** (`scripts/sat_curve_selftune_study10.py`, `data/round10_runs.json`).
ADR-204 forbids a PID reset on a coefficient change (Must Not: "Reset on a coefficient change"); rounds 1-9 zeroed
the integral and derivative on every tuner step. The decided rule (3 days) was rerun with `STEP_KEEPS_PID = True` on
2010-11, 2024-25 and 2025-26 and compared pair by pair with the reset runs: differences of -5 to +2 min/day, the
same end coefficient in 443 of 450 runs, worst pairs unchanged within 2 min/day. The no-tuner runs are identical in
both, as they must be. Conclusion: the firmware can follow ADR-204 and the study's conclusions stand.

**Round 11, overshoot margin 2.0 vs 3.0 on the calibrated model** (`scripts/sat_curve_selftune_study11.py`,
`data/round11_runs.json`): 2010-11 / 2024-25 / 2025-26, starts 1.0 / 1.4 / 1.8 / 2.4, 2 seeds, with and without the
decided tuner, paired by house, start and seed.

| winter | source | minutes outside +-0.3 C, no tuner, 2.0 vs 3.0 | with tuner, 2.0 vs 3.0 | starts per day, no tuner, 2.0 vs 3.0 |
|---|---|---|---|---|
| 2010-11 | gas | 200 vs 214 (2.0 better in 78 % of pairs) | 121 vs 139 | 34.9 vs 34.4 |
| 2010-11 | hp30 | 273 vs 274 | 116 vs 117 | 5.3 vs 5.1 |
| 2010-11 | hp60 | 211 vs 211 (3.0 better in 62 %) | 159 vs 157 | 27.3 vs 27.2 |
| 2024-25 | gas | 139 vs 159 (82 %) | 112 vs 135 | 37.2 vs 36.1 |
| 2024-25 | hp30 | 161 vs 161 | 57 vs 58 | 6.2 vs 5.8 |
| 2024-25 | hp60 | 129 vs 127 (3.0 better in 58 %) | 128 vs 126 | 33.3 vs 33.4 |
| 2025-26 | gas | 153 vs 173 (75 %) | 117 vs 142 | 36.2 vs 35.3 |
| 2025-26 | hp30 | 181 vs 182 | 66 vs 68 | 7.0 vs 6.5 |
| 2025-26 | hp60 | 147 vs 146 (3.0 better in 60 %) | 138 vs 137 | 31.2 vs 31.1 |

Fuel differs by at most 0.1 %. The 2.0 margin enters PWM slightly more often (PWM fraction gas 0.36-0.41 vs
0.32-0.38). Start counts are indicative only (section 2, point 1).
