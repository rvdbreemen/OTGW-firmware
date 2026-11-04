---
id: TASK-1440
title: >-
  SAT zones: port thermo-nova area.py zone aggregation as it is (Q18 A, entry
  116)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-29T05:02:00Z'
updated_date: '2026-10-29T07:32:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 601000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-29 Q18 A (entry 116): port thermo-nova's zone logic as it is. thermo-nova requested_setpoint (climate.py:372-392): setpoint = max(primary PID, areas output); areas output = int(0.75N) percentile capped at baseline + 5.0 (area.py:286-301); overshoot ceiling 22 + 10 - 4 x degrees over for areas more than 0.3 over target, min over areas, floored at 22, applied as min(setpoint, cap) (area.py:312-333, climate.py:388-389); an area counts only with output > 22 or valve >= 10 (area.py:166-175); ECO skips both (climate.py:377-379); aggregation also with one room. Firmware today (SATcontrol.ino:5516-5566) differs in eight ways (replaces primary, ceil(0.75N)-1, headroom added, overshoot subtracted, no 0.3 margin, no cap with one zone, counts zones above 10, runs only with 2+ zones and in ECO). Changes how SAT controls the boiler: decision recorded (entry 116), so no further OK needed for the logic itself.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Zone aggregation follows climate.py:372-392 and area.py:166-175, 286-301, 312-333 (host test red on HEAD, green on fix: N=4 picks the highest output, headroom caps, main PID never lowered, overshoot ceiling floored at 22, ECO ignores zones)
- [x] #2 Settings and docs that only served the old aggregation (fZoneAggregationHeadroom) handled and documented; firmware-sat-spec and sat-comparison updated
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Sources (opened 2026-10-29 at 7beaaf5, UPSTREAM_COMMIT verified): climate.py:369-394 requested_setpoint; area.py:166-175 requires_heat, :264-301 _PIDs.output (percentile 0.75, headroom 5.0, none -> MINIMUM_SETPOINT), :303-333 overshoot_cap (OVERSHOOT_MARGIN 0.3, COOLING_HEADROOM 10, COOLING_SLOPE 4, area.py:32-34). Decisions: entry 116 (Q18 A, port as it is) combined with ADR-199 (Robert 2026-10-24, ADR-199 Resolved: area.py:175/:322/:331 use the per-system cold setpoint): every thermo-nova COLD_SETPOINT 22 in area.py becomes satGetColdSetpoint() (28.2 radiators, 21.0 underfloor). Confirmed by C5 2026-10-29. Change: new pure helper satAggregateZoneSetpoint(primary, zoneOut[], zoneErr[], zoneUsable[], n, coldSp, headroom, eco) next to satZonePidOutput; the tick keeps calling satZonePidOutput(z, outsideTemp) per zone and passes the results. Counted zone = usable && output > cold (no per-zone valve in the firmware). areas = MIN 10 when none, else idx=max(0,min(n-1,int(0.75n))), min(highest, baseline+headroom), round 1. setpoint = max(primary, areas); cap = min over counted zones with error < -0.3 of cold+10-4*over, floored at cold, round 1; setpoint = min(setpoint, cap). ECO (iHeatingMode 1): primary only. Runs for every zone count (1-4), not only >1. Kept deliberately: zone diagnostics stay gated at iZoneCount > 1 (no new MQTT traffic for default users, C5 2026-10-29). Not touched: ECO solar/rounding difference (C5 asks Sergeant D as Q58), zone PID itself (TASK-1414..1418/1423). fZoneAggregationHeadroom kept as the cap headroom (default 5.0 = area.py:267); meaning changes from 'added' to 'maximum above the P75 baseline'; help text + spec + comparison updated. Tests: new tests/test_sat_zone_aggregation.py compiles the real helper and the real tick segment; red on a632e8e68, green on the fix. Existing slicing tests (test_sat_published_curve, test_sat_pid_cadence, test_sat_pid_derivative) kept green. Build: combo only, OTGW_BUILD_JOBS=4, after C5's signal; bump after rebase.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-29 evidence. Red on a632e8e68: python tests/test_sat_zone_aggregation.py -> Ran 14, FAILED (failures=12): ECO 52.0 (want 35), four zones 49.0 (want 46, old ceil(0.75N)-1 + headroom), primary 50 lowered to 37.0, one zone at count 1 35.0 (want 45, block skipped), overshoot 1.0 over 35.0 (want 34.2), floor 35.0 (want 28.2), margin, underfloor 20.0 (want 25.0), helper missing. Green on the fix: Ran 14, OK. Self-review found satZonePidAdvanceAll() returned at iZoneCount <= 1, so a counted zone 1 at count 1 would have been curve-only: fixed (every configured zone steps); new test_sat_pid_cadence test_g2: zone I after 600 s at count 1 0.000000 on a632e8e68 vs 0.071029 on the fix (= count 2); an unconfigured zone at count 1 stays excluded (I 0, output 10), so default single-room users see no change. Kept green: test_sat_published_curve (5, stubs extended for satGetColdSetpoint/iHeatingMode/helper), test_sat_pid_cadence (27), test/host/test_sat_pid_derivative (exit 0), settings-help.test.mjs (28/28, help JSON within 18 KB), test_sat_comfort_adjust_removed (9), test_sat_ground_truth_block (6). evaluate.py --quick: 85 checks, 0 failed, 0 warnings. Refute review vs climate.py:369-394 / area.py:166-175, 264-333: matches; remaining known differences documented: no per-zone valve, zone output clamped [10, max] by the zone PID (TASK-1414..1418/1423), roundf vs Python round-half-even only on exact ties, ECO does not round the primary or skip solar (Q58, C5). Debug line limited to ticks where a zone counts.

Docs (AC2): firmware-sat-spec.md, sat-comparison.md, c4-code-sat.md, c4-component-smart-thermostat.md, manuals en/nl, CHANGELOG updated by a docs agent and checked: content matches the helper; 39 shifted SATcontrol.ino cites in the new text corrected after the rebase on 2698f3e06; stale texts fixed (settingsHelp satzonecount, two P75 code comments, manuals 'Zone 1 always maps to the primary target', which the code does not do: zone setpoints only come from a push, SATcontrol.ino:2211). fZoneAggregationHeadroom kept as the cap headroom, documented as not binding with <= 4 zones. ECO solar (entry 118, Q58 A): tests/test_sat_zone_aggregation.py test_eco_applies_no_solar_offset red on the old code (37.0 with a 3.0 offset, want 40.0), green on the fix; COMFORT still applies it (37.0). All tests/test_sat_*.py OK after the rebase; test_sat_pwm_override stub extended (iHeatingMode). Not touched: ECO 0.1 rounding (entry 95, TASK-1423).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SAT zones now follow thermo-nova area.py as it is (Sergeant D entry 116, Q18 A) combined with ADR-199 (Robert 2026-10-24: area.py's COLD_SETPOINT sites use the per-system cold setpoint 28.2 / 21.0), and ECO takes no solar offset (entry 118, Q58 A). Shipped in 2.0.0-alpha.528+20610d9 (commit 20610d9eb, posted to #alpha-testing, message 1565267051780644964). Code: new satAggregateZoneSetpoint() (climate.py:369-394, area.py:166-175, 264-333 at 7beaaf5): setpoint = max(main, zones output), zones output = min(highest, P75 + headroom) round 1, a zone counts when usable and output > cold, overshoot cap cold + 10 - 4 x over beyond 0.3 floored at cold applied as min(), ECO main only; runs for every zone count 1-4. satZonePidAdvanceAll() now steps zone 1 at count 1 (found in self-review). Deliberate: zone diagnostics stay gated at > 1 zone (C5). AC1 evidence: tests/test_sat_zone_aggregation.py red on a632e8e68 (12 of 14 fail: ECO 52 for main 35, four zones 49 for 46, main 50 lowered to 37, one zone at count 1 ignored, no cap/floor/margin, underfloor) and red on the old solar line (ECO 37.0, want 40.0); green on the fix 16/16. test_sat_pid_cadence test_g2: zone 1 I after 600 s at count 1 0.000000 old vs 0.071029 fix; no zone data -> I 0, output 10, so default single-room users see no change. AC2: fZoneAggregationHeadroom kept as the cap headroom (default 5.0 = area.py:267), meaning changed and documented; with <= 4 zones it does not bind. firmware-sat-spec, sat-comparison, c4 docs, manuals en/nl, CHANGELOG, settingsHelp updated. Gates: all tests/test_sat_*.py OK, test/host/test_sat_pid_derivative OK, settings-help.test.mjs 28/28, evaluate --quick 86 checks 0 fail 0 warn, combo build exit 0 with fresh +20610d9 artifacts (and earlier +6b8c006, same code before the docs-only rebase). Not verified: a real multi-zone house or bench run with pushed zone data (host tests only); roundf vs Python round-half-even differs only on exact ties; ECO 0.1 rounding of the main output not done (entry 95, TASK-1423); zone PID itself unchanged (TASK-1414..1418/1423).
<!-- SECTION:FINAL_SUMMARY:END -->
