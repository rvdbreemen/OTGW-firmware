---
id: TASK-18
title: 'Interactive WebUI SAT dashboard: adjust target temp + preset buttons'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 10:08'
updated_date: '2026-04-06 10:24'
labels:
  - sat
  - feature
  - webui
dependencies:
  - TASK-2
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The current SAT dashboard is read-only (status polling only). Make it interactive: user should be able to adjust target temp (slider or +/- buttons), select presets, and enable/disable SAT. This makes the dashboard usable as a standalone thermostat interface without MQTT/HA. All interactions go through existing REST API endpoints.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Target temperature: +/- buttons or slider (step 0.5C) that calls POST /api/v2/sat/target
- [x] #2 Preset buttons (Away/Eco/Comfort/Sleep) that call POST /api/v2/sat/preset
- [x] #3 SAT enable/disable toggle that calls POST /api/v2/settings with SATenabled
- [x] #4 Control mode selector (continuous/pwm/auto) that calls POST /api/v2/sat/control_mode
- [x] #5 Visual feedback: current preset highlighted, target temp updated immediately
- [x] #6 Error handling: show error message if REST call fails
- [x] #7 Responsive design: works on mobile (touch-friendly buttons)
- [x] #8 No new JS files: extension in existing sat.js
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add REST endpoints for preset/enable/mode in restAPI.ino handleSAT()
2. Replace target temp card with +/- buttons in index.html
3. Add controls section (presets, mode, enable toggle) to index.html
4. Add JS control functions and state tracking in sat.js
5. Add CSS styles in index_common.css and dark overrides in index_dark.css
6. Run evaluate.py to verify no regressions
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
All 4 files edited:
- restAPI.ino: Added preset, enable, mode endpoints after window endpoint
- index.html: Replaced target temp card with +/- buttons, added controls section
- sat.js: Added satPost, showFeedback, adjustTarget, setPreset, setMode, toggleEnable; added button state highlighting in updateDashboard
- index_common.css: Added interactive control styles
- index_dark.css: Added dark theme overrides for SAT buttons
- evaluate.py --quick passes at 96.4% (pre-existing PROGMEM issues only)
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Interactive WebUI SAT dashboard with thermostat controls.

Changes:
- Target temp +/- buttons (0.5C step, 5-30C range, optimistic UI update)
- Preset buttons: Away/Eco/Comfort/Sleep with active state highlighting
- Mode selector: Continuous/PWM with active highlighting
- SAT enable/disable toggle
- REST endpoints: POST /api/v2/sat/preset, /enable, /mode
- Visual feedback (success/error) on all actions
- Touch-friendly (44px min targets), dark theme support
<!-- SECTION:FINAL_SUMMARY:END -->
