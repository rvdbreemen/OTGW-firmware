---
id: TASK-1410
title: >-
  SAT: compute the sun elevation locally from latitude/longitude and NTP time
  (solar gain needs it)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-26T20:13:00Z'
updated_date: '2026-10-31T16:10:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 548000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
gruijter 2026-10-26 19:29 (#alpha-testing msg 1564360234938540185): 'why was my sun position unknown this morning? My lat lon position were available in settings.' The firmware does not compute the sun position; lat/lon only feed the Open-Meteo fetch, and the sun elevation can only arrive over MQTT (OTGW/set/<id>/sat/sun_elevation, expires after 1 h). thermo-nova reads it from HA sun.sun (climate.py:504-506). After TASK-1408 (no solar gain without a sun position) units without an external elevation never detect solar gain. Proposal: compute elevation locally (standard NOAA/solar-position formula from lat/lon + UTC time, no dependency) when no external value is fresh. Behaviour change for SAT inputs: needs Robert's OK.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Sun elevation computed from lat/lon + NTP time within 0.5 degree of a reference (host test vs known values)
- [x] #2 An external MQTT value still takes precedence while fresh
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-30 maintainer decision (Robert, in session otgw-firmware-ea): released for pickup ('vrijgeven'); label needs-maintainer-ok removed. Priority after beta-1 blockers unless it is one.

2026-10-31: prerequisite of TASK-1525 (ADR-205 self-tune, entry 178: without a sun elevation the tuner holds); Robert's OK for the local computation given in session (entry 178); priority high with TASK-1525 (before beta.1).

2026-10-31 evidence (alpha.575+081ee4d, this session): tests/test_sat_sun_elevation.py HEAD 12/12 OK; same tests on pre-change SATcontrol.ino (4e0945d90~1, OTGW_SAT_INO) 12/12 FAILED. Covers Meeus elevation vs NOAA reference values within 0.5 deg (AC1) and external-value precedence <1 h / stale handling (AC2). Bench .61 alpha.575: bench_check SATTUNEH reason=flame_off, not no_sun, so the local sun elevation is valid on the device. evaluate.py --quick 80 passed/0 warn/0 fail; combo build OK (RAM 36.8%, flash 92.7%).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SAT computes the sun elevation locally (Meeus/NOAA) from latitude/longitude and NTP time (satSunElevationDeg/satSunElevationStep); an external MQTT/REST value wins while less than 1 h old. Evidence: AC1+AC2 host tests 12/12 green on HEAD, 12/12 red on the pre-change source; bench .61 alpha.575 shows tuner hours with a valid sun (reason not no_sun). Shipped alpha.571, published in the alpha.575 zip (msg 1566119828505235510).
<!-- SECTION:FINAL_SUMMARY:END -->
