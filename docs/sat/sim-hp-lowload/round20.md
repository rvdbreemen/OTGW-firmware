# TASK-1527 round 20: a modulating hybrid heat pump (Remeha Elga Ace 4 kW) under SAT

Date 2026-11-01. Robert: extend the model with a modulating heat pump based on the Remeha Elga Ace specifications, as a
separate model. Script `tools/research/sat/sat_hp_modulating_elga_study20.py`; data `round20_elga_sweep_runs.json` (curve sweep,
2025-26, seed 1, 126 runs) and `round20_elga_runs.json` (validation at the chosen curves, 3 seasons x 2 seeds,
108 runs).

## Specifications used (primary sources)

| Item | Value | Source |
|---|---|---|
| Heat output / COP at A7/W35 (4 kW model) | 4.45 kW / 5.4 | Product datasheet Elga Ace All-in-one, p.9 (EN 14511-2) |
| Heat output / COP at A2/W35 | 4.45 kW / 4.05 | same |
| Part-load capacity Pdh at Tj +12 / +7 / +2 / -7 C | 1.3 / 1.4 / 2.2 / 3.5 kW | Installation and service manual 7868458-02 (2024), Tab.17 p.20 (55 C application) |
| Lowest outdoor temperature (TOL) | -10 C | same |
| Minimum compressor standstill after a stop (HP155) | 8-10 min, default 10 | manual p.118 |
| Hybrid mode (HP061) | "Geen": heat pump first, boiler as backup; "Kosten" / "Primaire energy" / "CO2" use the COP threshold HP054 (default 2.5) | manual p.105-106, p.116 |
| Backup start delay | 8 min at -10 C to 30 min at +15 C (HP047-HP050) | manual p.128 |
| Boiler 28c output at 50/30 C | 3.1-26.1 kW, modulating | datasheet p.9 |

The Pdh at +12 C (1.3 kW) is above the EN 14825 reference demand at +12 C, which means the unit cannot modulate
lower: the model uses 1.3 kW as the minimum output. This matches the "about 1 kW" Sergeant D named as the key figure
(entry 192). Conflict noted: the manual calls "Kosten" the factory setting on p.106, the parameter table on p.116 lists
"Geen"; the model uses "Geen".

**Assumptions** (not in the documents): the unit's own flow control and its stop/restart rule (stop when the flow
exceeds CS + 3 K at minimum output, restart below CS - 1 K after the standstill), minimum run 10 min, capacity
between the datasheet points, COP between the two datasheet points with a defrost penalty below +2 C and Carnot
scaling for other flow temperatures, boiler efficiency 0.97, CH = 0 stops the unit. The absolute SCOP (5.3-5.7) is
very likely optimistic; only differences between the variants are used.

## Variants

- **sat**: SAT as the firmware runs it for a heat pump: continuous CS = PID output; the overshoot switch (flow >= CS
  + 2 K for 60 s, entry 87) enters PWM; PWM with the firmware mapper (30 min, minimum ON and OFF 10 min, entries 197
  and 198) and thermo-nova's ON-phase setpoints (return + 18 C before the start, then flow - 1 C; OFF: CS = 10, CH = 0).
- **own**: the same SAT curve and PID, but SAT never switches to PWM; the unit modulates and cycles by itself.
- **wc**: the unit on its own weather curve, no room feedback.

Best curve per variant (sweep): sat 1.6, own 1.6, wc 1.4.

## Results (3 seasons x 2 seeds, at the chosen curves)

| House, screed | variant | starts/day | median run (min) | mean abs error (C) | share of time in PWM | electricity (kWh) | gas (kWh) |
|---|---|---|---|---|---|---|---|
| 1992-2012, heavy | sat | 5.5 | 16 | 0.089 | 20 % | 1711 | 135 |
| | own | 1.2 | 776 | 0.091 | 0 | 1710 | 134 |
| | wc | 0.2 | 837 | 0.549 | 0 | 1613 | 45 |
| 1992-2012, light | sat | 5.4 | 18 | 0.059 | 17 % | 1704 | 76 |
| | own | 1.4 | 389 | 0.059 | 0 | 1705 | 71 |
| 2015, heavy | sat | 11.2 | 16 | 0.178 | 44 % | 981 | 4 |
| | own | 1.4 | 627 | 0.162 | 0 | 983 | 8 |
| 2015, light | sat | 14.5 | 17 | 0.116 | 46 % | 960 | 1 |
| | own | 2.2 | 252 | 0.106 | 0 | 960 | 2 |
| BENG 2021, heavy | sat | 14.5 | 16 | 0.215 | 55 % | 797 | 2 |
| | own | 1.3 | 611 | 0.198 | 0 | 803 | 3 |
| BENG 2021, light | sat | 19.5 | 17 | 0.154 | 61 % | 782 | 0 |
| | own | 2.3 | 235 | 0.137 | 0 | 781 | 0 |

Per season (all houses): starts sat / own 10.4 / 1.4 (2010-11), 12.8 / 1.7 (2013-14), 12.2 / 1.7 (2025-26); error
0.144 / 0.135, 0.128 / 0.118, 0.133 / 0.124 C.

## Findings

1. **SAT's PWM switch makes a modulating heat pump cycle 4-8x more, without any gain.** With the same curve and PID,
   letting the Elga Ace modulate and stop by itself gives 1.2-2.3 starts a day and runs of 4-13 hours; SAT with its
   PWM switch gives 5.4-19.5 starts and runs of about 16 minutes. Comfort is equal or slightly better without the
   switch (0.06-0.20 vs 0.06-0.22 C), electricity is the same within 1 %, gas within a few kWh per season.
2. **The cause is the minimum output.** In the newer houses the load is below the 1.3 kW minimum on most days, the
   flow rises above CS + 2 K, and SAT enters PWM 44-61 % of the heating time. In the 1992-2012 house it is 17-20 %.
3. **Pure weather compensation is worst for comfort** (0.34-0.55 C) in this model, which has internal gains and sun
   that a curve cannot see. In the oldest house it uses less electricity and gas, because it runs cooler than the
   target.
4. **The hybrid backup only matters in the undersized house** (1992-2012, design load 4.8 kW): 22-41 boiler hours and
   71-135 kWh gas per season, the same for sat and own.

Consequence for the design (reading, ours): for a modulating heat pump, SAT's best behaviour in this model is to
stay continuous and send only the CS (the "own control" option of Q-1527a), not to enter PWM. This is the opposite
of what PWM is for on a gas boiler: a gas boiler's own anti-cycling is short, a heat pump's own control already runs
long.

## Limits

The unit's own stop/restart rule is assumed; it decides the "own" result. A real Elga Ace capture under SAT and on its
own control (Sergeant D's tested unit, Robert's unit) is needed to confirm. Absolute COP optimistic; no defrost
cycles in time, no DHW, no start losses; one perfect room sensor; one emitter type (underfloor).
