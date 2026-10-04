---
id: TASK-1211
title: 'v2 UI: expose SAT per tab and clarify the external indoor-temperature switch'
status: To Do
assignee: []
created_date: '2026-10-04 12:53'
labels:
  - webui
  - v2
  - sat
dependencies: []
priority: medium
ordinal: 332000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Suggestions from Sergeant D (#alpha-testing, 2026-10-03) with the maintainer's direction (2026-10-04).
1. The SAT settings live under OTGW > Settings > SAT Settings; Sergeant D suggested moving them to the SAT tab or a fourth SAT sub-tab (screenshot in the thread). Maintainer: the tabs go from simple to pro user, so SAT should be exposed on each tab at the matching depth; on the simple tab only SAT on/off.
2. Switch wording: 'Use external temp source' is unclear. Proposal: label it like 'Use MQTT/REST as indoor temp source' and add the how-to as help text: MQTT publish to <prefix>/set/<node>/sat/indoor_temp (payload e.g. 20.8) or REST POST /api/v2/sat/externaltemp.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The simple tab offers SAT on/off; deeper tabs show the SAT settings that belong to their level (layout agreed with the maintainer before building)
- [ ] #2 The external-temperature switch has a clear label and help text naming the MQTT topic and the REST endpoint
- [ ] #3 Verified in a browser on the bench (screenshots); build green for the three targets
<!-- AC:END -->
