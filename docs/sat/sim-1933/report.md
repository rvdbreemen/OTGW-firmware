# TASK-1933: PWM exit simulation, report

Study: today's PWM underheat exit (flow <= request - 3.0 C for 180 s while the PWM status is ON; thermo-nova
`heating_control.py:308-356`, `const.py:22`; firmware `SATcycles.ino:39-40`) against Sergeant D's rule of entries 339/342
(flame on and request above the flow for 60 s, same gates). Thresholds and the decision rule were committed before the
first run: [thresholds.md](thresholds.md). Script: `tools/research/sat/sat_pwm_exit_sim_1933.py`; trace driver:
`tools/research/sat/sat_pwm_exit_trace_1933.py`. **Every simulation number below is a MODEL RESULT, not boiler proof.**

## 1. Pre-registered result

- Validity check V1: PASS. On the gruijter-like plants (era 3_1983-92, radiators, fixed flow, Pmin 5.5 kW, MM ignored)
  PWM is entered in 25 of 25 baseline runs.
- 30 s grid (1600 runs): e342 passes very_low_load, thermal_lag and minimum_power, and fails two classes on the toggle
  measure T1. Starts (T2) and comfort (T3) pass in every class.

  | Class | e342 mean toggles/day | limit (baseline + 1.0) | worst run (limit 4.0) |
  |---|---|---|---|
  | dhw | 1.141 | 1.081 | 12.5 |
  | ignition | 1.056 | 1.091 | 12.5 |

- 60 s sensitivity grid: the same picture (dhw mean 1.087 against limit 1.031, worst 17.0; ignition worst 12.0).
- **Under the pre-registered rule there is no recommendation to ship e342.**

## 2. Post-hoc analysis (labelled post-hoc: not part of the pre-registered rule)

### 2.1 Where the toggling comes from in the model

The worst runs are the gruijter-like plants. Trace of the worst run (dhw, 3_1983-92, rad, fixed, 5.5 kW, MM ignored,
seed 1; 48 h; MODEL RESULT):

| Variant | toggles/day | underheat-type exits/day | ignitions in a PWM ON phase | flow >= request at the first tick |
|---|---|---|---|---|
| e342 | 12.5 | 22.0 | 105 | 99 (6 never) |
| today | 1.0 | 2.5 | 125 | 122 (3 never) |

The ignition itself is not the problem: in 99 of 105 ON phases the flow is above the request at the first tick. The
e342 exits come later in the ON phase: 36 of 44 fire in the second minute. A typical one:

```
t (s)  request  flow   return  flame  PWM phase
3630   33.72    26.63  26.63   0      OFF
3660   33.74    39.79  14.60   1      ON (ignition)
3690   33.74    34.52  20.61   1      ON
3720   33.75    32.08  23.32   1      ON   request > flow
3750   33.75    32.21  23.45   1      ON   request > flow for 60 s -> e342 exit
```

Share of the flame-on PWM ON ticks with request > flow, by time since the ON phase began (30 s bins):

| s since ON start | 0 | 30 | 60 | 90 | 120 | 150 |
|---|---|---|---|---|---|---|
| model, e342 run | 0.03 | 0.09 | **0.35** | **0.25** | 0.01 | 0.01 |
| model, today run | 0.01 | 0.03 | 0.22 | 0.22 | 0.20 | 0.17 |
| field, gruijter (3 captures, 3906 ticks) | 0.04 | 0.06 | 0.05 | **0.00** | 0.03 | 0.02 |

### 2.2 Why the model does this (two sources)

1. **Model code.** The plant has one water node `Tr` (`sat_closedloop_sim_1412_19.py:152`): flow = `Tr + P/(2·mcp)`,
   return = `Tr - P/(2·mcp)`, with no transport delay. At ignition the boiler power appears at once, so the flow jumps
   about 13 C in one 10 s step and the return drops the same amount. The return then falls below the room air (14.6 C
   with the room at 20 C), which a radiator circuit cannot do: 125 of 5630 ticks in the e342 run have a return below
   19.5 C. After the spike the boiler modulates down (CS = flow - 1.0) and the flow settles at `Tr + Pmin/(2·mcp)`. After an
   OFF phase `Tr` has cooled (26.6 C here), so that settled flow lies 1-2 C below the request for the 60-90 s until the
   water warms up: long enough for the 60 s e342 rule, too short for today's 180 s rule.
2. **Field data** (EXPLORATORY, one house, gruijter's captures, kept local). The real boiler shows no such dip: at 60-90 s
   after the ON start the request is above the flow in 0-5 % of the ticks, against 25-35 % in the model; the e342 rule
   would have fired in 1 of 91 ON phases (TASK-1933 notes).

### 2.3 Reading (ours, post-hoc)

The two classes that fail are failed by a transient the lumped plant model produces and the measured boiler does not.
That makes the failure **likely a model artefact**, but this is not proven: the field side is one house (an Intergas with
MM ignored) and 30 s tick resolution, and other boilers could dip after an OFF phase. The pre-registered verdict stands
as written (no ship recommendation); this section is input for Sergeant D's decision, not a replacement for the rule.

## 3. What would settle it

- Field captures from a second house with PWM ON phases (another boiler type, ideally with MM honoured): the same
  "request above flow by time since ON start" profile.
- Or a plant model with a transport delay on the return (a second water node); then rerun the dhw and ignition classes.

## 4. Open points for Sergeant D (entry 342)

- Open point 2 (PWM ON gate): modelled as thermo-nova's gate, the exit only while the PWM status is ON
  (`heating_control.py:321-323`).
- Open point 3 (boiler temperature = flow): modelled as the flow temperature (MsgID 25).
