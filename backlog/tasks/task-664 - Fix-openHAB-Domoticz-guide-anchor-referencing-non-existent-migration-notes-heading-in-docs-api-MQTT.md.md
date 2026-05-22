---
id: TASK-664
title: >-
  Fix openHAB/Domoticz guide anchor referencing non-existent 'migration notes'
  heading in docs/api/MQTT.md
status: Done
assignee:
  - '@claude'
created_date: '2026-05-22 06:23'
updated_date: '2026-05-22 06:37'
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
- [x] #1 docs/api/MQTT.md is checked for the actual heading name covering retained-config cleanup (likely 'Topic-shape migration' or similar)
- [x] #2 OPENHAB.md and DOMOTICZ.md links updated to point directly at #the-actual-anchor (e.g. docs/api/MQTT.md#topic-shape-migration)
- [x] #3 Anchor verified to resolve when the rendered docs are browsed locally (markdown anchor pattern)
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed dead anchor in docs/guides/OPENHAB.md:70 and DOMOTICZ.md:72. Both lines now link to docs/api/MQTT.md#migration-note-zombie-discovery-configs-from-beta21-adr-071 (the actual heading at MQTT.md:403). Verified by mapping context ('zombie/orphan retained configs', 'stale Things'/'duplicate devices') to the heading that covers retained-config cleanup; the other Migration-note heading at line 387 covers the bSeparateSources transition, not zombie discovery configs. Committed as cca51261.
<!-- SECTION:FINAL_SUMMARY:END -->
