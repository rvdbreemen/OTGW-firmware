---
id: TASK-75
title: 'WebUI: SAT Thermostat view (simple, family-friendly)'
status: To Do
assignee: []
created_date: '2026-04-06 19:15'
labels:
  - webui
  - sat-dashboard
milestone: SAT WebUI Dashboard
dependencies:
  - TASK-74
references:
  - >-
    src/OTGW-firmware/data/index.html (lines 323-363, current temp cards +
    controls)
  - >-
    src/OTGW-firmware/data/sat.js (adjustTarget, setPreset, toggleEnable
    functions)
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Design and implement the simple "Thermostat" view for the SAT dashboard. This is the default view, meant for household members who just want to set the temperature. Minimal, clean, no technical jargon.

Layout (top to bottom):
1. **Big temperature display**: Current room temperature (large font, centered) with target temperature below it
2. **Temperature control**: Large +/- buttons to adjust target (0.5C steps), showing target prominently
3. **Preset buttons**: Away / Eco / Home / Comfort / Sleep - large, touch-friendly, highlighted when active
4. **Status line**: Single line showing current state: "Heating to 21.0C" or "Idle" or "Summer mode" or "Disabled"
5. **Simple temperature graph**: Room temp vs target, last 4 hours, no technical overlays

Hidden in this view: PID details, cycle tracking, coefficients, modulation, OPV, manufacturer, all diagnostic data, weather details, heating curve chart.

Design guidelines:
- Touch-friendly (minimum 44px tap targets)
- Works on mobile (responsive)
- Large readable fonts for temperature
- Color coding: blue=cooling/idle, orange=heating, red=error
- Preset buttons clearly show which is active
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Room temperature displayed prominently (large font, min 2rem)
- [ ] #2 Target temperature with +/- buttons (0.5C steps, touch-friendly 44px+)
- [ ] #3 Preset buttons: Away/Eco/Home/Comfort/Sleep with active state highlighting
- [ ] #4 Single status line summarizing current state in plain language
- [ ] #5 Simple temperature graph (room vs target, last 4 hours)
- [ ] #6 All technical sections hidden (PID, cycles, modulation, OPV, etc.)
- [ ] #7 Responsive layout works on mobile (320px minimum width)
- [ ] #8 Enable/Disable SAT toggle available but subtle (not prominent)
<!-- AC:END -->
