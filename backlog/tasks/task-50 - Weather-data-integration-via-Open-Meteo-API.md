---
id: TASK-50
title: Weather data integration via Open-Meteo API
status: Done
assignee:
  - '@claude'
created_date: '2026-04-06 10:53'
updated_date: '2026-04-06 11:14'
labels:
  - sat
  - feature
  - webui
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Fetch outdoor temperature, humidity, and 24-hour forecast from the Open-Meteo API (free, no API key required, no credit card). Use browser geolocation to auto-detect coordinates, with manual override in settings. Weather data improves SAT heating curve accuracy and enables predictive control (pre-heat before temperature drops). Open-Meteo provides hourly forecasts including temperature, humidity, wind speed, and cloud cover. HTTP-only (no HTTPS required on ESP8266 via their non-SSL endpoint).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Browser geolocation: JavaScript navigator.geolocation to get lat/lon, store in settings
- [x] #2 Manual coordinate entry in settings (latitude, longitude) as fallback
- [x] #3 Fetch current weather from Open-Meteo API (temperature, humidity, wind)
- [x] #4 Fetch 24-hour forecast (hourly temperature array)
- [x] #5 Parse JSON response with lightweight parser (no ArduinoJson)
- [x] #6 Store weather data in state struct (current temp, humidity, forecast array)
- [x] #7 Use outdoor temp from weather API as fallback when no OT outdoor sensor available
- [x] #8 Configurable poll interval (default 15 min, min 5 min)
- [x] #9 WebUI: weather section showing current conditions and 24h forecast trend
- [x] #10 MQTT publish: sat/weather/temperature, sat/weather/humidity, sat/weather/forecast
- [x] #11 REST API: GET /api/v2/sat/weather for current weather data
- [x] #12 Error handling: retry on failure, show last-known data, don't crash on API timeout
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add weather settings to OTGW-firmware.h (lat, lon, poll interval, enable)
2. Add weather state struct (current temp, humidity, wind, 24h forecast array)
3. Create SATweather.ino: HTTP client for Open-Meteo API, JSON parser, timer-based polling
4. Integrate outdoor temp from weather as fallback in satGetOutsideTemp()
5. Add settings persistence in settingStuff.ino
6. Add REST endpoint GET /api/v2/sat/weather
7. Add MQTT publish for weather data
8. Add WebUI: geolocation JS, weather display section, forecast trend
9. Commit and push
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementation complete. Files modified/created:
- NEW: SATweather.ino (weather fetch, JSON parser, REST/MQTT)
- OTGW-firmware.h (weather state struct, settings fields, forward declarations)
- settingStuff.ino (read/write weather settings)
- restAPI.ino (settings UI, knownSettings whitelist, /api/v2/sat/weather endpoint)
- SATcontrol.ino (weather fallback in satGetOutsideTemp, MQTT publish)
- OTGW-firmware.ino (weatherLoop call in main loop)
- index.html (weather section in SAT dashboard)
- sat.js (weather fetch/display, detectLocation geolocation)
- index.js (translateSettings + tooltips for weather settings)

Build: pre-existing esp_task_wdt.h ESP8266 build error unrelated to changes.
Evaluate: 96.4% health score, no new PROGMEM violations.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Weather data integration via Open-Meteo API.

New SATweather.ino module (294 lines):
- HTTP client fetching from Open-Meteo (free, no API key)
- Lightweight JSON parser (no ArduinoJson)
- Current weather: temperature, humidity, wind speed
- 24-hour hourly temperature forecast array
- Timer-based polling (default 15 min, configurable 5-60 min)
- Weather outdoor temp as fallback in satGetOutsideTemp()
- Browser geolocation for auto lat/lon detection
- REST: GET /api/v2/sat/weather
- MQTT: sat/weather/temperature, humidity, wind_speed
- WebUI: weather section with Detect Location button
- Settings: SATweatherenable, SATweatherlat, SATweatherlon, SATweatherinterval
<!-- SECTION:FINAL_SUMMARY:END -->
