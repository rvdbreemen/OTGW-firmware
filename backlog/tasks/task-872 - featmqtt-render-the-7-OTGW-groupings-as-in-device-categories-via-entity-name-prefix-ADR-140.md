---
id: TASK-872
title: >-
  feat(mqtt): render the 7 OTGW groupings as in-device categories via
  entity-name prefix (ADR-140)
status: To Do
assignee: []
created_date: '2026-06-15 14:26'
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
