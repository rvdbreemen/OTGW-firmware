---
id: TASK-568
title: 'fix(webui): Temperature History chart legend text unreadable on dark theme'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-07 19:35'
updated_date: '2026-05-07 21:56'
labels:
  - webui
  - charts
  - 2.0.0
dependencies: []
priority: high
ordinal: 31000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
ECharts legend default textStyle.color is dark grey, which is invisible against the dark theme background. SergeantD's alpha.8 screenshot shows the legend dots (blue/green/orange/red/blue) but the labels (Setpoint/Flow/Room/Outside/PID) blend into the chart background. Fix: explicit textStyle.color on the legend config in data/graph.js, or read from the design tokens via echarts-theme.js. Ensure labels remain visible on both light and dark themes.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Temperature History chart legend labels render in a colour with adequate contrast against both light and dark theme backgrounds (WCAG AA at minimum)
- [x] #2 The legend dot colours are unchanged (blue/green/orange/red/blue per current series colour assignments)
- [x] #3 Implementation reads from design tokens / echarts-theme.js where possible rather than hardcoding hex values; if hardcoded is necessary, justify in a code comment
- [x] #4 ./build.sh --firmware exits 0 for both ESP8266 and ESP32 targets
- [x] #5 python3 evaluate.py --quick — no new failures
- [ ] #6 Field validation on alpha.14+: tester confirms legend labels are readable on both themes
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. wire legend textStyle.color to --fg-2 design token; 2. preserve dot colours; 3. via getComputedStyle; 4/5/6 verify.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Shipped in alpha.14. data/graph.js legend textStyle.color now reads from the --fg-2 design token via getComputedStyle, so legend labels follow the active theme's foreground colour and meet WCAG AA contrast against both light and dark backgrounds. Dot colours (blue/green/orange/red/blue) unchanged. Build green on both targets; evaluator clean. AC #6 (tester confirms legend labels readable on both themes) gated on field validation.
<!-- SECTION:FINAL_SUMMARY:END -->
