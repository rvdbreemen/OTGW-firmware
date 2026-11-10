# TASK-1784: KD factor 0.05 against today's 0.063 for the warm-up (simulation)

**Result in one line.** Lowering the KD factor from 0.063 to 0.05 makes the warm-up 0.6-3.4 % faster in the median
(1.6-4.6 minutes on 1.5-7 hours). By the pre-registered rule the recommendation is **keep 0.063**: no class reaches
the 10 % warm-up gain. The change also does no measurable harm: no guard fails because of 0.05.

- **Order:** Sergeant D, #dev-sat-mqtt 2026-11-10 19:33 UTC, msg 1569781945460727873, entry 297 (relayed, not seen
  first-hand): "run a simulation with derivative coefficient 0.5 instead of 0.63".
- **Our reading:** 0.5 vs 0.63 means 0.05 vs 0.063, the `SAT_PID_KD_FACTOR` of `SATpid.ino:28`. The ratio is the
  same under either reading (0.794), so the result is the same.

Simulation only: no result is physical boiler proof.

- **Pre-registration:** `docs/sat/sim-1784/thresholds.md`, committed e9883eb93 before the first grid run. It reuses
  the TASK-1655 rules with 0.063 as the baseline.
- **Data:** `docs/sat/sim-1784/runs.json`: 1152 runs, with the sha256 of each script in its meta.
- **Scripts:** `tools/research/sat/sat_kd_005_sim.py` runs the TASK-1655 model with `kd = round3(K * 8400 * kp)`,
  the firmware form `SATpid.ino:223`. `tools/research/sat/sat_kd_005_chart.py` makes the chart.
- **Test:** `tests/test_sat_kd_005_sim.py`, 3/3 OK.

## 1. Fair comparison and validity

- **Same model as TASK-1655.** All 576 baseline (0.063) runs reproduce the committed TASK-1655 x0.9 runs exactly
  (`runs.json` meta `baseline_vs_1655_x09`: compared 576, mismatches 0). Same plant classes, warm-up grid (outdoor
  -5/5/12 C, start 2.5 or 4 K below target, flow, Pmin, MM) and guard scenarios.
- **Validity check (pre-registered).** In the radiator classes, the median D term during the approach at baseline is
  between -1.07 and -2.65 C on the setpoint. The rule requires <= -1.0 C, so the check **passed**: the model contains
  the D term holding back the warm-up that Sergeant D describes.

## 2. Result per plant class

- **B:** the median paired reduction of the time to within 0.3 K; the pre-registered threshold is 10 %.
- **"Fails also at 0.063":** the baseline run on the same inputs breaks the same absolute limit, so the change is not
  the cause. This is analysis after the run, as in TASK-1655.

| Plant class | B | Minutes saved (median) | Change in time to the deadband (median) | Guards failing |
|---|---|---|---|---|
| 1983-92 light rad | 1.7 % | 2.2 | -1.3 min | G2: worst run 84 starts/day, also 84 at 0.063 |
| 1983-92 light ufh | 3.3 % | 3.0 | -4.0 min | none |
| 1983-92 heavy rad | 1.2 % | 3.8 | -3.1 min | none |
| 1983-92 heavy ufh | 3.4 % | 4.6 | -6.0 min | none |
| 2015 light rad | 2.3 % | 3.4 | -4.6 min | G6: guard-run overshoot 0.88 K, 0.90 at 0.063 |
| 2015 light ufh | 1.0 % | 1.6 | -2.0 min | G1, G6: overshoot 1.12 / 1.50 K, 1.10 / 1.49 at 0.063 |
| 2015 heavy rad | 0.6 % | 2.7 | -3.6 min | none |
| 2015 heavy ufh | 0.9 % | 2.5 | -3.2 min | G1, G6: overshoot 0.72 / 0.98 K, 0.71 / 0.98 at 0.063 |

- **Pass count:** 0.05 passes 0 of 8 classes. B holds nowhere, so the rule outcome is "keep 0.063: no 10 % warm-up
  gain in any class".
- **Harm:** none attributable to 0.05. Every paired median change in overshoot, starts per day, RMS after reaching
  0.3 K and guard-run comfort share is between -0.02 and +0.01. The time to the deadband (0.1 K) is 1-6 minutes
  shorter in every class.

## 3. What the chart adds

`kd_005_vs_0063.png`:

- **Left panel:** the paired gain per run. The classes are tight, with one exception: 1983-92 heavy radiators.
  - Single runs there move by up to 100 minutes in either direction, for example 219 to 321 min and 216 to 171 min.
  - The paired median gain in that class is still +3.8 min.
  - Our reading, not tested: 0.05 shifts when the burner's on/off cycling starts in that slow, heavy house. A shift
    of one cycle moves the 0.3 K crossing by a whole cycle.
- **Right panel:** the median gain per class against the 10 % threshold.

`table.json` also holds the unpaired class medians per factor, the committed 0.07 data included. Those medians can
jump between clusters of a grid with few distinct outcomes, for example 224 vs 249 min in 1983-92 heavy rad while
the paired gain is +1.2 %. The pre-registered rule scores only the paired numbers.

## 4. Compared with the earlier KD steps

TASK-1655 (`docs/sat/sim-1655/report.md`) measured 0.07 to 0.063 (x0.9) and found a gain of 0.3-2.0 %. This study
measures 0.063 to 0.05 and finds 0.6-3.4 %. Together, going from 0.07 to 0.05 shortens the warm-up by a few percent
in this model. KD is not the lever for a 10 % shorter warm-up here, and lowering it does no harm either.

## 5. Not verified

- Real boilers and houses: no field data with 0.05 exists.
- The firmware outside the modelled logic.
- Plants outside the 8 classes, and night setbacks shorter than an evenly cooled house (as in TASK-1655).
