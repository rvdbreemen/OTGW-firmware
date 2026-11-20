---
id: TASK-2015
title: >-
  SAT web UI: the weather hint asks for latitude/longitude while HA pushes
  sat/sun_elevation (thaghostnl)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-18T06:34:00Z'
updated_date: '2026-11-18T12:15:00Z'
labels:
  - webui
  - sat
dependencies: []
priority: low
ordinal: 1033000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by thaghostnl in #alpha-testing 2026-11-17 20:35 UTC (https://discord.com/channels/812969634638725140/1514720723980259460/1572349530458816624), beta.170, solar gain on, weather fetch off, outdoor temperature via sat/outdoor_temp: the SAT page says 'Location detection needs HTTPS. Enter Latitude and Longitude...', although a fresh external sat/sun_elevation (younger than 1 h) takes precedence (TASK-1410). Robert (maintainer, relayed by the coordinator 2026-11-18): "Niet iedereen gebruikt Home Assistant (HA). Dus dat is geen given, maar als HA de zonnestand doorgeeft, dan mag je dat gebruiken natuurlijk." So latitude/longitude stays the default input for users without HA; when HA pushes sat/sun_elevation, the card uses that and does not ask for a location.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 With a fresh external sat/sun_elevation the weather card names that source and asks for no location (screenshot)
- [x] #2 The latitude/longitude and solar-gain help name sat/sun_elevation as the alternative
- [x] #3 Both paths shown with screenshots: without HA the card asks for latitude/longitude; with a fresh sat/sun_elevation from HA it names that source and asks for no location
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Plan (ba, 2026-11-18; no code before the coordinator's go). Finding: the SAT status JSON exposes neither the sun elevation nor its source (only solar_gain_active, SATcontrol.ino:4974), so this is firmware + web UI, with a bump, build and bench.
1. Firmware, one freshness rule: a helper satSunElevationSource(now) -> 0 none / 1 local / 2 external, using the exact test of satSunElevationStepAt (iSunElevExtMs != 0 && now - iSunElevExtMs < 3600000 = external; else bSunElevationValid && bSunElevationLocal = local), inside the satSunElevationStep marker block so tests/test_sat_sun_elevation.py compiles it; satSunElevationStepAt uses the same helper. The status emitter adds sun_elevation (only when valid) and sun_elevation_source ("external"/"local"/"none") next to solar_gain_active, from the snapshot (SatStatusSnap holds the full state copy). No new endpoint: the 2-in-flight rule holds.
2. Weather fetch still needs a location: lat/lon also feed the weather fetch. With weather fetch on and no lat/lon the card keeps asking for a location even with a fresh HA sun elevation; only the sun-position reason goes away. thaghost has fetch off.
3. Web UI (v2.js): move the location-hint decision out of the one-time satWireDetectLocation() into the polled render, outside renderSatWeather's early return for weather off. external: a line 'Sun position from Home Assistant (sat/sun_elevation): X deg', no location ask (unless weather fetch is on without lat/lon); local: as today plus the computed elevation; none: today's hint, now also naming sat/sun_elevation from HA as the alternative. The Detect button stays hidden on plain HTTP.
4. Help texts (settingsHelp.h, then regenerate settingsHelpGz.h): satweatherlat/satweatherlon and satsolargain name sat/sun_elevation as the alternative; satsolarminelev fixed (it says the gate needs MQTT; since TASK-1410 the firmware computes the elevation from lat/lon).
5. Translations: new English keys in v2.js; i18n/nl.json gets the Dutch strings; the other packs (de, el, ...) fall back to English and get a sync through the translate skill (named, not silently left).
6. Tests, red on the old code: one tests/webui/*.test.mjs for the render decision (external + weather off: source named, no location ask; external + weather on without lat/lon: still asks; none: asks), run alone; tests/test_sat_sun_elevation.py cases for the helper and a source check of the emitter field.
7. Evidence for AC1/AC3: screenshots on .61 by the bench tester with synthetic lat/lon (52.000, 5.000) or cropped: first without HA (asks for lat/lon), then after a published sun elevation on the real MQTT command topic (grep MQTTstuff.ino:713 table for the exact topic before the bench request), within the hour.
Open: TASK-2015 still carries needs-maintainer-ok although Robert's words are in the description; the coordinator to clear the label or confirm the quote is the OK.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-18 needs-maintainer-ok removed: released by Robert's direction, 2026-11-18 (coordinator); his words in the description are the OK for this scope. GO for the plan as written. The freshness rule is shared with the control step (satSunElevationStepAt): no SAT behaviour change, so no Sergeant D question.

2026-11-18 implemented (ba): SATcontrol.ino satSunElevationExternalFresh()/satSunElevationSource() in the satSunElevationStep block, the step and the status emitter share it; status JSON sun_elevation_source (external/local/none) + sun_elevation (when not none) next to solar_gain_active; v2.js satLocHintKind/satUpdateLocHint run on every SAT status and weather answer (thaghost's weather-off case included); help texts satweatherlat/lon, satsolargain, satsolarminelev (was wrong: said MQTT is required); i18n de/el/nl new keys + the pre-existing missing settings.label.satpwmswitchmargin (TASK-1888 left it out, i18n-lang was red on dev). Tests: tests/webui/sat-sun-source-hint.test.mjs 4/4 on the fix, 4/4 red with OTGW_DATA_DIR at origin data/; tests/test_sat_sun_elevation.py 21/21, red on origin SATcontrol.ino (no helper); sat-detect-location harness updated 7/7; i18n-lang 6/6 (was red on dev); v2-undefined-names 2/2; SAT-related webui tests 18 files green; 13 python tests on status JSON/help green; evaluate --quick 95 checks 0 FAIL. Synthetic screenshots (headless Chrome, fake device): task2015-ha-source.png (HA source named, no location ask), task2015-no-ha-asks-location.png (asks lat/lon, names sat/sun_elevation). Translations de/el written by me, not native-reviewed: a translate-skill review is advised.

2026-11-18 translations: de/el lines (sat_geo_hint, sat_sun_from_ha, sat_sun_ha_weather_loc, settings.label.satpwmswitchmargin) are NOT native-reviewed. The translate skill (two adversarial reviews + editor) was judged too heavy for 8 lines; checked by hand against i18n/glossary.md: row 66 weather/location = Wetter/Standort, kairos/topothesia (used), PWM untranslated (row 22). nl written by a Dutch-fluent agent. A translate-skill 'de/el sync' pass is advised later. Version renumbered to 371 at the rebase (370 = otgw-dev TASK-1995 Step B, coordinator).

Known limitation (2026-11-18, no fix for 2.0.0, coordinator): sun_elevation_source=local is only computed inside the SAT control pass (satSunElevationStep, called from satControlLoop SATcontrol.ino:8160, after the boiler-fault skip and the fallback returns). On a unit without an OT link, or while the boiler is offline, /v2/sat/status therefore reports none even when latitude/longitude are set. external does not depend on the pass: satHandleSunElevation (SATcontrol.ino:3334-3347) stamps iSunElevExtMs, and the emitter derives the source from it at emit time. The card is unaffected: satLocHintKind (v2.js:7877-7880) only separates external from the rest. The local value's only consumer is the solar-gain gate inside the control pass.

Bench PASS 2026-11-18 by otgw-bench-tester on .63 (Classic + S3), 2.0.0-beta.371+9073f9b, sha256 app/FS matched: no HA at 52.000/5.000 and at 0/0 the card asks for Latitude/Longitude and names sat/sun_elevation, Detect location hidden on plain HTTP; mosquitto_pub 23.4 on <top>/set/<node>/sat/sun_elevation gave source=external, sun_elevation 23.4 within 0.4 s and the card 'Sun position from Home Assistant (sat/sun_elevation): 23.4°. No location needed.' (plus the weather-fetch line when weather is on without a location); 0 JS exceptions in 4 runs; 4 scrubbed card screenshots in 09's scratchpad t2015/shots. local not benched (needs a live OT link; host test only). .63 restored to beta.360. Posted: Discord message 1572585909080031515 (flash zip 9073f9b).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The SAT weather card no longer asks for a location while Home Assistant publishes a fresh sat/sun_elevation. The SAT status JSON carries sun_elevation_source (external/local/none, the firmware's 1 h rule from TASK-1410 via one shared helper) and sun_elevation; v2.js satLocHintKind/satUpdateLocHint follow every status poll. AC1: host web UI test tests/webui/sat-sun-source-hint.test.mjs 4/4 on the fix, 4/4 red on the old data/; screenshot task2015-ha-source.png (synthetic data, session scratchpad shots2015/); bench .63 card-00-HA-23.4.png and card-52-5-HA-23.4.png (09). AC2: settingsHelp.h satsolarminelev, satweatherlat, satweatherlon and satsolargain name Home Assistant (sat/sun_elevation) as the alternative to latitude/longitude; settingsHelpGz.h regenerated; test_progmem_assets_deflated 4/4. AC3: task2015-no-ha-asks-location.png plus bench card-52-5-noHA.png and card-00-noHA.png (no HA, asks for latitude/longitude and names sat/sun_elevation) against the HA screenshots of AC1. Gates on 9073f9b after the rebase on origin/dev 90343abe1: test_sat_sun_elevation 21/21 (red on origin before the fix), i18n-lang 6/6, sat-detect-location 7/7, v2-undefined-names 2/2, evaluate --quick 88 pass 0 FAIL 0 WARN, esp32-combo build EXIT=0 (flash 93.7 %). Not verified on hardware: the local source (needs a live OT link; host test only); known limitation noted: without an OT link the status reports none instead of local, the card is unaffected. de/el translations checked against the glossary, not native-reviewed.
<!-- SECTION:FINAL_SUMMARY:END -->
