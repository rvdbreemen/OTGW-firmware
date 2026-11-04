---
id: TASK-79
title: 'WebUI: SAT dashboard CSS styling for three views'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-06 19:16'
updated_date: '2026-04-06 20:10'
labels:
  - webui
  - css
  - sat-dashboard
milestone: SAT WebUI Dashboard
dependencies:
  - TASK-74
references:
  - 'src/OTGW-firmware/data/index.css (line 1183+, current SAT styles)'
  - 'src/OTGW-firmware/data/index_dark.css (line 1189+, dark SAT styles)'
  - src/OTGW-firmware/data/index_common.css (shared styles)
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Create the CSS styles for the three SAT dashboard views. This includes: view visibility rules (data-view based show/hide), the thermostat view's large temperature display, the expert view's compact grid layout, the diagnostics view's traffic-light indicators and dense layout. Must support both light and dark themes. Must be responsive (mobile-first).

Key CSS components:
- .sat-view-simple / .sat-view-expert / .sat-view-diag on .sat-dashboard to control section visibility
- .sat-temp-hero: Large centered temperature display for Thermostat view
- .sat-status-summary: Single-line status for Thermostat view
- .sat-health-grid: Traffic-light indicator row for Diagnostics view
- .sat-health-indicator: Circle with green/yellow/red color
- .sat-compact-grid: Denser grid for Expert/Diagnostics views
- .sat-collapsible: Sections that can expand/collapse
- .sat-raw-data: Monospace JSON dump section
- Responsive breakpoints: 320px (phone), 768px (tablet), 1024px+ (desktop)
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 View visibility CSS: sections hidden/shown based on sat-view-* class on parent
- [x] #2 Thermostat view: large temperature hero (3rem+), centered layout
- [x] #3 Expert view: compact 2-column grid on desktop, single column on mobile
- [x] #4 Diagnostics view: traffic light indicators (24px circles, colored)
- [x] #5 Dark theme support in index_dark.css matching all new elements
- [x] #6 Responsive: single column below 768px, multi-column above
- [x] #7 Collapsible section animations (smooth height transition)
- [x] #8 Touch-friendly: all interactive elements min 44px tap target
- [x] #9 Dropdown selector styled consistently with existing nav
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
CSS styling for three views added to both light and dark themes (46d0fab8). View visibility via data-view attributes, larger temps in simple view, consistent dropdown styling.
<!-- SECTION:FINAL_SUMMARY:END -->
