---
id: TASK-84
title: 'WebUI: Settings page for all SAT configuration parameters'
status: To Do
assignee: []
created_date: '2026-04-06 19:23'
labels:
  - webui
  - settings
  - sat-parity
milestone: SAT WebUI Dashboard
dependencies:
  - TASK-83
references:
  - src/OTGW-firmware/data/index.html (existing settings page structure)
  - 'src/OTGW-firmware/data/index.js (translateSettings, settings save logic)'
  - 'Task #83 (REST API endpoints needed for saving)'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
All SAT settings are currently shown in the general settings page mixed with other firmware settings. Create a dedicated SAT settings section (or sub-page accessible from the SAT dashboard) that groups all SAT configuration parameters logically with proper input controls.

**Groups:**
1. **Core Control**: Enable, heating system, manufacturer, heating curve coefficient, deadband, control interval, target temp step, heating mode
2. **PID Tuning**: Automatic gains, gain multiplier, manual Kp/Ki/Kd (when auto=off), overshoot margin
3. **PWM**: Force PWM, cycles per hour, duty cycle, max modulation
4. **Temperature Sources**: Use external temp, sensor max age, BLE settings (ESP32)
5. **Presets**: Comfort/Eco/Away/Sleep/Activity/Home temperatures
6. **DHW**: DHW enabled, DHW setpoint
7. **Pressure**: Min/max pressure, max drop rate, pressure sensor max age
8. **Smart Features**: Solar gain (enable + settings), summer simmer (enable + settings), thermal comfort (enable + settings), thermal learning, multi-area
9. **Safety**: Window detection (enable + min open time), push setpoint, OVP (enable + value)
10. **Energy**: Boiler capacity, min/max consumption
11. **Sync**: Preset sync (enable + topic)
12. **Advanced**: Simulation, auto-tune, error monitoring, flame off offset, flow offset, modulation suppression

Each group should be collapsible. Input types: toggle switches for bools, number inputs with step/min/max for floats/ints, dropdowns for enums. Changes saved via REST API (task #83).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 All 52+ SAT settings accessible from dedicated SAT settings section
- [ ] #2 Settings grouped in 12 logical categories with collapsible headers
- [ ] #3 Bool settings use toggle switches
- [ ] #4 Float/int settings use number inputs with proper min/max/step/unit labels
- [ ] #5 Enum settings use dropdown selects (heating system, manufacturer, heating mode)
- [ ] #6 Changes saved via REST API with success/error feedback
- [ ] #7 Settings page accessible from SAT dashboard (all three views)
- [ ] #8 Responsive layout matching existing settings page style
- [ ] #9 Dark theme support
<!-- AC:END -->
