---
id: TASK-507
title: >-
  polish(satble+ui): show BLE temperature with 2 decimals throughout (debug log
  + dashboard)
status: Done
assignee:
  - '@claude'
created_date: '2026-05-01 20:32'
updated_date: '2026-05-01 20:37'
labels:
  - polish
  - satble
  - ui
  - esp32
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
George reported (follow-up to TASK-506) that the SAT BLE temperature should be shown with 2 decimals instead of 1. The underlying data is already 2-decimal precise — both ATC/pvvx and BTHome v2 BLE protocols deliver temperature as `int16 * 0.01`, MQTT publishes via `dtostrf(temp, 1, 2, value)` already use 2 decimals, and `state.sat.fBleTemp` is a full `float`. The precision is being thrown away only at three rendering sites:

1. `SATble.ino:322` — `SATBLEDebugTf("SAT BLE: sensor %s slot=%d temp=%.1f hum=%.1f ...")` — debug log on every accepted ad
2. `SATble.ino:441` — `SATBLEDebugTf("SAT BLE: best sensor slot=%d mac=%s temp=%.1f age=%ums")` — debug log on best-sensor selection
3. `data/sat.js:84` — `fmtTemp(v)` returns `v.toFixed(1) + '°C'` — used for room/target/outside/final_setpoint tiles plus simulation room/flow

The third is the user-facing fix; sites #1 and #2 are consistency in debug output (also seen in George's logs).

The fmtTemp change is global to all SAT dashboard temperatures by design: the OpenTherm spec native precision is f8.8 (~0.004°C), so 2 decimals is the right default for every temperature on the dashboard, not just the BLE-derived one. Target temperature has step=0.5 in the +/- buttons but rendering 21.50°C is still readable and consistent with the rest.

No backend/state/MQTT changes — those are already correct.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 SATble.ino:322 accepted-sensor log uses %.2f for temperature (and humidity, for consistency)
- [x] #2 SATble.ino:441 best-sensor log uses %.2f for temperature
- [x] #3 data/sat.js:84 fmtTemp() uses toFixed(2) instead of toFixed(1)
- [x] #4 All SAT dashboard tiles that use fmtTemp (room/target/outside/final_setpoint plus simulation room/flow) now render with 2 decimals
- [x] #5 python evaluate.py --quick exits 0; PROGMEM check still PASS; design-system class drift gate still PASS (no markup change)
- [x] #6 python build.py builds clean on ESP8266 and ESP32-S3; LittleFS image rebuilt so the JS change ships
- [x] #7 No backend/state/MQTT publishing format changes — those were already 2-decimal via dtostrf(..., 1, 2, ...)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
## Plan

Three surgical edits, no new state, no ADR. The data path already preserves 2-decimal precision; we only widen three rendering sites.

### Edit 1 — `src/OTGW-firmware/SATble.ino:322`
Change format string in the accepted-sensor SATBLEDebugTf:
- `temp=%.1f hum=%.1f` → `temp=%.2f hum=%.2f`

### Edit 2 — `src/OTGW-firmware/SATble.ino:441`
Change format string in the best-sensor SATBLEDebugTf:
- `temp=%.1f age=%ums` → `temp=%.2f age=%ums`

### Edit 3 — `src/OTGW-firmware/data/sat.js:84`
Change fmtTemp() return:
- `v.toFixed(1) + '°C'` → `v.toFixed(2) + '°C'`

### Verify
1. `python evaluate.py --quick` — must stay 0 failed checks; design-system drift gate unaffected (no markup change).
2. `python build.py` (full, not --firmware) — required because the LittleFS image needs to be rebuilt for the JS change to actually ship to the device.
3. Visual check of the diff to confirm no other temp formatter touched accidentally.

### Risk assessment
- Cosmetic only. No new precision; the data has always been 2-decimal in storage and over MQTT (`dtostrf(..., 1, 2, ...)`).
- Target temp tiles will now show "21.50°C" instead of "21.5°C". Slightly more verbose but consistent with the rest of the dashboard. If George dislikes that, easy to parametrise fmtTemp later (one optional arg). Not pre-engineering.
- ESP8266 RAM/Flash budget unchanged (PROGMEM string lengthens by 1 char per format spec — negligible).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Patch applied to three sites:
- `src/OTGW-firmware/SATble.ino:322` — accepted-sensor log: `%.1f` → `%.2f` for both temp and hum
- `src/OTGW-firmware/SATble.ino:441` — best-sensor log: `%.1f` → `%.2f` for temp
- `src/OTGW-firmware/data/sat.js:87` — fmtTemp(): `toFixed(1)` → `toFixed(2)`, with explanatory comment referencing OT spec native f8.8 precision and the BLE 2-decimal data path

Verified the three diffs cleanly; no other temperature formatter touched. Out-of-scope formatters (heating_curve toFixed(1), pid_output toFixed(1), etc.) deliberately left alone — those are diagnostic/PID-tuning fields, not 'the temperature sensor' from George's feedback. Easy follow-up if he wants those too.

AC #5: `python evaluate.py --quick` exits 0, score 97.0%, design-system class drift gate still PASS (no markup change, only a JS string formatter and two PROGMEM format strings).
AC #6: `python build.py` running in background (full build incl. LittleFS image rebuild — mandatory for JS change to ship).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Surfaces 2-decimal precision for SAT temperatures throughout the user-visible surface. The data path was already 2-decimal precise (BLE parsers `int16 * 0.01`, MQTT `dtostrf(temp, 1, 2, ...)`, `state.sat.fBleTemp` is full `float`); the precision was being thrown away only at three rendering sites.

**Source changes** (3 surgical edits, no new state, no ADR):
- `src/OTGW-firmware/SATble.ino:322` — accepted-sensor SATBLEDebugTf: `temp=%.1f hum=%.1f` → `temp=%.2f hum=%.2f`
- `src/OTGW-firmware/SATble.ino:441` — best-sensor SATBLEDebugTf: `temp=%.1f` → `temp=%.2f`
- `src/OTGW-firmware/data/sat.js:87` — `fmtTemp()`: `toFixed(1)` → `toFixed(2)`, with comment referencing OT spec native f8.8 precision (~0.004°C)

**User-visible impact**: George's BLE sensor temperature now reads e.g. `22.13°C` instead of `22.1°C` on the SAT dashboard tile. Same change applies to room/target/outside/final_setpoint and simulation tiles (all consume `fmtTemp`), giving consistent precision across the dashboard. Out-of-scope formatters (heating_curve, pid_output, coefficient — diagnostic/PID-tuning fields, not "the temperature sensor") deliberately untouched.

**Verification:**
- `python evaluate.py --quick`: exit 0, 0 failed checks, PROGMEM PASS, design-system class drift gate PASS, score 97.0% (no regression).
- `python build.py`: clean on both platforms — full build incl. LittleFS rebuild required for JS change to ship.
  - ESP8266: SUCCESS in 42s, RAM 84.6%, Flash 77.3% (unchanged).
  - ESP32-S3: SUCCESS, RAM 31.7%, Flash 95.9% (unchanged within rounding).

**No backend/state/MQTT format changes.** Pure rendering-precision fix.
<!-- SECTION:FINAL_SUMMARY:END -->
