---
id: TASK-18
title: 'Interactive WebUI SAT dashboard: adjust target temp + preset buttons'
status: To Do
assignee: []
created_date: '2026-04-05 10:08'
updated_date: '2026-04-05 10:24'
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
- [ ] #1 Target temperature: +/- buttons or slider (step 0.5C) that calls POST /api/v2/sat/target
- [ ] #2 Preset buttons (Away/Eco/Comfort/Sleep) that call POST /api/v2/sat/preset
- [ ] #3 SAT enable/disable toggle that calls POST /api/v2/settings with SATenabled
- [ ] #4 Control mode selector (continuous/pwm/auto) that calls POST /api/v2/sat/control_mode
- [ ] #5 Visual feedback: current preset highlighted, target temp updated immediately
- [ ] #6 Error handling: show error message if REST call fails
- [ ] #7 Responsive design: works on mobile (touch-friendly buttons)
- [ ] #8 No new JS files: extension in existing sat.js
<!-- AC:END -->
