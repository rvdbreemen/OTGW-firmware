# TASK-1527 round 23: candidate SAT settings for heat pumps, paired against the current settings, ten seasons

Date 2026-11-02. Closes the simulation part of TASK-1527 AC#2 ("candidate settings (PWM period, minimum on/off time,
flow cap, MM in PWM, hysteresis) scored on compressor starts per day first and comfort second, paired against current
settings, over at least 5 winters").

Script `scripts/sat_hp_candidate_sweep_study23.py` (round 21 library, SAT through the proven port
`scripts/sat_sim_port.py`); data `round23_candidate_runs.json` (840 runs) and `round23b_hysteresis_runs.json`
(360 runs, the hysteresis candidates). Units: Mitsubishi Ecodan WM50 (minimum 1.8 kW), NIBE S2125-8 (2.4 kW), Daikin Altherma 3 R 04 (3.2 kW);
the four round 21 houses (underfloor heating); ten heating seasons 2015-16 .. 2024-25; each unit at its round 21 best
curve. Port proof re-run on 2.0.0-beta.5 (0ab3edd99, the firmware with entries 197/198): 4/4 OK, mapper worst 0.003 s. Regression: "current" reproduces round 21 exactly (NIBE, 2015 house, 2024-25: 17.4 starts/day, 0.195 C).

"current" = SAT with the PWM switch active, period 1800 s, minimum ON and OFF 600 s (entries 197/198), flow cap 40 C
(entry 155), MM 100 in PWM (heat pumps, satComputeMM). One setting changes per candidate.

## Results (mean over 3 units x 4 houses x 10 seasons; paired against "current" in the same unit, house and season)

| Candidate | Starts/day | Room error (C) | Min/day outside +-0.3 C | Electricity (kWh) | Starts vs current | Runs with fewer starts | Runs with equal or better comfort |
|---|---|---|---|---|---|---|---|
| current | 17.0 | 0.162 | 219 | 1058 | | | |
| PWM period 3600 s | 9.9 | 0.163 | 221 | 1056 | -7.2 | 100 % | 42 % |
| PWM period 10800 s | 3.9 | 0.170 | 230 | 1057 | -13.1 | 100 % | 31 % |
| old minimum ON 180 s, no minimum OFF | 18.2 | 0.166 | 227 | 1058 | +1.1 | 0 % | 8 % |
| flow cap 35 C | 17.0 | 0.162 | 219 | 1058 | 0.0 | | |
| flow cap 45 C | 17.0 | 0.162 | 219 | 1058 | 0.0 | | |
| MM 0 in PWM | 16.6 | 0.163 | 221 | 1056 | -0.4 | 82 % | 42 % |
| SAT continuous (round 21, same units and houses) | 3.4 | 0.136 | 171 | 1052 | | | |

Per unit (starts/day / room error): Mitsubishi current 13.0 / 0.156, 3600 s 7.7 / 0.155, 10800 s 3.2 / 0.160;
NIBE 17.3 / 0.172, 10.1 / 0.174, 4.1 / 0.186; Daikin 04 20.8 / 0.158, 11.8 / 0.162, 4.4 / 0.166.

## Findings

1. **A longer PWM period is the strongest lever inside today's mechanism**: 3600 s cuts starts by 42 % at +0.002 C,
   10800 s by 77 % at +0.008 C (the cost is larger on the NIBE, +0.014 C). With the ON time following the room
   (entry 196), a long period no longer costs much comfort.
2. **Entries 197/198 (600 s minimum ON and OFF) beat the old 180 s rule in every run**: the old rule had more starts
   in 100 % of the runs and worse comfort in 92 %.
3. **Flow cap 35 / 40 / 45 C makes no difference on underfloor heating**: the flow stays far below 35 C. This setting
   matters on radiators, which this round does not model.
4. **MM 0 in PWM changes little** (-0.4 starts/day): the ON-phase suppression setpoint (flow - 1 C) already holds the
   heat pump near its minimum; MM only acts during the first minutes of an ON phase.
5. **SAT continuous stays the best option on both scores**: fewer starts than even the 3 h period (3.4 vs 3.9) and the
   best comfort (0.136 vs 0.170 C).

## Hysteresis (round 23b, SAT continuous, the heat pump's own stop/start band around CS)

SAT continuous, same 3 units x 4 houses x 10 seasons (360 runs). The 3 K / 1 K band is the round 21 assumption and
reproduces round 21 exactly (largest difference in starts/day 0.0).

| Heat pump's band around CS (stop above / start below) | Starts/day | Room error (C) | Min/day outside +-0.3 C | Median run | Electricity (kWh) |
|---|---|---|---|---|---|
| 1 K / 1 K | 9.9 | 0.111 | 131 | 71 min | 1046 |
| 3 K / 1 K (round 21) | 3.4 | 0.136 | 171 | 196 min | 1052 |
| 5 K / 3 K | 1.7 | 0.181 | 233 | 384 min | 1065 |

The band is the classic trade-off between starts and comfort, and it moves both in every paired run (100 % of the
runs in the same direction). Even the tight 1 K band (9.9 starts, 0.111 C) has fewer starts and better comfort than
SAT with its PWM switch (17.0 starts, 0.162 C). This band belongs to the heat pump's own control, not to SAT; SAT only
sets the CS it moves around.

## Limits

Underfloor heating only (flow cap untested where it matters); the units' own control is assumed; no start losses or
defrost; one perfect room sensor; three units. Not validated against a real heat-pump capture (TASK-1527 AC#1).
