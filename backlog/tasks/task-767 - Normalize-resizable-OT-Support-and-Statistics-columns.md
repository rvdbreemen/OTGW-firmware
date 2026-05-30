---
id: TASK-767
title: Normalize resizable OT Support and Statistics columns
status: In Progress
assignee:
  - '@codex'
created_date: '2026-05-30 11:37'
updated_date: '2026-05-30 11:54'
labels:
  - ui ot-support
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Fix the OT Support tab table so columns can be resized by dragging like the Statistics tab, and make both OT Support and Statistics tables size their default columns from the widest content so values fit cleanly.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 OT Support table columns can be resized by dragging column boundaries using the same interaction model as the Statistics table.
- [x] #2 OT Support default column widths are fit from the widest header/cell content so visible content fits cleanly without avoidable clipping.
- [x] #3 Statistics default column widths are fit from the widest header/cell content so visible content fits cleanly without avoidable clipping.
- [x] #4 Both tables remain usable on narrower viewports with horizontal scrolling rather than layout breakage.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Locate the OT Support and Statistics table code and identify the existing Statistics resize behavior.
2. Reuse or generalize that resize behavior for OT Support rather than adding a separate interaction.
3. Add content-fit default width calculation for both tables, preserving user drag overrides and horizontal scrolling.
4. Validate with focused static/build checks and, if practical, inspect the UI behavior in a browser.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Branch: dev (origin/dev ahead/behind state existed before task closure).
Coding agent: Codex (@codex).
Implementation: generalized the existing Statistics table resizer into shared OpenTherm table column helpers, added OT Support colgroup/resizer initialization, and moved common resizer structural CSS into index_common.css while keeping theme colors in light/dark CSS.
Content fit: both Statistics and OT Support now compute default col widths from measured header/cell text, set colgroup px widths, and maintain table min-width so narrow containers use horizontal scrolling. User drag widths persist per table with versioned localStorage keys.

Validation: node --check src\\OTGW-firmware\\data\\index.js passed; git diff --check passed aside from existing CRLF normalization warnings; Playwright browser smoke on http://127.0.0.1:8765/index.html confirmed 5 Statistics handles, 3 OT Support handles, no overflow for widest injected content, OT Support drag growth persisted 4 widths, and deliberately wide content produced horizontal scroll.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Normalized the Statistics and OT Support OpenTherm tables around one shared resize/content-fit path.

Changes:
- Added a colgroup to OT Support and initialized it with the same drag-to-resize behavior as Statistics.
- Replaced hard-coded table column widths with measured content-fit defaults for both tables.
- Preserved per-table drag persistence and table min-width handling so wide content scrolls horizontally instead of breaking layout.
- Shared the resizer structural CSS through index_common.css and kept light/dark handle colors theme-specific.

Validation:
- node --check src\\OTGW-firmware\\data\\index.js
- git diff --check -- src/OTGW-firmware/data/index.html src/OTGW-firmware/data/index.js src/OTGW-firmware/data/index.css src/OTGW-firmware/data/index_dark.css src/OTGW-firmware/data/index_common.css
- Playwright browser smoke against a temporary local static server, including drag persistence and horizontal-scroll checks.
<!-- SECTION:FINAL_SUMMARY:END -->
