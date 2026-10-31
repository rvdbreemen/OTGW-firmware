---
id: TASK-1410
title: >-
  SAT: compute the sun elevation locally from latitude/longitude and NTP time
  (solar gain needs it)
status: To Do
assignee: []
created_date: '2026-10-26T20:13:00Z'
updated_date: '2026-10-30T21:56:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
ordinal: 548000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
gruijter 2026-10-26 19:29 (#alpha-testing msg 1564360234938540185): 'why was my sun position unknown this morning? My lat lon position were available in settings.' The firmware does not compute the sun position; lat/lon only feed the Open-Meteo fetch, and the sun elevation can only arrive over MQTT (OTGW/set/<id>/sat/sun_elevation, expires after 1 h). thermo-nova reads it from HA sun.sun (climate.py:504-506). After TASK-1408 (no solar gain without a sun position) units without an external elevation never detect solar gain. Proposal: compute elevation locally (standard NOAA/solar-position formula from lat/lon + UTC time, no dependency) when no external value is fresh. Behaviour change for SAT inputs: needs Robert's OK.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Sun elevation computed from lat/lon + NTP time within 0.5 degree of a reference (host test vs known values)
- [ ] #2 An external MQTT value still takes precedence while fresh
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-30 maintainer decision (Robert, in session otgw-firmware-ea): released for pickup ('vrijgeven'); label needs-maintainer-ok removed. Priority after beta-1 blockers unless it is one.
<!-- SECTION:NOTES:END -->
