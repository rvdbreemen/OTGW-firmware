---
id: TASK-2
title: Implement presets fully (Away/Eco/Comfort/Sleep/Activity)
status: To Do
assignee: []
created_date: '2026-04-05 10:02'
updated_date: '2026-04-05 10:18'
labels:
  - sat
  - feature
  - mqtt
  - rest
  - webui
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SAT Python has 5 presets: Away, Sleep, Home (=Eco), Comfort, Activity. The current port has 3 settings (Comfort/Eco/Away) but no active preset switching mechanism. Implement an active preset system: user selects a preset, target temp switches automatically. Activity preset is used by window detection (task 9). Sleep preset is missing from settings struct. Preset must be switchable via MQTT, REST and WebUI. On preset switch: reset PID integral (prevents overshoot on large temp jumps).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 New setting settings.sat.fPresetSleep (default 16.0, range 5-25) added
- [ ] #2 New setting settings.sat.fPresetActivity (default 10.0, range 5-20) added
- [ ] #3 New state field state.sat.iActivePreset (enum: NONE=0, AWAY=1, ECO=2, COMFORT=3, SLEEP=4, ACTIVITY=5)
- [ ] #4 satHandlePreset() handler: switches target temp to preset value and resets PID integral
- [ ] #5 REST API: GET /api/v2/sat/status includes active_preset field
- [ ] #6 REST API: POST /api/v2/sat/preset with body away/eco/comfort/sleep/activity/none
- [ ] #7 MQTT subscribe: set/<nodeId>/sat/preset (payload: away/eco/comfort/sleep/activity/none)
- [ ] #8 MQTT publish: sat/preset (current active preset name as string)
- [ ] #9 MQTT publish: sat/target published after preset switch
- [ ] #10 WebUI: preset selector (dropdown or buttons) in SAT dashboard
- [ ] #11 WebUI: current preset visible in status badge area
- [ ] #12 Settings persistence: all 5 preset temperatures saved and loaded
- [ ] #13 HA auto-discovery: climate entity preset_modes updated in mqttha.cfg
<!-- AC:END -->
