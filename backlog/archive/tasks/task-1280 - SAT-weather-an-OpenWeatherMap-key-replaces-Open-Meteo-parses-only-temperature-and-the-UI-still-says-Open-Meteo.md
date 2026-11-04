---
id: TASK-1280
title: >-
  SAT weather: an OpenWeatherMap key replaces Open-Meteo, parses only
  temperature, and the UI still says Open-Meteo
status: Done
assignee:
  - '@claude'
created_date: 2026-10-21 09:31
updated_date: '2026-10-21T18:57:00Z'
labels:
  - sat
  - weather
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 409000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Report gruijter 2026-10-21 (#alpha-testing), fw 2.0.0-alpha.443+067b478. With a key set weatherFetch() only calls OWM (SATweather.ino ~522), no failover. weatherParseOwmTemp() reads only main.temp: humidity, wind, cloud cover and forecast stay 0/empty (/api/v2/sat/weather shows temperature 10.48, rest 0, forecast []). The Weather card keeps saying Open-Meteo. The 0 % humidity is accepted as valid input for comfort adjust (SATcontrol.ino ~4520). The OWM plan (plan/feature-openweathermap-onboarding-1.md) came from Open-Meteo returning 0 C, fixed in its Phase 0. Suggestion: remove OWM, or make it a real failover that parses humidity and wind and shows the active source. Needs a maintainer decision: remove OWM or keep it as failover.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Maintainer decision recorded: remove OWM or keep it as a real failover
- [x] #2 With the chosen design, no weather field is reported as valid 0 when the source does not provide it
- [x] #3 The Weather card shows the active source
- [x] #4 Bench or host test shows old vs new /api/v2/sat/weather output
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-21 maintainer decision: remove the OpenWeatherMap option; Open-Meteo is the only weather source. A stored OWM key is ignored and the setting disappears.

2026-10-21 RED bench (alpha.445+d23ef15, OTGW32 .88.61): weather enabled, lat 52.09 / lon 5.12, fake OWM key TESTKEY-FAKE-1280. After reboot the first fetch ran at 900 s (timer interval + 5 min boot gate): /api/v2/sat/weather fetch_errors 1, valid false, every field 0, forecast []. Open-Meteo was never tried because a key routes weatherFetch() to OWM only. Evidence %TEMP%\bench-1280\red-weather.json. Note for the green run: an earlier attempt without reboot never fetched, because OTcurrentSystemState.Toutside was non-zero (stale from a replay) and weatherLoop skips the fetch when the OT bus provides an outside temperature (by design).

2026-10-21 GREEN (alpha.445+9b7628e, fw+fs OTA, the settings.ini still holding the fake OWM key from the red run): the key is ignored on load, the first fetch (20:46, 900 s floor after boot) used Open-Meteo: valid true, 10.9 C, humidity 79 %, wind 21.6 km/h, cloud 82 %, pressure 1005.9, 24 h forecast (red: fetch_errors 1, all 0, forecast []). Weather card (SAT > Technical) shows 'Open-Meteo' with that data, 0 console errors (weather-card.png). Review (sonnet): one real defect fixed: the updateSetting debug mask lost SATweatherapikey, so an old settings.ini would print the real key to telnet on the first boot after the upgrade; the mask keeps the legacy field name. Stale golden scripts/json-golden/v2_debug.json (settings.sat.weather_key) updated. GET /sat/weather/needs-setup drops has_key (only reported the OWM key; v2.js never calls needs-setup). Gates: evaluate --quick 72/0, test_settings_whitelist OK, test_static_integrity 33 OK, openapi_compliance --golden 18/18, webui 45/0 (agent run). Bench restored: weather off, lat/lon 0; settings diff vs baseline = only the removed satweatherapikey. Not verified on the device: the mask line itself (settings are read at boot before telnet is up), checked by reading the code.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
OpenWeatherMap is removed; Open-Meteo is the only weather source (maintainer decision). An OWM key replaced Open-Meteo instead of backing it up and only the temperature was parsed. AC1: decision recorded. AC2: with the old key still stored, the fix fetches Open-Meteo with every field (10.9 C, 79 %, 21.6 km/h, forecast); the old firmware with a key returned only errors and zeros. AC3: the Weather card says Open-Meteo, which is now always true (screenshot). AC4: /api/v2/sat/weather old vs new captured. Removed: settings.sat.sWeatherApiKey / SATweatherapikey (old settings.ini loads, key ignored and dropped on the next write, still masked in the debug log), the OWM fetch/parse code, has_key in sat/weather/needs-setup. Not verified on the device: the debug mask line (boot-time, before telnet).
<!-- SECTION:FINAL_SUMMARY:END -->
