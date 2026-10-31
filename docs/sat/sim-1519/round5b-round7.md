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
4. The cliff/plateau shape supports asymmetric thresholds (Q-1519e option B: raise readily, lower carefully).

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

## 5. Limits

One weather year; 2 seeds; boiler and heat-pump cycling not validated (round 4); radiator size calibrated to an
anchor (entry 166) rather than measured; noise-free room sensor (a real 0.1 C sensor makes a per-minute slope spiky,
which would make D noisier, not more informative).
