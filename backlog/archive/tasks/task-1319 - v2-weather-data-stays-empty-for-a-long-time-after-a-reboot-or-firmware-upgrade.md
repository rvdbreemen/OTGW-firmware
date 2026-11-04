---
id: TASK-1319
title: >-
  v2: weather data stays empty for a long time after a reboot or firmware
  upgrade
status: Done
assignee:
  - '@claude'
created_date: '2026-10-23T13:13:00Z'
updated_date: '2026-10-23T22:33:00Z'
labels:
  - sat
  - weather
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 442000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
gruijter 2026-10-23 12:44Z (#alpha-testing, screenshot): after a firmware upgrade (and probably after a reboot) the weather data takes a long time to fill. Asks whether it can be fetched right away. Check the weather fetch schedule at boot (first fetch delay vs interval) and whether WiFi/NTP readiness gates it.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The first weather fetch after boot happens within a bounded time once WiFi and time are up (measured on the bench: seconds from boot to state.sat.weather.bValid)
- [x] #2 Old vs new boot-to-weather time recorded
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Root cause (SATweather.ino): weatherLoop has a 5-min start-up gate (WEATHER_POLL_MIN_SEC 300), then waits for DUE(timerWeatherPoll); that timer is declared with 900 s and runs from boot, so the first fetch came ~15 min after boot. Fix: while state.sat.weather.bValid is false, fetch right after the gate; a failed try waits 60 s, doubling, capped at iWeatherInterval (a fetch blocks the loop for its DNS/connect/read timeouts, so an offline unit must not retry every minute); no try is counted without coordinates; once valid, RESTART_TIMER so the periodic fetch follows iWeatherInterval later. OT Toutside skip unchanged. Not a SAT control change (thermo-nova reads weather from an HA entity, no fetch schedule). Verify on the bench: seconds from reboot to /api/v2/sat/weather valid, old vs new.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-24 evidence (OTGW32 bench, weather enabled with coordinates, OT Toutside 0, reboot via /ReBoot, poll /api/v2/sat/weather every 5 s): old firmware alpha.477/478 code: valid after ~911 s since the reboot request (temp 10.5, errors 0); fix (flashed over WiFi OTA): valid after ~313 s (temp 10.4, errors 0). Review (opus): no correctness bug; adopted: no backoff growth without coordinates (would delay the first fetch after onboarding by up to the interval), comment corrected (fetch blocks for DNS + connect + read timeouts, not 5 s). Noted, unchanged: iWeatherInterval is clamped 900-3600 so the cap guard is defensive; offline unit now tries at 300/360/480/720/1200 s then every 900 s (before: 900 s, then every 900 s). evaluate 0 fail 0 warn; build.bat combo SUCCESS.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The first weather fetch after a reboot now happens right after the 5-minute start-up wait instead of on the first 15-minute poll tick, with a 60 s doubling retry until it succeeds. Evidence: AC1 bench boot-to-valid 313 s on the fix; AC2 911 s on the old code, same scenario. Not verified: a unit without internet over a long run (backoff reasoned from code).
<!-- SECTION:FINAL_SUMMARY:END -->
