---
id: TASK-664
title: >-
  Fix openHAB/Domoticz guide anchor referencing non-existent 'migration notes'
  heading in docs/api/MQTT.md
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-22 06:23'
updated_date: '2026-05-22 06:25'
labels:
  - docs
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
docs/guides/OPENHAB.md:70 and docs/guides/DOMOTICZ.md:72 both instruct readers to 'clear zombie/orphan retained configs as documented in docs/api/MQTT.md (migration notes)'. docs/api/MQTT.md does discuss topic-shape migration but has no heading literally called 'migration notes'.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 docs/api/MQTT.md is checked for the actual heading name covering retained-config cleanup (likely 'Topic-shape migration' or similar)
- [ ] #2 OPENHAB.md and DOMOTICZ.md links updated to point directly at #the-actual-anchor (e.g. docs/api/MQTT.md#topic-shape-migration)
- [ ] #3 Anchor verified to resolve when the rendered docs are browsed locally (markdown anchor pattern)
<!-- AC:END -->
