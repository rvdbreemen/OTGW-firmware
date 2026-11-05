# TASK-1527 round 22b: hybrid boiler backup with a 5.5 kW boiler minimum, return-temperature efficiency and start losses

Date 2026-11-02. Follow-up of round 22 after Sergeant D's rationale (entry 210): the slave SAT is meant for the load band
between the heat pump's maximum (Elga Ace about 4 kW) and an older boiler's minimum (about 5.5 kW), where the boiler on
its own control cycles; running it longer keeps it above 95 % efficiency.

Script `tools/research/sat/sat_hp_hybrid_backup_study22.py --b` (round 21 library, SAT through the proven port); results
`round22b_boiler55_runs.json` (240 runs: 4 hybrids x 3 houses x 2 modes x 10 seasons 2015-16 .. 2024-25). Regression:
without `--b` the model reproduces round 22 exactly (Elga Ace, 1983-92 radiators, own, 2024-25: 2673 starts, 582 kWh).

## What changed against round 22

- Boiler minimum 5.5 kW (entry 210) instead of 3.1 kW. The 3.1 kW of round 22 came from a "Calenta 28c datasheet" whose
  version is not recorded; it may be the newer Calenta Ace. No manufacturer figure for the older Calenta was found
  (a forum post mentions 5.0 kW, unverified).
- Gas with a condensing efficiency from the boiler return, proxied by the system node temperature (A): 0.97 at 30 C or
  below, linear to 0.87 at 60 C (`gas_ret_kwh`). The fixed 0.97 is kept alongside (`gas_kwh`).
- Start losses: not simulated; each start adds L kWh of gas afterwards (they do not change the control). The break-even L
  below is the start loss at which the slave SAT uses no more gas than the boiler's own control.

## Results (mean per season, 10 seasons; houses where the boiler runs)

| House | Unit | own: starts | run | gas (kWh) | slave: starts | run | gas (kWh) | room error own / slave | break-even start loss |
|---|---|---|---|---|---|---|---|---|---|
| 1983-92 underfloor | Elga Ace 4 | 4365 | 1.1 min | 592 | 923 | 10.9 min | 959 | 0.103 / 0.075 | 0.11 kWh |
| | ATAG Energion 5 | 2704 | 1.2 | 325 | 537 | 11.1 | 567 | 0.072 / 0.072 | 0.11 |
| | Intergas Xtend 5 | 1658 | 1.2 | 207 | 348 | 11.4 | 378 | 0.071 / 0.072 | 0.13 |
| | Bosch 7000i 5 | 251 | 1.5 | 35 | 70 | 12.8 | 85 | 0.069 / 0.070 | 0.28 |
| 1983-92 radiators | Elga Ace 4 | 3552 | 1.1 | 578 | 1078 | 10.8 | 1160 | 0.062 / 0.061 | 0.24 |
| | ATAG Energion 5 | 1700 | 1.2 | 234 | 701 | 11.7 | 827 | 0.049 / 0.060 | 0.59 |
| | Intergas Xtend 5 | 1044 | 1.2 | 152 | 470 | 12.0 | 576 | 0.049 / 0.056 | 0.74 |
| 1992-2012 radiators | Elga Ace 4 | 190 | 1.4 | 26 | 77 | 11.8 | 87 | 0.057 / 0.058 | 0.54 |

Gas = return-temperature efficiency; with the fixed 0.97 the numbers differ by 0-2 %. The slave uses 13-147 kWh less
electricity per season (heat moves from the heat pump to the boiler).

## Findings

1. **With a 5.5 kW minimum the boiler on its own control cycles much harder than in round 22**: runs of 1.1-1.5 min and
   up to 4365 starts per season (Elga Ace, underfloor). The gap Sergeant D describes is reproduced and is wider than the
   round 22 model showed.
2. **The slave SAT cuts boiler starts by 55-80 % in those houses and makes runs about 11-13 min**. On underfloor heating
   with the Elga Ace it also improves comfort (0.103 -> 0.075 C); on radiators comfort is equal or somewhat worse
   (ATAG 0.049 -> 0.060 C, Intergas 0.049 -> 0.056 C).
3. **The return-temperature efficiency hardly changes the gas balance**: in this model the boiler sees the same system
   return in both modes, so both run at about the same condensing efficiency.
4. **The gas balance depends on the start loss, which is not known.** The slave uses no more gas than the boiler's own
   control only when each start costs at least 0.11-0.13 kWh (underfloor, Elga Ace / ATAG / Intergas) or 0.24-0.74 kWh
   (radiators). One own-control run delivers only about 0.1 kWh, so the start loss would have to be about as large as a
   whole run. Below that, the slave's gain is fewer starts and longer runs (wear, noise, comfort on underfloor), at a
   gas cost.

## Limits

The start loss per start, the efficiency curve, the separator loop and pump, the boiler's own hysteresis and
anti-cycling, the heat pump's backup request rule and the radiator water volume are assumptions; the boiler return is
proxied by the system node. No real hybrid capture yet; a capture of the boiler's gas use per start would settle
finding 4.
