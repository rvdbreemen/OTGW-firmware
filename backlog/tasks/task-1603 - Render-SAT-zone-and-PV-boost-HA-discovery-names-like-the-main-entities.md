---
id: TASK-1603
title: Render SAT zone and PV-boost HA discovery names like the main entities
status: Done
assignee: []
created_date: '2026-11-04T12:43:00Z'
updated_date: '2026-11-04T12:48:00Z'
labels: []
dependencies: []
ordinal: 744000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SAT zone and PV-boost Home Assistant discovery payloads are built in MQTTstuff.ino with a hostname prefix and underscores (e.g. "OTGW2_SAT_PV_PV_Boost_Delta"). The 1.x line (TASK-572/573) and the 2.0.0 main discovery path (MQTTHaDiscovery.cpp writeFriendlyName) already drop the hostname and render spaced Title Case names. These 2.0.0-only paths were never ported.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 SAT zone entities are named "SAT Zone <n> <Metric>" with no hostname prefix
- [x] #2 SAT PV-boost entities are named "SAT PV <Metric>" with no hostname prefix and no duplicated "PV"
- [x] #3 unique_id, state/command topics and discovery topics are unchanged
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SAT zone and PV-boost HA discovery entities now use the same naming as the main discovery path (TASK-572/574): no hostname prefix, spaces instead of underscores, and no duplicated "PV".

Examples:
- OTGW2_SAT_PV_PV_Boost_Delta -> SAT PV Boost Delta
- OTGW2_SAT_Zone_1_Output -> SAT Zone 1 Output

unique_id, state/command topics and discovery topics are unchanged, so existing HA entities keep their entity_id and history.

Verified: esp32-combo firmware builds (beta.71).
<!-- SECTION:FINAL_SUMMARY:END -->
