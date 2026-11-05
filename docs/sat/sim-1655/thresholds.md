# TASK-1655: pre-registered thresholds and decision rule for the KD warm-up simulation

Committed **before the first run on the grid below** (AC1). Changing a metric, a threshold, the grid or the decision
rule after the first run is allowed only as a new, dated section at the bottom; the report then shows the result
against both the original and the changed rule. A variant that fails is reported as failing; it is never re-run with
other inputs until it passes.

Before this commit the only runs were the unit tests in `tests/test_sat_kd_warmup_sim_1655.py`. They use inputs that are
not in the grid (outdoor 8 C and 0 C, start offset -3 K, 6 h) plus one fault-free 1412.19 row at the target. Their
numbers were not looked at beyond pass/fail, and they only measured the run time (about 0.2 s per 24 h run).

## Question

Sergeant D (`docs/sat/sergeant-d-corrections.md` entries 277 and 278) attributes the slow warm-up to the derivative term,
not to the integral. The integral stays exactly as thermo-nova (entry 277). He proposes KD -10 %. We simulate
KD x0.9 and x0.8 against today (x1.0).

- thermo-nova at 7beaaf5, `custom_components/sat`:
  - `kd = round(0.07 * 8400 * kp, 6)` (`pid.py:84-93`).
  - `D = round(kd * raw_derivative, 3)` (`pid.py:109-114`).
  - The raw derivative is frozen inside the 0.1 C deadband (`pid.py:205-207`).
- Firmware: `SATpid.ino:219`, `kd = satPidRound3(0.07f * SAT_PID_AGGRESSION_V3 * kp)`.

**Variant formula** (what a firmware change under AC3 would implement):

```
kd = satPidRound3(KD_FACTOR * 0.07f * SAT_PID_AGGRESSION_V3 * kp)   // float32 product, then 3-decimal rounding
```

with KD_FACTOR 1.0 (today), 0.9 or 0.8. P, I, the deadband, the derivative filter and the rounding of the other gains
are unchanged. Only kd is multiplied.

## Model

- **Controller:** the TASK-1412.19 SAT (`scripts/sat_closedloop_sim_1412_19.py` class `Sat`).
  - Curve, PID and PWM mapper through `scripts/sat_sim_port.py`, which `tests/test_sat_sim_port_equivalence.py` proves
    equal to the firmware.
  - The rest is modelled with citations in 1412.16 and 1412.19.
- **Loop:** a fault-free copy of the 1412.19 run loop with a room start offset.
- **Plant:** the 1412.16 / 1412.19 plant.
  - Two-node house (`scripts/sat_curve_selftune_study3.py` `house()`), radiator or underfloor water node.
  - Boiler with minimum power and MM honoured or ignored.
  - Room reading rounded to 0.1 C.
- **Start state:**
  - Room = 20 + d0.
  - Wall at its steady state for that room.
  - Water at the curve minus 2 K.
- **Scope:** the start models a house that cooled down evenly (wall included), not a short night setback. No result is
  physical boiler proof.
- **Faithfulness:** the test proves that the copy with x1.0 and d0 = 0 reproduces a committed 1412.19 row.

## Grid

**Plant classes** (8, reported separately): era {3_1983-92, 5_2015} x mass {light, heavy} x emitter {rad, ufh}.
Radiators and underfloor stay separate classes because the Kp divisor differs (3 / 4) and so does the water mass.

Within each class, for each KD factor {1.0, 0.9, 0.8}:

- **Warm-up runs (48 per class):**
  - Grid: flow {fixed, variable} x Pmin {3.5, 5.5 kW} x MM {honoured, ignored} x steady outdoor {-5, 5, 12 C} x start
    offset d0 {-2.5, -4.0 K}.
  - 24 h each.
- **Guard runs (24 per class):**
  - Grid: the non-warm-up 1412.16 scenarios {steady_5, gain_step_8, falling_demand} x flow x Pmin x MM.
  - d0 = 0, 24 h each, scored from hour 2 (as 1412.16).
  - These exist because a KD change acts all the time, not only during warm-up.

Total 8 x 72 x 3 = 1728 runs. The model has no random input, so each variant run is paired with the baseline run on the
same plant and inputs.

## Metrics (per run)

| Metric | Definition |
|---|---|
| t03 | first minute with room - target >= -0.3 K; not reached in 24 h counts as 1440 min (and is flagged) |
| t01 | same for -0.1 K (the deadband edge) |
| t00 | same for >= 0.0 K (the tail inside the deadband: frozen D as offset, integral rising; reported, not scored) |
| max_over | largest room - target over the whole run (warm-up runs) |
| rms_after_0_3 | RMS of room - target from t03 to the end of the run |
| rms_all | RMS of room - target over the whole run (reported, not scored) |
| starts_per_day | boiler starts x 24 / run hours |
| H1, H2 | SAT PWM ON-phase starts in any rolling 60 min <= 3; CS sent <= the configured maximum (as 1412.19) |
| share_in_0_3_scored, max_over_scored | guard runs: share of minutes within +-0.3 K, and largest overshoot, from hour 2 |
| D_approach_mean | mean D term (C on the setpoint) over the PID ticks with error > 0.1 K before t01 |

## Decision rule

Per plant class and variant, all deltas are paired (variant minus baseline on the same inputs) and the statistic is
the median over the class's runs.

- **B (benefit):** the median relative reduction of t03 is >= 10 %.
- **G1:** max_over <= 0.6 K in every warm-up run, and the median increase of max_over is <= 0.1 K.
- **G2:** starts per day <= 72 in every run, and the median relative increase is <= 10 %.
- **G3:** the median increase of rms_after_0_3 is <= 0.02 K.
- **G4:** the median change of t01 is <= 0 min, and no run that reaches -0.3 K at baseline fails to reach it with the
  variant.
- **G5:** H1 and H2 hold in every run.
- **G6** (guard runs): the median decrease of share_in_0_3_scored is <= 0.02, and max_over_scored <= 0.6 K in every
  guard run.

A variant **passes a class** when B and G1-G6 all hold.

**Recommendation**, decided in this order:

1. **x0.8** if it passes >= 6 of the 8 classes and no guard G1-G6 fails in any class.
2. Otherwise **x0.9** on the same rule.
3. Otherwise **keep KD**:
   - if B holds in no class for either variant, the result is "KD is not the lever for the warm-up in this model";
   - otherwise it is "trade-off", and the report names the failing guards and classes.

**Validity check, reported before the recommendation:**

- Sergeant D's entry 278 example has D at about -2 to -2.5 C on the setpoint while the room warms at 0.5 C/h.
- The baseline counts as reproducing the starvation when the median D_approach_mean of the baseline warm-up runs is
  <= -1.0 C in the radiator classes.
- If it is not, the report says that this model cannot answer his question, whatever B shows.

## Changes after the first run

(none)
