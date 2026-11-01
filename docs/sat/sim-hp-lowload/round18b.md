# TASK-1527 round 18b: heat-pump PWM as the firmware runs it

> **Correction (2026-11-01, round 19b/19c).** Every SAT PWM row below uses the heating curve 1.8. In SAT PWM that curve
> makes these houses run warm; the right curve in PWM is 0.8-1.0 (round 19b sweep, confirmed on all 3 seasons and both
> seeds in round 19c). The same 1.8 is close to right for degree minutes and the heat pump's own control. So the
> comparison "degree minutes and own control are clearly better on comfort" below does not hold. With each control at
> its own best curve (heavy screed, 3 seasons): SAT PWM 180 min 7.6 starts/day at 0.168 C, degree minutes 3.0 at
> 0.188 C, own control 2.0 at 0.206 C. Only the starts advantage of degree minutes and own control survives. The
> 60 min minimum ON costs less at the tuned curve: +0.015 C heavy (6.4 vs 7.6 starts), +0.09 C light (7.1 vs 9.2).
> See round19.md.

Date 2026-11-01. Rounds 12-17 modelled "SAT PWM as now" with the ON time fixed at the start of each period. The
firmware re-evaluates the 5-range mapper on every step (SATcontrol.ino `satApplyPWM`; thermo-nova `pwm.py:135`,
`:205`), so the ON phase follows the room (entry 196, checked on the real code with tests/test_sat_hp_pwm.py's
harness). This round adds that behaviour as variant `fw` to `scripts/sat_hp_onoff_study13.py` and reruns the
underfloor on/off heat pump. It includes Sergeant D's proposed type (entry 195): period 180 min, minimum ON 60 min.

Plant as round 15/17: on/off heat pump on underfloor heating, 3 houses (1992-2012, 2015, BENG 2021), heavy (8 MJ/K)
and light (3 MJ/K) screed, full heating seasons 1 Oct .. 30 Apr (2010-11, 2013-14, 2025-26), 2 seeds: 12 runs per
variant per season, 324 runs. Script `scripts/sat_hp_fw_pwm_study18b.py`, data `round18b_fw_pwm_runs.json`.
Check: the old `pwm 30` variant reproduces round 17 exactly (2025-26, 1992-2012, heavy, seed 1: 38.4 starts/day,
0.248 C).

Radiators are not rerun: their calibration file (`round5c_runs.json`) is not in the repo. Their round 17 numbers
use the same fixed-ON assumption and are likely too pessimistic as well.

## Heavy screed (8 MJ/K)

| variant | starts/day | max/day | median run (min) | mean abs error (C) | RMS (C) | min/day outside +-0.3 C | electricity vs fw 30 |
|---|---|---|---|---|---|---|---|
| old model, fixed ON, 30 min | 33.7 | 48 | 14 | 0.325 | 0.400 | 547 | +0.2 % |
| firmware, 30 min (today) | 34.6 | 50 | 14 | 0.314 | 0.388 | 514 | 0 |
| firmware, 40 min | 26.9 | 38 | 17 | 0.315 | 0.388 | 516 | 0.0 % |
| firmware, 120 min, min ON 3 min | 10.6 | 16 | 45 | 0.317 | 0.388 | 530 | 0.0 % |
| firmware, 180 min, min ON 3 min | 7.7 | 12 | 62 | 0.317 | 0.387 | 541 | +0.1 % |
| firmware, 120 min, min ON 60 min | 7.0 | 14 | 60 | 0.359 | 0.426 | 686 | +0.6 % |
| **firmware, 180 min, min ON 60 min (entry 195)** | **6.3** | 11 | 64 | **0.352** | 0.419 | 647 | +0.4 % |
| degree minutes -120 | 2.6 | 7 | 180 | 0.203 | 0.325 | 273 | -1.5 % |
| heat pump's own control | 2.0 | 6 | 278 | 0.206 | 0.307 | 258 | -1.7 % |

## Light screed (3 MJ/K)

| variant | starts/day | max/day | median run (min) | mean abs error (C) | RMS (C) | min/day outside +-0.3 C | electricity vs fw 30 |
|---|---|---|---|---|---|---|---|
| old model, fixed ON, 30 min | 36.9 | 48 | 12 | 0.312 | 0.360 | 544 | +0.4 % |
| firmware, 30 min (today) | 39.9 | 54 | 12 | 0.289 | 0.338 | 438 | 0 |
| firmware, 40 min | 31.4 | 43 | 15 | 0.289 | 0.338 | 444 | 0.0 % |
| firmware, 120 min, min ON 3 min | 13.7 | 20 | 34 | 0.291 | 0.342 | 495 | +0.1 % |
| firmware, 180 min, min ON 3 min | 10.6 | 17 | 45 | 0.292 | 0.344 | 525 | +0.3 % |
| firmware, 120 min, min ON 60 min | 7.7 | 19 | 60 | 0.420 | 0.467 | 1090 | +2.2 % |
| **firmware, 180 min, min ON 60 min (entry 195)** | **7.4** | 15 | 60 | **0.403** | 0.452 | 987 | +1.9 % |
| degree minutes -120 | 4.5 | 9 | 102 | 0.169 | 0.256 | 183 | -1.5 % |
| heat pump's own control | 5.0 | 13 | 108 | 0.150 | 0.227 | 128 | -2.2 % |

The ranking is the same in each of the three seasons (heavy screed, mean abs error: firmware 30 min
0.293-0.340 C, 180/60 0.336-0.372 C, degree minutes 0.184-0.224 C).

## Findings

1. **The correction holds.** With the ON time following the room, a longer period cuts starts without a comfort
   cost: 180 min with a 3 min minimum gives 7.7 instead of 34.6 starts a day at the same error (0.317 vs 0.314 C).
   Round 17 had predicted 0.395 C for 180 min. Today's 30 min was modelled about right (0.325 vs 0.314 C).
2. **A 60 min minimum ON costs comfort and saves few starts.** Against 180 min with a 3 min minimum: 6.3 vs 7.7
   starts a day, but 0.352 vs 0.317 C (heavy) and 0.403 vs 0.292 C (light screed: 987 vs 525 min/day outside
   +-0.3 C). At low demand every run is stretched to 60 min, so the floor overshoots. The light floor shows it most.
3. With a 180 min period the runs are long by themselves: median 62 min on heavy screed with only the 3 min hold.
4. Degree minutes and the heat pump's own control stay clearly ahead on both starts and comfort (2-5 starts a day,
   0.15-0.21 C). This round does not change that.

## Round 18c: minimum ON 10 minutes (entry 197)

Same plant and seeds, `--round18c`, data `round18c_minon10_runs.json` (360 runs). Starts/day / mean abs error (C):

| period, min ON | heavy screed | light screed |
|---|---|---|
| 30 min, 3 min (today) | 34.6 / 0.314 | 39.9 / 0.289 |
| 30 min, 10 min | 30.2 / 0.312 | 35.4 / 0.294 |
| 40 min, 10 min | 24.9 / 0.315 | 29.5 / 0.293 |
| 60 min, 10 min | 18.2 / 0.317 | 22.0 / 0.292 |
| 120 min, 10 min | 10.4 / 0.318 | 13.5 / 0.292 |
| 180 min, 10 min | 7.7 / 0.318 | 10.6 / 0.293 |
| 180 min, 60 min | 6.3 / 0.352 | 7.4 / 0.403 |

A 10 minute minimum costs no comfort at any period (at most +0.005 C) and already cuts starts at 30 minutes
(34.6 -> 30.2). It is the better minimum for the 3 hour type than 60 minutes (finding 2).

## Limits

No start losses or defrost in the COP, so fewer starts and longer runs get no efficiency credit and compressor wear is
not valued (that is the point of a minimum run); no minimum OFF time; the heat pump's response is assumed; model plant,
not validated against a heat-pump capture; radiators not rerun.
