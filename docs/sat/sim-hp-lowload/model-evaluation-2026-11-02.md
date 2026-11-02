# Critical evaluation of the SAT simulation model (rounds 1-23), 2026-11-02

Question: does the model cover typical Dutch housing from the 1950s to now, across house types, thermal mass, emitters
and heat sources, so that the results of all rounds can be read as valid for that stock?

**Answer: no.** The model covers one house type (a 120 m2 terraced house) in six as-built era classes, with a single
zone. It is a sound tool for paired, relative comparisons of control strategies on that house. It is not a model of the
housing stock. Below, what it is, where it falls short, and what that means for the results so far.

Facts below are read from the scripts at origin/dev (2026-11-02); the house values come from
docs/sat/sim-1519/house-typology-research.md, which labels them itself as derived estimates.

## 1. What the model is

| Part | How it is modelled | Source quality |
|---|---|---|
| House | Two nodes (air, wall) plus a direct air-to-outside path. One reference: terraced house, 120 m2, 300 m3, 60 m2 facade, 60 m2 roof, 60 m2 ground floor, 25 m2 windows (`sat_curve_selftune_study3.py:18-28`) | Derived by us from facade Rc per era; TABULA/EPISCOPE typology was **not opened** (research page section 1) |
| Eras | pre-1965 (H 620 W/K), 1965-75 (440), 1983-92 (250), 1992-2012 (160), 2015 (95), BENG 2021 (80) | Estimates; **1975-1983 is missing** although the research page lists its Rc requirement |
| Thermal mass | ISO 13790 classes: heavy 260 kJ/m2K (31 MJ/K) for every era; light 110 kJ/m2K in some gas rounds; 10 % of it in the air node | ISO class confirmed; the 10 % split and the coupling G_aw 2880 W/K are estimates |
| Underfloor | One screed node 8 MJ/K (3 MJ/K light), exponent 1.1, water 1050 W/K, flow cap 40 C (`sat_hp_onoff_study12.py:34-39`) | Plausible magnitudes, not measured |
| Radiators | One water/steel node 0.6 MJ/K (heat-pump rounds) or the round 4 loop (60 l, pump 628 W/K), exponent 1.3; K calibrated per house so a chosen curve balances the season | The calibration gives every house **exactly fitting** radiators |
| Heat pumps | Ten units from datasheets (round 21); each unit's own control is generic: stop at CS + 3 K at minimum output, restart at CS - 1 K, output proportional to the flow error; COP interpolated; no defrost, no start losses, no buffer, no hot-water priority | Datasheet minimum/maximum/COP partly read, partly secondary; the control is an assumption that decides results (round 23c finding 9) |
| Boilers | 3.1-26 kW (source not recorded), later 5.5 kW minimum (entry 210); efficiency fixed 0.97, in 22b from the return temperature; own control generic (CS +- 2 K, 3 min anti-cycling) | Assumptions; start losses only as a break-even |
| SAT | Python port proven equal to the compiled firmware PID, curve, PWM mapper and CH decision (`tests/test_sat_sim_port_equivalence.py`, 4/4 on beta.5) | Strong, from round 21 on; rounds 12-20 used an inline PID that differs |
| Weather | Open-Meteo archive, one location (52.13 N 6.13 E), 10 seasons (gas rounds up to 20 winters) | Good; no wind-driven infiltration |
| Occupants | Same gains for every house (cooking peak, 1-3 random loads, a party every 14 days), solar through a fixed 1.5 m2 aperture | Assumption, identical across eras |
| Validation | One 4 h field capture fitted with a one-mass model (K/H about 1, tau about 110 h) | **No 24-72 h validation of the two-node model, no heat-pump capture** |

## 2. Coverage against the Dutch housing stock

| Dimension | Covered | Not covered |
|---|---|---|
| Building period | 6 classes, as built | 1975-1983; renovated states (double glazing, cavity or roof insulation added later), which describe most pre-1975 homes today |
| House type | Terraced house 120 m2 | Corner and semi-detached, detached, apartments (gallery, porch, high-rise), small and large homes |
| Mass | Heavy masonry for all; light timber in some gas rounds | Mass per era (lighter roofs and floors in post-war system building), concrete-core apartments |
| Emitters | Underfloor (one screed node), radiators (one node) | Underfloor downstairs plus radiators upstairs (common), low-temperature radiators and fan convectors (common with heat pumps), thermostatic valves, air heating (1970s-80s), oversized or undersized radiators |
| Zones | One zone, one perfect room sensor | Multiple rooms, a thermostat in a different room than the load, closed valves |
| Heat sources | Gas boiler, on/off and modulating heat pumps, hybrids with separator | Hot-water priority of a heat pump (daily stops), buffer tanks, defrost, real unit control (degree minutes, NIBE/Daikin logic) |
| Which rounds use which houses | Gas self-tune (rounds 3-11): all six classes in round 3, later mostly 1983 onwards plus pre-1965 in round 6. Heat pumps on underfloor (12-21, 23): **only 1992-2012, 2015, BENG**; heat pumps on radiators (14, 15, 17): also 1983-92 (`sat_curve_selftune_study5b.py:20`). Hybrids (22, 22b): 1983-92 and 1992-2012 | Heat pumps and hybrids in pre-1983 homes were never simulated |

## 3. Critical review of the studies so far

1. **Relative results are more robust than absolute ones.** Every comparison is paired (same house, weather and seed),
   so a ranking of strategies on the model house holds up much better than the absolute starts per day, kWh or room
   error, none of which has been validated against a real house or a real heat pump.
2. **Several conclusions hang on one assumption.** The heat pump's own stop band decided the margin result (round 23c,
   finding 9); the start loss decides the hybrid gas balance (round 22b, finding 4); the calibrated radiators hide the
   case where a house has too little emitter for low flow temperatures, which is the main real-world limit for heat
   pumps in older homes.
3. **Errors found and corrected along the way:** the PWM ON time modelled as fixed per period (entry 196, round 18b),
   unfair comparisons at curve 1.8 (19b/19c), an inline PID that differed from the firmware (rounds 12-20, replaced in
   21), a one-tick-early PWM switch (rounds 21-23, small effect, 23c), an untraceable boiler minimum (22, fixed in 22b).
   Each correction changed numbers, some changed conclusions (round 19). That pattern argues for caution with any result
   that has not been re-run on the current model.
4. **The SAT side is the strongest part.** Since round 21 the controller is the firmware's own logic (proven per minute);
   the weakest parts are the plant: house, emitters and the units' own control.
5. **Selection of houses follows the question, not the stock.** Heat-pump rounds chose houses where a heat pump fits
   (1992 onwards on underfloor, 1983 onwards on radiators); that is reasonable for a design question, but the results cannot be read as "SAT for heat pumps in
   Dutch homes" in general.

## 4. What the results so far can and cannot support

Can support (relative, on the model house, mostly 1992 onwards):
- SAT continuous gives 4-6x fewer compressor starts than SAT with its PWM switch, with equal or better comfort (round 21).
- Among PWM settings, a longer period cuts starts at a small comfort cost (rounds 17, 23, 23c); 600 s minimum ON/OFF
  beats 180 s (round 23).
- With a heat pump too small for the house, a boiler on its own control cycles hard behind a separator; the slave SAT
  cuts those starts (rounds 22, 22b).
- The gas-boiler curve self-tune findings (rounds 3-11) on terraced houses from all six classes.

Cannot support yet:
- Any absolute number (starts per day, kWh, comfort) for a real home.
- Results for apartments, detached homes, renovated pre-1975 homes, 1975-83 homes, mixed emitters, multi-room homes.
- Heat pumps or hybrids in pre-1983 homes, and the effect of hot-water priority, defrost or buffer tanks.
- The gas balance of the slave SAT (depends on an unknown start loss).

## 5. What would close the gap

1. Typology from the primary source: TABULA/EPISCOPE Netherlands (types x periods, including 1975-83 and renovation
   levels) instead of our derived single reference house.
2. Emitter sizing as a variable (undersized / matched / oversized radiators, low-temperature radiators, mixed
   underfloor plus radiators) instead of calibration to fit.
3. Heat-pump realism: hot-water priority stops, defrost, the unit's own control per brand where documented.
4. Validation: a 24-72 h capture of a real house (gas, then a heat pump; TASK-1527 AC#1, research page section 4).
