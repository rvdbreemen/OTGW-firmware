---
id: TASK-1257
title: >-
  SAT 'Detect location' always fails on the device: browser geolocation needs a
  secure context
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-19 17:33'
updated_date: '2026-10-19 18:57'
labels:
  - bug
  - webui
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 389000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by gruijter 2026-10-19 (screenshot: Weather Open-Meteo, Detect location -> 'Location error'). v2.js (~4289-4302) uses navigator.geolocation, which browsers only allow on HTTPS or localhost; the device is plain HTTP by design (no HTTPS), so the button cannot work. Workaround: enter latitude/longitude by hand in Settings > SAT > Weather.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 On a non-secure context the button is hidden or replaced by a hint pointing to the manual latitude/longitude fields (screenshot)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementation (uncommitted, awaiting commit stage):
- data/v2.js: new satGeoUsable() = window.isSecureContext !== false && navigator.geolocation. New satWireDetectLocation() (called from wireSat): on a non-secure context or without the API it hides #satDetectLoc and shows #satLocHint 'Location detection needs HTTPS. Enter Latitude and Longitude under SAT settings > Weather.' (labels verified: SAT_L2 group 'Weather', fields 'Latitude'/'Longitude', heading 'SAT settings' on the same SAT page). On a secure context (HTTPS reverse proxy) the button is wired as before.
- satDetectLocation(): guards on satGeoUsable(); error callback maps GeolocationPositionError code 1/2/3 to 'Location denied: the browser blocked access' / 'Location unavailable' / 'Location timed out' (fallback 'Location error').
- data/v2.html: hidden <div class=sub id=satLocHint> under the button. The hint is separate from #satCoords because renderSatWeather() overwrites satCoords on every weather poll.
- No other geolocation call in data/ (grep).
- Test: tests/webui/sat-detect-location.test.mjs (7 tests, runs the real functions cut from v2.js): fix 7/7 pass; against HEAD v2.js the test fails (functions absent). Behavioural old-vs-fix repro with an insecure-context stub (geolocation present, fails code 1 like Chrome on http): OLD = button visible, click -> 'Location error'; FIX = button hidden, hint shown.
- node --test tests/webui/*.test.mjs 35/35 pass; tests/test_static_integrity.py OK (33); evaluate.py --quick exit 0, 0 failed.
- AC#1 screenshot still open: needs the bench (plain HTTP device).

Finalize run 2026-10-19 (alpha.439): build.bat esp32-combo SUCCESS (firmware 238 s + buildfs 17 s), fresh build/OTGW-firmware-esp32-combo-2.0.0-alpha.438+a99fd95.ino.bin 20:55:09, .littlefs.bin 20:55:29, .elf kept; .pio/data-staging/v2.js contains satWireDetectLocation. evaluate.py --quick: 79 checks, 72 passed, 0 warn, 0 failed, exit 0. node --test tests/webui/*.test.mjs 35/35. tests/test_static_integrity.py 33 OK. sat-detect-location.test.mjs: fix 7/7; against HEAD v2.js 6/7 fail on assertions (HTTPS happy path passes on both, never broken). old-repro.mjs: OLD button visible, click -> 'Location error', no hint; FIX button hidden, hint 'Location detection needs HTTPS. Enter Latitude and Longitude under SAT settings > Weather.' Docs: ch05-sat-thermostat.md and RELEASE_NOTES_2.0.0.md say geolocation needs HTTPS. AC#1 left unchecked: it asks for a screenshot of the real UI on a plain-HTTP device; no device flashed in this run (flashing not permitted here).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Cause: navigator.geolocation only works in a secure context (HTTPS or localhost). The device serves plain HTTP by design, so 'Detect location' always failed with 'Location error'. Fix (v2.js/v2.html): satGeoUsable() checks isSecureContext and the API; satWireDetectLocation() hides #satDetectLoc on an insecure context and shows #satLocHint pointing at SAT settings > Weather > Latitude/Longitude; behind an HTTPS proxy the button works as before, and geolocation error codes 1/2/3 now give specific messages. Docs updated (manual ch05, release notes). Evidence AC#1: logic proven by tests/webui/sat-detect-location.test.mjs (fix 7/7, HEAD 6/7 fail on assertions) and the old-vs-fix repro (OLD button visible + 'Location error'; FIX button hidden + hint). Build (esp32-combo) SUCCESS, evaluate --quick 0 failed, webui 35/35, static integrity 33 OK. Open: AC#1 screenshot on a plain-HTTP device after flashing alpha.439 (bench), so the task stays In Progress.
<!-- SECTION:FINAL_SUMMARY:END -->
