---
id: TASK-3049
title: >-
  Web UI: remaining contrast and small-text gaps the TASK-2997 scan showed
  (legend glyphs, Home chip values, language code, dropdown caret, tiny text)
status: To Do
assignee: []
created_date: '2026-11-25T06:31:00Z'
updated_date: '2026-11-27T05:18:00Z'
labels:
  - webui
  - a11y
milestone: 2.1.0
dependencies: []
priority: low
ordinal: 1153000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Left over from TASK-2997 (impeccable detect + in-page scan, light theme, all pages forced visible; evidence OTGW-build-artifacts-safe/evidence-TASK-2997/). Real contrast gaps the detector does not count: connection-legend glyphs (white on status colours, 2.2-3.7:1: check, !, x, circle); Home chip values #ff5a3c and #2f8fe8 on the schematic card (~2.9-3.2:1, 15 px bold); language code in the menu (EN on the active tint 3.70, NL 3.84); dropdown caret and check in the view picker (2.72, 1.00 for the hidden check); disabled button text is excluded on purpose. Plus the impeccable findings tiny-text (15) and undersized-ui-text (13): functional text under 11 px. Low priority, 2.1.0 (coordinator, 2026-11-25).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Each listed pair reaches 4.5:1 for text (3:1 for large text and UI components) in light and dark, with rows added to tests/webui/contrast-tokens.test.mjs
- [ ] #2 impeccable detect on v2.html shows no low-contrast finding and the undersized-ui-text findings are fixed or waived with a reason per item
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-27 otgw-ux-designer, plan (not started; low priority, milestone 2.1.0): (1) trace each listed pair to its element with the in-page scan and add one row per pair to tests/webui/contrast-tokens.test.mjs, red first. (2) Fix by token, not by element: use the existing text tokens (var(--hot-text) exists; a --cold-text and a legend-glyph colour need adding in ds-tokens.css light and dark) for the Home chip values on the schematic card; deepen the status-colour fills behind the connection-legend glyphs; darken the active-language tint in the menu; fix the caret. All CSS; no v2.js needed unless a chip colour is set inline (then otgw-webui). (3) undersized-ui-text (13 items): list them with their selectors, raise each to 11 px or waive with a reason. Needs one browser pass at 1400 and 375 px, light and dark.
<!-- SECTION:NOTES:END -->
