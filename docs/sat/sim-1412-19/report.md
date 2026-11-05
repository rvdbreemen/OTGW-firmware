# TASK-1412.19: closed-loop low-load simulation (pre-registered)

Simulation of SAT's modelled control logic against documented plant models, 2026-11-05. **No result here is physical
boiler or compressor proof (AC3).** A pass or fail is about the model of SAT (section 1) on these plant models.

| Item | Value |
|---|---|
| Thresholds and classes | `thresholds.md`, committed and pushed as `b3b096a1a` (2026-11-05 00:19 +01:00) before any run; unchanged |
| Script | `scripts/sat_closedloop_sim_1412_19.py`, sha256 `82f8aad5a992...` for the scored run (full value in `runs.json` `meta`); the committed version adds only the `S6_DIAG` diagnostic list used for section 3, which does not change any scored value |
| Model base | `scripts/sat_lowload_study_1412_16.py` sha256 `01031b894cd0...`, port `scripts/sat_sim_port.py` sha256 `361b1ca6129d...` |
| Firmware the model follows | origin/dev `a16184306`; port proven equal (curve, PID, PWM mapper) by `tests/test_sat_sim_port_equivalence.py` 4/4 |
| Raw output | `runs.json` (1440 runs, every metric, event and seed per run) (the per-class `parts/` files the merge reads are not committed; they hold the same runs) |
| Reproduce | `python scripts/sat_closedloop_sim_1412_19.py --out docs/sat/sim-1412-19 --classes <list>` then `--merge` |
| Checks | `python tests/test_sat_closedloop_sim_1412_19.py` 5/5 OK: same seed gives identical output (H5), and each hard check H1-H4 goes red on a mutant that removes the behaviour it guards (hour limit, CS clamp, silence after hand-back, curve-only on a lost room) and green without it |

## 1. What was simulated

Grid: 9 classes x eras 1983-92 / 2015 x radiators / underfloor x fixed / variable flow x Pmin 3.5 / 5.5 kW (7.0 kW in
the minimum-power class) x MM honoured / ignored x seeds 1-5 = 1440 runs of 48 h, first 6 h not scored. Classes,
behaviours and their sources are as pre-registered in `thresholds.md`.

**Model fixes after a smoke run, before the scored run.** A 2-seed smoke run (not scored, not kept) showed H1 and H2
violations that came from the 1412.16 model, not from the firmware. The model was changed to follow the firmware, the
thresholds were not touched:
1. The hourly limit of 3 PWM starts also applies to the PWM entry and survives a PWM reset (TASK-1401,
   `SATcontrol.ino:1652-1656`). thermo-nova differs here: its `pwm.reset()` sets `_current_cycle = 0`
   (`pwm.py:87-90`), so a PWM re-entry starts a fresh count. The firmware's choice is the stricter one.
2. Every CS is clamped to the effective maximum (`SATcontrol.ino:1645-1646`). Without it the suppression setpoint
   (flow - 1) right after a hot-water draw, with the flow sensor still at about 60 C, exceeded the 50 C underfloor limit.
3. The ON time restarts at the first ignition of the phase (TASK-1382; thermo-nova `pwm.py:184-193`).
The 1412.16 study has the first two gaps too; its comfort figures do not depend on them, its start counts may be
slightly high in the runs with repeated PWM re-entries.

## 2. Result against the pre-registered thresholds

| class | runs | H1-H4 fail runs | S1 | S2 | S3 | S4 | S5 | S6 | class pass |
|---|---|---|---|---|---|---|---|---|---|
| very_low_load | 160 | 0 | 100 % | 100 % | 100 % | 100 % | - | 76 % | no |
| thermal_lag | 160 | 0 | 89 % | 100 % | 100 % | 100 % | - | 93 % | no |
| minimum_power | 160 | 0 | 88 % | 100 % | 100 % | 100 % | - | 81 % | no |
| trv_pump | 160 | 0 | 66 % | 100 % | 35 % | 49 % | 86 % | 46 % | no |
| dhw | 160 | 0 | 91 % | 100 % | 100 % | 100 % | - | 82 % | no |
| ignition | 160 | 0 | 94 % | 100 % | 100 % | 100 % | - | 80 % | no |
| room_fault | 160 | 0 | 91 % | 93 % | 100 % | 100 % | 95 % | 74 % | no |
| outside_fault | 160 | 0 | 91 % | 100 % | 100 % | 86 % | 100 % | 79 % | no |
| transport | 160 | 0 | 91 % | 100 % | 100 % | 99 % | 100 % | 79 % | no |

Percentages are the share of runs that meet the soft threshold; the class passes when every hard check holds in every
run and every soft threshold in at least 90 % of the runs. **Every hard threshold (H1-H5) holds in all 1440 runs. No
class passes**, because of the soft thresholds below. As pre-registered, nothing was re-run with other inputs.

## 3. The failures, and what causes them

**S6 (PWM ON phases with less than 180 s of flame) fails in every class.** A second pass over all 337 failing runs
(diagnostic only, same inputs; the cause is recorded per short phase) separates real short burns from phases where the
burner keeps burning after the phase ends:
- 1331 short phases end on the ON timer with a real burn under 180 s; 1204 of them are in the `trv_pump` class. With the
  TRVs closed to 15 % or the pump at half flow, the flow jumps at ignition, the boiler's own high limit (stop at
  CS + 5 K, an assumption, see section 4) trips against the ignition hold setpoint (return + 18, heating_control.py:385-448),
  and the burner cannot relight against the suppression setpoint (flow - 1) for the rest of the phase.
- 248 short phases are cut by the cold cutoff (PID output falling to the 28.2 / 21.0 cold setpoint within the first
  minutes of an ON phase), mostly in very low load. This follows the decided rule: the cold cutoff switches PWM off at
  once (entries 12, 199); it does not wait for the 180 s minimum.
- 185 phases end on a PWM exit (to continuous) or the cutoff while the burner keeps burning; those are not short burns
  (124 + 61), so S6 as pre-registered over-counts by about 10 %. The threshold stays as registered; the report shows both.
- 55 real short burns end on a PWM exit.

**S1 (share within +-0.3 K below 80 %)** comes mostly from one plant: underfloor, fixed flow, 2015 house (111 of its 180
runs; median share 0.73, median overshoot +0.35 K). The curve balanced at 5 C over-heats this light-load underfloor house
on mild days; the same plant was SAT's weakest in TASK-1412.16. In `trv_pump` it is the TRV closure itself.

**S3 and S4 in `trv_pump`**: median undershoot -1.23 K (worst -2.96 K) and median 72.8 boiler starts per day (worst 289).
The forced 2 h TRV closure starves the room by construction (in reality TRVs close because rooms are warm), and the
half-flow pump makes the boiler's own control short-cycle in continuous mode, where SAT does not decide the starts.
The scenario is harsher than real TRV behaviour; it is reported as registered.

**S4 in `outside_fault`** (86 %): after the hand-back the room thermostat behind the gateway runs the boiler
(assumption, the TPI baseline at a fixed 60 / 40 C), which starts more often than SAT.

**S2 in `room_fault`** (93 %): while the room reading is lost SAT runs on the curve only (entries 38, 46); in mild
weather that overshoots up to 0.6 K and more in some runs before the reading returns. Recovery within 4 h (S5) holds in
95 % of the runs.

## 4. Limitations (AC3)

- Everything in section 1 of `docs/sat/sim-1412-16/report.md` applies: one reference house per era, single zone, no
  field validation of the two-node model, generic boiler control, no transport dead time, simple TRV law.
- The boiler's own high limit at CS + 5 K is an assumption that drives the S6 failures in `trv_pump`; a boiler with a
  higher limit or a pump overrun would not trip there. The 1412.16 sensitivity case (3 K) shows the direction.
- The fallback after a hand-back (a TPI room thermostat) and "the boiler keeps the last command during a silence" are
  assumptions; real thermostats and boilers differ.
- During a hot-water draw the boiler is modelled as hot-water only with the flow sensor at 60 C; MM honoured or ignored
  makes little difference in these runs.
- The PWM IDLE state after a refused entry is modelled as CS=10/CH=0; in thermo-nova and the firmware the flame-off hold
  applies but CH is 0 (satChEnableDecide), so the boiler stays off either way.
- A simulator pass is never physical proof; these results feed TASK-1412.20/.21 (bench simulation) and the field plan.

## 5. Acceptance criteria

- **AC1** (candidate controller against documented plant models for very low load, thermal lag, minimum power,
  pump/TRV changes, DHW, delayed ignition and sensor/transport faults): sections 1 and 2, 9 classes, 1440 runs.
- **AC2** (pre-registered comfort, cycling, temperature and protection thresholds pass across parameter ranges and
  seeded fault runs; inputs/seeds/raw traces reproducible): thresholds pre-registered (`b3b096a1a`), all hard thresholds
  pass in every run, reproducibility proven (H5 test). **The soft thresholds do not pass in any class** (section 2):
  AC2 is not met. Its failures are recorded with their causes (section 3).
- **AC3** (limitations and failed scenarios recorded; no simulator result labelled physical proof): sections 3 and 4.

Inputs for Sergeant D (observations, not proposals; one question at a time through docs/sat/open-questions.md):
the cold cutoff can end a PWM ON phase after less than 180 s of flame (entries 12, 199 vs the 180 s minimum of entries
31, 80); and with throttled flow the return + 18 ignition hold can trip a boiler's high limit at once.
