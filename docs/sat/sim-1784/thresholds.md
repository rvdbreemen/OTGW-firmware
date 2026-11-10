# TASK-1784: pre-registration for the KD factor 0.05 simulation

Committed **before the first grid run** (AC1). The rules of `docs/sat/sim-1655/thresholds.md` apply unchanged
(metrics, grid, guards G1-G6, benefit B, validity check). This file only states what is different. A change after
the first run goes in a dated section at the bottom, and the report shows both rules.

## Order and reading

Sergeant D, #dev-sat-mqtt, 2026-11-10 19:33 UTC, msg 1569781945460727873 (relayed by the coordinator, not seen
first-hand): "run a simulation with derivative coefficient 0.5 instead of 0.63".

- **Our reading:** the derivative gain factor `SAT_PID_KD_FACTOR` (`src/OTGW-firmware/SATpid.ino:28`, 0.063 since entry 279 /
  TASK-1656) goes to 0.05.
- **Why the reading is safe:** 0.5 / 0.63 and 0.05 / 0.063 are the same ratio (0.794). The run is the same under
  either reading of the decimal point.
- **No other candidate:** no other derivative coefficient near 0.63 exists in `SATpid.ino`. The derivative filter
  alpha is adaptive, `dt / (interval + dt)` (`SATpid.ino:324`).

## Differences from TASK-1655

| Item | TASK-1655 | TASK-1784 |
|---|---|---|
| Baseline | factor 1.0 = 0.07 (thermo-nova `pid.py:93`) | **0.063** (firmware today, entry 279) |
| Variant | 0.9 and 0.8 times 0.07 | **0.05** (ratio 0.794 to the baseline) |
| Gain formula | `round3(f32(f) * f32(0.07) * 8400 * kp)` | `round3(f32(K) * 8400 * kp)`, the firmware form `SATpid.ino:223` |
| Runs | 8 x 72 x 3 = 1728 | 8 x 72 x 2 = 1152 |

- **Paired deltas:** 0.05 minus 0.063 on the same plant and inputs.
- **Fairness:** `tests/test_sat_kd_005_sim.py` shows that the 0.063 baseline reproduces committed TASK-1655 x0.9 rows
  exactly. The run also compares every one of its 576 baseline rows with the committed x0.9 rows and reports the
  number of mismatches. Any mismatch is reported, and the result is then not called a like-for-like comparison.
- **Context column:** the committed TASK-1655 x1.0 (0.07) numbers may be shown next to the results. They are not
  re-run and not scored.

## Recommendation rule

- **0.05:** recommended if it passes >= 6 of the 8 plant classes and no guard G1-G6 fails in any class.
- **Keep 0.063:** in every other case, with one of two reasons:
  - "no 10 % warm-up gain in any class" if B holds nowhere;
  - otherwise "trade-off", with the failing guards and classes named.
- **Guards that also fail at baseline:** named as such, as in the TASK-1655 report. That is analysis after the run,
  not part of the rule.

Simulation only: no result is physical boiler proof.

## Changes after the first run

- 2026-11-10, after the run: the order time above (19:33 UTC) is local time; the message id gives 18:33 UTC. Source detail only; no metric, threshold, grid or rule changed. The script docstring keeps 19:33 because its sha256 is pinned in `runs.json`.
