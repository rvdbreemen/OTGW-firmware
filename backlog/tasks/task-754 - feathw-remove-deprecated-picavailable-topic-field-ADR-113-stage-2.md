---
id: TASK-754
title: 'feat(hw): remove deprecated picavailable topic/field (ADR-113 stage 2)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-29 09:05'
updated_date: '2026-05-29 20:36'
labels: []
milestone: 2.1.0
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
ADR-113 deprecated otgw-pic/picavailable (MQTT) and the picavailable REST field for one release in favour of otgw-firmware/hardware_type. This task removes them in the release AFTER alpha.89: drop the sendMQTTDataPic(picavailable) publish (MQTTstuff.ino), the picavailable REST entries (restAPI.ino ~2320/2512), the ha_lbl_pic_available HA discovery row + its mqttHaSensorIndex slot, and the index.js fallback that keys on picavailable when hardware_type is absent. Update docs/api/MQTT.md. Verify no HA dashboard regression for testers who migrated to hardware_type.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 picavailable removed from MQTT publish, REST device-info, and HA discovery
- [ ] #2 index.js no longer references picavailable; selects solely on hardware_type
- [ ] #3 docs/api/MQTT.md updated; mqttHaSensorIndex consistency PASS; build green; prerelease bump
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Stage 2 split into two commits due to concurrent index.js edits (settings-page work holding that file).

COMMIT 1 (done, d021e0f, alpha.99) — MQTT + HA discovery + docs (non-coupled, no index.js):
- Removed sendMQTTDataPic(picavailable) publish (MQTTstuff.ino).
- Removed ha_lbl/ha_name_pic_available PROGMEM + pseudo-ID 249 discovery row (MQTTHaDiscovery.cpp); decremented mqttHaSensorIndex[] for ids 250-254 (-1 each); MQTT_HA_SENSOR_COUNT stays 385 (now == true row count). evaluate.py HA Sensor Index Consistency: PASS.
- Dropped picavailable from OTGW-firmware.h topic comment + docs/api/MQTT.md.
- Incidental fix: id-254 SAT flame_status sensor was never discovered (386 rows vs COUNT 385 => sIdx<385 excluded it). Now rows==COUNT==385, id 254 discovered correctly (live publisher SATcontrol.ino:2186).

COMMIT 2 (DEFERRED — blocked on index.js availability):
- restAPI.ino: remove picavailable from /device/info (2322) and /health (2513).
- index.js: drop picAvailable global (145); applyPICAvailability() drop 'available' param + fallback, select solely on hardwareType==='otgw-classic'; fix routing at ~3303 (if(picAvailable)=>board-class gate, else PIC-flash page unreachable); remove dead picInfo.available (4530); remove 'picavailable' label-map rows (6743,6783); convert all call sites to 2-arg.
- This set is COUPLED (REST field removal + 3303 fix must ship atomically). Do once index.js is no longer concurrently edited; re-check git status on index.js before editing.
<!-- SECTION:NOTES:END -->
