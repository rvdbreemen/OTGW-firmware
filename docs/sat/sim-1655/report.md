# TASK-1655: KD x0.9 and x0.8 against today for the warm-up (simulation)

**Result in one line.** The model shows the D term holding back the warm-up, as Sergeant D describes. But lowering KD by
10 % or 20 % makes the warm-up only 0.3-4 % faster: a few minutes on 1.5-7 hours. By the pre-registered rule the
recommendation is **keep KD at 0.07**: no 10 % warm-up gain in any class, so KD is not the lever for the warm-up in this
model.

On harm, x0.9 causes one guard failure of its own: G4 in 2015 heavy rad, where the deadband is reached 0.8 min later in
the median. The other failing guards (G1, G2, G6) also fail at baseline on the same inputs. That "also at baseline"
reading is our analysis after the run, not part of the pre-registered rule. In 1983-92 light rad the worst run's starts
per day rose from 77 to 84; the median change there is 0.

Sergeant D has already decided 0.063 (= x0.9, entry 279). This study is new evidence on that decision, not a ruling on it.

Simulation only: no result is physical boiler proof. Pre-registration: `docs/sat/sim-1655/thresholds.md`, committed
be3dd1726 before the first grid run. Data: `docs/sat/sim-1655/runs.json` (1728 runs; meta holds the sha256 of every
script and the thresholds commit). Script: `scripts/sat_kd_warmup_sim_1655.py`. Test:
`tests/test_sat_kd_warmup_sim_1655.py` (4/4 OK; the loop with KD x1.0 and no start offset reproduces a committed
1412.19 row exactly).

## 1. Validity check (pre-registered, reported first)

The check asks whether the baseline reproduces the starvation. The rule: in the radiator classes, the median over runs
of the mean D term during the approach is <= -1.0 C on the setpoint. The approach is the PID ticks with error > 0.1 K,
before -0.1 K is reached.

| Plant class | Baseline median D during the approach (C) |
|---|---|
| 1983-92 light rad | -2.85 |
| 1983-92 heavy rad | -1.43 |
| 2015 light rad | -2.83 |
| 2015 heavy rad | -1.15 |
| 1983-92 light ufh | -2.29 |
| 1983-92 heavy ufh | -1.26 |
| 2015 light ufh | -1.62 |
| 2015 heavy ufh | -0.87 |

**Passed.** All four radiator classes are at -1.15 C or lower. The light houses match his example of about -2 to
-2.5 C (entry 278). So the model does contain the effect he points at, and the result below answers his question.

## 2. Result per plant class (pre-registered rule)

B is the median paired reduction of the time to within 0.3 K; it must be >= 10 %. G1-G6 are the guards. "Fails also at
baseline" means the baseline run on the same inputs breaks the same absolute limit. It is a property of the plant, not
of the KD change: the paired deltas of that guard are 0.00-0.01 K.

| Plant class | Baseline median t03 | x0.9: B | x0.9 minutes saved | x0.8: B | x0.8 minutes saved | Guards failing (both factors) |
|---|---|---|---|---|---|---|
| 1983-92 light rad | 106 min | 2.0 % | 2.6 | 4.0 % | 4.4 | G2: the worst run has 84 starts/day with both factors, against 77 at baseline; baseline also breaks the 72 limit; median delta 0 % |
| 1983-92 light ufh | 96 min | 1.8 % | 1.6 | 3.7 % | 3.4 | none |
| 1983-92 heavy rad | 236 min | 0.3 % | 1.0 | 2.9 % | 7.0 | none |
| 1983-92 heavy ufh | 159 min | 1.8 % | 2.5 | 3.7 % | 5.0 | none |
| 2015 light rad | 175 min | 0.9 % | 1.8 | 2.0 % | 3.6 | G6: guard-run overshoot 0.87-0.90 K, also at baseline (0.86) |
| 2015 light ufh | 187 min | 0.5 % | 0.9 | 1.0 % | 1.8 | G1, G6: overshoot 1.1 / 1.5 K, also at baseline (1.09 / 1.49) |
| 2015 heavy rad | 411 min | 0.3 % | 1.1 | 0.4 % | 2.2 | x0.9 only G4: median t01 +0.8 min later |
| 2015 heavy ufh | 318 min | 0.4 % | 1.2 | 1.0 % | 2.7 | G1, G6: overshoot 0.71 / 0.98 K, also at baseline (0.71 / 0.98) |

- **x0.8:** B holds in 0 of 8 classes.
- **x0.9:** B holds in 0 of 8 classes.
- **Recommendation by the rule:** keep KD, reason "no 10 % warm-up gain in any class" (`runs.json` `recommendation`).

Paired deltas that matter for harm, x0.9 against baseline, as medians per class:

- **Overshoot:** +0.00 K (largest +0.003).
- **Starts per day:** +0 % (largest +1.6 %, 2015 light rad).
- **RMS after arrival:** +0.000 to +0.003 K.
- **Guard-run share within 0.3 K:** unchanged.
- **Hard limits:** H1 (<= 3 PWM starts per hour) and H2 (CS clamp) hold in all 1728 runs.

Changes caused by x0.9 itself: G4 in 2015 heavy rad (median t01 +0.8 min) and, in 1983-92 light rad, the worst run's starts per day (77 -> 84, median delta 0).

## 3. Sergeant D's design intent: the deadband within 8400 s (entry 279)

Reported, not scored: entry 279 came after the pre-registration. 8400 s is 140 min. The table gives the share of the
48 warm-up runs per class that reach -0.1 K within 140 min.

| Plant class | x1.0 | x0.9 | x0.8 | Median t01 x1.0 / x0.9 |
|---|---|---|---|---|
| 1983-92 light ufh | 30/48 | 30/48 | 30/48 | 127 / 124 min |
| 1983-92 heavy ufh | 10/48 | 10/48 | 10/48 | 307 / 305 min |
| 1983-92 light rad | 6/48 | 12/48 | 10/48 | 166 / 167 min |
| 2015 light ufh | 8/48 | 8/48 | 8/48 | 211 / 210 min |
| the other four classes | 0/48 | 0/48 | 0/48 | 244-539 min |

Only 1983-92 light rad moves: from 6 to 12 runs. Everywhere else x0.9 changes nothing against the 140 min target.

## 4. Why a lower KD barely helps here (diagnosis, two traces)

This explanation rests on two traces only: 1983-92 light rad and 2015 heavy rad, outdoor 5 C, d0 -2.5 K, fixed flow,
Pmin 5.5 kW, MM honoured, x1.0 against x0.8. It is a reading of those runs, not a proven general cause.

- **First 30 min:** the boiler runs flat out (24 kW, CS 62 C). The derivative term is still small and does not matter.
- **After that, the light house:**
  - The boiler sits at its minimum power (5.5 kW) or switches through PWM.
  - D is -2.4 to -2.9 C on the setpoint, so a 20 % lower KD moves the setpoint by about 0.5 C (CS 38.6 -> 38.2 at 1 h).
  - At minimum power this does not raise the heat output (P 5500 W in both runs). So the room warms at the same rate.
- **After that, the heavy house:** most of the heat goes into the cold wall. The room follows the wall, not the
  setpoint. The traces are almost identical: t03 283.0 against 283.2 min.

So in this model the warm-up is limited by the boiler's minimum-power and PWM regime and by the thermal mass. The few
degrees the derivative takes off the setpoint are not what limits it. Individual runs scatter by up to +-40 % either
way: a shifted PWM phase can bring the arrival forward or push it back by one cycle. This is why the rule uses paired
medians.

Model limits that bear on this:

1. **Even cool-down:** the start state is a house that cooled down evenly, wall included. A short night setback with a
   warm wall would reach the band sooner, and the share taken by the derivative may differ there. That is not simulated.
2. **Simple boiler:** the model has no modulation band between Pmin and full power, other than `draw + 1000 * (CS - flow)`.
3. **Fixed curve:** coefficient 1.6, with K calibrated to it.

## 5. Firmware formula check

Entry 279 writes `kd = satPidRound3(0.063f * SAT_PID_AGGRESSION_V3 * kp)`, while this study used
`round3(0.9f * 0.07f * 8400 * kp)`. In float32 the two products are not bit-identical. After the 3-decimal rounding they
are equal for every kp from 0.001 to 200 in steps of 0.001: 0 mismatches over 200000 values. So these results hold for
the firmware form of entry 279.

## 6. Recorded entries checked

- **Entry 277** (the integral stays as thermo-nova, reduce KD instead): **agrees**. The integral code is untouched and
  only kd is scaled.
- **Entry 278** (his question and the -10 % proposal):
  - **answered**: the starvation is real in the model, but -10 % gives about 1-2 % faster warm-up.
  - Citations, opened in this session in `custom_components/sat` at 7beaaf5:
    - `kd = round(0.07 * 8400 * self.kp, 6)` (`pid.py:84-93`).
    - `D = round(kd * raw_derivative, 3)` (`pid.py:109-114`).
    - The raw derivative is frozen inside the deadband (`pid.py:205-207`).
- **Entry 279** (KD factor 0.063, deadband in 8400 s):
  - The simulation shows almost no harm from 0.063: one own guard failure (G4, 2015 heavy rad, +0.8 min) and one run with more starts (77 -> 84).
  - It does not show a warm-up gain that would make the 8400 s target reachable in more runs, except in 1983-92 light
    rad (6 -> 12 of 48).
  - **Conflict to put to Sergeant D:** his stated reason (a faster warm-up) is not borne out by the model. That is for
    him to weigh; this report does not overrule his decision.
- **Entry 95** (gains rounded to 3 decimals, Ki and Kd from the rounded Kp): **agrees**. The port does the same, and
  the factor is applied before the rounding.
- **Entry 212** (reference stays 7beaaf5): deviated from for the kd factor only, on Sergeant D's authority (entry 279).
  Nothing in this study changes that.

## 7. Draft message for Sergeant D (one question, new evidence on entry 279)

> <@821036882946424852> We ran your KD change (entry 279, 0.07 -> 0.063) in our simulator: 1728 runs, 8 house types,
> radiators and underfloor, cold starts 2.5 K and 4 K below target. Your diagnosis holds: during warm-up the D term
> pulls the setpoint down by 1.2-2.9 C. But 0.063 makes the warm-up only 1-2 % faster, about 1-3 minutes, and -20 %
> gives 2-4 %. The reason in the model: after the first half hour the boiler runs at its minimum power or in PWM, so
> 0.25-0.5 C more setpoint adds no heat. In heavy houses the cold walls set the pace. 0.063 is close to harmless: no
> change in overshoot or comfort, but in one house type the deadband comes 0.8 min later, and in another one run went
> from 77 to 84 starts a day.
> One question: which do you want?
> A) Keep 0.063 as you decided in entry 279. Small effect, almost no harm. We implement it in the firmware as planned.
> B) Back to 0.07, as thermo-nova (`pid.py:93` at 7beaaf5). No firmware change and no deviation from thermo-nova,
>    because the warm-up gain is too small to justify one.
> C) Keep 0.07 for now and look at the minimum-power / PWM phase of the warm-up, where the model says the time is lost.
>    That means a new simulation first, and no firmware change yet.
> Our recommendation is B, combined with the study in C: our pre-registered rule says keep KD, since no house type got
> 10 % faster. A is your call and does almost no harm, if you prefer it.

## 8. Not verified

- The result is not checked on a real boiler or house: no bench or field run.
- A night setback with a warm wall, a modulating boiler between Pmin and full power, and curve coefficients other than
  1.6 are not simulated.
- The mechanism in section 4 rests on two traces, not on the whole grid.
- The faithfulness test covers d0 = 0 with no gain input. The cold starts (d0 != 0) and the guard scenario with a gain input
  (gain_step_8) are not compared with a committed 1412.16 or 1412.19 row.

## 9. What was posted (2026-11-05, after this report)

6b (lead session) posted the numbers verbatim in #dev-sat-mqtt (msg 1567864588442009651), with one change: the
recommendation is **C, defined as keep his 0.063 and study the minimum-power / PWM phase of the warm-up**, not B.

- **Departure from the pre-registered rule.** The posted recommendation is not what the rule gives: the rule's outcome
  is "keep KD". The departure is deliberate, for two reasons.
  - Robert's standing rule is "best result, not parity": thermo-nova is a reference, not a mandate. The reason given
    for B in section 7 ("no deviation from thermo-nova") is a parity argument and does not count under that rule.
  - 0.063 is Sergeant D's design choice (entry 279), and this run shows almost no harm from it: the two effects of its
    own are in section 2.
- **What the departure does not change.** The study's finding stands: KD is not the lever for the warm-up in this
  model, and the time is lost in the minimum-power / PWM phase.
- **Option C changed meaning.** Section 7's draft defined C as "keep 0.07 and study". The posted C keeps 0.063 instead.
- **Firmware task.** The firmware task for entry 279 is TASK-1656 (lead lane).
