---
id: TASK-969
title: 'v2 parity: Statistics boiler-unsupported + active-overrides panels'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-30 23:09'
updated_date: '2026-07-01 04:57'
labels: []
dependencies: []
ordinal: 181000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Verified gap (grep 0/1 hits): Classic Statistics has a boiler-unsupported-messages panel + an active gateway-overrides panel. v2 Statistics lacks both.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 v2 Statistics shows boiler-unsupported messages
- [x] #2 v2 Statistics shows active gateway overrides
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
v2 parity: added the two Classic Statistics diagnostic panels to the v2 Statistics tab. 'Active gateway overrides' (GET /api/v2/otgw/overrides, ADR-118 — the gateway-override store, separate from OT-Direct overrides): rows show MsgID + friendly name + kind(answer/substituted) + value + age. 'Boiler unsupported messages' (GET /api/v2/otgw/boiler-support): unsupported_read/write rows with MsgID + name + mode. Reuses ble-row/ble-nm/set-group (no drift). Verified on .39: both panels render with correct empty-states ('No active gateway overrides' / 'None — ... (or no boiler seen)'), 0 console errors. Populated rows exercised via the same render path.
<!-- SECTION:FINAL_SUMMARY:END -->
