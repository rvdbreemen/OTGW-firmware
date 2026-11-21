---
goal: OpenWeatherMap Onboarding - auto-detect missing outside temperature and guide user through API key + location setup
version: 1.3
date_created: 2026-05-02
last_updated: 2026-05-02
owner: rvdbreemen/OTGW-firmware
status: 'Phase 0 complete; Phase 1-6 partially landed via PR #559 squash; remainder tracked in TASK-510'
tags: [feature, weather, sat, ui, onboarding]
---

# Introduction

![Status: In Progress](https://img.shields.io/badge/status-In%20Progress-yellow)

When OTGW-firmware starts up and the SAT subsystem has been running for at least 5 minutes
without a valid outside temperature (neither from the OT bus nor the existing weather provider),
the Web UI automatically guides the user through configuring **OpenWeatherMap (OWM) One Call 3**.

The flow:
1. Detect "no reliable outside temp" after 5 minutes in the browser.
2. Prompt the user for an OWM API key (link to openweathermap.org) if one is not stored.
3. Obtain geographic location from firmware settings, falling back to the browser Geolocation API.
   The browser Geolocation API is also used silently to auto-prefill lat/lon whenever it is not yet set.
4. Let the user confirm or correct the location.
5. Validate the location and API key by making a real OWM API call from the browser (HTTPS).
6. On acceptance, persist the API key and coordinates to firmware settings and trigger first fetch.
7. Thereafter, firmware polls OWM at most once every **15 minutes** (hard-capped).

**Prerequisite: Open-Meteo validation**
Before adding OWM, the existing Open-Meteo integration must be confirmed to work end-to-end.
Users have reported that entering lat/lon leaves the outside temperature at zero. This must be
diagnosed and fixed first (Phase 0). Open-Meteo remains the zero-config fallback; OWM is additive.

> **Why OWM alongside the existing Open-Meteo provider?**
> Open-Meteo (free, no key) is already implemented and remains the default when no OWM key is
> configured. Both APIs are gated at 15 minutes maximum to avoid excessive calls.

---

## 1. Requirements & Constraints

- **REQ-001**: Detect "no reliable outside temperature" after 5 minutes of firmware uptime.
  "No reliable" means `OTcurrentSystemState.Toutside == 0.0` AND `state.sat.weather.bValid == false`.
  Detection is browser-driven by polling `/api/v2/sat/weather/needs-setup`.
- **REQ-002**: Onboarding modal sequence shown only once per browser session.
- **REQ-003**: OWM API key stored in firmware settings (`sWeatherApiKey[64]`) persisted to `settings.ini`.
- **REQ-004**: Location from firmware settings if set; otherwise browser Geolocation API. User must confirm.
  Additionally, when the SAT panel first opens and no coordinates are set, the browser Geolocation API
  is used silently to auto-populate the lat/lon fields (no button click required).
- **REQ-005**: OWM validation call made from the browser (HTTPS). Firmware uses plain HTTP for ongoing fetches.
- **REQ-006**: Firmware polls both Open-Meteo and OWM at most once per **15 minutes**. Hard guard in firmware.
  For Open-Meteo the default is already 15 min (`iWeatherInterval = 900`). For OWM a separate hard-floor
  timer of 900 s is introduced. Neither can be configured below 15 minutes.
- **REQ-007**: OWM temperature injected via `satHandleExternalTemp()` so all SAT consumers receive it.
- **REQ-008**: OWM API key never stored in browser localStorage or any persistent browser storage.
- **REQ-009**: All new firmware string literals use F() / PSTR() (PROGMEM). No raw String class (ADR-004).
- **REQ-010**: OWM support added to `SATweather.ino`, not a new file.
- **REQ-011**: Weather fetching (both Open-Meteo and OWM) must NOT start until both conditions are met:
  (a) firmware uptime >= 300 s (5 minutes), AND (b) no valid outside temperature from the OT bus
  (`OTcurrentSystemState.Toutside == 0.0f`). This guard lives in `weatherLoop()`.
- **REQ-012**: When the SAT panel first activates in the browser and no coordinates are saved in firmware
  settings (fWeatherLat == 0 && fWeatherLon == 0), the browser Geolocation API is called silently
  (no user prompt, no modal) to auto-populate the setting. If geolocation is unavailable the UI
  shows a silent hint to use the "Detect Location" button or enter values manually.
- **REQ-013**: The existing Open-Meteo integration must be validated end-to-end before OWM is added.
  This includes confirming the API URL, JSON parsing, coordinate handling, and timer behavior are
  all correct and produce a non-zero temperature. Any bugs found during this validation must be
  fixed as part of Phase 0 before Phase 1 begins.
- **CON-001**: ESP8266 ~40 KB usable RAM. OWM `/data/2.5/weather` JSON is ~400 bytes; 4096-byte buffer sufficient.
- **CON-002**: ESP8266 firmware uses HTTP only (no TLS). OWM plain-HTTP `/data/2.5/weather` used for firmware.
  Browser uses HTTPS OWM One Call 3 directly only during onboarding validation.
- **CON-003**: OWM free tier 1000 calls/day. At 15-min intervals = 96 calls/day. Well within limit.
- **CON-004**: Browser Geolocation API requires secure context (HTTPS or localhost) in Chrome.
  UI must fall back to manual lat/lon input when navigator.geolocation is unavailable.
- **GUD-001**: Use existing `showFeedback()` toast pattern for non-blocking messages. Modals only for blocking decisions.
- **GUD-002**: All modals use native HTML `<dialog>` element (Chrome 37+, Firefox 98+, Safari 15.4+).
  Style with existing design-system tokens from `ds-tokens.css`.
- **GUD-003**: Existing "Detect Location" button (`SAT.detectLocation()`) must continue to work unchanged.
- **PAT-001**: New setting named `SATweatherapikey` stored as `sWeatherApiKey[64]` in `SATSection` of `OTGWSettings`.
- **PAT-002**: Firmware rate-limit uses `DECLARE_TIMER_SEC` / `DUE()` from `safeTimers.h`.

---

## 2. Implementation Steps

### Implementation Phase 0 - Open-Meteo validation and fixes (prerequisite)

- GOAL-000: Diagnose and fix the Open-Meteo integration so outside temperature is correctly
  populated when lat/lon are configured. This phase must be verified before Phase 1 begins.

| Task | Description | Completed | Date |
| ---- | ----------- | --------- | ---- |
| TASK-000 | **Investigate zero-temperature bug**: With `bWeatherEnable=true` and valid lat/lon set, reproduce the "outside temp stays at zero" condition. Check via telnet debug output: (a) does `weatherFetch()` actually run (is `bWeatherEnable` true and `DUE(timerWeatherPoll)` firing)? (b) does the HTTP GET succeed with code 200? (c) is the JSON "current" section present and does `weatherJsonGetFloat()` find `temperature_2m`? Log findings. | | |
| TASK-001 | **Fix `weatherJsonGetFloat()` + `weatherJsonGetArray()` PROGMEM key handling**: Both functions previously used `snprintf_P(search, sizeof(search), PSTR("\"%s\":"), key)` where `key` is a `PGM_P` (PROGMEM) — `%s` reads a RAM pointer, not a flash pointer. On ESP8266, unaligned flash reads cause Exception(2). Fixed by adding `char keyBuf[32]; strncpy_P(keyBuf, key, sizeof(keyBuf)-1);` before building the search string, and using `snprintf(search, sizeof(search), "\"...\", keyBuf)` (RAM-only). | ✓ | 2026-05-02 |
| TASK-001a | **Expand Open-Meteo API to full SAT data set** (developer request): Update `weatherFetch()` URL to request all current-conditions fields relevant for SAT thermal load (`temperature_2m`, `apparent_temperature`, `relative_humidity_2m`, `is_day`, `precipitation`, `rain`, `showers`, `snowfall`, `weather_code`, `cloud_cover`, `pressure_msl`, `surface_pressure`, `wind_speed_10m`, `wind_direction_10m`, `wind_gusts_10m`) plus 24-hour hourly forecasts (`temperature_2m`, `relative_humidity_2m`, `dew_point_2m`, `apparent_temperature`, `precipitation_probability`, `cloud_cover`, `cloud_cover_low`, `cloud_cover_mid`). Add corresponding fields to `state.sat.weather` struct in `SATtypes.h`. Add new static hourly arrays (`_weather_forecastDewPt[]`, `_weather_forecastCloud[]` as uint8_t, `_weather_forecastPrecipProb[]` as uint8_t). Increase heap buffer from 4096 to 5120 bytes; lower guard from +4096 to +2048 (response is ~3-4 KB). Update `weatherSendStatusJSON()` and `weatherPublishMQTT()` to expose all new fields. Add `weatherJsonGetArrayU8()` helper for integer arrays. | ✓ | 2026-05-02 |
| TASK-002 | **Verify `satGetOutsideTemp()` uses weather data when Toutside == 0**: The priority chain in `SATcontrol.ino` uses weather data only when `OTcurrentSystemState.Toutside == 0.0f`. Add telnet debug log entry when weather fallback is activated so the operator can confirm it is in use. | | |
| TASK-003 | **Add `weatherLoop()` 5-minute startup gate and outside-temp guard**: Change `weatherLoop()` to: `if (state.uptime.iSeconds < 300) return;` (5-minute startup delay) and `if (OTcurrentSystemState.Toutside != 0.0f) return;` (skip when OT bus already provides outside temp). Both guards apply to both Open-Meteo and OWM paths. | ✓ | 2026-05-02 |
| TASK-004 | **Fix minimum interval for Open-Meteo**: Enforce `iWeatherInterval >= 900` in `updateSetting()` (currently the setting allows 60 s as minimum per `sendJsonSettingObj` range). Change the `constrain()` floor from 60 to 900 to match the 15-minute requirement. Update the `sat-grp-weather` settings group `min` value in `index.js` to 900 accordingly. | ✓ | 2026-05-02 (TASK-511) |
| TASK-005 | **Validate auto-start after 5 minutes**: Reboot firmware, enable weather with valid lat/lon. Confirm via telnet debug log that the first fetch does NOT occur in the first 5 minutes, and DOES occur after 5 minutes when `Toutside == 0`. Confirm temperature reads non-zero and expanded telnet line shows e.g. `Weather: 7.3C (feels 4.1C), 72% RH, 12.5 km/h wind, 75% cloud, WMO 3, 24 h forecast, 4212 bytes`. | | |

### Implementation Phase 1 - Firmware: OWM setting + HTTP fetch

- GOAL-001: Add OWM API key storage to firmware settings and implement a plain-HTTP OWM fetch
  in `SATweather.ino`, hard-limited to 15-minute intervals.

| Task | Description | Completed | Date |
| ---- | ----------- | --------- | ---- |
| TASK-006 | **`SATtypes.h`**: Add `char sWeatherApiKey[64] = ""` to `SATSection` of `OTGWSettings` after `iWeatherInterval`. | | |
| TASK-007 | **`settingStuff.ino`**: Add `writeJsonStringKV(file, F("SATweatherapikey"), settings.sat.sWeatherApiKey, true)` in `writeSettings()` and `else if (strcasecmp_P(field, PSTR("SATweatherapikey")) == 0) strlcpy(settings.sat.sWeatherApiKey, newValue, sizeof(settings.sat.sWeatherApiKey))` in `updateSetting()`. | | |
| TASK-008 | **`SATweather.ino`**: Declare `static const uint16_t WEATHER_OWM_MIN_SEC = 900` (15 min) and `DECLARE_TIMER_SEC(timerOWMPoll, 900, CATCH_UP_MISSED_TICKS)`. | | |
| TASK-009 | **`SATweather.ino` - `weatherFetchOWM()`**: Implement function that: (a) guards `sWeatherApiKey[0] != '\0'`; (b) guards `DUE(timerOWMPoll)`; (c) builds URL `http://api.openweathermap.org/data/2.5/weather?lat=%.4f&lon=%.4f&units=metric&appid=%s` with `snprintf_P`; (d) fetches via `WiFiClient` + `HTTPClient`; (e) parses `main.temp` using `weatherJsonGetFloat()` (note: OWM wraps temp in "main" object, not top-level); (f) calls `satHandleExternalTemp()` to inject value; (g) updates `state.sat.weather.fTemperature`, `bValid`, `iLastUpdateMs`; (h) respects 4096-byte heap buffer + malloc guard. | | |
| TASK-010 | **`SATweather.ino` - `weatherLoop()`**: After the startup gate (TASK-003), if `sWeatherApiKey[0] != '\0'` call `weatherFetchOWM()` (15-min hard floor); else call existing `weatherFetch()` (Open-Meteo, 15-min default). Mutually exclusive paths. The 5-minute startup gate and outside-temp guard added in TASK-003 apply to both paths. | | |
| TASK-011 | **`restAPI.ino` - mask key**: In SAT settings GET response, expose `sendJsonSettingObj(F("satweatherapikey"), settings.sat.sWeatherApiKey[0] != '\0' ? "***" : "", "s")`. Never expose actual key. | | |
| TASK-012 | **`restAPI.ino` - allowlist**: Add `"satweatherapikey"` to `kSettingsAllowList[]`. | | |
| TASK-013 | **`restAPI.ino` - `outside_temp_valid`**: In `satSendStatusJSON()`, add boolean field `outside_temp_valid` = true when `abs(satGetOutsideTemp()) > 0.1f` OR `bExternalOutdoorValid` OR `weather.bValid`. | | |
| TASK-014 | **`OTGW-firmware.h`**: Add forward declaration `void weatherFetchOWM();`. | | |

### Implementation Phase 2 - REST: `needs-setup` endpoint

- GOAL-002: Provide a single cheap REST endpoint the browser polls to decide whether to show the wizard.

| Task | Description | Completed | Date |
| ---- | ----------- | --------- | ---- |
| TASK-015 | **`restAPI.ino` - `handleSatWeatherNeedsSetup()`**: GET handler returning `{"needs_setup": bool, "uptime_seconds": N, "outside_temp_valid": bool, "api_key_configured": bool}`. `needs_setup` is true when `state.uptime.iSeconds >= 300 && !outside_temp_valid && !api_key_configured`. | | |
| TASK-016 | **`restAPI.ino` - route**: Add `{ PSTR("GET"), PSTR("/api/v2/sat/weather/needs-setup"), handleSatWeatherNeedsSetup, false }` to `kV2Routes[]`. | | |

### Implementation Phase 3 - Browser: modal dialog infrastructure

- GOAL-003: Add a lightweight native-dialog modal system styled with existing design-system tokens.

| Task | Description | Completed | Date |
| ---- | ----------- | --------- | ---- |
| TASK-017 | **`index.html`**: Add four `<dialog>` elements at end of `<body>`: `dlg-owm-no-temp`, `dlg-owm-apikey`, `dlg-owm-location`, `dlg-owm-validate`. Each uses `<form method="dialog">` for native close semantics and `.owm-dialog` class for styling. | | |
| TASK-018 | **`index.html` - Dialog 1 (`dlg-owm-no-temp`)**: Heading "No outdoor temperature detected"; body text; "Configure Weather API" primary button; "Dismiss" secondary button; paragraph with link to `https://openweathermap.org`. | | |
| TASK-019 | **`index.html` - Dialog 2 (`dlg-owm-apikey`)**: `<input type="password" id="owm-key-input" maxlength="64" autocomplete="off">`; link to `https://home.openweathermap.org/api_keys`; "Next" button (disabled until input non-empty); "Back" button; "Free tier: 1000 calls/day" note. | | |
| TASK-020 | **`index.html` - Dialog 3 (`dlg-owm-location`)**: `<input type="number" id="owm-lat-input">` and `<input type="number" id="owm-lon-input">` with `step="0.0001"` and min/max constraints; "Detect from browser" button (`id="owm-btn-geolocate"`); "Validate" primary button; "Back" button; `<p id="owm-location-status">` for inline status. | | |
| TASK-021 | **`index.html` - Dialog 4 (`dlg-owm-validate`)**: Read-only `<div id="owm-validate-result">` showing resolved city name + current temperature from OWM; "Accept & Save" primary button; "Back" button. | | |
| TASK-022 | **`components.css` - `.owm-dialog`**: Add `dialog.owm-dialog` rules using `--color-*`, `--radius-*`, `--space-*`, `--font-*` tokens; `::backdrop` with `rgba(0,0,0,0.5)` overlay; `dialog[open]` centered display. Under 60 lines. | | |

### Implementation Phase 4 - Browser: onboarding logic + auto-prefill location in `sat.js`

- GOAL-004: Implement the full onboarding state machine — polling, dialog steps, OWM validation,
  and settings save. Also add silent auto-prefill of coordinates from browser Geolocation API
  when the SAT panel activates and no coordinates are configured.

| Task | Description | Completed | Date |
| ---- | ----------- | --------- | ---- |
| TASK-023 | **`sat.js` - silent auto-prefill on panel activation**: When the SAT panel first becomes visible, check server settings for `satweatherlat` and `satweatherlon`. If both are zero (not yet configured), call `navigator.geolocation.getCurrentPosition()` silently (no modal, no feedback message). On success, POST the resolved lat/lon to `POST /api/v2/settings` and enable weather (`SATweatherenable=1`). On failure (permission denied, unavailable), show a one-line non-blocking hint in the SAT feedback area: "Set your location for outdoor temperature." Do NOT show this hint more than once per session. | | |
| TASK-024 | **`sat.js` - `owmOnboarding` IIFE**: Self-contained module (IIFE pattern matching existing `SAT` IIFE) exposing `owmOnboarding.init()` and `owmOnboarding.suppress()`. | | |
| TASK-025 | **`sat.js` - `_checkNeedsSetup()` polling**: Calls `fetch(APIGW + 'v2/sat/weather/needs-setup')`; if `needs_setup === true` and not suppressed -> `_showStep1()`. First poll fires after `setTimeout(startPolling, 300000)` (5 min), then `setInterval(..., 60000)`. Stops when complete or suppressed. All fetch calls include `.catch()`. | | |
| TASK-026 | **`sat.js` - `_showStep1()`**: Opens `dlg-owm-no-temp` via `showModal()`. "Configure" -> `_showStep2()`. "Dismiss" -> `suppress()` which sets `_suppressed = true` and writes `sessionStorage.setItem('owm-onboarding-dismissed','1')`. | | |
| TASK-027 | **`sat.js` - `_showStep2()`**: Opens `dlg-owm-apikey`. If server returned `"***"` sentinel show placeholder "Key already configured - enter new key to replace". Enables "Next" only when input length > 0 and value != "***". "Next" -> `_showStep3()`. "Back" -> `_showStep1()`. | | |
| TASK-028 | **`sat.js` - `_showStep3()`**: Opens `dlg-owm-location`. Pre-fills lat/lon from server settings if non-zero (reuses values already saved by TASK-023 auto-prefill if it ran). If lat/lon are still zero, calls `navigator.geolocation.getCurrentPosition()` on dialog open (this is the explicit, user-visible call). Shows live status in `owm-location-status`. "Detect from browser" button re-triggers geolocation. "Validate" -> `_validateAndShowStep4()`. "Back" -> `_showStep2()`. | | |
| TASK-029 | **`sat.js` - `_validateAndShowStep4()`**: Reads lat/lon from inputs, validates range. Reads API key from `owm-key-input`. Sets `owm-location-status` to "Validating...". Calls OWM One Call 3 HTTPS directly from browser: `fetch('https://api.openweathermap.org/data/3.0/onecall?lat='+lat+'&lon='+lon+'&units=metric&exclude=minutely,hourly,daily,alerts&appid='+key)`. On HTTP 200: also calls OWM reverse geocoding `/geo/1.0/reverse` to resolve city name; populates `owm-validate-result` with city name + temperature. On error (4xx/5xx/network): shows inline error in `owm-location-status`, does NOT advance. All `fetch()` calls include `.catch()`. | | |
| TASK-030 | **`sat.js` - `_showStep4()`**: Opens `dlg-owm-validate` with populated city + temperature result. "Accept & Save" -> `_saveAndFinish()`. "Back" -> `_showStep3()`. | | |
| TASK-031 | **`sat.js` - `_saveAndFinish()`**: POSTs API key, lat, lon, and `SATweatherenable=1` to `POST /api/v2/settings`. On success: closes all dialogs, calls `showFeedback('Weather configured - fetching data...', false)`, stops polling, triggers `fetchWeather()` after 2 s. On error: `showFeedback('Save error: ' + e.message, true)`. | | |
| TASK-032 | **`sat.js` - session suppression**: Module-level `var _suppressed = false`. On `init()`, if `sessionStorage.getItem('owm-onboarding-dismissed') === '1'` set `_suppressed = true`. On dismiss, write the key. | | |
| TASK-033 | **`sat.js` - `init()` wiring**: Call `owmOnboarding.init()` inside `SAT.init()`. Also wire the silent auto-prefill (TASK-023) in the same init path. Guard with `var _initialized = false`. | | |

### Implementation Phase 5 - Settings panel: API key field

- GOAL-005: Expose the API key in the SAT settings panel for power users.

| Task | Description | Completed | Date |
| ---- | ----------- | --------- | ---- |
| TASK-034 | **`index.js` - weather settings group**: In `sat-grp-weather`, add `{ key: 'satweatherapikey', label: 'OWM API Key', type: 's', maxlength: 64, inputType: 'password' }`. Update settings renderer to render `inputType: 'password'` as `<input type="password">`. | | |
| TASK-035 | **`index.js` - tooltip**: Add `["SATweatherapikey", "OpenWeatherMap API key. Get a free key at openweathermap.org. Leave blank to use Open-Meteo (free, no key needed)."]` to `settingTooltips`. | | |
| TASK-036 | **`index.js` - label map**: Add `["SATweatherapikey", "SAT OWM API Key"]` to the label map. | | |

### Implementation Phase 6 - Verification & documentation

- GOAL-006: Verify correct end-to-end behaviour for both Open-Meteo and OWM, and update documentation.

| Task | Description | Completed | Date |
| ---- | ----------- | --------- | ---- |
| TASK-037 | **Manual test - Open-Meteo end-to-end**: With `bWeatherEnable=true` and valid lat/lon (auto-filled or manual), reboot, wait 5+ minutes, confirm telnet shows a successful weather fetch with non-zero temperature, and confirm SAT outside temp shows the weather value. | | |
| TASK-038 | **Manual test - 15-minute gate**: Confirm neither Open-Meteo nor OWM fetches more than once per 15 minutes (verified via telnet log timestamps). | | |
| TASK-039 | **Manual test - 5-minute startup gate**: Reboot with weather enabled. Confirm no fetch occurs in the first 5 minutes. Confirm first fetch fires within 1 minute after the 5-minute mark. | | |
| TASK-040 | **Manual test - OWM firmware fetch**: Set key + coordinates via `POST /api/v2/settings`. Verify `GET /api/v2/sat/weather` shows `valid: true` and non-zero temperature. | | |
| TASK-041 | **Manual test - auto-prefill**: Open SAT panel in browser with no coordinates set. Confirm lat/lon are silently auto-filled if geolocation is available. Confirm polite hint shown if geolocation is unavailable. | | |
| TASK-042 | **Manual test - full OWM wizard**: Disable weather, reboot, wait 5 min. Verify Dialog 1 appears. Step through all 4 dialogs. Verify key + lat/lon saved. Verify weather updates within 1 min. | | |
| TASK-043 | **Manual test - dismiss**: Dismiss Dialog 1. Verify no reappearance same session. Verify reappears after page reload if still unconfigured. | | |
| TASK-044 | **Manual test - insecure context**: Access device over `http://` in Chrome. Verify Dialog 3 shows the fallback message and manual input fields accept values. | | |
| TASK-045 | **Manual test - invalid key**: Enter wrong API key. Verify inline error and no advance to Dialog 4. | | |
| TASK-046 | **`python evaluate.py`**: Passes with no new failures (PROGMEM, no raw String, no Serial.print after init). | | |
| TASK-047 | **`python build.py --firmware`**: Clean firmware build succeeds without errors. | | |
| TASK-048 | **Documentation**: Update README.md (or wiki) to describe Open-Meteo default, OWM option, 15-minute rate limit, and key registration link. | | |

---

## 3. Alternatives

- **ALT-001: Use Open-Meteo for everything** - Already implemented, free, no key. Rejected because
  the user explicitly requested OpenWeatherMap and an API-key-based flow. Open-Meteo remains as fallback.
- **ALT-002: Use OWM One Call 3 from firmware via HTTPS (BearSSL)** - ESP8266 TLS costs 20-30 KB RAM
  and ~3 s per handshake; project ADR forbids TLS in firmware. Rejected.
- **ALT-003: Proxy OWM through a firmware endpoint** - Adds complexity and still hits the TLS problem.
  Rejected.
- **ALT-004: Use localStorage for dismiss state** - More durable but the user should be reminded after
  a new browser session if still unconfigured. sessionStorage is intentional.
- **ALT-005: Replace Open-Meteo entirely** - Removes the zero-config no-account default. Open-Meteo
  is kept as fallback when no OWM key is configured.
- **ALT-006: Use 10-minute floor for OWM** - Initially planned but changed to 15 minutes per developer
  request to match Open-Meteo's default interval and give even more safety margin on the free-tier quota.

---

## 4. Dependencies

- **DEP-001**: `safeTimers.h` - DECLARE_TIMER_SEC / DUE() (already present).
- **DEP-002**: `ESP8266HTTPClient.h` / `HTTPClient.h` - already used in SATweather.ino.
- **DEP-003**: `http://api.openweathermap.org/data/2.5/weather` - OWM Current Weather (plain HTTP). No new library.
- **DEP-004**: `https://api.openweathermap.org/data/3.0/onecall` - OWM One Call 3 (HTTPS, browser-only).
- **DEP-005**: `https://api.openweathermap.org/geo/1.0/reverse` - OWM Reverse Geocoding (HTTPS, browser-only).
- **DEP-006**: Browser Geolocation API - already used by SAT.detectLocation() in sat.js.
- **DEP-007**: Native HTML `<dialog>` element - Chrome 37+, Firefox 98+, Safari 15.4+.

---

## 5. Files

- **FILE-001**: `src/OTGW-firmware/SATtypes.h` - Add `sWeatherApiKey[64]` to `OTGWSettings.sat`.
- **FILE-002**: `src/OTGW-firmware/SATweather.ino` - Fix `weatherJsonGetFloat()` PROGMEM bug; add startup gate;
  add OWM timer (`WEATHER_OWM_MIN_SEC = 900`), `weatherFetchOWM()`, update `weatherLoop()`.
- **FILE-003**: `src/OTGW-firmware/settingStuff.ino` - Persist and parse `SATweatherapikey`; tighten minimum
  interval floor to 900 s.
- **FILE-004**: `src/OTGW-firmware/restAPI.ino` - Allowlist, masked key, `needs-setup` handler, `outside_temp_valid`.
- **FILE-005**: `src/OTGW-firmware/OTGW-firmware.h` - Forward-declare `weatherFetchOWM()`.
- **FILE-006**: `src/OTGW-firmware/data/index.html` - Four `<dialog>` elements for wizard.
- **FILE-007**: `src/OTGW-firmware/data/sat.js` - Silent auto-prefill on panel activation + `owmOnboarding` IIFE.
- **FILE-008**: `src/OTGW-firmware/data/components.css` - `.owm-dialog` styles.
- **FILE-009**: `src/OTGW-firmware/data/index.js` - API key field, tightened interval minimum, tooltip, label map.

---

## 6. Testing

- **TEST-001**: `GET /api/v2/sat/weather/needs-setup` returns `needs_setup:true` when uptime >= 300 s, Toutside == 0, weather not valid, no key configured.
- **TEST-002**: Returns `needs_setup:false` after key + coordinates saved and first OWM fetch succeeds.
- **TEST-003**: Weather fetching does NOT occur in the first 5 minutes after boot (Open-Meteo and OWM both gated).
- **TEST-004**: Weather fetching does NOT occur when `OTcurrentSystemState.Toutside != 0` (OT bus has valid outside temp).
- **TEST-005**: Neither Open-Meteo nor OWM fetches more than once per 15 minutes.
- **TEST-006**: `GET /api/v2/settings` returns `satweatherapikey:"***"` - never the actual key.
- **TEST-007**: Open-Meteo fetch with valid lat/lon returns a non-zero temperature (validates Phase 0 fix).
- **TEST-008**: `python build.py --firmware` succeeds with zero errors.
- **TEST-009**: `python evaluate.py` passes with no new failures.
- **TEST-010**: Browser auto-prefills lat/lon silently on SAT panel activation (geolocation available).
- **TEST-011**: Browser shows silent hint (not a modal) when geolocation unavailable; no crash.
- **TEST-012**: Dialog 1 appears automatically after 5 min with no outside temp (Chrome, Firefox, Safari).
- **TEST-013**: Dismiss suppresses dialog for the session; reappears after page reload.
- **TEST-014**: Dialog 3 shows fallback message and manual input when served over HTTP in Chrome.
- **TEST-015**: Invalid API key -> inline error in Dialog 3, no advance to Dialog 4.
- **TEST-016**: "Accept & Save" POSTs key, lat, lon, and SATweatherenable=1.
- **TEST-017**: After successful onboarding, SAT weather section shows valid temperature within 2 min.
- **TEST-018**: Settings panel renders API key field as `<input type="password">` with correct tooltip.

---

## 7. Risks & Assumptions

- **RISK-001**: OWM may retire the plain HTTP `/data/2.5/weather` endpoint. Mitigation: Open-Meteo fallback active.
- **RISK-002**: Browser Geolocation blocked over `http://`. Mitigation: Dialog 3 always shows manual lat/lon input.
- **RISK-003**: Open-Meteo API URL or response format may have changed. Mitigation: TASK-000 validates this explicitly.
- **RISK-004**: `weatherJsonGetFloat()` PROGMEM `%s` vs `%S` issue on ESP8266 may be the root cause of the zero-temperature bug. Mitigation: TASK-001 addresses this explicitly.
- **RISK-005**: `CATCH_UP_MISSED_TICKS` on `timerOWMPoll` could fire immediately after a long stall. Mitigation: the 5-minute startup guard prevents premature fetches even after a stall.
- **ASSUMPTION-001**: OWM `/data/2.5/weather` plain HTTP endpoint remains accessible from ESP8266.
- **ASSUMPTION-002**: OWM One Call 3 and Reverse Geocoding endpoints accessible from browser without CORS issues.
- **ASSUMPTION-003**: The `weatherJsonGetFloat()` PROGMEM key issue is the primary cause of Open-Meteo returning zero. If TASK-001 does not fix it, further investigation of the Open-Meteo API response format is needed.
- **ASSUMPTION-004**: Users open the SAT panel within a reasonable time, or the 60-second polling tick fires shortly after.

---

## 8. Related Specifications / Further Reading

- [OpenWeatherMap One Call 3 API](https://openweathermap.org/api/one-call-3)
- [OpenWeatherMap Current Weather Data - plain HTTP](https://openweathermap.org/current)
- [OpenWeatherMap Reverse Geocoding](https://openweathermap.org/api/geocoding-api#reverse)
- [Browser Geolocation API - MDN](https://developer.mozilla.org/en-US/docs/Web/API/Geolocation_API/Using_the_Geolocation_API)
- [HTML dialog element - MDN](https://developer.mozilla.org/en-US/docs/Web/HTML/Element/dialog)
- [Open-Meteo API docs](https://open-meteo.com/en/docs)
- src/OTGW-firmware/SATweather.ino - existing Open-Meteo weather implementation
- src/OTGW-firmware/SATcontrol.ino - satGetOutsideTemp() priority chain
- src/OTGW-firmware/SATtypes.h - OTGWSettings.sat and OTGWState.sat.weather
- src/OTGW-firmware/data/sat.js - existing detectLocation() and fetchWeather() patterns
- docs/adr/README.md - ADR index (ADR-004 String rule applies to all new firmware code)

---

## Changelog

| Version | Date | Changes |
| ------- | ---- | ------- |
| 1.0 | 2026-05-02 | Initial plan |
| 1.1 | 2026-05-02 | Added Phase 0 Open-Meteo validation; changed OWM minimum interval from 10 to 15 minutes; added 5-minute startup gate (REQ-011); added browser auto-prefill of lat/lon on SAT panel activation (REQ-012, TASK-023); added Open-Meteo validation tasks (REQ-013, TASK-000 to TASK-005); renumbered all tasks; updated CON-003 from 144 to 96 calls/day; added ALT-006 |
