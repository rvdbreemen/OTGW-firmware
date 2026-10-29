---
id: TASK-1441
title: >-
  SAT Thermal comfort: indoor humidity source (no weather fallback? BLE
  humidity?) as thermo-nova
status: Done
assignee:
  - '@claude'
created_date: '2026-10-29T05:11:00Z'
updated_date: '2026-10-29T07:35:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 602000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Split from TASK-1282 (2026-10-29). Thermal comfort computes the Summer Simmer Index from humidity (climate.py:276-277). The firmware falls back to the outdoor weather humidity when no indoor value is present (SATcontrol.ino ~5486: state.sat.fHumidity = state.sat.weather.fHumidity), and BLE sensor humidity never feeds fHumidity. thermo-nova takes humidity from the configured humidity sensor only. Needs a question to Sergeant D (A: indoor only as thermo-nova, B: keep weather fallback, C: other) before any change; then implement with a host test red on HEAD.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Question asked and answer recorded in docs/sat/sergeant-d-corrections.md
- [x] #2 Implemented per the answer with a host test red on HEAD, green on fix
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Entry 117 (Q57 A) + the TASK-1282 review note. 1) Remove the weather-humidity fallback (SATcontrol.ino ~5484-5488). 2) BLE room-sensor humidity (state.sat.fBleHumidity, SATble.ino ~892) becomes an indoor source: when a BLE reading arrives it updates state.sat.fHumidity / iHumidityLastMs / bHumidityValid like satHandleHumidity() (MQTT/REST) does, so the most recent valid reading wins (our reading in entry 117, announced on Discord). 3) One validity rule: humidity is valid only while (millis() - iHumidityLastMs) <= iHumidityTimeoutS*1000; used by the SSI input (~1695) AND by the published SSI / summer_simmer_index / perception (~3585), which today check only bHumidityValid. Without valid indoor humidity Thermal comfort uses the room temperature (existing fallback). Test: host test on the real code: weather humidity present + no indoor humidity -> SSI not used (red on HEAD); BLE humidity -> used; stale pushed humidity -> SSI not published (red on HEAD). Docs: firmware-sat-spec, sat-comparison, manuals humidity text.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-29 Q57 asked of Sergeant D in #alpha-testing (citations opened in session: thermo-nova climate.py:282-287, firmware SATcontrol.ino:5484-5488, SATble.ino:892). Waiting for his answer.

2026-10-29 Q57 answered A (entry 117, msg 1565232358192455792): indoor only (MQTT/REST or BLE room sensor humidity), no weather fallback; without indoor humidity Thermal comfort uses the room temperature. Our reading for push vs BLE priority: most recent valid reading wins.

2026-10-29 input from the TASK-1282 review: satUpdateComfort() was the only code that cleared state.sat.bHumidityValid (only with comfort adjust AND summer simmer on). After its removal a pushed humidity never invalidates; the SSI path (SATcontrol.ino ~1695-1696) still checks iHumidityLastMs against iHumidityTimeoutS and falls back to the room temperature, but sat/ssi, summer_simmer_index and summer_simmer_perception (~3585) check only bHumidityValid and keep publishing an SSI from stale humidity. The weather fallback sets iHumidityLastMs once and never refreshes. TASK-1441 should give humidity one validity rule (age vs iHumidityTimeoutS) used by both the SSI input and the published SSI, when removing the weather fallback.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Thermal comfort uses indoor humidity only (Sergeant D entry 117, Q57 A): the outdoor-weather fallback is removed; a fresh BLE room-sensor humidity reading (satNoteBleHumidity, only on a new decoded reading, same slot selection as the BLE room temperature) is an indoor source next to MQTT/REST pushes, most recent valid reading wins; one freshness rule satHumidityFresh() (age vs iHumidityTimeoutS) for the SSI input and the published SSI; a stale humidity is marked invalid in the control loop so sat/humidity_valid, REST and debug agree. Evidence: tests/test_sat_humidity_source.py runs the real code: on HEAD 452442094 FAILED (failures=3): weather humidity used (31.29 != 24.0), stale value still published, BLE ignored; on the fix 7 OK. SAT suites green on the combined tree (zone_aggregation 16, comfort_adjust_removed 9, pid_cadence 27, mqtt_state 26, room_loss 12, solar_gain 4, ot_loss 24); evaluate --quick 0 FAIL; build in the commit gate. Not verified: on a device with a BLE humidity sensor; satBLEUpdateState itself is not compiled in the host test (bridge tested at its function boundary). Found and filed: TASK-1458 (published SSI is computed from the SSI with Thermal comfort on, pre-existing).
<!-- SECTION:FINAL_SUMMARY:END -->
