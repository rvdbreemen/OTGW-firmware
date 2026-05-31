---
id: TASK-791
title: >-
  Statistics: clearer boiler-unsupported panel with msgid, friendly name and OT
  name
status: Done
assignee:
  - '@claude'
created_date: '2026-05-31 20:22'
updated_date: '2026-05-31 20:27'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The yellow 'Boiler does not implement these OpenTherm messages' panel at the bottom of the Statistics tab renders entries mashed together (e.g. '24Trwrite') because the id/label/mode spans have no separation and the human-readable name is missing. Render it as a clear table: decimal msgid, human-readable message name (OTmap friendlyname), OT message name (OTmap label), and read/write mode. Requires the /api/v2/otgw/boiler-support endpoint to also emit the friendly name.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 GET /api/v2/otgw/boiler-support emits a friendly name per entry alongside id and label, without buffer truncation
- [x] #2 Statistics panel renders a readable table: Dec | Message (friendly) | OT name (label) | Mode, one row per unsupported message
- [x] #3 Panel stays hidden when there are no unsupported messages; aria-label text remains descriptive
- [x] #4 Works in light and dark themes
- [x] #5 python build.py exits 0 and python evaluate.py --quick shows no new failures
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Boiler-unsupported Statistics panel now renders a clear table instead of mashed entries.

Backend (restAPI.ino): GET /api/v2/otgw/boiler-support now emits friendly (OTmap friendlyname) alongside id and label for both unsupported_read and unsupported_write; per-entry buffer bumped 64->160 to avoid truncation of longer friendly names.

Frontend: index.html switches the <ul> to a <table> (Dec | Message | OT name | Mode); index.js builds <tr>/<td> rows including the friendly name and still composes a descriptive aria-label; index.css + index_dark.css restyle from the old grid-list to a bordered table (mono id/label, nowrap mode, light + dark).

Hidden-when-empty behaviour and the catch/older-firmware fallback are unchanged.

Validation: python build.py exit 0 (firmware + filesystem 1.6.1-beta), python evaluate.py --quick 0 failures. Committed b380a64d, pushed origin/dev. Rendering is deterministic from the API JSON; on-device is a quick visual confirmation. 2.0.0 port queued as TASK-792 (To Do).
<!-- SECTION:FINAL_SUMMARY:END -->
