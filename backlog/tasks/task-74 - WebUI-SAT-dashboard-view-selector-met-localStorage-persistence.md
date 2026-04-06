---
id: TASK-74
title: 'WebUI: SAT dashboard view selector met localStorage persistence'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-06 19:15'
updated_date: '2026-04-06 20:10'
labels:
  - webui
  - sat-dashboard
milestone: SAT WebUI Dashboard
dependencies: []
references:
  - 'src/OTGW-firmware/data/index.html (lines 314-457, current SAT page)'
  - src/OTGW-firmware/data/sat.js (SAT module)
  - 'src/OTGW-firmware/data/index.css (line 1183, SAT page styles)'
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Add a dropdown selector to the SAT WebUI page (top-right, next to the status badge) that switches between three dashboard views: Simple (default), Expert, and Diagnostics. The selection is stored in localStorage so it persists across page reloads and sessions. When switching views, sections are shown/hidden with CSS classes - no page reload needed. The dropdown replaces nothing existing, it's an addition to the sat-header div.

Implementation approach:
- Add a select element in sat-header (right-aligned, after badges)
- Three options: "Thermostat" (default), "Expert", "Diagnostics"
- On change: store in localStorage('sat-dashboard-view'), call a view switcher function
- View switcher adds/removes CSS class on sat-dashboard div: sat-view-simple, sat-view-expert, sat-view-diag
- Each sat-section gets a data-view attribute: "simple", "expert", "diag", or combinations
- CSS rules hide sections not matching the current view
- On page load: read localStorage, set dropdown, apply view
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Dropdown select element in sat-header, right-aligned after badges
- [x] #2 Three options: Thermostat (default), Expert, Diagnostics
- [x] #3 Selection persisted in localStorage('sat-dashboard-view')
- [x] #4 View switch is instant (CSS show/hide), no page reload
- [x] #5 Default view is 'Thermostat' when no localStorage value exists
- [x] #6 Each sat-section tagged with data-view attribute for visibility control
- [x] #7 Dropdown styled to match existing UI theme (light + dark)
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
View selector dropdown added to SAT header (46d0fab8). Three options: Thermostat/Expert/Diagnostics. Persisted in localStorage. CSS class switching for instant view changes.
<!-- SECTION:FINAL_SUMMARY:END -->
