---
id: TASK-1280
title: >-
  SAT weather: an OpenWeatherMap key replaces Open-Meteo, parses only
  temperature, and the UI still says Open-Meteo
status: To Do
assignee: []
created_date: '2026-10-21 09:31'
updated_date: '2026-10-21 09:33'
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
- [ ] #1 Maintainer decision recorded: remove OWM or keep it as a real failover
- [ ] #2 With the chosen design, no weather field is reported as valid 0 when the source does not provide it
- [ ] #3 The Weather card shows the active source
- [ ] #4 Bench or host test shows old vs new /api/v2/sat/weather output
<!-- AC:END -->
