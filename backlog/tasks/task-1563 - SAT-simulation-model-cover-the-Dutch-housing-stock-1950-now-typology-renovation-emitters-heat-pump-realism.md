---
id: TASK-1563
title: >-
  SAT simulation model: cover the Dutch housing stock 1950-now (typology,
  renovation, emitters, heat-pump realism)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-02T16:51:00Z'
updated_date: '2026-11-02T17:24:00Z'
labels:
  - sat
dependencies: []
priority: medium
ordinal: 708000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Model evaluation 2026-11-02 (docs/sat/sim-hp-lowload/model-evaluation-2026-11-02.md): the simulation covers one 120 m2 terraced house in six as-built eras (1975-83 missing), one zone, calibrated radiators, generic heat-pump control. Robert chose to extend the model first, then re-run the key comparisons and analyse all results. Design and simulation only, no firmware change.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 House typology taken from a primary source (TABULA/EPISCOPE NL or equivalent), types x periods incl. 1975-83, with renovation levels; each parameter cited
- [ ] #2 Emitter sizing as a variable (undersized/matched/oversized radiators, LT radiators, mixed underfloor + radiators)
- [ ] #3 Heat-pump realism added where documented: hot-water priority stops, defrost; unit control assumption shown as a sensitivity
- [x] #4 Regression: the extended model reproduces the old reference house results when given its parameters
- [ ] #5 Key comparisons re-run over the extended stock (SAT continuous vs PWM, PWM period, min ON/OFF, hybrid backup) and an analysis of all results with the validity range per conclusion
- [ ] #6 Typical houses chosen to match the Dutch stock: share of each type x period (and heating system where known) from a primary source (CBS/WoON or TABULA NL stock statistics), cited; results also reported as a stock-weighted aggregate
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Source (read, primary): RVO Voorbeeldwoningen 2022 (51 dwellings = Dutch stock to 2018, WoON2018, NTA 8800), extracted to docs/sat/sim-stock/rvo_voorbeeldwoningen_2022.json by scripts/sat_stock_extract_rvo2022.py.
1. House model per dwelling: H from RVO areas x U per element (NTA 8800 floor reduction for crawl space/ground, A, cited), ventilation from the RVO ventilation type (NTA 8800 forfaitary flows, cited), thermal mass from the construction type (NTA 8800 classes: heavy masonry/concrete, timber floors pre-1965 as medium; A where the sheet is silent). Same two-node structure as today, scaled per dwelling (floor area, envelope). Party walls to heated neighbours: no loss (RVO areas are external only).
2. Emitters: radiators sized for the oorspronkelijk heat loss at 90/70 and -10 C (the as-built design), so later insulation makes them oversized by H_orig/H_huidig: a physically motivated size instead of calibrating them to fit. Floor heating (45/40) where RVO lists it (several 2006-18). Mixed underfloor + radiators and under-/over-sizing as a sensitivity on a few dwellings.
3. States: huidig (WoON2018) for gas boilers and hybrids; pakket 2 (RVO's heat-pump package: insulated to the standard, C5a, heat pump) for heat pumps; oorspronkelijk only as a check.
4. Heat-pump realism: hot-water priority blocks (combi heat pump, daily blocks, A cited), defrost as a capacity/COP penalty below +7 C (A), the unit's stop band 1/3/5 K as a sensitivity.
5. Regression: the old reference house parameters reproduce round 21 exactly (gate before any new run).
6. Runs (10 seasons): gas SAT continuous vs PWM vs current on 51 dwellings huidig; heat pumps (3 units by minimum output) SAT continuous vs PWM on 51 dwellings pakket 2; hybrids own vs slave on huidig dwellings where the heat pump is too small. Results per dwelling and stock-weighted (RVO counts). Estimated about 13 CPU-hours at 4 workers; 5 seasons halves it.
7. Report: per conclusion the validity range (types, periods, states) and what still rests on assumptions.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-02 Robert (mid-turn): 'Doe ook typisch huizen doorrekenen van de NL woningmarkt overeenkomstig met de voorraad' (dictated; read as: compute typical houses of the Dutch housing market in line with the stock, i.e. weighted by stock shares). Reading reported back to Robert.

2026-11-02 AC#1: typology from RVO Voorbeeldwoningen 2022 (primary, read): 51 dwellings, 9 types x 7-8 periods (1975-91 class covers 1975-83), levels oorspronkelijk/huidig/3 packages; extracted by scripts/sat_stock_extract_rvo2022.py to docs/sat/sim-stock/rvo_voorbeeldwoningen_2022.json; stock counts 7.47 M dwellings (100.2 %). Parameters cited or labelled A1-A6 in scripts/sat_stock_houses.py and docs/sat/sim-stock/README.md. Plausibility vs RVO NTA 8800 heat demand (scripts/sat_stock_nta_check.py): huidig model/NTA median 1.01 (p10 0.91, p90 1.14), pakket2 1.07 (0.97-1.14) after A2b (sealed ventilation 0.35/h; was 1.23 with 0.5/h), oorspronkelijk 1.09, pakket3 1.20 (not used). AC#4: HOUSE hook in sat_hp_library_study21.py; default path unchanged (NIBE 2015 2024-25 17.3525 starts/d, 0.1947 C; hybrid Elga rad own 2673 starts, 582 kWh) and the reference house fed through the hook gives identical results (r == r2 True). Gas boilers added as units gas_boiler_min35/min55 (A). Finding from RVO: pakket 2 (heat pump) uses radiators at 55/47 in 218 of 251 variants, while SAT caps heat pumps at 40 C (entry 155): round 24 runs both caps. Unit picks pakket 2 (smallest electric unit covering the design load at -7 C): Mitsubishi WM50 42 of 51 dwellings.
<!-- SECTION:NOTES:END -->
