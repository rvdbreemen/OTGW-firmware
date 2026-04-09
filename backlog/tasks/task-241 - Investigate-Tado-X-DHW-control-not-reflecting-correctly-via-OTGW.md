---
id: TASK-241
title: 'Investigate: Tado X DHW control not reflecting correctly via OTGW'
status: To Do
assignee: []
created_date: '2026-04-09 16:49'
updated_date: '2026-04-09 16:49'
labels:
  - bug
  - needs-info
dependencies: []
references:
  - 'Discord #beta-testing'
  - user crashevans
  - '2026-04-09'
  - 'Discord #english-support'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
User crashevans reports that with Tado X controller + Ideal Logic Combi ESP1 35 boiler + OTGW, DHW temperature set via Tado app does not align with boiler display / OTGW / HA. App defaults to 43°C, changing it in app does not propagate correctly. Also: ID 14 (MaxRelModLevelSetting) is sent as 0.00% and boiler replies Unknown-Data-Id, causing setpoint-led cycling instead of modulation. Previously worked with Tado V3 + same setup. Sergeant D explains that Ideal boilers do not support max relative modulation management via OT — boiler only accepts CS/TSet control. DHW issue needs logs to determine if firmware or config.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Determine if DHW control misalignment is a firmware bug or Tado X / config issue
- [ ] #2 If firmware bug: DHW setpoint sent by Tado X is correctly propagated through OTGW to boiler and HA
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-04-09: crashevans shared 8 log attachments in #beta-testing (not accessible via Discord MCP). Sergeant D (sergeantd) confirmed Ideal boilers do not support max relative modulation management — Tado therefore drives via TSet causing cycling. The 43°C DHW default comes from the mqttha.cfg initial value. Need raw OTGW logs and HA screenshots to isolate DHW discrepancy.
<!-- SECTION:NOTES:END -->
