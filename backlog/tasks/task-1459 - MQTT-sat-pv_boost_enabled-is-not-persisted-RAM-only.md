---
id: TASK-1459
title: MQTT sat/pv_boost_enabled is not persisted (RAM only)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-29T07:19:00Z'
updated_date: '2026-10-29T13:02:00Z'
labels:
  - sat
  - mqtt
  - bug
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 619000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the sat/* MQTT.md audit (docs session, 2026-10-29, dev 549e93312); code read only, no device. satHandlePvBoostEnabled (SATcontrol.ino:1949-1961) sets RAM only, while the other SAT switches go through updateSetting (MQTTstuff.ino:669+); the settings key exists (settingStuff.ino:1117).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 An MQTT toggle of sat/pv_boost_enabled survives a reboot (test red on HEAD, green on fix)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
The kSatMqttCmds row for pv_boost_enabled pointed at a typed handler (_satPvBoostEnabledCmd -> satHandlePvBoostEnabled, SATcontrol.ino:1949-1961) that set settings.sat.bPvBoostEnabled in RAM only. The other SAT switches use a settingKey row, which dispatchSatMqttCmd() forwards to updateSetting() (the debounced save path); updateSetting('SATpvboostenabled') exists (settingStuff.ino:1117-1123) and already clears a running boost on disable. Change the row to { "pv_boost_enabled", "SATpvboostenabled", nullptr } and remove the now unused adapter and satHandlePvBoostEnabled. Test: tests/test_sat_pv_boost_persist.py compiles the real typedef, struct, kSatMqttCmds[] and dispatchSatMqttCmd() with every named handler stubbed and updateSetting recorded. Docs: MANUAL.md (passthrough table, gated by test_sat_mqtt_state), c4-code-mqtt.md.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-29 evidence. Red on 3ea094e9d (OTGW_MQTT_INO = git show HEAD:MQTTstuff.ino): python tests/test_sat_pv_boost_persist.py -> Ran 4, FAILED (failures=2): pv_boost_enabled 1/0 reached the RAM-only handler, updateSetting not called (key/value empty). Green on the fix (rebased on 8176c49a7): Ran 4, OK. Behaviour difference, accepted as 'like the other switches' (C5 brief): the old handler ignored an empty payload; updateSetting turns an empty payload into 0 (disable), as for every other settingKey row; HA never sends an empty switch payload. test_sat_mqtt_state (docs gate) caught MANUAL.md: row moved to the saved-setting table, 26 OK. c4-code-mqtt.md also listed enabled/control_mode/preset as passthrough although they are handlers; corrected. Found, not changed (reported to C5): preset and window also set settings.sat.fTargetTemp in RAM only; likely deliberate (temporary target).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
MQTT sat/pv_boost_enabled is now persisted like every other SAT switch: the kSatMqttCmds row is { pv_boost_enabled, SATpvboostenabled, nullptr }, so dispatch goes through updateSetting() (which saves); the RAM-only _satPvBoostEnabledCmd and satHandlePvBoostEnabled() are gone. Behaviour note (accepted by the coordinator): an empty payload now disables the boost, as for every settingKey row. MANUAL.md moves the row to the passthrough table; c4-code-mqtt.md and CHANGELOG updated.

Shipped: 689cb05b1, 2.0.0-alpha.532 (pushed to origin/dev), zip OTGW-firmware-esp32-combo-2.0.0-alpha.532+689cb05-flash.zip posted to #alpha-testing (msg 1565349517736546385).

Evidence AC#1 (this session, after the replay onto 103255a2f): tests/test_sat_pv_boost_persist.py compiles the real kSatMqttCmds table and dispatchSatMqttCmd with a recorded updateSetting. Against 103255a2f MQTTstuff.ino (OTGW_MQTT_INO): FAILED (failures=2: test_pv_boost_enabled_goes_through_update_setting, test_disable_goes_through_update_setting_too); against the fix: Ran 4 tests, OK. Persistence across reboot follows from updateSetting() writing settings, which the test proves is now called. All 31 tests/test_sat_*.py + test_ha_discovery_json exit 0; evaluate.py --quick 0 FAIL. Build as in TASK-1460 (same zip).

Not verified: an actual reboot on a device after an MQTT toggle.
<!-- SECTION:FINAL_SUMMARY:END -->
