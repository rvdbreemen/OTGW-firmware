---
id: TASK-954
title: Wire v2 dashboard OUTSIDE reading to the weather-data API (sat/weather)
status: Done
assignee:
  - '@claude'
created_date: '2026-06-29 22:25'
updated_date: '2026-06-29 22:30'
labels: []
dependencies: []
ordinal: 167000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
User: the v2 dashboard OUTSIDE temperature is not wired to the actual weather-data API. v2.js sourced model.outside only from OT MsgID 27 (Toutside) + the otmonitor seed (both OT-bus); it never fetched /api/v2/sat/weather, so the OpenWeatherMap-backed outside temp from the weather-compensation feature was ignored. Fix: add fetchWeather() polling sat/weather; when enabled+valid, its temperature drives OUTSIDE (model.wxValid); OT Toutside / otmonitor become the fallback when weather is invalid.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 v2 fetches /api/v2/sat/weather on init + on a 60s interval
- [x] #2 When weather enabled+valid, OUTSIDE shows the weather-API temperature; OT Toutside (MsgID 27) yields to it
- [x] #3 When weather invalid/disabled, OUTSIDE falls back to OT Toutside / otmonitor (no regression)
- [x] #4 littlefs build green; evaluate --quick no new failures; prerelease bumped
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
DONE 2026-06-30 (alpha.294): added fetchWeather() polling /api/v2/sat/weather on init + every 60s. When weather enabled+valid, its temperature drives model.outside + sets model.wxValid; OT MsgID 27 (Toutside) and the otmonitor seed both yield while wxValid (case 27 gated, fetchSeed skips 'outside'), and resume as fallback when weather is invalid/disabled. Playwright-verified (mocked sat/weather valid 8.4 -> model.outside=8.4, wxValid=true, #aOut='8.4°'). littlefs SUCCESS, evaluate --quick 0-fail.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
v2 dashboard OUTSIDE now wired to the weather-data API (sat/weather): the OpenWeatherMap temperature drives OUTSIDE when weather compensation is enabled+valid, with OT Toutside as the fallback.
<!-- SECTION:FINAL_SUMMARY:END -->
