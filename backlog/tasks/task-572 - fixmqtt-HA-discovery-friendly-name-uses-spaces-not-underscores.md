---
id: TASK-572
title: 'fix(mqtt): HA discovery friendly-name uses spaces, not underscores'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-07 20:25'
updated_date: '2026-05-07 21:55'
labels:
  - mqtt
  - ha-discovery
  - ux
  - 2.0.0-port-needed
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Andre on Discord (2026-05-07, screenshot of HA OTGW device card): all OTGW entities show as 'OTGW_DHW_Control', 'OTGW_Boiler_exhaust_temperature', etc. — snake_case_with_underscores. His other MQTT integrations (X-Sense smoke detectors, etc.) show normal names with spaces ('OTGW DHW Control', 'OTGW Boiler exhaust temperature'). The friendly-name in HA's UI should be human-readable. Scope per Andre: JUST the friendly-name in the discovery configuration template. Entity_id, unique_id, stat_t topic must keep underscores (those drive integrations and topic subscriptions; renaming them is breaking). Implementation: add a writeFriendlyName helper in mqtt_configuratie.cpp that transforms underscores to spaces while writing, apply to the three name-field sites (sensor at line ~1985, binary_sensor at ~2084, Dallas at ~2306). Replace the literal underscore separator between hostname and friendlyName with a space in all three sites.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 writeFriendlyName helper added in mqtt_configuratie.cpp; transforms '_' -> ' ' char-by-char while preserving the existing writer chunking
- [x] #2 Sensor name field (line ~1985-1996) uses writeFriendlyName for the friendlyName segment, and a literal space (not underscore) between hostname and friendlyName
- [x] #3 Binary sensor name field (line ~2084-2091) gets the same transformation
- [x] #4 Dallas name field (line ~2306-2313) uses ' Temperature ' literal (spaces) instead of '_Temperature_'
- [x] #5 Entity_id, unique_id, stat_t topic, and discovery topic path (homeassistant/.../config) still use underscore form — only the human-facing 'name' field is transformed
- [x] #6 python build.py --firmware exits 0 on dev
- [x] #7 python evaluate.py --quick — no new failures
- [x] #8 Prerelease bump beta.26 -> beta.27 committed alongside
- [x] #9 Field validation: tester confirms entities now show 'OTGW DHW Control' / 'OTGW Boiler exhaust temperature' in the HA device card; topic subscriptions and existing automations still work (deferred per CLAUDE.md self-verification policy)
- [x] #10 Port to 2.0.0 line as alpha.18 (cross-tree task)
<!-- AC:END -->
