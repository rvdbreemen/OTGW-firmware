---
id: TASK-872
title: >-
  feat(mqtt): render the 7 OTGW groupings as in-device categories via
  entity-name prefix (ADR-140)
status: In Review
assignee: []
created_date: '2026-06-15 14:26'
updated_date: '2026-06-16 08:36'
labels: []
dependencies:
  - TASK-871
ordinal: 88000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
ADR-140: after TASK-871 collapses discovery to one device, render the seven former device groupings (Boiler, Thermostat, Gateway, ESP, OT-Core, SAT, Sensors) as seven CATEGORIES within that one device. Repurpose the retained deviceForOTId classification to prepend a category name-prefix to each entity's friendlyName (e.g. 'Boiler Control Setpoint', 'Thermostat Room Setpoint', 'ESP Free Heap', 'SAT ...', 'Sensors ...'). HA constraint: only 3 native within-device sections exist (primary/Config/Diagnostic via entity_category), so the 7 categories are a naming-prefix grouping (visible ordering in the entity list + dashboard-filterable); optionally also emit one HA label per category. Keep entity_category as an ORTHOGONAL secondary layer: config for writable setpoints/limits/toggles, diagnostic for fault/counter/connectivity/PIC-settings readbacks. Match the 1.6.x naming idiom (_ to space, Title Case, hostname dropped from entity names). Fold in the LOW finding: route the binsensor object_id through sanitizeHaObjectId like the sensor path.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Each entity name carries its category prefix derived from deviceForOTId (7 categories: Boiler/Thermostat/Gateway/ESP/OT-Core/SAT/Sensors)
- [ ] #2 entity_category set as secondary axis (config for writable settings, diagnostic for readbacks)
- [ ] #3 binsensor object_id sanitized (sanitizeHaObjectId parity with sensor path)
- [ ] #4 Build green 3 targets; evaluate.py --quick no new failures
- [ ] #5 Field-validation: HA shows one device whose entities visibly group by the 7 category prefixes
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Classification audit (2026-06-16): ADVANCEABLE. Commit 7e3da75d shipped TASK-871+872 single-device + 5 source prefixes (esp_/pic_/otd_/sat_/sensors_) per accepted ADR-140 (the 7-category text in this task's ACs is stale vs the maintainer directive folded into ADR-140 while Proposed). Verified state of each AC: AC#2 entity_category present in the HA tables (HaEntityCat::diagnostic/none/config), satisfied. AC#1 partially divergent: the source-engine prefix is written into uniq_id only (MQTTHaDiscovery.cpp:2375), NOT into the visible friendlyName; the visible name carries only the bilateral _boiler/_thermostat token via ctx.sourceName (2386-2390, set 2843-2854). AC#5 (field validation) arbitrates whether uniq_id/entity_id grouping reads as 'visible'. AC#3 NOT met: composeBinSensorPayload writes raw label into uniq_id (MQTTHaDiscovery.cpp:2488) without sanitizeHaObjectId; the sensor path sanitizes via idLabel (2349-2353,2376). Build-verifiable slice: mirror the sensor path in composeBinSensorPayload (copy label->idLabel, sanitizeHaObjectId(idLabel), write idLabel at 2488), then build 3 targets + evaluate.py --quick for AC#4. AC#5 stays field-blocked.
<!-- SECTION:NOTES:END -->
