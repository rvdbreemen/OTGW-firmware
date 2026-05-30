---
id: TASK-768
title: Fit OT support and statistics table widths to content
status: Done
assignee:
  - '@codex'
created_date: '2026-05-30 13:41'
updated_date: '2026-05-30 20:55'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Make the OT support and Statistics tab tables choose column widths from their widest visible text. The table width should be the sum of measured column widths, with the scroll container keeping the page within the viewport when content is wider than the available area.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Each OT support and Statistics table column sizes from the widest visible header or cell text for that column.
- [x] #2 The table total width is derived from the sum of computed column widths instead of a fixed or uniform width.
- [x] #3 Existing manual column dragging remains usable after the automatic sizing changes.
- [x] #4 When computed content-fit width is wider than the viewport/container, the OT table scroll container keeps the page contained and the cells keep their measured width instead of clipping text.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Locate the OT support and Statistics table rendering and resize logic.\n2. Identify the current automatic column width calculation and how drag-resized widths are stored.\n3. Change automatic sizing so each column measures the widest visible header/cell text and sums those widths for the table.\n4. Keep wide tables contained with the existing horizontal scroll container instead of shrinking columns until text clips.\n5. Preserve manual drag resizing behavior and run syntax, evaluator, and rendered layout checks.\n6. Update Backlog acceptance criteria, notes, and final summary.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Branch: dev (working tree already dirty before this task; unrelated existing changes were left untouched).
Agent: Codex.
Implementation: replaced the OpenTherm table min-width update with column-width application that computes each column from the widest rendered header/body text, sets the table width to the computed column sum, and proportionally constrains columns to the parent/viewport width. Preserved drag resizing and capped drag growth to the available table width. Bumped table width storage keys to avoid stale prior sizing data.
Validation: node --check src\OTGW-firmware\data\index.js; .\.venv\Scripts\python.exe evaluate.py --quick --no-color; Playwright rendered layout check at 800px viewport with normal rows, over-wide rows, and synthetic drag resize.

Follow-up 2026-05-30: user screenshot showed OT Support Boiler cells such as 'no read support...' still visually truncating, so AC #1/#3 need rework. The fix must preserve content-width columns and use horizontal scrolling when the content-fit sum is wider than the available viewport, instead of scaling columns down until text clips. Also apply the correction in the 2.0.0 worktree.

Follow-up implementation: removed automatic viewport scaling from content-fit widths, changed OT table max-width to none so explicit measured widths are honored inside the horizontal scroll container, added browser scrollWidth as a measurement floor for warning/long-label text, and bumped storage keys to otStatsColWidths.v4 / otSupportColWidths.v3 to avoid stale narrow widths. Validation: node --check src\OTGW-firmware\data\index.js; .\.venv\Scripts\python.exe evaluate.py --quick --no-color; Playwright rendered checks at 816px and 360px confirmed zero clipped OT Support cells including '⚠ no read support' and container scrolling without page overflow; Playwright drag check confirmed manual resize grows the column while table width remains within available width.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implemented the follow-up OT table sizing correction for the 1.6.1 dev worktree. Columns now keep their measured content-fit widths instead of being scaled down to the viewport, table max-width no longer defeats explicit column widths, and measurement uses browser scrollWidth as a floor so values like '⚠ no read support' and long message names do not ellipsize. Wider content stays contained by the existing horizontal scroll container. Validation passed: node --check, evaluate.py --quick --no-color, rendered no-clipping checks at 816px and 360px, and a manual drag-resize check.
<!-- SECTION:FINAL_SUMMARY:END -->
