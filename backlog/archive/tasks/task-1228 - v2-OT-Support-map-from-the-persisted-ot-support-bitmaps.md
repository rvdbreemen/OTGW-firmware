---
id: TASK-1228
title: 'v2: OT Support map from the persisted ot-support bitmaps'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-15 05:08'
updated_date: '2026-10-15 14:06'
labels:
  - webui
  - 2.0.0
  - v2
milestone: m-2
dependencies: []
priority: low
ordinal: 366000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up port from the classic UI (decided 2026-10-15: core ports first, these after; not blocking TASK-1223). Gap audit docs/research/2026-10-15-classic-vs-v2-gap-audit.md row 27: classic reads otgw/ot-support (thermostat read/write, boiler acked/unsupported, persisted across reboots); v2 only knows what it saw since page load.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 v2 OT Support map is seeded from otgw/ot-support on load
- [x] #2 It distinguishes thermostat read/write and boiler acked/unsupported like classic
- [x] #3 Bench: works on the combo unit with 0 console errors (screenshot + console log)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Bench 2026-10-15 OTGW32 (.88.61, alpha.425+4755422, safeboot layout): .tmp/bench-followups/after/ ui-*.json + png; 0 console errors in the final runs: OT Support seeded from otgw/ot-support right after load (msgid 0 coloured 'Thermostat only' before any traffic). The bus is offline, so the boiler acked/unsupported states were shown only in the harness (fake device): read/write and acked/unsupported distinguished, plus a new red 'unsup' cell.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
v2 OT Support map is seeded from the persisted ot-support bitmaps and distinguishes thermostat read/write and boiler acked/unsupported. Evidence: bench seeding, harness for the boiler states.
<!-- SECTION:FINAL_SUMMARY:END -->
