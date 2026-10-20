---
id: TASK-1258
title: 'SAT power/energy: optional DHW capacity and correct ''rated input'' semantics'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-19 17:33'
updated_date: '2026-10-20 20:59'
labels:
  - feature
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 390000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Feature request by gruijter 2026-10-19 (Intergas HReco 30). sat/power = RelModLevel/100 x SATboilercapacity while the flame is on (SATcontrol.ino ~2963-2976), for CH and DHW alike. Many boilers report modulation relative to the active max (CH limit ~12 kW, DHW limit ~23 kW), so DHW is under-counted. Request: optional 'DHW capacity' used when the DHW-active status bit is set; same for the SATboilerratedkw gas estimate. Also: UI label 'Boiler rated output' (v2.js ~1792) while SATtypes.h documents it as rated input; the gas estimate multiplies rated x efficiency (heat output, not gas input) and has a flow-based branch using DHWFlowRate (MsgID 19). Needs design review before implementation.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Design decision recorded (DHW capacity setting yes/no, label, gas-estimate formula)
- [x] #2 If implemented: sat/power uses DHW capacity while DHW is active, host test shows CH and DHW cases
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-19 Sergeant D (#alpha-testing): Intergas boilers ignore the max-modulation command (MM=); output is limited through the boiler's own parameters (CH limit param 3, DHW limit param 4). So modulation % is relative to whichever limit is active, which supports a separate DHW capacity setting rather than deriving it from MM.

2026-10-20 maintainer decision (full package): a) optional setting 'DHW capacity' (0 = use boiler capacity) used while the DHW-active status bit is set, for sat/power and the gas estimate; b) UI label 'Boiler rated output' -> 'Boiler rated input', setting key unchanged; c) drop the DHWFlowRate (MsgID 19) branch from the gas estimate, keep modulation x rated input.

2026-10-20 maintainer decision (revises b/c after the advisor found that a fixed nameplate input over-reports whenever the CH limit is below the nameplate, e.g. HReco 30: ~30 kW gas reported vs ~13 kW real at 100% CH): power = mod/100 x active capacity (DHW capacity while the boiler reports DHW active, 0 = use boiler capacity; else boiler capacity); estimated gas input = power / efficiency. boiler_rated_kw stays only as the enable switch (>0) and upper clamp; the DHWFlowRate branch goes; clamps use the active maximum; DHW from the boiler status bit (MsgID 0 slave DHW active).

Review team (sonnet rules/docs, opus correctness) fixes: (1) MAJOR: the DHW branch returned before satPublishMQTT(), so MQTT never carried the DHW power; satPublishMQTT() now also runs before the DHW return and the summer-simmer return (shadow/epsilon cache keeps it once per tick). (2) energy saves only ran on the full control path; moved into satPersistEnergyIfDue() right after satUpdatePowerEnergy(), so DHW/summer energy is persisted. (3) gas clamp is now a hard cap at rated input (was a step at 1.1x), matching the docs; test case in the band added. (4) replay fixture had 40 frames with odd parity (firmware drops those); recomputed, plus tests for parity and phases. (5) docs said sat/power in W; it is kW (ch05/h05). Noted, not changed: one control interval per CH<->DHW transition is booked at the other capacity (right-endpoint integration, bounded); sat sim reads bus modulation in this function (pre-existing); efficiency GET metadata min 0 vs clamp 0.5 (pre-existing). Docs list a sat/power HA sensor that discovery never publishes -> TASK-1276. Tests: test_sat_power_energy.py 12/12 (HEAD: 4 red + the in-band cap), whitelist OK, evaluate --quick 0 FAIL.

2026-10-20 bench OTGW32 .88.61, /simulate replay of tests/webui/fixtures/dhw-power-replay.txt (CH 60 s then DHW 60 s, RelModLevel 100), SAT enabled, external room temp 19.5, boiler_capacity 12, boiler_rated_kw 30. RED alpha.441: power_kw 12 in both phases; energy_kwh frozen during DHW (0.200) then jumped to 0.50 when CH resumed; gas estimate used mod x 30 x 0.92 = 27.6 kW. GREEN (this change, fw+fs via safeboot/OTA, dhw_capacity 23): power_kw 12 in CH, 23 in DHW; energy keeps integrating in DHW (~0.19 kWh per 30 s tick); gas estimate = energy / 0.92 exactly (0.7751 -> 0.8425). MQTT (temporary broker at the device's configured 192.168.88.32): sat/power received 12.00 and 23.00 (23 exists only in the DHW phase, so it was published during DHW). Settings restored: diff vs baseline only the new satdhwcapacity key; original otgw_simulation.log restored. Evidence %TEMP%\bench-1258\red-alpha441.json, green-fix.json, mqtt-power.txt.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
sat/power now uses the boiler's active limit: a new optional DHW capacity (0 = boiler capacity) while the boiler reports DHW, boiler capacity otherwise; the estimated gas input is power / efficiency, enabled and capped by 'Boiler rated input' (relabelled from 'rated output', key unchanged); the meaningless DHW-flow branch is gone. Found on the way: power/energy were frozen during DHW because the DHW branch returned before the update, MQTT publish and energy saves; all three now run on the DHW and summer paths. AC1: design decision recorded in the notes (maintainer chose power / efficiency). AC2: host test compiling the real satUpdatePowerEnergy() (12 cases, 4+1 red on HEAD), plus bench replay old vs new (DHW 12 -> 23 kW, energy no longer frozen, MQTT carries 23). Not verified: a live boiler doing DHW (replay only); existing energy totals are not reset, so old and new semantics mix in long-running totals (release notes say so).
<!-- SECTION:FINAL_SUMMARY:END -->
