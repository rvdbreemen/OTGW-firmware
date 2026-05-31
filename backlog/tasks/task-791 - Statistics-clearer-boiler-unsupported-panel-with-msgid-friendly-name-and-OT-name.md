---
id: TASK-791
title: >-
  Statistics: clearer boiler-unsupported panel with msgid, friendly name and OT
  name
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-31 20:22'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The yellow 'Boiler does not implement these OpenTherm messages' panel at the bottom of the Statistics tab renders entries mashed together (e.g. '24Trwrite') because the id/label/mode spans have no separation and the human-readable name is missing. Render it as a clear table: decimal msgid, human-readable message name (OTmap friendlyname), OT message name (OTmap label), and read/write mode. Requires the /api/v2/otgw/boiler-support endpoint to also emit the friendly name.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 GET /api/v2/otgw/boiler-support emits a friendly name per entry alongside id and label, without buffer truncation
- [ ] #2 Statistics panel renders a readable table: Dec | Message (friendly) | OT name (label) | Mode, one row per unsupported message
- [ ] #3 Panel stays hidden when there are no unsupported messages; aria-label text remains descriptive
- [ ] #4 Works in light and dark themes
- [ ] #5 python build.py exits 0 and python evaluate.py --quick shows no new failures
<!-- AC:END -->
