---
id: TASK-754
title: 'feat(hw): remove deprecated picavailable topic/field (ADR-113 stage 2)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-29 09:05'
updated_date: '2026-05-29 20:11'
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
