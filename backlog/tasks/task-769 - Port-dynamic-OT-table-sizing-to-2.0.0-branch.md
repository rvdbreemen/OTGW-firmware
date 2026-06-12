---
id: TASK-769
title: Port dynamic OT table sizing to 2.0.0 branch
status: Done
assignee:
  - '@codex'
created_date: '2026-05-30 16:48'
updated_date: '2026-05-31 06:39'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Apply the OT support and Statistics table dynamic content-fit sizing behavior to the feature-dev-2.0.0-otgw32-esp32-sat-support worktree. Column widths should follow the widest visible text, and wide tables should stay contained by the horizontal scroll container instead of viewport scaling that clips cells.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Statistics and OT Support tables in the 2.0.0 worktree size each column from the widest visible header or cell text.
- [x] #2 Manual drag resizing remains usable and cannot grow the table beyond the available width.
- [x] #3 The 2.0.0 worktree validates with syntax/evaluator checks relevant to the Web UI change.
- [x] #4 The computed table width is the sum of measured column widths, with wider content contained by the OT table scroll container instead of clipped by viewport scaling.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Inspect the 2.0.0 worktree status and current OT table sizing implementation.
2. Compare the TASK-768 logic against the 2.0.0 files and port only the needed deltas.
3. Preserve existing 2.0.0 worktree changes and keep the port scoped to OT support/Statistics table sizing.
4. Correct content-fit behavior so measured column widths are not scaled down; rely on the scroll container for narrow viewports.
5. Run syntax/evaluator checks and rendered browser layout checks.
6. Update acceptance criteria, notes, final summary, and task status.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Branch/worktree: feature-dev-2.0.0-otgw32-esp32-sat-support at D:\Users\Robert\Documents\GitHub\RvdB\OTGW-firmware-2.0.0. Existing unrelated dirty files were left untouched.
Agent: Codex.
Implementation: ported TASK-768 dynamic sizing to the 2.0.0 Web UI asset layout. Replaced the stats-only resize helper with shared Statistics/OT Support table sizing, added a colgroup to otSupportTable, capped computed and dragged widths to the available parent/viewport width, and updated components.css so fixed table layout/ellipsis respects the computed widths.
Validation: node --check src\OTGW-firmware\data\index.js; python evaluate.py --quick --no-color (PASS with one existing ESP abstraction warning); Playwright rendered layout check at 800px viewport for normal rows, over-wide rows, and synthetic drag resize.

Follow-up 2026-05-30: same issue as TASK-768 in 1.6.1. User screenshot showed OT Support Boiler cells truncating 'no read support...'. The earlier viewport cap squeezes content-fit columns too far; the corrected port should let the table width follow measured content and rely on the scroll container to keep the page usable.

Follow-up implementation: mirrored the corrected TASK-768 behavior in the 2.0.0 worktree. Removed automatic viewport scaling from content-fit widths, changed components.css table max-width to none, added browser scrollWidth as a measurement floor for warning/long-label text, and bumped storage keys to otStatsColWidths.v4 / otSupportColWidths.v3 to avoid stale narrow widths. Validation: node --check src\OTGW-firmware\data\index.js; python evaluate.py --quick --no-color (no failures, one existing ESP abstraction warning); Playwright rendered checks at 816px and 360px confirmed zero clipped OT Support cells including '⚠ no read support' and container scrolling without page overflow; Playwright drag check confirmed manual resize grows the column while table width remains within available width.

Milestone: 2.0.0. Cross-worktree sibling created on feature-dev-2.0.0-otgw32-esp32-sat-support per user decision.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ported the corrected OT table sizing behavior to the 2.0.0 feature worktree. Columns now keep measured content-fit widths instead of being scaled down to the viewport, components.css no longer caps the explicit table width, and measurement uses browser scrollWidth as a floor so values like '⚠ no read support' and long message names do not ellipsize. Wider content stays contained by the OT table scroll container. Validation passed: node --check, evaluate.py --quick --no-color with one existing ESP abstraction warning, rendered no-clipping checks at 816px and 360px, and a manual drag-resize check.
<!-- SECTION:FINAL_SUMMARY:END -->
