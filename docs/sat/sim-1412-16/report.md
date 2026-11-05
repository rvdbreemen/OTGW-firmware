# TASK-1412.16: SAT duty estimate and PID against controlled plant models (simulation)

Simulation study, 2026-11-05. **No energy, COP or comfort improvement and no controller change follows from it (AC3).**
Any replacement of SAT's controller needs its own approved ADR and task; nothing here proposes one.

| Item | Value |
|---|---|
| Firmware commit the study models | `a16184306` (origin/dev, 2026-11-05) |
| Port proven equal to the firmware | `tests/test_sat_sim_port_equivalence.py` 4/4 OK on `b1fe73d2e`; `git diff b1fe73d2e a16184306` is empty for `SATpid.ino`, `SATcontrol.ino`, `SATchEnable.h`, `tools/research/sat/sat_sim_port.py`, so the proof holds for `a16184306` |
| Script | `tools/research/sat/sat_lowload_study_1412_16.py`, sha256 `01031b894cd0...` (full value in `runs.json` `meta`) |
| Port | `tools/research/sat/sat_sim_port.py`, sha256 in `runs.json` `meta` |
| Raw output | `runs.json` (864 runs + 54 sensitivity runs, per-period records), `traces.json` (three 24/12 h traces) |
| Reproduce | `python tools/research/sat/sat_lowload_study_1412_16.py --out docs/sat/sim-1412-16` (no random numbers; same inputs give the same bytes) |
| Checks | `python tests/test_sat_lowload_study_1412_16.py`: 6/6 OK (determinism, steady energy balance, energy closure in transient scenarios, PWM entry at low load, MM-honoured burner at Pmin, a mutant boiler that ignores the PWM OFF phase is detected) |

## 1. What "SAT" is in this study

| Part | Status | Source |
|---|---|---|
| Heating curve, PID (P, I with the 0.1 K deadband reset, held derivative), 60 s cadence | **proven** equal to the firmware | port, equivalence test |
| PWM duty-range mapper (on/off seconds per duty, 180 s minimum) | **proven** | port, equivalence test |
| Duty formula `(pid - base) / (Teff - base)`, Teff = flow at ignition, then 0.3/0.7 average for 180 s | modelled, not proven | SATcontrol.ino:1398-1439; thermo-nova pwm.py:130-168, 229-237 |
| Cold cutoff 28.2 (radiators) / 21.0 (underfloor), heat again above cold + 0.5 | modelled | SATchEnable.h `satColdCutoffLatched`, SATcontrol.ino:135; entries 12, 199 |
| Continuous to PWM after flow >= CS + 2 for 60 s with the flame on | modelled | entries 22, 37, 41 |
| ON phase: CS = return + 18 before ignition and for 20 s after it, then flow - 1; OFF phase CS = 10, CH = 0 | modelled | thermo-nova heating_control.py:385-448, const.py:151-153 (opened 2026-11-05); entries 5, 45 |
| 3 starts per hour, period 1200 s (gas) | modelled | entries 30, 52, 80 |
| Leave PWM after 300 s of saturated duty | modelled | thermo-nova heating_control.py:358-371 (SATURATION_SUSTAIN_SECONDS) |
| DHW, sensor loss, transport queues, presets, solar offset, multi-zone, MM=T at maximum duty | **absent** | out of scope; LL-012/LL-013 cover them elsewhere |

## 2. Plants, scenarios, baselines

**House**: the two-node model of round 3 (air 10 % and wall 90 % of 31 MJ/K heavy mass, or light 13 MJ/K), eras 1983-92
(H 250 W/K) and 2015 (H 95 W/K). **Emitters**: radiators (water node 0.6 MJ/K, exponent 1.3, pump 628 W/K) or underfloor
screed (8 MJ/K, exponent 1.1, 1050 W/K); K balanced so heating curve 1.6 carries the house at 5 C outdoor.
**Flow**: fixed (constant pump) or variable (TRV throttling: emitter conductance and pump flow scale with the room error,
minimum 15 %). **Boiler**: Pmin 3.5 or 5.5 kW, Pmax 24 kW; own control: start below CS - 2 K after 3 min standstill,
output = emitter draw + 1000 W/K x (CS - flow), stop above CS + 5 K at minimum output (A, see section 6). In PWM the
boiler either honours MM=0 (burns at Pmin) or ignores MsgID 14 and modulates by itself (gruijter's Intergas answered
1204/1204 MsgID 14 writes with Unknown-DataID on 2026-11-04).

**Scenarios** (identical for every controller): steady at -5 / 5 / 12 C (24 h), warm-up from 2.5 K below target at 5 C
(12 h), falling demand (outdoor 2 -> 12 C over 6 h), internal gain step +1500 W for 4 h at 8 C. 16 plants x 6 scenarios.

**Baselines** (textbook forms, tuned on a small published grid per plant on the steady 5 / steady 12 / gain-step
scenarios; not proposals):
- `pi`: bounded PI on the room error added to the curve, output clamped to [cold, max], back-calculation anti-windup.
  Grid kp 3/6/12/24 x Ti 0.5/1/2/4 h.
- `tpi`: time-proportional room on/off with integral (clamped to +-band/2, frozen while saturated), fixed CS 60 C
  (radiators) / 40 C (underfloor) during ON. Grid band 0.5/1/1.5/3 K x cycle 10/20 min.
- `wc`: the curve alone (native weather compensation).
- `wc_room`: curve + k x room error. Grid k 1/3/6/10/15.

Two picks per baseline: **rms** (lowest summed RMS room error) and **fair** (lowest RMS among settings whose starts per
day do not exceed SAT's on the same plant; 40 of 64 plant/baseline pairs had such a setting, the rest fall back to the
fewest starts). 18 of 64 rms picks sit on the edge of the grid, so the rms baselines could be tuned further: the
comparison is not tilted in SAT's favour by under-tuning, but it is also not an optimum.

**Not pre-registered.** The metrics and grids were fixed during this session, and two model choices were changed after a
first look at the output: the boiler stop threshold (2 K -> 5 K, because at 2 K SAT never stayed 60 s above CS + 2 and so
never entered PWM, which contradicts gruijter's capture where it did at 08:5x) and the grid widening. Both are disclosed
and the 3 K stop threshold is kept as a sensitivity case. TASK-1412.19 pre-registers its thresholds before its first run.

## 3. AC1: what the duty estimate says about delivered heat (R12)

Per SAT PWM period (one ON start to the next, PWM exits excluded), over all scenarios of each plant group:

| emitter | flow | Pmin kW | MM | periods | r(duty, delivered kW) | r(duty, need kW) | mean abs(duty - flame share) | flame power kW | delivered / need |
|---|---|---|---|---|---|---|---|---|---|
| rad | fixed | 3.5 | ignored | 229 | 0.81 | 0.51 | 0.060 | 4.26 | 1.02 |
| rad | fixed | 3.5 | honoured | 229 | 0.92 | 0.65 | 0.048 | 3.50 | 1.05 |
| rad | fixed | 5.5 | ignored | 336 | 0.81 | 0.70 | 0.076 | 6.22 | 1.05 |
| rad | fixed | 5.5 | honoured | 293 | 0.74 | 0.65 | 0.098 | 5.50 | 1.00 |
| rad | variable | 3.5 | ignored | 287 | 0.87 | 0.76 | 0.061 | 4.41 | 1.04 |
| rad | variable | 3.5 | honoured | 279 | 0.92 | 0.52 | 0.052 | 3.50 | 1.01 |
| rad | variable | 5.5 | ignored | 366 | 0.79 | 0.75 | 0.101 | 6.58 | 1.04 |
| rad | variable | 5.5 | honoured | 346 | 0.81 | 0.74 | 0.080 | 5.50 | 1.05 |
| ufh | fixed | 3.5 | ignored | 314 | 0.82 | 0.59 | 0.050 | 4.88 | 1.04 |
| ufh | fixed | 3.5 | honoured | 337 | 0.94 | 0.68 | 0.022 | 3.50 | 1.01 |
| ufh | fixed | 5.5 | ignored | 406 | 0.91 | 0.64 | 0.032 | 6.70 | 1.08 |
| ufh | fixed | 5.5 | honoured | 423 | 0.94 | 0.75 | 0.041 | 5.50 | 1.07 |
| ufh | variable | 3.5 | ignored | 370 | 0.88 | 0.45 | 0.022 | 4.79 | 1.03 |
| ufh | variable | 3.5 | honoured | 371 | 0.96 | 0.49 | 0.023 | 3.50 | 0.95 |
| ufh | variable | 5.5 | ignored | 447 | 0.93 | 0.64 | 0.032 | 6.55 | 1.09 |
| ufh | variable | 5.5 | honoured | 463 | 0.91 | 0.66 | 0.047 | 5.50 | 1.08 |

How to read it:
- **r(duty, delivered) is high (0.74-0.96) but says little**: in this model the duty sets the ON time, and the burner
  runs at a near-constant power during ON, so delivered heat follows the duty by construction.
- **The R12 answer is r(duty, need): 0.45-0.76.** The duty ratio is a temperature ratio; it tracks what the house
  actually loses only loosely, and least with variable flow on underfloor (0.45-0.49). It must not be read as a heat or
  power figure. Over a whole period the delivered heat still balances the need (0.95-1.09) because the PID closes the
  loop on the room, not because the ratio is a heat measure.
- **The burner runs at Pmin when MM is honoured**, so an ON phase of duty d delivers about d x Pmin x period, not
  d x Pmax. When MM is ignored (as on gruijter's boiler) the ON-phase power is 12-25 % above Pmin.
- **Start events**: one boiler start per PWM period in every group (the 3-per-hour limit holds in all runs).
- Field cross-check (one house, one capture, gruijter 2026-11-04): the duty jumps at each ignition (0.23 -> 0.77 at
  19:33, settling to 0.47 in 3 min) because Teff is reseeded from the still-low flow; the model shows the same pattern
  (`mean abs(settled - start duty)` 0.01-0.10 in `runs.json`).

## 4. AC2: SAT against the baselines on identical scenarios

Means over all scenarios, Pmin 3.5 and 5.5 and both flow kinds (full per-run values in `runs.json`):

| era | emitter | controller | RMS room error K | share within +-0.3 K | starts/day |
|---|---|---|---|---|---|
| 1983-92 | rad | SAT (MM honoured / ignored) | 0.116 / 0.109 | 0.946 / 0.944 | 30.2 / 33.6 |
| 1983-92 | rad | PI rms / fair | 0.080 / 0.166 | 0.994 / 0.914 | 62.4 / 51.0 |
| 1983-92 | rad | TPI rms / fair | 0.082 / 0.160 | 0.994 / 0.966 | 141.2 / 55.7 |
| 1983-92 | rad | WC | 0.643 | 0.581 | 40.7 |
| 1983-92 | rad | WC + room rms / fair | 0.115 / 0.451 | 0.947 / 0.597 | 67.7 / 37.4 |
| 1983-92 | ufh | SAT | 0.145 / 0.149 | 0.910 / 0.929 | 37.9 / 37.0 |
| 1983-92 | ufh | PI (both picks) | 0.108 | 0.987 | 8.9 |
| 1983-92 | ufh | TPI rms / fair | 0.111 / 0.111 | 0.982 / 0.982 | 96.6 / 66.0 |
| 1983-92 | ufh | WC | 0.754 | 0.476 | 2.0 |
| 1983-92 | ufh | WC + room | 0.139 | 0.982 | 7.2 |
| 2015 | rad | SAT | 0.170 / 0.167 | 0.887 / 0.896 | 54.2 / 53.3 |
| 2015 | rad | PI rms / fair | 0.143 / 0.181 | 0.924 / 0.897 | 43.5 / 44.0 |
| 2015 | rad | TPI rms / fair | 0.150 / 0.156 | 0.938 / 0.949 | 79.8 / 52.7 |
| 2015 | rad | WC | 0.529 | 0.581 | 48.2 |
| 2015 | rad | WC + room rms / fair | 0.184 / 0.272 | 0.868 / 0.803 | 56.6 / 44.2 |
| 2015 | ufh | SAT | 0.311 / 0.337 | 0.771 / 0.732 | 38.4 / 36.9 |
| 2015 | ufh | PI (both picks) | 0.214 | 0.858 | 4.1 |
| 2015 | ufh | TPI (both picks) | 0.234 | 0.868 | 35.2 |
| 2015 | ufh | WC | 0.632 | 0.506 | 2.2 |
| 2015 | ufh | WC + room | 0.251 | 0.853 | 3.8 |

Warm-up from 2.5 K below target (1983-92, Pmin 5.5, fixed flow), minutes to within 0.3 / 0.1 K, overshoot:

| emitter | SAT (MM hon.) | PI rms / fair | TPI | WC | WC + room |
|---|---|---|---|---|---|
| rad | 224 / 399, +0.17 | 22 / 25, +0.21 ; 155 / 189, +0.75 | 24 / 26, +0.20 | 397 / 454, +0.52 | 225 / 292, +0.16 |
| ufh | 141 / 286, +0.06 | 53 / 60, +0.27 | 78 / 89, +0.13 | 483 / 553, +0.36 | 74 / 131, +0.26 |

Observations (simulation only, for discussion; not proposals):
1. On radiators SAT reaches the band with about half the starts of the best-RMS baselines; with the starts held at SAT's
   level ("fair") the baselines lose most of their RMS advantage. A trade-off, not a winner.
2. On underfloor heating SAT starts the boiler 37-40 times a day where PI and WC + room need 4-9: SAT's PWM cycles a
   slow screed that the continuous baselines keep warm with long runs. This is the most visible difference in the study.
3. The slow last 0.3 K of SAT's warm-up (about 2 h on radiators) comes from the integral reset outside the 0.1 K deadband
   (thermo-nova pid.py:187-189, proven in the port). gruijter's capture of 2026-11-04 shows the same slow approach
   (19.2 -> 19.5 C took about 2 h). This is review item R13 / LL-020; any change is Sergeant D's decision.
4. Native WC alone misses the target by 0.5-0.75 K RMS because one curve cannot fit -5 to 12 C in these houses; room
   influence fixes most of it.
5. MM honoured or ignored changes SAT's comfort figures by less than 0.03 K RMS in this model: the ON phase is short and
   the flow - 1 suppression keeps the boiler near Pmin anyway; the difference only matters when the emitter draw exceeds
   Pmin.

## 5. Parameter sensitivity (radiators, fixed flow, 1983-92, Pmin 5.5; steady 5, steady 12, gain step; fair picks)

| H | K | mass | boiler stop | controller | RMS K | within 0.3 | starts/day |
|---|---|---|---|---|---|---|---|
| x0.8 | x0.8 | heavy | 5 K | SAT / PI / WC+room | 0.051 / 0.063 / 0.791 | 1.00 / 1.00 / 0.00 | 59 / 83 / 71 |
| x0.8 | x1.2 | heavy | 5 K | SAT / PI / WC+room | 0.064 / 0.074 / 1.469 | 1.00 / 0.99 / 0.00 | 52 / 114 / 80 |
| x1.2 | x0.8 | heavy | 5 K | SAT / PI / WC+room | 0.059 / 0.065 / 0.226 | 1.00 / 1.00 / 0.75 | 62 / 63 / 67 |
| x1.2 | x1.2 | heavy | 5 K | SAT / PI / WC+room | 0.089 / 0.057 / 0.930 | 1.00 / 1.00 / 0.00 | 48 / 83 / 52 |
| x1.0 | x1.0 | light | 5 K | SAT / PI / WC+room | 0.111 / 0.066 / 0.986 | 0.97 / 1.00 / 0.00 | 67 / 102 / 69 |
| x1.0 | x1.0 | heavy | 3 K | SAT / PI / WC+room | 0.066 / 0.061 / 0.476 | 1.00 / 0.99 / 0.18 | 63 / 163 / 214 |

SAT's comfort figure is stable across +-20 % house loss and emitter size; the "fair" WC + room setting is sensitive to the
emitter balance (it had to keep a low gain to match SAT's starts). A lower boiler stop threshold (3 K) raises the
baselines' starts sharply and leaves SAT's nearly unchanged, because SAT's PWM, not the boiler, decides SAT's starts.

## 6. Model limits

- One reference house per era with a single zone; the two-node model has no 24-72 h field validation and no heat-pump
  capture (docs/sat/sim-hp-lowload/model-evaluation-2026-11-02.md).
- The air node is 10 % of the mass and there is no transport dead time: fast continuous controllers (PI with kp 24)
  look better in warm-up than they would on a real pipe run. The PI warm-up figures are an upper bound.
- The boiler's own control is generic; the 5 K stop threshold is an assumption chosen to match the PWM entry seen in one
  capture (gruijter, 2026-11-04 08:5x). Real boilers differ (anti-cycling, pump overrun, minimum run time).
- Variable flow is a simple proportional TRV law, not a hydraulic model.
- Room sensor 0.1 K resolution, no noise; no solar input; gains only in the gain-step scenario.
- Energy closes exactly by construction (|Q + G - L - dU| / L below 1e-6 in every run, asserted by the test); the heat
  figures are model heat, not fuel, and include stored heat in the warm-up and falling-demand scenarios.

## 7. Acceptance criteria

- **AC1** (LL-016 compares the temperature-ratio estimate with integrated heat delivery, room error and actual start events
  across fixed/variable flow and radiator/low-temperature plants): section 3 (per period: duty vs delivered and needed
  power, flame share, flame power, starts) and section 4 (room error), 16 plant groups.
- **AC2** (repaired SAT baseline vs bounded PI/TPI and native weather/room-influence baselines under identical scenarios;
  model limits and parameter sensitivity published): sections 2, 4, 5, 6.
- **AC3** (no energy/COP/comfort improvement or new algorithm approval inferred from simulation; a controller replacement
  needs a separate approved ADR/task): this report makes no such claim and proposes no change. Observations 2 and 3 are
  inputs for Sergeant D and the R13/LL-020 review, not decisions.
