---
id: TASK-792
title: >-
  feat-2.0.0: port TASK-791 — clearer boiler-unsupported panel (msgid, friendly
  name, OT name)
status: Done
assignee:
  - '@claude'
created_date: '2026-05-31 20:26'
updated_date: '2026-06-01 05:23'
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
- [x] #1 GET /api/v2/otgw/boiler-support emits a friendly name per entry alongside id and label, no buffer truncation
- [x] #2 Statistics panel renders a table: Dec | Message (friendly) | OT name (label) | Mode
- [x] #3 Panel hidden when no unsupported messages; aria-label remains descriptive
- [x] #4 Works in light and dark themes
- [x] #5 python build.py exits 0 (per-env SUCCESS verified) and python evaluate.py --quick clean
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-06-01T07:23:36+02:00: Closed per maintainer directive. AC#4 dark/light visual verified by token construction (theme inherits via [data-theme=dark] overrides), on-device visual waived as gate.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ported dev TASK-791 (commits b380a64d + c2a436a1 + 67ded212, final state) to 2.0.0, adapted to the design-token system.

What changed:
- restAPI.ino /api/v2/otgw/boiler-support: emits "friendly" (OTmap friendlyname) alongside id+label for both unsupported_read and unsupported_write. Per-entry buffer 64 -> 160 to avoid truncation. Kept 2.0.0's single-PSTR-format + leading-comma loop structure (TASK-696 flash optimisation), not dev's two-format-string shape.
- index.html: replaced <ul id="boilerUnsupportedList"> with a <table class="boiler-unsupported-table"> + thead (MsgID / Description / OpenTherm Name / Direction) + tbody id="boilerUnsupportedList".
- index.js refreshBoilerSupport/addUnsupportedItem: builds <tr> rows with td.boiler-unsupported-id, td.boiler-unsupported-friendly (reads e.friendly), td.boiler-unsupported-label, and Direction as <td><span class="boiler-unsupported-pill">. aria-label text now includes the friendly name. Hidden-when-empty behaviour preserved.
- components.css: redesigned the panel as a diagnostic notice card (left accent border + warning glyph ::before, uppercase muted column legend with hairline rule, mono MsgID/OpenTherm-Name vs proportional Description, row separators + hover, rounded Direction pill). Removed obsolete .boiler-unsupported-list / -mode rules.

2.0.0 deviation from dev: dev hard-codes hex per separate index.css/index_dark.css files. 2.0.0 has a single token-driven components.css with [data-theme="dark"] re-declaring tokens. The redesign is written ONCE using --status-warn*, --status-warn-text, --font-mono, --radius-pill/lg, --space-* and color-mix(in srgb, var(--status-warn-text) N%, transparent); dark theme follows automatically. The left accent uses var(--status-warn) (dev used a slightly darker literal); fully token-driven and theme-correct. 10px legend/pill font kept as a literal (no token below --fs-caption 11px).

Build: esp8266 fw+fs SUCCESS, esp32 fw+fs SUCCESS. evaluate.py --quick clean — Design-System Class Drift PASS (new classes boiler-unsupported-table/-friendly/-pill all have selectors).

AC #4 (light AND dark) verified by token construction, not visually — dark inherits the redesign through [data-theme="dark"] token overrides. Visual confirmation left for the maintainer on-device gate.
<!-- SECTION:FINAL_SUMMARY:END -->
