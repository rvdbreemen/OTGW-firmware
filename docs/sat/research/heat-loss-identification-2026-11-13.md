# Heat-loss and heating-curve identification from captures, and a tuner convergence test (TASK-1883)

Date: 2026-11-13. Source lessons: `docs/sat/vtherm-comparison-2026-11-13.md`, lessons 4 and 5.

This is evidence tooling, not a SAT design document. Nothing here proposes a change to SAT behaviour or a coefficient
for any unit. Every number from a capture is **EXPLORATORY**.

## Summary

| AC | Result |
|---|---|
| 1. Library comparison | Six candidates assessed and one more noted, all on primary sources (PyPI, CRAN, GitHub API). Choice: **base on numpy**. The fit is a 3-parameter linear least squares plus a moving-block bootstrap, about 30 lines. statsmodels plus arch is the upgrade path; ctsmTMB (R) is the tool for rigorous grey-box work. |
| 2. Prototype | `docs/sat/research/heat-loss-id/heat_loss_id.py` was run on a 31.5 h capture (two contiguous gruijter exports, 114 flame cycles, 64 bins) and on a 9.2 h capture. **No valid estimate.** The full model fails the physical guard (g < 0). The q = 0 variant passes every guard, but its 95 % CI on the coefficient at 5 C outside is [-1.40, 2.57], which covers every plausible coefficient. The 9 h capture is below the 40-cycle and 40-bin minimums. |
| 3. Convergence test | `tests/test_sat_tuner_convergence.py` compiles the real tuner blocks and the real `satCalcHeatingCurve()` and closes the loop with a 1R1C house over 30 to 70 simulated days. **Green on the real tuner** (5 tests, OK). **Red on a sign-flip mutant** (diverges to the clamp, 4 failures) and **on a 10x-step mutant** (12 to 18 direction changes, 4 failures). |

## AC1: candidate comparison

Checked on 2026-11-13 against PyPI JSON (`https://pypi.org/pypi/<name>/json`), CRAN (`https://crandb.r-pkg.org/<name>`)
and the GitHub REST API. "Fit" is judged for our case: offline, laptop-side analysis of capture CSVs. Nothing runs on the device.

| Candidate | Latest release | Licence | Dependencies | Fit |
|---|---|---|---|---|
| **numpy** (`numpy.linalg.lstsq`, `cond`) | 2.5.4, 2026-10-10 (2.4.6 installed here) | BSD-3-Clause (+0BSD, MIT, Zlib, CC0 parts) | none | Covers the whole method: least squares, condition number, percentiles. The block bootstrap is about 15 lines. Installed in the system Python 3.12 on this laptop (the analyze-capture environment itself was not checked). |
| **scipy** (`optimize.least_squares`, `curve_fit`) | 1.18.1, 2026-08-21 (1.15.2 installed) | BSD (OSI classifier) | numpy | Needed only for a non-linear model, such as the radiator exponent n ≈ 1.3 or bounded parameters. Not needed for the linear model. Already installed. |
| **statsmodels** (OLS, robust and HAC errors) | 0.15.0, 2026-08-27 | BSD-3-Clause | numpy, scipy, pandas, patsy, formulaic, packaging | Adds diagnostics we would otherwise write by hand: HAC standard errors for autocorrelated bins, the condition number, influence measures. Not installed here. A sensible next step if the analysis grows. |
| **arch** (`MovingBlockBootstrap`, `StationaryBootstrap`) | 9.0.0, 2026-11-11 | NCSA | numpy, pandas, scipy, statsmodels | A tested block bootstrap with optimal block-length selection. It pulls in statsmodels, so it makes sense only together with that. |
| **ctsmTMB** (R, continuous-time stochastic grey-box models) | 1.1.1, CRAN 2026-07-14 | GPL-3 | R, TMB, RTMB, Rcpp, Matrix, ggplot2 and more | The method built for building thermal identification (the successor line of CTSM-R). The right tool for a rigorous RC-model study with process noise. It needs an R toolchain, and GPL-3 is fine for an offline tool but not for code shipped in the repo. |
| **locie/pySIP** (Python, stochastic state-space building models) | v1.0.0, 2023-08-17 (repo pushed 2026-11-09, 16 stars) | **No licence file found** (LICENSE, LICENSE.md, LICENSE.txt, COPYING, LICENCE all 404; the API reports no licence) | pymc, pytensor, numba, arviz, pydantic 1, pins numpy ^1.23 | Methodologically a good match. It is ruled out by the missing licence (without one, no reuse rights) and by its numpy 1.x pin and heavy Bayesian stack. |
| BuildingsPy 5.2.2 (2026-10-08, BSD-3) | | | | A Modelica simulation helper, not an identification library. Listed only because a search for building-model packages turns it up. |

Note: the PyPI package `pysip` (1.1.0, 2019, Apache 2.0, IntegralDefense) is an unrelated SIP-protocol package, not locie/pySIP.

**Recommendation: base on numpy, build the small rest.** The model is linear in three parameters. Adding statsmodels
and arch would bring five transitive packages for one lstsq call and one resampling loop. The trade-off flips when we
need HAC errors, model selection or a non-linear emitter law. Then adopt statsmodels plus arch, both permissively
licensed and actively released. If identification becomes a real study instead of a prototype, use ctsmTMB offline in R.

## AC2: prototype (EXPLORATORY)

Script: `docs/sat/research/heat-loss-id/heat_loss_id.py` (numpy only). Results: `docs/sat/research/heat-loss-id/results/*.json`.

### Method

This follows the VTherm approach (tpi_identification.py:98-198, as summarised in the comparison document), adapted to
the web-UI graph export (`epoch_ms, flow, ret, chSet, roomSet, room, outside, mod, flame, dhw_on, ch`, 5 s samples).

- Each 20-minute bin gives one equation: `dTin/dt = g (Tout - Tin) + B u + q`.
  - `dTin/dt` is the least-squares slope of the room temperature over the bin, in K/h.
  - `u` is the mean of `(flow - room)` while CH is enabled, else 0.
- Bins are excluded when they contain hot water or have less than 80 % sample coverage.
- Guards:
  - 0 < g < 1 /h;
  - B > 0;
  - condition number < 1000;
  - at least 40 bins and at least 40 flame cycles;
  - the 70/30 chronological hold-out must beat a zero-slope model.
- The CI comes from a moving-block bootstrap over bins: block of 3 bins (1 h), 2000 resamples, percentile 95 %.
  - It is reported twice. `ci95_unconditional` uses every resample with B > 0. `ci95_guarded` uses only resamples
    inside the guards, so it is a conditional interval.
- Mapping to the coefficient: at room temperature T* = 19 C the steady state needs
  `Tflow_req = T* + (g (T* - Tout) - q) / B`.
  - `c*` is the coefficient at which the firmware curve (`satCalcHeatingCurve`, radiators, base 27.2) gives `Tflow_req`.
  - The base 27.2 and the curve form were checked against the 2026-11-12 SATTICK lines. curve = 30.20 at target 19.0 and
    outside 6.6 matches coefficient 1.8 after the 0.1 C rounding.
- Variant `--no-q` sets q = 0. That also drops sun and internal gains, so it is a constrained model, not a better one.

### Data

| Capture | Duration | Flame cycles | Bins | Outside range |
|---|---|---|---|---|
| gruijter 2026-11-10 + 2026-11-11 graph exports (contiguous, 165 s gap) | 31.5 h | 114 | 64 | 2.6 to 8.0 C |
| gruijter 2026-11-12 afternoon graph export (beta.214) | 9.2 h | 23 | 23 | 7.2 to 9.5 C |

The 9 h capture is too short. With 23 cycles and 23 bins it is below both 40-minimums, and the hold-out does not beat
the zero model. Any interval from it describes the noise, not the house.

### Results

Commands, run from the scratchpad with the capture paths as arguments:
`python -I heat_loss_id.py [--no-q] <1110.csv> <1111.csv>` and `python -I heat_loss_id.py [--no-q] <1112pm.csv>`.

| Run | g /h | B /h | q K/h | cond | hold-out RMSE (zero model) | Guards | c* at Tout 5 C (95 % CI, unconditional) |
|---|---|---|---|---|---|---|---|
| 31.5 h, full | -0.0056 | 0.0166 | -0.235 | 234 | 0.504 (0.538) | **fail**: g < 0; 969 of 2000 resamples outside the guards | 0.56 [-1.23, 2.60] |
| 31.5 h, q = 0 | 0.0103 | 0.0159 | 0 | 2.6 | 0.503 (0.538) | all pass, but the hold-out beats the zero model only marginally | 0.40 [-1.40, 2.57] |
| 9.2 h, full | -0.2125 | -0.0010 | -2.217 | 387 | 0.229 (0.204) | **fail**: g, B, bins, cycles, hold-out | undefined (B < 0) |
| 9.2 h, q = 0 | -0.0017 | 0.0054 | 0 | 2.6 | 0.254 (0.204) | **fail**: g, bins, cycles, hold-out | -5.75 [-63.8, 4.56] |

Reading, all EXPLORATORY:

- Under the guard rule we copy from VTherm, the full model gives **no valid estimate** on either capture.
- The q = 0 variant passes the guards on the 31.5 h capture. But its coefficient interval at 5 C, [-1.40, 2.57],
  covers every coefficient anyone has proposed. So it cannot tell 1.12 from 1.8, and this report does not try to.
- The fit explains about 20 % of the variance (r² 0.20). The rest is setback transients, wall mass and sensor quantisation.
- The q = 0 c* changes with outside temperature: 0.40 at 5 C, 1.03 at 0 C, 1.19 at -5 C. The linear house and the
  quadratic curve do not have the same shape. The 0 C and -5 C values lie outside the measured 2.6 to 8.0 C, so they are extrapolation.

### Why the estimate is weak (caveats)

1. **c* maps mean flow onto the curve setpoint.** That holds only while the boiler modulates continuously. At these mild
   temperatures the boiler cycles (flame on 17 % of the 9 h capture). The mean flow is then set by the load, not by the
   curve setpoint, so mean flow and setpoint are different quantities.
2. **Weak identification at mild weather.** One coefficient step moves the curve by `cv/4` per unit. At 5 C outside and
   19 C target cv ≈ 8.75, so a 1 K error in `Tflow_req` already moves c* by about 0.46.
3. **Little excitation.** Outside temperature varies only 5.4 K in 31 h. `Tout - Tin` is then close to a constant and
   correlates with the intercept, which is why the full model trades g against q (g < 0, q < 0).
4. **The model is too simple.** The setback transients (roomSet 15 to 19.5 C) and the wall mass break the single time
   constant. A 2R2C model or a stochastic grey-box model (ctsmTMB) is the remedy, and it needs more data.
5. **The emitter is linear here.** Radiator output goes as ΔT^1.3, and the drive `u` uses flow only (ret was missing in parts).
6. **Too short.** VTherm asks for at least 40 cycles. A useful coefficient estimate also needs a range of outside
   temperatures, so several days, preferably including cold ones.

What would make this useful: a capture of several days with outside temperatures down to about 0 C, steady targets
(no setback), and the boiler's MsgID 17 modulation plus its rated power for an energy-based `u`.

## AC3: tuner convergence test

File: `tests/test_sat_tuner_convergence.py`. It imports `extract()`, `STUBS` and `compile_cpp()` from
`tests/test_sat_curve_selftune.py`, which cut the real `satCurveTune` blocks out of `SATcontrol.ino`. It also cuts
`satCalcHeatingCurve()` and the `SAT_HC_BASE_OFFSET_RAD` / `SAT_HC_REF_TEMP` constants out of the same file. The
tuner is not re-implemented.

Before writing it, I checked the existing coverage. `tests/test_sat_sim_boiler_model.py` tests the simulated boiler
(steady burn, cycling at low load, the self-tune minimum modulation in simulation). `tests/test_sat_closedloop_sim_1412_19.py`
tests the low-load closed loop. Neither runs the tuner for weeks against a known correct coefficient.

### Design

- **House (fixture).** A 1R1C house, `dTin/dt = K (Tflow - curve(c_true, Tin, Tout)) + gains(t)`, with K = 0.1 /h/K.
  - By construction the room is in equilibrium exactly when the flow equals the real firmware curve at `c_true`.
    So the correct coefficient is known.
  - The time constant is 1/(K c_true), about 5.5 h at 1.8.
- **Controller (fixture, not the firmware PID).** A PI controller around the curve with Kp = 4, Ki = 0.4 /h and
  anti-windup at ±15 C. The flow is clamped to 10 to 75 C.
  - The tuner's signal is `flow - curve`, which matches the firmware definition `clamp(pidRaw) - curve`.
  - At steady state the PI terms carry `curve(c_true) - curve(c)`.
- **Disturbances.** The outside temperature follows a daily sine (0 ± 3 C). There is an evening gain of +0.15 K/h
  (18:00 to 21:00) and a morning loss of -0.15 K/h (06:00 to 09:00), so both ADR-228 phases get ticks every day.
  The sun is kept at 5°, so no hour is excluded for sun.
- **Loop.** One control tick every 60 s calls `satCurveTuneNoteTick()` and `satCurveTuneService()`. The tuner writes the
  coefficient through the stubbed `updateSetting()`, and the next tick's curve uses the new value.
  - The 70-day scenario crosses the 49.7-day wrap of the ms clock, as a device would.
  - The window file goes to a temp directory.
- **Convergence predicate.** The tolerance is derived from the band, not tuned by eye.
  - The tuner holds once |signal| ≤ 1.0 C. One 0.1 step moves the signal by 0.1/4 · cv. With target 20 C and Tout ≈ 0 C,
    cv = 20, so the tuner may legitimately stop within 4 · 1.0 / 20 = **0.20** of `c_true`.
  - Required: the first move goes toward `c_true`, there is no direction change, the final value is within 0.20, and
    there is no change in the last 15 days.
- **Mutants.** They are applied in memory to the extracted text. Each replacement must match exactly once, and the repo
  source is never touched.
  - `sign`: the raise branch steps down and the lower branch steps up.
  - `step10`: the step is 1.0 instead of 0.1. That moves the signal by about 5 C, more than the band can absorb.
  - `test_each_mutant_fails_the_convergence_predicate` asserts that both fail. `OTGW_TUNER_MUTANT=sign|step10` builds
    the whole suite from a mutant for the raw red run.

### Evidence

Green on the real tuner, `python tests/test_sat_tuner_convergence.py` (MSVC via vswhere, not skipped):

```
test_both_phases_get_data_and_the_decisions_come_from_agreeing_phases ... ok
test_converges_without_oscillation ... ok
test_each_mutant_fails_the_convergence_predicate ... ok
test_one_sided_disturbance_holds_safely_on_disagreeing_phases ... ok
test_tolerance_comes_from_the_band ... ok
Ran 5 tests in 24.837s
OK
  raise_1.0_to_1.8: final=1.70 steps=7 dir_changes=0 last_step_day=22 tol=0.20
  lower_2.6_to_1.4: final=1.50 steps=11 dir_changes=0 last_step_day=32 tol=0.20
  already_right: final=1.80 steps=0 dir_changes=0 last_step_day=- tol=0.20
  mutant sign raise_1.0_to_1.8: final=0.10 steps=9 dir_changes=0 -> ['first move away from c_true ((2, 1.0, 0.9))', 'final 0.10 not within 0.20 of 1.80']
  mutant sign lower_2.6_to_1.4: final=4.90 steps=23 dir_changes=0 -> ['first move away from c_true ((2, 2.6, 2.7))', 'still moving on day 68 of 70', 'final 4.90 not within 0.20 of 1.40']
  mutant step10 raise_1.0_to_1.8: final=2.00 steps=13 dir_changes=12 -> ['12 direction changes', 'still moving on day 57 of 60']
  mutant step10 lower_2.6_to_1.4: final=0.60 steps=16 dir_changes=14 -> ['14 direction changes', 'still moving on day 69 of 70', 'final 0.60 not within 0.20 of 1.40']
  one-sided raise_1.0_to_1.8: final=1.50 steps=5 reason=phases_disagree wmean3=0.87 fmean3=1.35
  one-sided lower_2.6_to_1.4: final=1.50 steps=11 reason=phases_disagree wmean3=-1.36 fmean3=-0.58
```

Through the project runner, `python scripts/run_tests.py --no-host tuner_convergence curve_selftune`:

```
PASS        39.3s  tests/test_sat_tuner_convergence.py  (5 tests)
PASS       116.8s  tests/test_sat_curve_selftune.py  (82 tests)
files: 2  passed: 2  failed: 0  timeout: 0  skipped: 0
```

Red on the sign-flip mutant, `OTGW_TUNER_MUTANT=sign python tests/test_sat_tuner_convergence.py`, exit 1:

```
FAIL: test_converges_without_oscillation (scenario='raise_1.0_to_1.8')
AssertionError: Lists differ: ['first move away from c_true ((2, 1.0, 0.[38 chars].80'] != []
FAIL: test_converges_without_oscillation (scenario='lower_2.6_to_1.4')
FAIL: test_one_sided_disturbance_holds_safely_on_disagreeing_phases (scenario='raise_1.0_to_1.8')   AssertionError: -1 != 1
FAIL: test_one_sided_disturbance_holds_safely_on_disagreeing_phases (scenario='lower_2.6_to_1.4')   AssertionError: 1 != -1
Ran 5 tests   FAILED (failures=4, skipped=1)
  raise_1.0_to_1.8: final=0.10 ...   lower_2.6_to_1.4: final=4.90 ...
```

Red on the 10x-step mutant, `OTGW_TUNER_MUTANT=step10 python tests/test_sat_tuner_convergence.py`, exit 1:

```
FAIL: test_converges_without_oscillation (scenario='raise_1.0_to_1.8')
AssertionError: Lists differ: ['12 direction changes', 'still moving on day 57 of 60'] != []
FAIL: test_converges_without_oscillation (scenario='lower_2.6_to_1.4')
AssertionError: Lists differ: ['14 direction changes', 'still moving on [47 chars].40'] != []
FAIL: test_one_sided_disturbance_holds_safely_on_disagreeing_phases (both scenarios)   AssertionError: 16 != 0 / 18 != 0
Ran 5 tests   FAILED (failures=4, skipped=1)
```

(The skip in the mutant runs is the in-suite mutant check, which is pointless when the suite itself is built from a mutant.)

### Observation from the fixture (for routing, not a conclusion)

With the evening gain pulse alone (no morning loss), the warm-up ticks fall mostly on the overshoot side. The warm-up
mean then sits below the floating mean.

- **Raise case.** The real tuner stops at 1.50 with `phases_disagree`: warm-up +0.87 (inside the band), floating +1.35
  (says higher). That is 0.30 short of `c_true` = 1.8, outside the 0.20 band tolerance.
- **Lower case.** It also stops at 1.50, 0.10 from `c_true` = 1.4. Warm-up says lower (-1.36), floating holds (-0.58).

This is consistent with the ADR-228 agreement rule as recorded (entries 303-305) and with the floating-phase vote of
ADR-230 (entries 313 and 315; the fixture shows `fvote=1`): no change unless both voting phases agree. The test asserts only the safety properties for this case: never away from `c_true`, no direction change, and
closer than the start. The stopping point is not pinned.

Whether a one-sided disturbance pattern like this occurs in real houses, and whether it matters, is a question for
Sergeant D. This report draws no conclusion about it.

## What was NOT verified

- **Library candidates were not run.** statsmodels, arch, ctsmTMB and pySIP were compared on metadata (release, licence,
  dependencies, repository files), not by running them on our data. statsmodels and arch are not installed here.
- **pySIP's licence.** No licence file was found. I did not check the source headers or ask the authors.
- **CTSM-R itself.** I did not check it. `ctsmr` is not on CRAN; only ctsmTMB is.
- **The prototype's physics.** It is not validated against a known house: no ground truth for g, B or the coefficient
  exists for the gruijter house. The base offset 27.2 and coefficient 1.8 are cross-checked against one SATTICK line
  only, and the earlier captures may have run on a different coefficient (1.12 before entry 301).
- **The heat input proxy.** `u = (flow - room)` while CH is on is an assumption. Pump state, MsgID 17 modulation and
  boiler power were not used.
- **Single-house closed loop only.** The convergence test uses one house structure, one PI fixture and one disturbance
  pattern (plus the one-sided variant). Other gains, emitter laws, setback schedules, PWM operation, hot-water hours,
  sun exclusions and heat pumps were not simulated.
- **The firmware PID was not used.** The PI controller is a fixture, not the firmware PID. The test does not show how
  the firmware PID's own integral handling interacts with the tuner; this report makes no claim about that handling.
- **The call sites.** The two call sites in `satControlLoopRun()` that build the tick input are not reached here, as in
  the existing selftune test. Only the signal definition was mirrored.
- **No bench or field run.** Nothing in this report ran on a device.
