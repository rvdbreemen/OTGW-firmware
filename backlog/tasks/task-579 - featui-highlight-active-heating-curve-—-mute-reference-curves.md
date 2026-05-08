---
id: TASK-579
title: 'feat(ui): highlight active heating curve — mute reference curves'
status: To Do
assignee: []
created_date: '2026-05-08 15:37'
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
- [ ] #1 All reference (non-active) curves are rendered in a visually subdued style: low opacity or muted/grey color palette, thinner stroke weight
- [ ] #2 The active/selected curve is rendered in a prominent, high-contrast color with a noticeably thicker stroke
- [ ] #3 A visible marker (dot or similar) is placed on the active curve at the current outside temperature position
- [ ] #4 The visual distinction is clear in both light and dark theme
- [ ] #5 No regression: all curves still render correctly when a different coefficient/target is selected (active curve updates accordingly)
<!-- AC:END -->
