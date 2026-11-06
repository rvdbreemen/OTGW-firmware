---
id: TASK-1671
title: Public beta v2.0.0-beta.105 (beta.104 code + release commit)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-06T07:48:00Z'
updated_date: '2026-11-06T08:33:00Z'
labels:
  - release
  - beta
milestone: 2.0.0
dependencies: []
ordinal: 794000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-06 08:40: public beta now; 1 h soak of candidate beta.104 on .61 (OTGW32) instead of .63, because .63 runs the beta.95 24 h soak until ~15:00. Delta since v2.0.0-beta.99: TASK-1451, TASK-1652, TASK-1663, TASK-1664, TASK-1670.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Release notes RELEASE_NOTES_2.0.0-beta.105.md, CHANGELOG rollover, README link
- [x] #2 Candidate soak: beta.104 1 h on .61 with MQTT, bootcount unchanged, no crashlog
- [ ] #3 Release commit built (esp32-combo), OTA on .61, health checked
- [ ] #4 Tag pushed, draft prerelease with assets and SHA256SUMS, published with --latest=false, latest stays 1.x
- [ ] #5 #alpha-testing zip post and #beta-testing announcement
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-06 08:55: notes docs/releases/RELEASE_NOTES_2.0.0-beta.105.md (from beta.99; new 'What changed since beta.99' with TASK-1451/1652/1663/1664/1670, Known issue TASK-1669 heap drift, soak wording: 1 h on the OT-Direct unit .61, not on a Classic); checks: 0 TODO/TBD/XXX/em dash, 0 relative links. CHANGELOG [Unreleased] -> [2.0.0-beta.105] with link refs; README links -> beta.105.

2026-11-06 09:31 candidate soak beta.104+deff449 on .61 (OT-Direct unit, Robert's choice): uptime 01:00 since the 08:30 OTA, bootcount 5 unchanged, mqttconnected true, crashlog none, freeheap 72416 (09:04) -> 71832 (09:31) (weather-fetch leak, TASK-1672). 09:32 release build 2.0.0-beta.105+704cc1e (Build completed successfully, artefacts 09:03-09:04) OTA app+fs exit 0, settings 177 keys changed none, v2.js 266415 B complete.
<!-- SECTION:NOTES:END -->
