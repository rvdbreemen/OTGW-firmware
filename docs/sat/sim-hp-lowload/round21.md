# TASK-1527 round 21: ten heat pumps and hybrids under SAT, ten heating seasons

Date 2026-11-02. Goal: ten popular heat pumps and hybrids as a library of models with sources, the real SAT in the
simulation, at least the last ten winters. A design validation run:
nothing in the firmware changes.

Script `tools/research/sat/sat_hp_library_study21.py`; SAT port `tools/research/sat/sat_sim_port.py`; proof
`tests/test_sat_sim_port_equivalence.py`; weather `weather_season10/` (Open-Meteo archive, 52.13 N 6.13 E, 1 Oct -
30 Apr, 2015-16 .. 2024-25; a spot check of 2013-14 against the older download differs by 0.08 K on average, 0.6 K at
most, Open-Meteo reprocessing); curve sweep `round21_library_sweep_runs.json` (2024-25, coefficient 1.0 / 1.6), chosen
curves `round21_best_curves.json`; results `round21_library_runs.json` (800 runs).

## SAT in the simulation is the firmware's SAT

`tests/test_sat_sim_port_equivalence.py` drives the same input traces through the port and through the compiled
firmware functions: the whole SATpid.ino and satCalcHeatingCurve (16 combinations of 4 room traces, 4 curves,
underfloor and radiators, 2880 minutes each): output identical in 100 % of the minutes; the PWM mapper of satApplyPWM
(388 cases, worst 0.014 s; 8 range-edge cases fall on one side of the float32 edge); satChEnableDecide exact. A port
with one changed derivative rule is caught (the test goes red). 4/4 OK on 2.0.0-beta.2 (c922f17d9). On 2.0.0-beta.5 (0ab3edd99) the mapper case failed until the test used the heat pump's minimum ON of 600 s that beta.4 put in the firmware (entry 197, TASK-1542); with 600 s: 4/4 OK, 392 mapper cases, worst 0.003 s. Not covered by
the proof and modelled in the simulation loop: the PWM entry switch and exits, the ON-phase setpoints, the room sensor
(0.1 C resolution, A), presets, solar gain. Rounds 12-20 used an inline PID that differs from the firmware (no
derivative EMA, no "skip within 60 s" rule, curve rounded to 1 decimal); round 21 replaces it.

## The ten units

| Unit | Type | Min output | Max / nominal at A7/W35 | COP A7/W35 | Source quality |
|---|---|---|---|---|---|
| Remeha Elga Ace 4 | hybrid | 1.3 kW | 4.45 kW | 5.4 | datasheet + installer manual (read) |
| ATAG Energion M Hybrid 5 | hybrid | 1.5 kW (A) | 4.4 kW | 5.02 | datasheet (read); minimum from a sister model leaflet |
| Intergas Xtend 5 | hybrid | 1.8 kW | 5.0 kW | 4.68 | brochure (read) |
| Bosch Compress 7000i AW 5 | hybrid | 2.0 kW | 6.0 kW (A2) | 4.7 | secondary (search) |
| Mitsubishi Ecodan PUZ-WM50 | electric | 1.8 kW | 5.0 / 5.6 kW | 5.0 | product sheet (read) |
| NIBE S2125-8 | electric | 2.4 kW | 6.7 kW | 5.18 | secondary (search) |
| Daikin Altherma 3 R ERGA04 | electric | 3.2 kW | 4.3 kW | 5.10 | data book (read) |
| Vaillant aroTHERM plus 7 | electric | 3.25 kW | 7.37 kW | 4.42 | HP Keymark (read) |
| Daikin Altherma 3 R ERGA08 | electric | 3.3 kW | 7.5 kW | 4.60 | data book (read; merged-cell reading ours) |
| Panasonic Aquarea J 7 | electric | 3.4 kW | 7.0 kW | 4.76 | EU page (secondary); minimum from Keymark via search |

Minimum output = the ErP part-load capacity Pdh where it is above the reference demand (+7 or +12 C), or a published
modulation range. Full source strings are in `UNITS[*]["src"]` of the script.

## Variants and plant

- "sat": SAT as decided: the overshoot switch to PWM (entry 87), heat pumps with 600 s minimum ON and OFF (entries
  197/198), hybrids with SAT's hybrid rules (180 s minimum ON, at most 3 starts per hour, entry 188).
- "cont": the same curve and PID, SAT never enters PWM; the unit modulates and cycles by itself.
- Four houses on underfloor heating (1992-2012, 2015, BENG heavy; BENG light screed); each unit at its best curve per
  variant (sweep: the high-minimum units need curve 1.0 under "sat", everything else 1.6).
- Assumptions (A): each unit's own stop/restart rule (stop 3 K above CS at minimum output, restart 1 K below), minimum
  run and standstill 10 min unless published; COP between the published points; no start losses, no defrost.

## Results (10 seasons, 4 houses, mean per season)

| Unit | Min output | SAT now: starts/day | room error | time in PWM | SAT continuous: starts/day | room error | electricity now vs continuous |
|---|---|---|---|---|---|---|---|
| Remeha Elga Ace 4 | 1.3 | 8.6 | 0.147 | 19 % | 1.7 | 0.140 | +0.1 % |
| ATAG Energion M Hybrid 5 | 1.5 | 10.7 | 0.151 | 23 % | 1.9 | 0.141 | +0.1 % |
| Mitsubishi Ecodan WM50 | 1.8 | 13.0 | 0.156 | 31 % | 2.4 | 0.139 | +0.3 % |
| Intergas Xtend 5 | 1.8 | 13.8 | 0.157 | 30 % | 2.4 | 0.139 | +0.3 % |
| Bosch Compress 7000i 5 | 2.0 | 15.8 | 0.162 | 35 % | 2.7 | 0.138 | +0.5 % |
| NIBE S2125-8 | 2.4 | 17.3 | 0.172 | 41 % | 3.3 | 0.137 | +0.9 % |
| Daikin Altherma 3 R 04 | 3.2 | 20.8 | 0.158 | 51 % | 4.6 | 0.132 | +0.3 % |
| Vaillant aroTHERM plus 7 | 3.25 | 21.3 | 0.162 | 52 % | 4.7 | 0.131 | +0.6 % |
| Daikin Altherma 3 R 08 | 3.3 | 21.4 | 0.161 | 53 % | 4.8 | 0.130 | +0.5 % |
| Panasonic Aquarea J 7 | 3.4 | 21.5 | 0.160 | 53 % | 5.0 | 0.130 | +0.9 % |

Robustness: in all 400 paired runs (unit x house x season) SAT continuous has fewer starts; in 379 of 400 its room
error is equal or lower. The starts ratio per season stays within 4.2-6.1 for every unit. Worst season room error:
0.179 C (now) vs 0.157 C (continuous). Per house: 1992-2012 13.6 vs 2.9 starts/day, 2015 15.6 vs 2.6, BENG 14.1 vs 2.3,
BENG light floor 22.3 vs 5.6.

## Findings

1. **For every unit, SAT continuous gives 4-6x fewer compressor starts with equal or better comfort** and the same
   electricity (within 1 %). The pattern found on the Elga Ace in round 20 holds across the market.
2. **The minimum output decides how much SAT's PWM takes over**: 19 % of the heating time at 1.3 kW, about 30 % at
   1.8 kW, 41 % at 2.4 kW, 51-53 % at 3.2-3.4 kW. The nominal size hardly matters (Daikin 04 and 08 behave the same).
3. Matches Sergeant D's verdict (entry 192): a low minimum output (about 1 kW) is what makes a heat pump work well.

## Limits

The units' own control is assumed (it decides the "continuous" result); underfloor heating only; NIBE, Bosch and
Panasonic partly from secondary sources; no start losses or defrost in the COP; one perfect room sensor. A capture of a
real heat pump under SAT and on its own control (TASK-1527 AC#1) is the check this model needs.
