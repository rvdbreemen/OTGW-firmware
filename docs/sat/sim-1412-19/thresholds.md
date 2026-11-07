# TASK-1412.19: pre-registered thresholds for the closed-loop low-load simulation

Committed **before the first simulation run** (AC2). Changing a threshold, a scenario or a parameter range after the
first run is allowed only as a new, dated section below the original, and the report must then show the result against
both the original and the changed value. A run that fails a threshold is reported as a failure; it is never re-run with
other inputs until it passes.

Scope: the simulation tests SAT's control logic as modelled in `tools/research/sat/sat_lowload_study_1412_16.py` (curve, PID and PWM
mapper proven equal to the firmware by `tests/test_sat_sim_port_equivalence.py`; the rest modelled with citations,
see `docs/sat/sim-1412-16/report.md` section 1) against documented plant models. **No result is physical boiler or
compressor proof** (AC3), and a pass says nothing about the firmware outside the modelled logic.

## Scenario classes (AC1)

| Class | What | Firmware behaviour modelled (source) |
|---|---|---|
| very low load | steady outdoor 12 and 14 C; Pmin 7 kW boiler (oversized) | as 1412.16 |
| thermal lag | room sensor with a 15 min first-order lag (sensor in a far room); light and heavy mass; underfloor | as 1412.16 |
| minimum power | Pmin 3.5 / 5.5 / 7.0 kW | as 1412.16 |
| pump / TRV change | at a seeded time all TRVs close to 15 % for 2 h, then reopen; and a pump flow step to 50 % | as 1412.16 |
| DHW | seeded hot-water draws (2-15 min, 0-6 per day) while CH is asked: the boiler heats water only, DHW bit set | PWM ON timer paused during DHW and the start-up sequence restarted after it (TASK-1422, entry 95); sustained-switch guard of 300 s after the draw (TASK-1429; thermo-nova const.py:19) |
| delayed ignition | each start lights after a seeded 5-120 s delay; 1 in 10 starts fails and retries after 60 s | the ON phase is timed from ignition, SAT waits for the flame (TASK-1382, entry 80) |
| room sensor fault | room reading lost for a seeded 1-6 h, then restored | heating on the curve only, P/I/D reset (entries 38, 46; SATcontrol.ino:2298, :6943) |
| outside sensor fault | outside reading missing for longer than 1 h | SAT hands the boiler back (entries 56, 57; SATcontrol.ino:2407-2420) |
| transport fault | command delay 5-30 s (seeded) on every CS/CH write; and a 2 min OpenTherm silence | delay: the boiler acts on the old command meanwhile; silence over 30 s: hand-back once (CS=0, CH=1, MM=T), nothing sent until a boiler reply (entries 39, 47; SATcontrol.ino:3825-3859) |

Parameter ranges: eras 1983-92 and 2015; radiators and underfloor; fixed and variable flow; Pmin 3.5 / 5.5 / 7.0 kW;
MM honoured and ignored; seeds 1-5 for every seeded quantity (fault start, duration, draw pattern, ignition delay).
Each run is 48 h; the first 6 h are settling and not scored (except the warm-up check).

## Thresholds (AC2)

Hard thresholds fail the run on any single violation; soft thresholds are scored per run.

| Id | Kind | Metric | Threshold | Applies to |
|---|---|---|---|---|
| H1 | hard | SAT PWM ON-phase starts in any rolling 60 min | <= 3 (gas) | all |
| H2 | hard | control setpoint sent | <= the configured maximum (62 C radiators, 50 C underfloor) | all |
| H3 | hard | commands sent after an OpenTherm hand-back, before a boiler reply | 0 | transport silence |
| H4 | hard | requested setpoint while the room reading is lost | equals the heating curve (no P, I or D) | room fault |
| H5 | hard | same inputs and seed | byte-identical output | all |
| S1 | soft | share of scored minutes with the room within +-0.3 K of target | >= 0.80 | all except fault windows |
| S2 | soft | largest room overshoot above target in scored minutes | <= +0.6 K | all except fault windows |
| S3 | soft | largest room undershoot below target in scored minutes | >= -0.6 K | all except fault windows |
| S4 | soft | boiler starts per day | <= 72 | all |
| S5 | soft | minutes after a fault ends until the room is back within +-0.3 K | <= 240 | room, outside, TRV, transport faults |
| S6 | soft | PWM ON phases shorter than 180 s of flame (flame lost or DHW excluded) | 0 | all |

Pass rule per scenario class: every hard threshold holds in every run, and every soft threshold holds in at least 90 %
of the runs of that class. Failed runs and failed classes are listed with their inputs and seeds in the report.

## Changes after the first run

(none)
