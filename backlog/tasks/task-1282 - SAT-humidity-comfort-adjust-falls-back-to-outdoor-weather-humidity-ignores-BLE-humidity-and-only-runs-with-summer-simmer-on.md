---
id: TASK-1282
title: >-
  SAT humidity: comfort adjust falls back to outdoor weather humidity, ignores
  BLE humidity, and only runs with summer simmer on
status: Done
assignee:
  - '@claude'
created_date: 2026-10-21 09:32
updated_date: '2026-10-29T05:54:00Z'
labels:
  - sat
  - humidity
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 411000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Report gruijter 2026-10-21 (#alpha-testing). Comfort / thermal-comfort fall back to outdoor weather humidity when no indoor value is pushed: wrong quantity, and 0 % with OWM. BLE sensor humidity (sat/ble_humidity) is not used as indoor humidity. Comfort adjust only runs when summer simmer is also enabled (SATcontrol.ino ~2942), which looks unintended. Its 30-min staleness is hardcoded instead of using humidity_timeout_s. Suggestion: indoor sources only (MQTT/REST/BLE), no weather fallback, and show the active humidity source in the UI. Changes SAT control behaviour (comfort adjust changes the target), so the plan needs Sergeant D's OK.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Sergeant D's decision recorded (entry 114, Q32 C) before implementation
- [x] #2 Comfort adjust is removed: satUpdateComfort and every call site, the bComfortAdjust setting and its REST, MQTT (incl. HA discovery), web UI and debug exposure; a stored SATcomfortadjust value in settings is ignored without error (host/source test red on HEAD, green on fix)
- [x] #3 Thermal comfort (Summer Simmer Index in the PID, climate.py:276-277) is unchanged (existing tests green)
- [x] #4 Docs updated: firmware-sat-spec, sat-comparison, MQTT/REST API docs, c4-code-sat; build and evaluate --quick green
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Code confirmed (SATcontrol.ino): ~4731 copies weather humidity into state.sat.fHumidity when no direct value is valid (outdoor quantity; 0 % with the removed OWM); BLE humidity (state.sat.fBleHumidity, SATble.ino ~892) never feeds fHumidity; satUpdateComfort ~3147 returns unless bSummerSimmer is also on; the 30 min staleness is hardcoded ~3152 while settings.sat.iHumidityTimeoutS exists (used for thermal comfort ~1265). thermo-nova uses only an indoor humidity sensor, no weather fallback (climate.py current_humidity); comfort adjust is our own feature (Task #28/#47).
Plan: 1) indoor sources only: MQTT/REST push (existing), and BLE humidity from the BLE sensor that provides the room temperature (or any fresh BLE humidity when BLE is the indoor source); drop the weather fallback. Priority: pushed MQTT/REST value if fresh, else BLE. 2) comfort adjust no longer requires summer simmer. 3) staleness of humidity for comfort uses iHumidityTimeoutS (same as thermal comfort). 4) status/MQTT/UI show the active humidity source (none/mqtt/ble). Verification: host test of the source selection and comfort gate (red on HEAD), bench with pushed humidity / BLE sim / weather on.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-21 plan posted to Sergeant D (#alpha-testing msg 1562558755898523739); open points: comfort adjust outside summer, pushed value vs BLE priority. Waiting for his OK; cycle continues.

2026-10-22: TASK-1292 ground-truth spec is on dev: docs/sat/thermo-nova-spec.md (cited, Alexwijn/SAT releases/thermo-nova @ 7beaaf5), docs/sat/firmware-sat-spec.md and docs/sat/sat-comparison.md (443 rows, 434 verified). This task waits for Sergeant D's validation of the thermo-nova spec; the plan must quote the thermo-nova code for its area with file:line citations (CLAUDE.md 'SAT ground truth' block) before it is resumed.

2026-10-25 gruijter re-reported all four points (#alpha-testing 1563792462328111185); verified at HEAD by a workflow with a refuting second reader (wf_16ada525): weather fallback SATcontrol.ino:5052-5059, gate on bSummerSimmer :3460, hardcoded 1800000UL :3465-3467 (also clears bHumidityValid, which turns Thermal comfort off; iHumidityTimeoutS used at :1469), BLE humidity never copied into fHumidity. Gate fix conflicts with Sergeant D entry 32 (humidity only when Summer Simmer index / Thermal comfort is on): queued as Q32. Answered gruijter 1563816427851354243.

2026-10-28 Q32 answered C (entry 114): remove the firmware-only comfort adjust (satUpdateComfort, bComfortAdjust, its REST/MQTT/UI/docs); humidity only via Thermal comfort (SSI as PID input), as thermo-nova climate.py:276-277. Scope rewritten from 'fix the gate' to removal. Touches SATcontrol.ino: after TASK-1368.

2026-10-29 scope narrowed to entry 114 (Q32 C): remove comfort adjust. The weather-humidity fallback (SATcontrol.ino ~5486) and BLE humidity as indoor source affect Thermal comfort's SSI input and are NOT decided by entry 114: moved to a follow-up task with a question for Sergeant D.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Comfort adjust removed (Sergeant D entry 114, Q32 C): satUpdateComfort and its call, settings bComfortAdjust / fComfortHumidity / fComfortMaxOffset, state fComfortOffset, REST fields and settings schema, MQTT command rows and state topics, the HA switch (cleaned up in HA through kRetiredSatEntities: empty retained config + state, TASK-1332 pattern), web UI fields, debug lines, help text, goldens, docs (API, manuals en/nl, c4, firmware-sat-spec, sat-comparison, CHANGELOG). satEffectiveTarget() is fTargetTemp + PV boost. An old SATcomfortadjust key in settings.ini is ignored (updateSetting has no catch-all). Evidence: tests/test_sat_comfort_adjust_removed.py 9 OK on fix, FAILED (failures=6) on HEAD acab024d7 (source-level checks: names, dispatch, call, HA switch/retire, settings key); Thermal comfort untouched (test_sat_pid_cadence 26 OK compiles the real satEffectiveTarget; SSI path unchanged); mqtt_state 26, pwm_override 73, room_loss 12, manual_sat_commands 6, static_integrity 33, test_evaluate 80 OK; node --check v2.js OK; evaluate --quick 0 FAIL; build.bat esp32-combo SUCCESS 06:53. Review (opus): no defect; external_temp HA entity identical after the switch reorder (ids from strings, not the case number); dhw_enable gate still index 10. Not done: sat/comfort_humidity and sat/comfort_max_offset retained topics stay on brokers (no HA entity; clutter only). Not verified: HA actually dropping the switch on a live broker; test/host/test_ha_discovery_json.py cannot run (pre-existing compile failure, TASK-1443). Humidity validity follow-up moved to TASK-1441.
<!-- SECTION:FINAL_SUMMARY:END -->
