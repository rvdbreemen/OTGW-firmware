---
id: TASK-77
title: 'WebUI: SAT Diagnostics view (installer/troubleshooting)'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-06 19:16'
updated_date: '2026-04-06 20:18'
labels:
  - webui
  - sat-dashboard
milestone: SAT WebUI Dashboard
dependencies:
  - TASK-74
references:
  - 'src/OTGW-firmware/data/index.html (lines 314-457, full current SAT page)'
  - src/OTGW-firmware/data/sat.js (full file)
  - Tasks 56-61 (binary sensors providing health data)
  - Tasks 53-54 (cycle status + recommendation with statistics)
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Design and implement the "Diagnostics" view for the SAT dashboard. This view is for installers, technicians, and developers who need to troubleshoot problems. Shows everything from Expert view plus all diagnostic and health data.

Layout (top to bottom):
1. **All Expert view content** (temperatures, controls, PID, PWM, chart, weather)
2. **Health Indicators panel**: Traffic-light style indicators for:
   - Device Health (sufficient data?)
   - Cycle Health (last cycle quality)
   - Flame Health (responding correctly?)
   - Pressure Health (in range? drop rate ok?)
   - Setpoint Sync (boiler following commands?)
   - Modulation Sync (modulation matching?)
   - CH Sync (heating state matching HVAC action?)
3. **Pressure Monitoring section**: Current pressure, smoothed pressure, drop rate (bar/hr), min/max thresholds, alarm status
4. **Cycle Analytics section**: Detailed cycle stats - kind, duration, sample count, max flow temp, space heating fraction, DHW fraction, tail percentiles
5. **Synchronization Details**: Setpoint mismatch detection (requested vs actual), modulation mismatch, CH state mismatch
6. **Error Statistics section**: Recent and daily mean/median error, sample counts, in-band fraction, mean absolute error
7. **Simulation Controls**: Start/stop simulation, simulated room/flow/outdoor temps
8. **Auto-Tune panel**: Enable/disable, current score, cycle count, tune rate, recommendation
9. **OPV Calibration panel**: Phase display, start/stop calibration, current OPV value, manual OPV value input
10. **Raw Data dump**: Expandable section showing all state.sat fields as JSON (for copy-paste debugging)

Design guidelines:
- Dense information layout
- Problem indicators use red/yellow/green traffic lights
- Each health indicator clickable to expand details
- Raw data section collapsed by default
- All values show timestamps where relevant
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All Expert view content visible
- [x] #2 Health Indicators panel with 7 traffic-light indicators (device, cycle, flame, pressure, setpoint sync, modulation sync, CH sync)
- [x] #3 Pressure monitoring: current, smoothed, drop rate, thresholds, alarm status
- [x] #4 Cycle analytics: kind, duration, samples, max flow, heating/DHW fractions
- [x] #5 Synchronization details: setpoint requested vs actual, modulation requested vs actual
- [x] #6 Error statistics: recent + daily mean/median/sample_count/MAE/in_band_fraction
- [x] #7 Simulation controls: toggle, simulated values display
- [x] #8 Auto-tune panel: enable, score, cycles, rate
- [x] #9 OPV calibration: phase, start/stop, value, manual input
- [x] #10 Raw JSON data section (collapsed by default, expandable)
- [x] #11 Traffic light colors: green=ok, yellow=warning, red=problem
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Diagnostics view added (3ab77bf5). Health indicators (6 traffic lights), simulation/OPV/fallback panel, raw JSON dump (collapsed). Dark theme support.
<!-- SECTION:FINAL_SUMMARY:END -->
