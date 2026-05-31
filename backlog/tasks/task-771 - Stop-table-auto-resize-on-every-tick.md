---
id: TASK-771
title: Stop table auto-resize on every tick
status: Done
assignee:
  - '@codex'
created_date: '2026-05-31 07:44'
updated_date: '2026-05-31 07:55'
labels:
  - bug web-ui
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Fix the table layout regression where the table width grows on every UI tick even when rows or content have not changed.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Table width recalculation runs only when relevant table structure/content changes, not on every tick.
- [x] #2 Existing table updates still render correctly when rows are added or changed.
- [x] #3 Validation covers the resize regression or documents why it cannot be directly automated.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Locate the Web UI table sizing code and the tick/update path that calls it.
2. Change recalculation so it runs only after relevant table row/content changes.
3. Validate with the smallest available UI/static test or targeted code inspection, and document any non-automated verification gap.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Branch: dev. Coding agent: Codex.
Root cause: the table auto-fit path was called from recurring table refreshes, and measureTableCellWidth used current scrollWidth as a minimum after pixel widths were already applied. That made each repeat measurement feed the previous width back into the next width.
Change: index.js now uses table-specific fit signatures so stats/support tables auto-fit only when stable row/support structure changes, and content measurement no longer uses scrollWidth as a feedback source.
Validation: node --check src/OTGW-firmware/data/index.js; Select-String confirmed no direct fitTableColumnsToContent('otStatsTable'/'otSupportTable') calls and no scrollWidth reference remain; python evaluate.py --quick --no-color passed 36 checks with 0 failures.

Follow-up after review comment: renamed the auto-fit cell measurement helper to measureTableCellContentWidth and added an explicit guard comment. Static search shows no scrollWidth/offsetWidth/old helper use remains in the auto-fit path; remaining clientWidth/getBoundingClientRect reads are outside content measurement and belong to drag/viewport sizing.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed the OpenTherm table auto-width growth regression by making auto-fit content-driven instead of layout-width-driven.

Changes:
- Added stable table fit signatures so recurring stats/support refreshes do not remeasure unchanged table structure on every tick.
- Changed auto-fit measurement to use canvas text measurement plus padding/border only, and explicitly avoid reading cell layout widths that can feed prior column widths back into the next pass.
- Kept manual column drag behavior intact; layout reads remain only for viewport/drag sizing, not content auto-fit.

Validation:
- node --check src/OTGW-firmware/data/index.js
- Static search confirmed no scrollWidth/offsetWidth/old helper use remains in the auto-fit content measurement path.
- python evaluate.py --quick --no-color passed: 36 checks, 0 failures.
<!-- SECTION:FINAL_SUMMARY:END -->
