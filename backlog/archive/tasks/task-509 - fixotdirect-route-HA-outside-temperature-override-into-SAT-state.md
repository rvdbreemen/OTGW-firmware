---
id: TASK-509
title: 'fix(otdirect): route HA outside temperature override into SAT state'
status: Done
assignee:
  - '@codex'
created_date: '2026-05-01 21:27'
updated_date: '2026-05-02 08:40'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
HA outside-temperature override commands should travel through the existing OTGW command route and be visible to SAT as the current outside temperature. Current logs show SAT remains at outside=0.0 and no OTDirect OT= command reaches the handler in the provided trace.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 HA outside-temperature command topics that map to OT= are accepted by the generic MQTT OTGW command route.
- [x] #2 A numeric OT= command accepted by OTDirect updates the local outside-temperature state used by SAT without requiring a boiler echo.
- [x] #3 Regression coverage protects the MQTT alias and OTDirect state-update contract.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Trace HA outside-temperature MQTT discovery and generic command dispatch to identify why OT= is not emitted. 2. Keep the existing OTGW command route and add the missing accepted topic alias if needed. 3. Update the OTDirect OT= handler so accepted numeric overrides update the local Toutside state used by SAT immediately. 4. Add focused regression checks and run targeted verification.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Observed in user logs: SAT cycles repeatedly report outside=0.0, while OTDirect logs CS/MM/CH commands but no OT= command and no MsgID 27 request frame. Patched existing route: MQTT outside_temp alias maps to OT=, and accepted numeric OT= updates OTcurrentSystemState.Toutside for SAT immediately.

Focused regression test passed: .venv\\Scripts\\python.exe tests\\test_evaluate.py TestHAOutsideTemperatureRoute. ESP32 build was attempted with build.py --target esp32 but is blocked before route verification by unrelated SATble compile errors: _bleSensors/BLESensorData/SAT_BLE_MAX_SENSORS are unresolved in SATble.ino.

Live hardware log on 2026-05-02 confirms the route effect: SAT cycles repeatedly report outside=11.9, curve=29.2, and final control uses that value instead of the previous outside=0.0. This provides device-side proof that the outside temperature is now visible to SAT; remaining observed instability is BLE room-temperature staleness, not the outside-temperature route.

Closure verification on 2026-05-02: full .venv\\Scripts\\python.exe build.py passed for ESP8266 and ESP32-S3; .venv\\Scripts\\python.exe evaluate.py --quick passed with 58 passed, 2 warnings, 0 failed, health 97.0%; .venv\\Scripts\\python.exe tests\\test_evaluate.py passed 47 tests; .venv\\Scripts\\python.exe tests\\test_build.py passed 9 tests. The two evaluator warnings are pre-existing/general checks and not introduced by this route change.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed the HA outside-temperature override path for SAT by making accepted OT= commands update OTcurrentSystemState.Toutside immediately after the MsgID 27 write is queued. Added regression coverage that pins both the MQTT outside/outside_temp alias mapping to OT= and the OTDirect local-state update. Live device logs now show SAT using outside=11.9 and curve=29.2 instead of outside=0.0. Verified with full build.py, evaluate.py --quick, tests/test_evaluate.py, and tests/test_build.py.
<!-- SECTION:FINAL_SUMMARY:END -->
