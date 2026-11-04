---
id: TASK-579
title: 'feat(ui): highlight active heating curve — mute reference curves'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-08 15:37'
updated_date: '2026-05-08 17:21'
labels:
  - ui
  - sat
  - graph
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The SAT Expert page Heating Curve (Stooklijn) graph renders all reference curves in bold, equally-weighted colors. The active/selected curve is indistinguishable at a glance. Fix: render all reference curves in a muted, low-opacity color (e.g. grey or desaturated tones) and draw the active curve in a prominent, high-contrast color with a thicker stroke and a marker dot. Reported by SergeantD on alpha.20 (2026-05-08).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All reference (non-active) curves are rendered in a visually subdued style: low opacity or muted/grey color palette, thinner stroke weight
- [x] #2 The active/selected curve is rendered in a prominent, high-contrast color with a noticeably thicker stroke
- [x] #3 A visible marker (dot or similar) is placed on the active curve at the current outside temperature position
- [x] #4 The visual distinction is clear in both light and dark theme
- [x] #5 No regression: all curves still render correctly when a different coefficient/target is selected (active curve updates accordingly)
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implemented active heating curve highlighting in sat.js:

Changes:
- Replaced per-curve color array with REF_CURVE_COLOR_LIGHT/DARK constants (low-opacity grey) and ACTIVE_CURVE_COLOR (#ff6600 orange) + ACTIVE_CURVE_WIDTH/REF_CURVE_WIDTH constants
- buildCurveOption() now accepts theme parameter; reference curves use muted refColor, active curve uses orange with 3px stroke
- Added Curve Pos scatter series (orange dot, 10px) on active curve at current outside temperature position
- updateCurveChart() partial-update path now refreshes both Curve Pos (second-to-last) and Current (last) scatter series each poll cycle
- Fixed tooltip formatter to extract val[1] from [x,y] pairs for line series
- initCurveChart() reads actual body dark class for initial theme instead of hardcoded 'light'
<!-- SECTION:FINAL_SUMMARY:END -->
