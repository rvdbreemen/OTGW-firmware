---
id: TASK-792
title: >-
  feat-2.0.0: port TASK-791 — clearer boiler-unsupported panel (msgid, friendly
  name, OT name)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-31 20:26'
updated_date: '2026-05-31 21:18'
labels:
  - ui
  - frontend
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the dev-branch Statistics panel improvement (dev TASK-791) to the 2.0.0 worktree. The yellow 'Boiler does not implement these OpenTherm messages' panel should render a readable table (Dec | Message friendly name | OT name | Mode) instead of mashed-together entries. Requires /api/v2/otgw/boiler-support to also emit the OTmap friendlyname (label is the OT name). Mirror dev: backend adds friendly to the JSON (bump the per-entry buffer to avoid truncation), index.html switches the <ul> to a <table>, index.js builds <tr>/<td> rows, and index.css + index_dark.css style the table. Verify the 2.0.0 endpoint/struct shapes match (OTlookup_t.friendlyname exists in 2.0.0) and adapt to any 2.0.0-specific differences. Do NOT start until the in-flight 2.0.0 MQTT on-change port is landed.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 GET /api/v2/otgw/boiler-support emits a friendly name per entry alongside id and label, no buffer truncation
- [ ] #2 Statistics panel renders a table: Dec | Message (friendly) | OT name (label) | Mode
- [ ] #3 Panel hidden when no unsupported messages; aria-label remains descriptive
- [ ] #4 Works in light and dark themes
- [ ] #5 python build.py exits 0 (per-env SUCCESS verified) and python evaluate.py --quick clean
<!-- AC:END -->
