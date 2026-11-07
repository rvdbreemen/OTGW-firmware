# TASK-1527 round 22: hybrid boiler backup, the boiler's own control against Sergeant D's slave SAT

Date 2026-11-02. Sergeant D's proposal (entries 206-209): a master SAT on the heat pump stays continuous; a slave SAT
between the heat pump and the boiler takes the heat pump's CS as its requested setpoint and runs SAT's gas logic
(continuous until the overshoot switch, then gas PWM), because "when Elga stops heating and instructs the gas boiler to
run, gas boilers cycling like hell". This round models that backup.

Script `tools/research/sat/sat_hp_hybrid_backup_study22.py` (uses the round 21 library and SAT port); results
`round22_hybrid_backup_runs.json` (240 runs: 4 hybrids x 3 houses x 2 modes x 10 seasons 2015-16 .. 2024-25).

## Model

- The heat pump's backup request (CH = 1 and its CS to the boiler) starts after the HP047-050 delay (8-30 min,
  Elga Ace manual p.128) with the heat pump at full output and the flow 2 K short of CS; it ends after 15 min of
  spare heat-pump capacity or when CH ends; CH = 0 / CS = 0 stops the slave (entry 208).
- Hydraulic separator (entries 208/209): the boiler has its own primary loop (30 kJ/K, A) with its own pump
  (930 W/K, A) and controls on its own primary flow; both primaries feed the separator, the system draws from it.
  Without the separator the boiler cycled far less (first test, 2020-21: 768 vs 2925 starts), which is why the cycling
  Sergeant D describes only appeared once the separator was in.
- "own": the boiler's own control on the heat pump's CS: on below CS - 2 K after a 3 min anti-cycling time, off above
  CS + 2 K, modulating 3.1-26 kW ("Calenta 28c datasheet", version not recorded, possibly the Calenta Ace; round 22b uses 5.5 kW) (A for the hysteresis and anti-cycling).
- "slave": SAT's gas logic on the heat pump's CS: continuous, overshoot switch (boiler flow >= CS + 2 K for 60 s) to
  gas PWM (1200 s period, 180 s minimum ON, at most 3 starts per hour, ON phase at minimum modulation), back through
  the saturation (300 s) and underheat (180 s) exits (thermo-nova heating_control.py:262-329, :358-376).
- Houses: 1983-92 on underfloor heating and on radiators (water node 0.6 MJ/K, A), 1992-2012 on radiators; master SAT
  continuous; curve 1.6.

## Results (mean per season, 10 seasons)

| House | Unit | own: boiler starts | mean run | gas kWh | slave: boiler starts | mean run | gas kWh | room error own / slave |
|---|---|---|---|---|---|---|---|---|
| 1983-92 radiators | Elga Ace 4 | 2523 | 2.3 min | 577 | 935 | 15.9 min | 852 | 0.061 / 0.055 |
| | ATAG Energion 5 | 1026 | 3.6 | 243 | 536 | 16.3 | 511 | 0.048 / 0.054 |
| | Intergas Xtend 5 | 640 | 3.7 | 159 | 384 | 16.7 | 379 | 0.048 / 0.052 |
| | Bosch 7000i 5 | 91 | 4.9 | 25 | 103 | 16.9 | 106 | 0.048 / 0.050 |
| 1983-92 underfloor | Elga Ace 4 | 1951 | 6.3 | 739 | 790 | 21.9 | 932 | 0.085 / 0.073 |
| | ATAG Energion 5 | 642 | 12.8 | 420 | 370 | 27.4 | 543 | 0.067 / 0.071 |
| | Intergas Xtend 5 | 395 | 16.1 | 283 | 236 | 28.7 | 361 | 0.069 / 0.072 |
| | Bosch 7000i 5 | 66 | 28.8 | 66 | 42 | 32.9 | 76 | 0.069 / 0.070 |
| 1992-2012 radiators | Elga Ace 4 | 127 | 4.7 | 29 | 62 | 18.3 | 65 | 0.057 / 0.057 |
| | others | 1-10 | | 0-4 | 1-10 | | 1-9 | 0.057 |

Heat-pump starts per day are about the same in both modes (1 on underfloor, 8-12 on radiators).

## Findings

1. **With the separator the boiler does cycle hard on its own control** when the heat pump is too small: up to 2500
   starts per season with runs of 2-6 min (Elga Ace, the smallest heat pump, in the 1983-92 house). Sergeant D's
   observation is reproduced.
2. **The slave SAT cuts boiler starts by 36-63 % and makes the runs longer**, most where the cycling is worst: on
   radiators runs go from 2-5 min to about 16-17 min (3.4-6.9x), on underfloor from 6-29 min to 22-33 min (1.1-3.5x).
   One exception: Bosch on radiators, where the boiler hardly runs, gets slightly more starts (91 -> 103). Comfort is
   the same.
3. **It costs gas**: 1.15-1.5x more gas per season on underfloor and with the Elga Ace, up to 2-4x on radiators with
   the bigger units where the absolute amounts are small (Elga Ace radiators 577 -> 852 kWh; Bosch radiators
   25 -> 106 kWh), with somewhat less electricity. Running the boiler at its minimum for longer shifts heat from the
   heat pump to the boiler.
4. Where the heat pump is large enough (1992-2012 house, the bigger units) the boiler hardly runs and the mode does not
   matter.

## Limits

The boiler's hysteresis and anti-cycling, the separator loop size and pump, the heat pump's backup request rule and
the radiator water volume are assumptions; no real hybrid capture yet. The gas penalty depends on how long the slave
keeps the boiler at minimum; a slave that stops the boiler as soon as the heat pump catches up would change it.
