# TASK-1519 rounds 5 analysis, 5b and 7

Date: 2026-10-31. Scripts: `scripts/sat_curve_selftune_study4.py` (model, new flags), `scripts/sat_curve_selftune_study5b.py`,
`scripts/sat_curve_selftune_study7.py`. Data: `docs/sat/sim-1519/data/round5b_*.json`, `round7_runs.json`, and
`round5_results_raw.json` (round 5, 7,992 runs). Weather: Open-Meteo archive 52.1N 6.2E, 2025-11-01..2026-02-28.

All numbers are model output. The model is a two-node house per building era with boiler / heat-pump minimum output
and thermo-nova PWM rules; the boiler's own cycling is not validated against the field (round 4). It ranks options;
a field test has to confirm them.

## 1. Round 5 result, and why its reference was wrong

Round 5 scored tuners against `best_coeff2()` (`scripts/sat_curve_selftune_study3.py:30-33`), which was 1.80 for every
house. With the tuner the end coefficient was further from 1.80 (0.44-0.57) than without (0.33), and hp30 with the
band ended at start - 0.4 in almost every house. That looked like a biased tuner. It was a biased reference.

**Signal ablation** (one winter per case, no tuner, per heating minute classified as PWM, clamped high/low, heat
source at minimum or off, or modulating; mean of setpoint - curve per class):

- Clamped hours (40 C heat-pump cap, 10 C floor): at most 1.7 % of hours, and their sign is positive. Not the cause.
- Minimum-output hours: strongly negative (-1.3 to -5.9 C of flow), room above target. They push the curve down.
- Modulating hours at c = 1.8 were also negative (1992-2012 heavy hp30: -3.1 C, room +0.12 above target).
  So the curve 1.8 really was too high in this model.

**Cause:** `best_coeff2` assumes flow = mean radiator temperature + 5 C and no internal gains. The round-4 hydraulics
(`study4.run`) put the flow at mean + Q / (2 x 628 W/K), about +2.4 C at 3 kW, and the house gets about 135 W of
internal gains plus sun. A fixed-coefficient sweep (no tuner, round 5 settings) put the comfort optimum at 0.5-1.3
in most houses, against 1.6-2.4 in the field (entry 166).

**Fix (round 5b):** size the radiators per house (kr) so that the round-4 hydraulics with mean gains balance at 1.8
at the season mean (4.69 C). Result: kr 0.715 (1983-92), 0.616 (1992-2012), 0.543 (2015), 0.522 (BENG).

## 2. Calibrated model: a cliff and a plateau

Fixed-coefficient sweep 0.8-2.6, minutes per day outside +-0.3 C (seed 1, margin 2.0):

| house, source | 0.8 | 1.0 | 1.2 | 1.4 | 1.8 | 2.2 | 2.6 | optimum |
|---|---|---|---|---|---|---|---|---|
| gas 1983-92 heavy | 936 | 577 | 104 | 66 | 56 | 93 | 198 | 1.8 |
| gas 1992-2012 heavy | 267 | 177 | 36 | 24 | 23 | 24 | 34 | 1.7 |
| gas 1992-2012 light | 418 | 304 | 97 | 97 | 282 | 500 | 703 | 1.3 |
| gas BENG heavy | 653 | 175 | 50 | 53 | 57 | 62 | 77 | 1.3 |
| hp30 1992-2012 heavy | 912 | 534 | 64 | 34 | 25 | 29 | 40 | 1.7 |
| hp60 1992-2012 heavy | 296 | 169 | 71 | 58 | 45 | 48 | 88 | 1.7 |

Below about 1.2 comfort collapses; from 1.3 to 2.4 it is nearly flat. The light (fast) house is the exception: a
high curve raises kp (= coefficient x curve / 3, thermo-nova `pid.py:69-70`) and the PID swings.

## 3. Round 5b: tuners on the calibrated model (630 winters)

5 houses x 3 sources x starts 1.0 / 1.8 / 2.4 x 2 seeds, daily rule threshold 1 C step 0.1, each run paired against
no tuner (same house, weather, seed). d_out = tuner - none, minutes per day outside +-0.3 C.

| source, rule | start 1.0 | start 1.8 | start 2.4 | worst pair |
|---|---|---|---|---|
| gas, 2 d, PWM excluded | -97 | 0 | -0.5 | +100 |
| gas, 2 d, PWM + minimum excluded | -106 | 0 | 0 | 0 |
| hp30, 2 d, PWM excluded | -503 | -0.5 | -4 | +10 |
| hp30, 2 d, PWM + minimum excluded | -504 | -0.2 | -2 | +3 |
| hp60, 2 d, PWM excluded | -50 | 0 | -16 | +24 |
| hp60, 2 d, PWM + minimum excluded | -55 | 0 | -8 | +1 |

(medians per start; worst pair over all starts.) Changes per winter 0.7-6.9, flips 0-1.5.

Findings:
1. The round 4/5 claim "the tuner does not help heat pumps" is withdrawn; it came from the reference error.
2. The tuner's value is the rescue from a curve that is too low. From 1.8 it does nothing; from 2.4 it lowers slowly.
3. Excluding minimum-output hours removes the cases where the tuner made comfort worse, at the cost of a slower and
   less complete rescue (gas from 1.0 ends at 1.32 instead of 1.50; hp60 at 1.23 instead of 1.48).
4. ~~The cliff/plateau shape supports asymmetric thresholds (Q-1519e option B).~~ Withdrawn: round 5c simulated B and
   found no comfort gain and more churn (section 6).

## 4. Round 7: I + D against the curve integral (entry 170, 540 winters)

Same model plus PID derivative held inside the 0.1 C deadband (thermo-nova `pid.py:205-207`); the tuner computes its
own derivative kd x room slope every minute (kd = 588 kp, `pid.py:92`), never held. Clean hours exclude PWM and
minimum output. Rule: daily, 2 days, threshold 1 C, step 0.1.

**Expectation stated before the run** (Discord msg 1566025420162211863): a slope averaged over 2-3 days telescopes to
(room at end - room at start) / window, so D adds little to the window mean.

| signal | gas vs none, mean | hp30 vs none, mean | hp60 vs none, mean | same end coefficient as CI |
|---|---|---|---|---|
| CI (setpoint - curve, control) | -53.6 | -157.3 | -16.1 | - |
| CI + D (entry 170) | -53.6 | -157.1 | -16.2 | 89 / 90 |
| I + D (PID integral + D) | -6.8 | -29.2 | +2.1 | 28 / 90 |
| I (PID integral) | -6.5 | -28.8 | +1.6 | 28 / 90 |
| CI + guard (hour not clean if room moved > 0.1 C) | -53.5 | -157.3 | -15.9 | 90 / 90 |

Magnitude check (1992-2012 heavy, start 1.0): the tuner D is active (per-minute rms 1.00 C of flow for gas, 0.62 for
hp30; hourly-mean rms 0.17 / 0.14), but its 48 h window mean is at most 0.024 C, against a curve-integral window mean
of +0.74 (gas) and +2.25 (hp30). D is 1-3 % of the signal.

Findings:
1. The expectation holds: CI + D behaves as CI. D neither helps nor hurts the daily rule.
2. The PID integral (with or without D) cannot rescue a curve that is too low: it stays at 1.0 (d_out 0), and for hp30
   it makes 14 changes and 4 flips per winter. This repeats round 1 with the deadband reset (`pid.py:187-189`).
3. D as a per-hour guard at 0.1 C/h changes nothing measurable.
4. If D is to matter, it has to act on a shorter horizon than the daily decision (open question for Sergeant D).

## 6. Rounds 5c-5g: the clean-hour rule, option B, and the blind spot in low-load houses

Scripts `scripts/sat_curve_selftune_study5c.py`, `study5d.py` (with `--cold` for 5e), `study5f.py` (with `--guard`
for 5g); data `round5c_runs.json` .. `round5g_runs.json`. All on the round 5b model, daily rule 2 days, step 0.1,
paired against no tuner; starts 1.0 / 1.2 / 1.4 / 1.8 / 2.4 on houses balanced at 1.8. `clean_h` = clean hours the
tuner saw per winter (scored days).

**Option B (Q-1519e: raise at 0.5 C, lower at 1.0 C) is refuted.** Against the symmetric 1.0 C rule: no comfort
gain (gas rescue from 1.0: median -85 vs -97 min/day), more churn (hp30 from 1.0: 14.3 vs 10.4 changes, 5.0 vs 1.8
flips per winter). Advice for Q-1519e stays A.

**The optimum elsewhere is found, slowly.** Houses balanced at 2.4 started at 1.8 end at 2.1-2.4 (hp30 -6 to -17
min/day); houses balanced at 1.6 end at 1.7. Some BENG runs do not move (too few clean hours).

**Blind spot.** In 2015 and BENG houses a curve of 1.0 drives SAT into PWM at minimum output, so with the round 5b rule
the tuner sees 0 clean hours all winter and stays at 1.0, while the room is outside +-0.3 C for 144-278 min/day
(mostly too cold). Every attempt to fill it was tested:

| variant | rescue new houses from 1.0 (gas / hp60, median min/day) | plateau starts 1.8-2.4, gas worst pair |
|---|---|---|
| round 5b rule (PWM + minimum excluded both ways) | 0 / 0 | 0 |
| 5d: exclude PWM / minimum only while room above target (per minute) | -118 / -166 | +416 |
| 5e: count a PWM / minimum hour only if its hour-mean room < -0.3 C | -80 / -69 | +363 |
| 5f: cold-day net (2-day mean over all heating minutes < -0.2 C -> step up) | -112 / -159 | +509 |
| 5f: same at -0.3 C | 0 / -67 | +145 |
| 5g: net -0.2 / -0.3 C, skipping days with swing > 0.5 C or > 60 min at max flow | 0 / 0 | 0 |

Why each fails: a one-sided selection on room temperature (5d, 5e) keeps the cold half of the cycling and drifts the
curve up on the plateau (selection bias). The cold-day net (5f) has no selection, but in the light 1992-2012 house a
high curve makes the PID swing, the swing reads as cold days, the net raises the curve, kp grows (kp = coefficient x
curve / 3) and the curve runs to 3.3-3.4 (the round 3 feedback loop). The swing guard (round 6) stops that, but the
low-load rescue days also swing > 0.5 C (PWM cycling), so it blocks the rescue too. With the signals in the model,
"cold because the curve is too low at low load" and "cold on average because the PID oscillates" look alike. This is
an open design question (Q-1519g, queued for Sergeant D), not a parameter to tune further.

## 7. Where the evidence points (to be confirmed by Sergeant D and in the field)

- Signal: curve integral (setpoint - curve), entry 163. Adding D changes nothing (round 7); the PID integral cannot
  rescue a curve that is too low.
- Clean hours: SAT heating, no hot water, no sun, SAT not in PWM and the heat source not at minimum output, both ways
  (round 5b rule). Worst pair over rounds 5c-5g with this rule: +0 min/day (gas), +14 (hp30), +2 (hp60).
- Decision: daily on 2-3 days, symmetric 1 C, step 0.1, at most 0.2 per change (entries 165/167). No asymmetry.
- Known gap: a curve far too low in a 2015/BENG house whose boiler or heat pump minimum is above the demand is not
  corrected. The user sees a cold room; the log should say why the tuner holds ("no clean hours: PWM / minimum output").

## 8. Round 8: a second winter (2024-25), 2-day window (Robert 2026-10-31)

Script `scripts/sat_curve_selftune_study8.py`; data `round8_runs.json`, weather `weather_2024_25.json` (Open-Meteo
archive, same grid cell 52.13N 6.13E, 2024-11-01..2025-02-28) next to `weather_2025_26.json`. Same houses and radiator
sizes (calibrated on 2025-26, so 2024-25 is out of sample). Rule as decided (entries 163, 165, 167, 171-174): curve
integral, clean hours without PWM and minimum output, daily on 2 days, symmetric 1 C, step 0.1. Check: the 2025-26
rule runs equal round 5d bit for bit (150 / 150).

Winters: 2024-25 is milder at the extremes (minimum -5.1 vs -9.4 C, coldest day -2.1 vs -4.5 C, 353 vs 445 frost
hours) but slightly colder on average (heating mean 4.46 vs 4.69 C), with less sun (644 vs 708 hours >= 50 W/m2).

Fixed sweep, minutes per day outside +-0.3 C at 1.0 / 1.2 / 1.8 / 2.4, 2024-25 (2025-26 in brackets):

| house, source | 1.0 | 1.2 | 1.8 | 2.4 | optimum |
|---|---|---|---|---|---|
| gas 1992-2012 heavy | 138 (177) | 30 (36) | 20 (23) | 23 (28) | 1.5 (1.7) |
| gas 1992-2012 light | 259 (304) | 80 (97) | 301 (282) | 678 (653) | 1.3 (1.3) |
| gas BENG heavy | 133 (175) | 35 (50) | 51 (57) | 64 (72) | 1.2 (1.3) |
| hp30 1992-2012 heavy | 493 (534) | 45 (64) | 17 (25) | 26 (33) | 1.9 (1.7) |
| hp60 1992-2012 heavy | 121 (169) | 58 (71) | 41 (45) | 64 (73) | 2.0 (1.7) |

The cliff below about 1.2 and the plateau above it hold in the second winter; optima move 0.1-0.3, inside the
plateau. The light (fast) house keeps its narrow window (1.2-1.6).

Rule against no tuner, paired, 2024-25 (2025-26 in brackets):

| source | from 1.0, median | from 1.2-1.4, median | from 1.8-2.4, median | worst pair | changes / flips per winter | clean hours |
|---|---|---|---|---|---|---|
| gas | -29 (-106) | 0 (0) | 0 (0) | +2 (0) | 1.2 / 0.0 (1.4 / 0.0) | 72 (67) |
| hp30 | -470 (-504) | -7 (-16) | 0 (-1) | +8 (+14) | 5.0 / 0.4 (5.7 / 0.9) | 561 (563) |
| hp60 | -2 (-55) | 0 (0) | 0 (0) | 0 (+2) | 0.7 / 0.0 (1.5 / 0.0) | 25 (42) |

Findings:
1. The rule stays safe in a second winter: worst pair +2 / +8 / 0 min/day, at most 0.5 flips per winter.
2. The rescue from a curve that is too low is slower in the milder winter for gas and hp60 (median -29 vs -106,
   -2 vs -55): fewer cold hours mean more PWM / minimum-output hours, so fewer clean hours (hp60: 25-30 per winter).
   This is the blind spot of entry 174 in a milder form, and the hourly log (entry 172) is what makes it visible.
3. The 2-day window did not chase its own steps in either winter (flips <= 0.9). Entry 168 (60-72 h response) and the
   2-day window: after a step the rule waits for 2 new qualifying days, which with >= 6 clean hours per day takes
   at least 48 h and in practice longer. Rounds 1 and 5 compared windows of 1-4 days: comfort was the same; the 1-day
   window made more changes (about 11 vs 6.6 per winter for 3 days, round 1), 2 and 3 days were close (round 5).
   Whether 2 days is long enough for entry 168 is a judgement for Sergeant D; the simulation shows no harm from it.

## 9. Round 9: twenty winters (2005-06 .. 2024-25), 2-day and 3-day window (Robert 2026-10-31)

Script `scripts/sat_curve_selftune_study9.py`; weather `data/weather20/` (Open-Meteo archive, same grid cell, Nov-Feb,
120 days each); results per winter in `data/round9/<winter>.json`. Radiators as calibrated on 2025-26 (all 20 winters
out of sample). Per winter: coarse sweep (c = 1.0-2.4, seed 1) and the decided rule with a 2-day and a 3-day window
against no tuner, starts 1.0 / 1.2 / 1.4 / 1.8 / 2.4, 2 seeds, gas / hp30 / hp60: 11,100 winters in total. Check:
round 9's 2024-25 runs equal round 8 (300 / 300).

The 20 winters range from a heating-hour mean of 2.38 C (2010-11) to 6.78 C (2006-07), minimum -15.5 C (February
2021), 62 to 1,029 frost hours.

Rescue from a curve of 1.0 (median min/day outside +-0.3 C, tuner minus no tuner, 2 d / 3 d), and worst pair (2 d):

| winter | gas | hp30 | hp60 | worst pair gas / hp30 / hp60 |
|---|---|---|---|---|
| 2005-06 | -181 / -157 | -702 / -697 | -113 / -91 | 0 / +4 / +5 |
| 2006-07 (mildest) | -20 / -9 | -167 / -170 | -2 / -2 | +16 / +3 / +5 |
| 2009-10 | -418 / -390 | -660 / -651 | -268 / -259 | 0 / +27 / +17 |
| 2010-11 (coldest) | -415 / -393 | -665 / -668 | -265 / -256 | 0 / +25 / +29 |
| 2013-14 (fewest frost h) | 0 / 0 | -198 / -201 | 0 / 0 | +13 / +8 / 0 |
| 2019-20 | 0 / 0 | -215 / -219 | 0 / 0 | +4 / +11 / 0 |
| 2020-21 (-15.5 C) | -62 / -30 | -382 / -378 | 0 / -2 | +3 / +3 / +4 |
| 2024-25 | -29 / -9 | -470 / -470 | -2 / 0 | +2 / +8 / 0 |

(all 20 rows: `data/round9/`; table above is a selection.)

All pairs over 20 winters (1,000 per source and window):

| source, window | median | mean | worst pair | pairs worse by > 5 min/day | rescue from 1.0, median | flips per winter, mean | runs with >= 3 flips |
|---|---|---|---|---|---|---|---|
| gas, 2 d | 0 | -30.5 | +16 | 1.2 % | -33 | | |
| gas, 3 d | 0 | -28.7 | +16 | 1.8 % | -19 | | |
| hp30, 2 d | -4.1 | -85.4 | +28 | 4.3 % | -384 | | |
| hp30, 3 d | -4.0 | -84.3 | +19 | 3.2 % | -375 | | |
| hp60, 2 d | 0 | -11.8 | +33 | 2.0 % | -5 | | |
| hp60, 3 d | 0 | -10.1 | +21 | 0.9 % | 0 | | |
| all, 2 d | | | | | | 0.31 | 88 / 3,000 |
| all, 3 d | | | | | | 0.17 | 25 / 3,000 |

Changes per winter: mean 2.9 (2 d) and 2.4 (3 d), at most 17 and 14.

Findings:
1. Over 20 winters the rule never makes comfort much worse: worst pair +33 min/day (hp60, 2 d), and 0.9-4.3 % of
   pairs are worse by more than 5 min/day.
2. The gain scales with the cold: in cold winters (2009-10, 2010-11) the rescue from a curve of 1.0 is -415 min/day
   for gas; in mild winters (2013-14, 2019-20) the gas and hp60 tuners hardly move, because nearly every hour is at
   minimum output (the blind spot of entry 174). hp30 rescues in every winter.
3. Flips concentrate in hp30 houses near the optimum (start 1.8): with ~560 clean hours a winter the rule decides
   often and steps +-0.1 around a right value that moves with the weather. 3 days halves that (88 -> 25 runs with
   >= 3 flips) and lowers the worst pairs (hp30 +28 -> +19, hp60 +33 -> +21).
4. The price of 3 days is a slower rescue for gas and hp60 (median -33 -> -19 and -5 -> 0 from a curve of 1.0).
5. The sweep optimum of the 1992-2012 heavy house moves between 1.6 and 2.0 from winter to winter (coarse grid).

The 2-day / 3-day choice (Q-1519h) is a trade-off: 2 days rescues faster, 3 days is steadier and matches entry 168.

## 10. Limits

Weather: 2025-26 for rounds 1-7, plus 2024-25 (round 8) and 20 winters 2005-06 .. 2024-25 (round 9); 2 seeds; boiler and heat-pump cycling not validated (round 4); radiator size calibrated to an
anchor (entry 166) rather than measured; noise-free room sensor (a real 0.1 C sensor makes a per-minute slope spiky,
which would make D noisier, not more informative).
