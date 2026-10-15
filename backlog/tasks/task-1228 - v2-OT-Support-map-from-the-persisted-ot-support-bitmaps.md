---
id: TASK-1228
title: 'v2: OT Support map from the persisted ot-support bitmaps'
status: To Do
assignee: []
created_date: '2026-10-15 05:08'
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
- [ ] #1 v2 OT Support map is seeded from otgw/ot-support on load
- [ ] #2 It distinguishes thermostat read/write and boiler acked/unsupported like classic
- [ ] #3 Bench: works on the combo unit with 0 console errors (screenshot + console log)
<!-- AC:END -->
