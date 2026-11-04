---
id: TASK-1225
title: >-
  Fix: v2 heating colour token --hot is dropped by an early-closed comment in
  ds-tokens.css
status: Done
assignee:
  - '@claude'
created_date: '2026-10-14 20:21'
updated_date: '2026-10-14 22:04'
labels:
  - bug
  - webui
milestone: m-2
dependencies: []
priority: medium
ordinal: 355000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
ds-tokens.css lines 118-123: the comment text contains a literal */ twice ("zone-*/tile-*" and "*/"), so the comment closes early and the rest becomes CSS garbage that swallows the next declaration. Headless Edge on the source file: getPropertyValue('--hot') is empty while --cold/--dhw are set. v2-bundle.css inherits it (statusline heating dot, arrowHot, chip-hot use var(--hot)). Found while testing the TASK-1224 minifier: rcssmin joins the lines and the broken string then also eats --cold, --dhw and --gauge-track (60 instead of 211 declarations).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Comment reworded with no */ inside; headless Edge reports --hot=#ff5a3c and the same declaration count for source, v2-bundle.css and the minified copy
- [x] #2 A host test fails when any shipped .css file has an odd number of double quotes on a line after CSS comments are stripped
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Bench 2026-10-15 (combo .88.61): baseline unminified image getComputedStyle --hot = empty; after flashing the fixed+minified image --hot = #ff5a3c, --cold #2f8fe8, --dhw #ff9b3c, 0 console errors (.tmp/bench-1224/baseline/ui.json vs .tmp/bench-1224/after/ui-v2.json). Host: headless Edge on source 211 -> 212 declarations after the fix, minified copy identical. Guard test TestCssCommentsCloseCleanly in tests/test_build_staging.py failed on the stale v2-bundle.css and passes after regeneration (old-vs-fix). The guard test lands in the TASK-1224 commit because it imports build.py staging code.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Reworded the ds-tokens.css comment that contained a literal star-slash: it closed the comment early and the leftover text swallowed --hot (and, once minified, --cold/--dhw/--gauge-track too). v2-bundle.css regenerated. Evidence: AC1 headless Edge 211->212 declarations, --hot set; on the bench --hot went from empty (baseline) to #ff5a3c. AC2 tests/test_build_staging.py::TestCssCommentsCloseCleanly fails on the old bundle, passes on the fix (committed with TASK-1224).
<!-- SECTION:FINAL_SUMMARY:END -->
